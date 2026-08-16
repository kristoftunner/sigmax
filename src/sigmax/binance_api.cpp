#include "binance_api.hpp"

#include <charconv>
#include <cstdlib>
#include <optional>

#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/json.hpp>
#include <boost/json/value.hpp>
#include <boost/system/detail/error_code.hpp>
#include <string>

#include "log.hpp"
#include "order_type.hpp"

namespace sigmax {

BinanceApi::BinanceApi(const std::vector<std::string> instruments) : instruments_(instruments), ssl_ctx_{ ssl::context::tlsv12_client } {}

const std::string BinanceApi::BuildSubscribeMessage()
{
    std::string base_message = R"({
        "method": "SUBSCRIBE",
        "params": [
        )";
    for (const std::string &instrument : instruments_) { base_message += "\"" + instrument + "@bookTicker\","; }
    base_message += R"({
        ],
        "id": 1
    })";

    return base_message;
}

BinanceApi::ApiReturn BinanceApi::Connect()
{
    boost::system::error_code ec;
    tcp::resolver resolver{ io_ctx_ };
    ws_ = std::make_unique<websocket::stream<ssl::stream<tcp::socket>>>(io_ctx_, ssl_ctx_);

    // Verify the remote server's certificate
    ssl_ctx_.set_verify_mode(ssl::verify_peer, ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to very peer: {}", ec.message());
        return ApiReturn::CONNECTION_ERROR;
    }

    // Look up the domain name
    auto const results = resolver.resolve(kBinanceHost, std::to_string(kBinancePort), ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to resolve {}:{} - {}", kBinanceHost, kBinancePort, ec.message());
        return ApiReturn::CONNECTION_ERROR;
    }

    // Make the connection on the IP address we get from a lookup
    auto ep = net::connect(boost::beast::get_lowest_layer(*ws_.get()), results, ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to connect to {}: {}", results.begin()->host_name(), ec.message());
        return ApiReturn::CONNECTION_ERROR;
    }

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), kBinanceHost)) {
        LOG_ERROR("Failed to setup SNI hostname: ret{}", static_cast<int>(::ERR_get_error()));
        return ApiReturn::CONNECTION_ERROR;
    }

    // Set the expected hostname in the peer certificate for verification
    ws_->next_layer().set_verify_callback(ssl::host_name_verification(kBinanceHost), ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to set verify callback: {}", ec.message());
        return ApiReturn::CONNECTION_ERROR;
    }

    // Update the host_ string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    const std::string host = std::string(kBinanceHost) + ':' + std::to_string(ep.port());

    // Perform the SSL handshake
    ws_->next_layer().handshake(ssl::stream_base::client, ec);
    if (ec.failed()) {
        LOG_ERROR("SSL handshake failed with host {}: {}", kBinanceHost, ec.message());
        return ApiReturn::CONNECTION_ERROR;
    }

    // Set a decorator to change the User-Agent of the handshake
    ws_->set_option(websocket::stream_base::decorator([](websocket::request_type &req) {
        req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " sgmax-datafeed");
    }));

    // Perform the websocket handshake
    ws_->handshake(host, "/ws", ec);
    if (ec.failed()) {
        LOG_ERROR("Handshake failed: {}", ec.message());
        return ApiReturn::CONNECTION_ERROR;
    }

    const std::string subscribe_msg = BuildSubscribeMessage();
    ws_->write(net::buffer(subscribe_msg), ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to send subscribe msg: {} - {}", subscribe_msg, ec.message());
        return ApiReturn::SUBSCRIPTION_ERROR;
    }
    boost::beast::flat_buffer read_buffer;
    /// TODO: add a timeout
    ws_->read(read_buffer, ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to read from binance: {}", subscribe_msg, ec.message());
        return ApiReturn::SUBSCRIPTION_ERROR;
    }

    LOG_INFO("Successfully connected to binance websocket and subscribed to instruments: {}", subscribe_msg);
    return ApiReturn::SUCCESS;
}

BinanceApi::ApiReturn BinanceApi::Close() { ws_->close(websocket::close_code::normal); }

static std::optional<std::int64_t> ParseFixed(const std::string_view &fp_number)
{
    /// Find the "." -> if not found, ret nullopt
    /// calculate the number: 10e8 * integer part + 10e(8 - partial part numbers) * partial part
    if (const std::size_t point_idx{ fp_number.find(".") }; point_idx != std::string_view::npos) {
        int64 real_part{ 0 }, fractional_part{ 0 };
        const std::string_view real_part_str{ fp_number.substr(0, point_idx) };
        const std::string_view fractional_part_str{ fp_number.substr(point_idx + 1, fp_number.size() - (point_idx + 1)) };
        auto [_1, ec_real]{ std::from_chars(real_part_str.data(), real_part_str.data() + real_part_str.size(), real_part) };

        /// truncate the franctional parts to 8 decimals, since that is the maximum we represent in the fixed point number
        const std::size_t frac_decimals = fractional_part_str.size() > 8 ? 8 : fractional_part_str.size();
        auto [_2,
            ec_fractional]{ std::from_chars(fractional_part_str.data(), fractional_part_str.data() + frac_decimals, fractional_part) };
        if ((ec_real == std::errc()) && (ec_fractional == std::errc())) {
            const int64 real{ static_cast<int64>(pow(10, kFixedPointShift)) * real_part };
            const int64 fractional{ static_cast<int64>(pow(10, kFixedPointShift - frac_decimals)) * fractional_part };
            const int64 result{ real_part + fractional_part };
            return result;
        }
    }

    return std::nullopt;
}

static std::optional<BidsAsks> ParseBidAsk(const boost::json::array &tuple)
{
    const std::string price_str{ tuple[0].as_string() };
    const std::string quantity_str{ tuple[1].as_string() };
    const auto price{ ParseFixed(price_str) };
    const auto quantity{ ParseFixed(quantity_str) };
    if (price && quantity) {
        return BidsAsks{ price.value(), quantity.value() };
    } else {
        if (!price) LOG_ERROR("Failed to parse string to price: {price_str}");
        if (!quantity) LOG_ERROR("Failed to parse string to price: {quantity_str}");
        return std::nullopt;
    }
}

/// @brief Parsing a book event
/// Example message:
static std::optional<BookDepthUpdate> ParseBookEvent(const boost::json::value &message)
{
    BookDepthUpdate event{};
    try {
        const std::string evet_type{ boost::json::value_to<std::string>(message.at("e")) };
        event.event_ts = boost::json::value_to<Timestamp>(message.at("E"));
        const std::string symbol_str{ boost::json::value_to<std::string>(message.at("s")) };
        event.symbol = StrToSymbol(symbol_str);
        event.first_update_id = boost::json::value_to<std::int64_t>(message.at("U"));
        event.final_update_id = boost::json::value_to<std::int64_t>(message.at("u"));
        const boost::json::array &bids{ message.at("b").as_array() };
        for (const boost::json::value &val : bids) {
            const std::optional<BidsAsks> bid{ ParseBidAsk(val.as_array()) };
            if (bid) { event.bids.emplace_back(bid.value()); }
        }
        const boost::json::array &asks{ message.at("a").as_array() };
        for (const boost::json::value &val : asks) {
            const std::optional<BidsAsks> ask{ ParseBidAsk(val.as_array()) };
            if (ask) { event.asks.emplace_back(ask.value()); }
        }

    } catch (boost::system::system_error &error) {
        return std::nullopt;
    }
}

std::expected<BookDepthUpdate, BinanceApi::ApiReturn> BinanceApi::DepthUpdate()
{
    /// TODO: TECH DEBT - use async read instead
    beast::flat_buffer buffer;
    boost::system::error_code ec;
    ws_->read(buffer, ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to read from websocket: {}", ec.message());
        return std::unexpected(ApiReturn::CONNECTION_ERROR);
    }

    /// Parse book event
    std::string_view sv{ static_cast<const char *>(buffer.cdata().data()), buffer.size() };
    boost::json::value message{ boost::json::parse(sv, ec) };
    if (!ec) {
        LOG_ERROR("Failed to parse input message into json: {}", sv);
        return std::unexpected(ApiReturn::INVALID_MESSAGE);
    }
    const auto bookEvent{ ParseBookEvent(message) };

    if (bookEvent) {
        return bookEvent.value();
    } else {
        LOG_ERROR("Failed to parse message into BookEvent: {}", sv);
        return std::unexpected(ApiReturn::INVALID_MESSAGE);
    }
}
}// namespace sigmax
