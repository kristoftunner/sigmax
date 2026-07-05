#include <expected>

#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

namespace net = boost::asio;// from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;// from <boost/asio/ssl.hpp>
namespace http = boost::beast::http;// from <boost/beast/http.hpp>
namespace websocket = boost::beast::websocket;// from <boost/beast/websocket.hpp>
namespace beast = boost::beast;

using tcp = boost::asio::ip::tcp;// from <boost/asio/ip/tcp.hpp>

namespace sigmax {
/// TODO: create proper ApiReturn values
enum class ApiReturn {
    SUCCESS = 0,
    INTERNAL_FAILURE = 1,
};

class BinanceApi
{
public:
    ApiReturn Connect();
    std::expected<beast::flat_buffer, ApiReturn> Read();
    ApiReturn Close();

private:
    static constexpr const char *kBinanceHost{ "wss://stream.binance.com" };
    static constexpr int kBinancePort{ 9443 };

    ssl::context m_ctx;
    net::io_context m_ioCtx;
    std::unique_ptr<websocket::stream<ssl::stream<tcp::socket>>> m_ws;
};
}// namespace sigmax
