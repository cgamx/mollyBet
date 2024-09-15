#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ssl.hpp>

//-------------------------------------
namespace beast     = boost::beast;         // from <boost/beast.hpp>
namespace http      = beast::http;          // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;     // from <boost/beast/websocket.hpp>
namespace net       = boost::asio;          // from <boost/asio.hpp>
namespace ssl       = boost::asio::ssl;     // from <boost/asio/ssl.hpp>
using tcp           = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

//-------------------------------------
class Session : public std::enable_shared_from_this<Session> {
    using UserParserFunc = std::function<bool(const std::string_view &)>;

    tcp::resolver mResolver;
    ssl::stream<beast::tcp_stream> mStream; // HTTPS
    websocket::stream<ssl::stream<beast::tcp_stream>> mWS;  // WSS
    beast::flat_buffer mBuffer;
    std::string mHost;
    std::string mPort;
    std::string mToken;
    std::string mUserName;
    std::string mPassword;
    UserParserFunc mUserParser;

    http::request<http::string_body> mReq;
    http::response<http::string_body> mRes;

public:
    // Resolver and socket require an io_context
    explicit Session(net::io_context &ioc, ssl::context &ctx);

    // Set credentials before calling run
    void setCredentials(const std::string &user, const std::string &password) { mUserName = user, mPassword = password;  }

    // Start the asynchronous operation
    bool run(const std::string &host, const std::string &port, UserParserFunc userParser);

protected:
    void onResolve(beast::error_code ec, tcp::resolver::results_type results);

    void onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);

    void onSSLHandshake(beast::error_code ec);

    void onWSSHandshake(beast::error_code ec);

    void onWritePost(beast::error_code ec, std::size_t bytes_transferred);
    
    void onReadToken(beast::error_code ec, std::size_t bytes_transferred);

    void onReadData(beast::error_code ec, std::size_t bytes_transferred);

    void onClose(beast::error_code ec);
};