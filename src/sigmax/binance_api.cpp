#include "binance_api.hpp"

#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/field.hpp>
#include <string>

#include "log.hpp"

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
        LOG_ERROR("Failed to connect to {}: {}", results->host_name(), ec.message());
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

    const auto subscribe_msg = BuildSubscribeMessage();
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

std::expected<beast::flat_buffer, BinanceApi::ApiReturn> BinanceApi::Read()
{
    beast::flat_buffer buffer;
    boost::system::error_code ec;
    ws_->read(buffer, ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to read from websocket: {}", ec.message());
        return std::unexpected(ApiReturn::CONNECTION_ERROR);
    } else {
        return std::move(buffer);
    }
}
}// namespace sigmax
