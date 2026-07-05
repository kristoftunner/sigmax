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
class BinanceApi
{
public:
    /// TODO: create proper ApiReturn values
    enum class ApiReturn {
        SUCCESS = 0,
        CONNECTION_ERROR = 1,
        SUBSCRIPTION_ERROR = 2,
    };
public:
    BinanceApi(const std::vector<std::string> instruments);

    ApiReturn Connect();
    std::expected<beast::flat_buffer, ApiReturn> Read();
    ApiReturn Close();

private:
    const std::string BuildSubscribeMessage();
private:
    static constexpr const char *kBinanceHost{ "stream.binance.com" };
    static constexpr int kBinancePort{ 9443 };

    /// \note: networking/websocket specific members
    ssl::context ssl_ctx_;
    net::io_context io_ctx_;
    std::unique_ptr<websocket::stream<ssl::stream<tcp::socket>>> ws_;

    const std::vector<std::string> instruments_;
};
}// namespace sigmax
