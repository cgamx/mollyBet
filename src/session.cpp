#include "session.h"
#include "log/logger.h"
//--
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/strand.hpp>
//--
#include <nlohmann/json.hpp>
//--
#include <cstdint>
#include <string>
#include <string_view>
#include <functional>

//-------------------------------------
using namespace MindShake;

using json = nlohmann::json;

// Report a failure
//-------------------------------------
static void
fail(beast::error_code ec, char const* what) {
    Logger::error("Beast Error {} on {}: {}", ec.value(), what, ec.message());
}

// Utils
//-------------------------------------
static std::string
bufferToString(beast::flat_buffer &buffer) {
    return std::string {
        static_cast<const char *>(buffer.data().data()),
        buffer.data().size()
    };
}

//-------------------------------------
static std::string_view
bufferToStringView(beast::flat_buffer &buffer) {
    return std::string_view {
        static_cast<const char *>(buffer.data().data()),
        buffer.data().size()
    };
}

//-------------------------------------

// Note: I'm using snake_case for the session class to follow the boost beast examples style
// It's actually the style I like the least. In general I prefer camelCase or PascalCase

//-------------------------------------
Session::Session(net::io_context &ioc, ssl::context &ctx)
    : mResolver(net::make_strand(ioc))
    , mStream(net::make_strand(ioc), ctx)
    , mWS(net::make_strand(ioc), ctx)
{
}

//-------------------------------------
bool
Session::run(const std::string &host, const std::string &port, UserParserFunc userParser) {
    if (host.empty() || port.empty()) {
        Logger::error("Run: Invalid parameters: host = '{}', port = '{}'", host, port);
        return false;
    }
    if (!userParser) {
        Logger::error("Invalid function");
        return false;
    }

    // Save these for later
    mHost = host;
    mPort = port;
    mUserParser = userParser;

    // Look up the domain name
    mResolver.async_resolve(host,port,beast::bind_front_handler(&Session::on_resolve, shared_from_this()));

    return true;
}

//-------------------------------------
void
Session::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    Logger::debug("on_resolve");
    if (ec) {
        return fail(ec, "resolve");
    }

    if (mToken.empty()) {
        // Set a timeout on the operation
        beast::get_lowest_layer(mStream).expires_after(std::chrono::seconds(30));
        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(mStream).async_connect(results, beast::bind_front_handler(&Session::on_connect, shared_from_this()));
    }
    else {
        // Set a timeout on the operation
        beast::get_lowest_layer(mWS).expires_after(std::chrono::seconds(30));
        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(mWS).async_connect(results, beast::bind_front_handler(&Session::on_connect, shared_from_this()));
    }
}

//-------------------------------------
void
Session::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
    Logger::debug("on_connect");
    if (ec) {
        return fail(ec, "connect");
    }

    if (mToken.empty()) {
        // Set a timeout on the operation
        beast::get_lowest_layer(mStream).expires_after(std::chrono::seconds(30));

        // Perform the SSL handshake
        mStream.async_handshake(ssl::stream_base::client, beast::bind_front_handler(&Session::on_ssl_handshake, shared_from_this()));
    }
    else {
        // Set a timeout on the operation
        beast::get_lowest_layer(mWS).expires_after(std::chrono::seconds(30));

        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (!SSL_set_tlsext_host_name(mWS.next_layer().native_handle(), mHost.c_str())) {
            ec = beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
            return fail(ec, "connect");
        }

        // Update the mHost string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        mHost += ':' + std::to_string(ep.port());

        // Perform the SSL handshake
        mWS.next_layer().async_handshake(ssl::stream_base::client, beast::bind_front_handler(&Session::on_ssl_handshake, shared_from_this()));
    }
}

//-------------------------------------
void
Session::on_ssl_handshake(beast::error_code ec) {
    Logger::debug("on_ssl_handshake");
    if (ec) {
        return fail(ec, "ssl_handshake");
    }

    if (mToken.empty()) {
        mReq.version(11);
        mReq.method(http::verb::post);
        mReq.target("/v1/sessions/");
        mReq.set(http::field::host, mHost);
        mReq.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        mReq.set(http::field::content_type, "application/json");
        mReq.body() = fmt::format(R"({{"username": "{}", "password": "{}"}})", mUserName, mPassword);
        mReq.prepare_payload();

        http::async_write(mStream, mReq, beast::bind_front_handler(&Session::on_write_post, shared_from_this()));
    }
    else {

        // Turn off the timeout on the tcp_stream, because
        // the websocket stream has its own timeout system.
        beast::get_lowest_layer(mWS).expires_never();

        // Set suggested timeout settings for the websocket
        mWS.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

        // Set a decorator to change the User-Agent of the handshake
        mWS.set_option(websocket::stream_base::decorator([](websocket::request_type &req) {
            req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async-ssl");
        }));

        // Perform the websocket handshake
        std::string target = "/v1/stream/?token=" + mToken;
        mWS.async_handshake(mHost, target, beast::bind_front_handler(&Session::on_wss_handshake, shared_from_this()));
    }
}

//-------------------------------------
void
Session::on_wss_handshake(beast::error_code ec) {
    Logger::debug("on_handshake");
    if (ec) {
        return fail(ec, "handshake");
    }

    mWS.async_read(mBuffer, beast::bind_front_handler(&Session::on_read, shared_from_this()));
}

//-------------------------------------
void
Session::on_write_post(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    Logger::debug("on_write_post");

    if (ec) {
        return fail(ec, "write post");
    }

    // Read a message into our buffer
    http::async_read(mStream, mBuffer, mRes, beast::bind_front_handler(&Session::on_read_token, shared_from_this()));
}

//-------------------------------------
void 
Session::on_read_token(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    Logger::debug("on_read_token");

    if (ec) {
        return fail(ec, "read token");
    }

    bool ok = true;

    try {
        json data = json::parse(mRes.body());

        mToken = data["data"];

        std::string status = data["status"];

        Logger::debug("Token: '{}', status: '{}'", mToken, status);
    }
    catch (...) {
        Logger::exception("Error getting token");
        ok = false;
    }
    
    mBuffer.consume(mBuffer.size());

    if (ok) {
        mResolver.async_resolve(mHost, mPort, beast::bind_front_handler(&Session::on_resolve, shared_from_this()));
    }
    else {
        mStream.async_shutdown(beast::bind_front_handler(&Session::on_close, shared_from_this()));

    }
}

//-------------------------------------
void
Session::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    Logger::debug("on_read");

    if (ec) {
        return fail(ec, "read");
    }

    auto received = bufferToStringView(mBuffer);
    //Logger::info("Received: {}", received);
    bool syncFound = mUserParser(received);
    mBuffer.consume(mBuffer.size());
    if (syncFound == false) {
        mWS.async_read(mBuffer, beast::bind_front_handler(&Session::on_read, shared_from_this()));
    }
    else {
        mWS.async_close(websocket::close_code::normal, beast::bind_front_handler(&Session::on_close, shared_from_this()));
    }
}

//-------------------------------------
void
Session::on_close(beast::error_code ec) {
    Logger::debug("on_close");
    if (ec) {
        return fail(ec, "close");
    }

    // If we get here then the connection is closed gracefully
}
