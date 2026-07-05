#include "binance_api.hpp"

#include <boost/beast/core/flat_buffer.hpp>
#include <string>

#include "log.hpp"

namespace sigmax {

ApiReturn BinanceApi::Connect()
{
    /// TODO: all these functions are throwing on error, catch those
    auto const address = net::ip::make_address(kBinanceHost);
    //
    // The SSL context is required, and holds certificates
    ssl::context ctx{ ssl::context::tlsv12_client };

    // Verify the remote server's certificate
    ctx.set_verify_mode(ssl::verify_peer);

    tcp::resolver resolver{ m_ioCtx };
    m_ws = std::make_unique<websocket::stream<ssl::stream<tcp::socket>>>(m_ioCtx, ctx);

    // Verify the remote server's certificate
    boost::system::error_code ec;
    m_ctx.set_verify_mode(ssl::verify_peer, ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to very peer: {}", ec.message());
        return ApiReturn::INTERNAL_FAILURE;
    }

    // Look up the domain name
    auto const results = resolver.resolve(kBinanceHost, std::to_string(kBinancePort), ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to resolve {}: {}", kBinanceHost, ec.message());
        return ApiReturn::INTERNAL_FAILURE;
    }

    // Make the connection on the IP address we get from a lookup
    auto ep = net::connect(boost::beast::get_lowest_layer(*m_ws.get()), results, ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to connect to {}: {}", results->host_name(), ec.message());
        return ApiReturn::INTERNAL_FAILURE;
    }

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(m_ws->next_layer().native_handle(), kBinanceHost)) {
        /// TODO: create a return statement here + log error
        throw beast::system_error(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
    }

    // Set the expected hostname in the peer certificate for verification
    m_ws->next_layer().set_verify_callback(ssl::host_name_verification(kBinanceHost), ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to set verify callback: {}", ec.message());
        return ApiReturn::INTERNAL_FAILURE;
    }

    // Update the host_ string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    const std::string host = kBinanceHost + ':' + std::to_string(ep.port());

    // Perform the SSL handshake
    m_ws->next_layer().handshake(ssl::stream_base::client, ec);
    if (ec.failed()) {
        LOG_ERROR("SSL handshake failed with host {}: {}", kBinanceHost, ec.message());
        return ApiReturn::INTERNAL_FAILURE;
    }

    // Set a decorator to change the User-Agent of the handshake
    m_ws->set_option(websocket::stream_base::decorator([](websocket::request_type &req) {
        req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
    }));

    // Perform the websocket handshake
    m_ws->handshake(host, "/", ec);
    if (ec.failed()) {
        LOG_ERROR("Handshake failed: {}", ec.message());
        return ApiReturn::INTERNAL_FAILURE;
    }

    return ApiReturn::SUCCESS;
}

ApiReturn BinanceApi::Close() { m_ws->close(websocket::close_code::normal); }

std::expected<beast::flat_buffer, ApiReturn> BinanceApi::Read()
{
    beast::flat_buffer buffer;
    boost::system::error_code ec;
    m_ws->read(buffer, ec);
    if (ec.failed()) {
        LOG_ERROR("Failed to read from websocket: {}", ec.message());
        return std::unexpected(ApiReturn::INTERNAL_FAILURE);
    } else {
        return std::move(buffer);
    }
}
}// namespace sigmax
