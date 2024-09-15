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
fail(beast::error_code ec, const char *function) {
    Logger::error("{}: Beast Error {}: {}", function, ec.value(), ec.message());
}

// Utils
//-------------------------------------
static std::string
bufferToString(beast::flat_buffer &buffer) {
    return beast::buffers_to_string(buffer.data());
}

//-------------------------------------
static std::string_view
bufferToStringView(beast::flat_buffer &buffer) {
    auto buffers = buffer.data();
    auto size = boost::asio::buffer_size(buffers);
    if (size == 0)
        return {};

    const char *data = static_cast<const char*>(buffers.data());
    return { data, size };
}

//-------------------------------------
constexpr const auto kConnectionTimeout = std::chrono::seconds(30);

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
        Logger::error("Invalid user function");
        return false;
    }

    if (mUserName.empty() || mPassword.empty()) {
        Logger::error("Credentials are not set");
        return false;
    }

    // Save these for later
    mHost = host;
    mPort = port;
    mUserParser = userParser;

    // Look up the domain name
    mResolver.async_resolve(host,port,beast::bind_front_handler(&Session::onResolve, shared_from_this()));

    return true;
}

//-------------------------------------
void
Session::onResolve(beast::error_code ec, tcp::resolver::results_type results) {
    Logger::debug(__func__);
    if (ec) {
        return fail(ec, __func__);
    }

    if (mToken.empty()) {
        // Set a timeout on the operation
        beast::get_lowest_layer(mStream).expires_after(kConnectionTimeout);
        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(mStream).async_connect(results, beast::bind_front_handler(&Session::onConnect, shared_from_this()));
    }
    else {
        // Set a timeout on the operation
        beast::get_lowest_layer(mWS).expires_after(kConnectionTimeout);
        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(mWS).async_connect(results, beast::bind_front_handler(&Session::onConnect, shared_from_this()));
    }
}

//-------------------------------------
void
Session::onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
    Logger::debug(__func__);
    if (ec) {
        return fail(ec, __func__);
    }

    if (mToken.empty()) {
        // Set a timeout on the operation
        beast::get_lowest_layer(mStream).expires_after(kConnectionTimeout);

        // Perform the SSL handshake
        mStream.async_handshake(ssl::stream_base::client, beast::bind_front_handler(&Session::onSSLHandshake, shared_from_this()));
    }
    else {
        // Set a timeout on the operation
        beast::get_lowest_layer(mWS).expires_after(kConnectionTimeout);

        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (!SSL_set_tlsext_host_name(mWS.next_layer().native_handle(), mHost.c_str())) {
            ec = beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
            return fail(ec, __func__);
        }

        // Update the mHost string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        mHost += ':' + std::to_string(ep.port());

        // Perform the SSL handshake
        mWS.next_layer().async_handshake(ssl::stream_base::client, beast::bind_front_handler(&Session::onSSLHandshake, shared_from_this()));
    }
}

//-------------------------------------
void
Session::onSSLHandshake(beast::error_code ec) {
    Logger::debug(__func__);
    if (ec) {
        return fail(ec, __func__);
    }

    if (mToken.empty()) {
        mReq.version(11);
        mReq.method(http::verb::post);
        mReq.target("/v1/sessions/");
        mReq.set(http::field::host, mHost);
        mReq.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        mReq.set(http::field::content_type, "application/json");
        json body;
        body["username"] = mUserName;
        body["password"] = mPassword;
        mReq.body() = body.dump();
        //mReq.body() = fmt::format(R"({{"username": "{}", "password": "{}"}})", mUserName, mPassword);
        mReq.prepare_payload();

        http::async_write(mStream, mReq, beast::bind_front_handler(&Session::onWritePost, shared_from_this()));
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
        mWS.async_handshake(mHost, target, beast::bind_front_handler(&Session::onWSSHandshake, shared_from_this()));
    }
}

//-------------------------------------
void
Session::onWSSHandshake(beast::error_code ec) {
    Logger::debug(__func__);
    if (ec) {
        return fail(ec, __func__);
    }

    mWS.async_read(mBuffer, beast::bind_front_handler(&Session::onReadData, shared_from_this()));
}

//-------------------------------------
void
Session::onWritePost(beast::error_code ec, std::size_t bytesTransferred) {
    boost::ignore_unused(bytesTransferred);
    Logger::debug(__func__);

    if (ec) {
        return fail(ec, __func__);
    }

    // Read a message into our buffer
    http::async_read(mStream, mBuffer, mRes, beast::bind_front_handler(&Session::onReadToken, shared_from_this()));
}

//-------------------------------------
void
Session::onReadToken(beast::error_code ec, std::size_t bytesTransferred) {
    boost::ignore_unused(bytesTransferred);
    Logger::debug(__func__);

    if (ec) {
        return fail(ec, __func__);
    }

    bool ok = true;

    try {
        json data = json::parse(mRes.body());

        mToken = data["data"];

        std::string status = data["status"];

        Logger::debug("Token: '{}', status: '{}'", mToken, status);
    }
    catch (const json::parse_error &e) {
        Logger::error("Error parsing JSON: {}", e.what());
        ok = false;
    }
    catch (const std::exception &e) {
        Logger::error("Exception: {}", e.what());
        ok = false;
    }
    catch (...) {
        Logger::exception("Unknown exception occurred getting token");
        ok = false;
    }

    mBuffer.consume(mBuffer.size());

    if (ok) {
        mResolver.async_resolve(mHost, mPort, beast::bind_front_handler(&Session::onResolve, shared_from_this()));
    }
    else {
        mStream.async_shutdown(beast::bind_front_handler(&Session::onClose, shared_from_this()));

    }
}

//-------------------------------------
void
Session::onReadData(beast::error_code ec, std::size_t bytesTransferred) {
    boost::ignore_unused(bytesTransferred);
    Logger::debug(__func__);

    if (ec == websocket::error::closed) {
        Logger::info("WebSocket connection closed");
        return;
    }
    if (ec) {
        return fail(ec, __func__);
    }

    auto received = bufferToStringView(mBuffer);
    //Logger::info("Received: {}", received);
    bool syncFound = mUserParser(received);
    mBuffer.consume(mBuffer.size());
    if (syncFound == false) {
        mWS.async_read(mBuffer, beast::bind_front_handler(&Session::onReadData, shared_from_this()));
    }
    else {
        mWS.async_close(websocket::close_code::normal, beast::bind_front_handler(&Session::onClose, shared_from_this()));
    }
}

//-------------------------------------
void
Session::onClose(beast::error_code ec) {
    Logger::debug(__func__);
    if (ec && ec != websocket::error::closed) {
        return fail(ec, __func__);
    }

    // If we get here then the connection is closed gracefully
}
