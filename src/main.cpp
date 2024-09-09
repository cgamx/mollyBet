//------------------------------------------------------------------------------
// From Example: WebSocket SSL client, asynchronous
//------------------------------------------------------------------------------

#include "root_certificates.hpp"
#include "log/loggerColorConsole.h"
#include "session.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <unordered_set>

//-------------------------------------
using namespace MindShake;

//-------------------------------------
static bool
getValue(const json &j, std::string &value, const std::string &key = "none") {
    try {
        value = j.get<std::string>();
        return true;
    }
    catch (const json::type_error &e) {
        Logger::exception("Type error parsing key '{}': {}", key, e.what());
    }
    catch (const json::out_of_range &e) {
        Logger::exception("Out of range error parsing key '{}': {}", key, e.what());
    }
    catch (const json::exception &e) {
        Logger::exception("Exception error parsing key '{}': {}", key, e.what());
    }
    catch (const std::exception &e) {
        Logger::exception("Exception error parsing key '{}': {}", key, e.what());
    }
    catch (...) {
        Logger::exception("Exception error parsing key '{}'", key);
    }

    return false;
}

//-------------------------------------
static void
iterateJSON(const json &j, bool &syncFound, std::string &token, std::unordered_set<std::string> &competitions) {
    if (j.is_object()) {
        std::string key, value;
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it.value().is_object() || it.value().is_array()) {
                iterateJSON(it.value(), syncFound, token, competitions);
                continue;
            }

            key = it.key();
            if (key == "competition_name") {
                if (getValue(it.value(), value, key)) {
                    competitions.emplace(value);
                }
            }
            else if (syncFound && key == "token") {
                if (getValue(it.value(), value, key)) {
                    token = value;
                }
            }
        }
    }
    else if (j.is_array()) {
        for (const auto &item : j) {
            iterateJSON(item, syncFound, token, competitions);
        }
    }
    else if (j.is_string()) {
        auto name = j.get<std::string>();
        if (name == "sync") {
            syncFound = true;
        }
    }
}

//-------------------------------------
int
main(int argc, char** argv) {
    LoggerColorConsole   console;
    Logger::setLevel(LogLevel::Info);

    // Check command line arguments.
    if(argc != 3) {
        Logger::error(R"(
Usage: {0} <host> <port> <text>"
Example:
    {0} api.mollybet.com 443
        )", argv[0]);

        return EXIT_FAILURE;
    }

    auto const host = argv[1];
    auto const port = argv[2];

    // The io_context is required for all I/O
    net::io_context ioc;

    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::tlsv12_client};

    // This holds the root certificate used for verification
    load_root_certificates(ctx);

    bool syncFound {};
    std::string token;
    std::unordered_set<std::string> competitions;

    // Launch the asynchronous operation
    auto session = std::make_shared<Session>(ioc, ctx);
    session->set_credentials("devinterview", "OwAb6wrocirEv");
    bool result = session->run(host, port, [&](const std::string_view &received) {
        json data = json::parse(received);

        iterateJSON(data, syncFound, token, competitions);
        return syncFound;
    });
    if (result == false) {
        return EXIT_FAILURE;
    }

    // Run the I/O service. The call will return when
    // the socket is closed.
    try {
        ioc.run();
    }
    catch (const std::exception &e) {
        Logger::exception("Exception: {}", e.what());
    }
    catch (...) {
        Logger::exception("Exception: IOC");
    }

    if (syncFound) {
        Logger::debug("Sync Found: token = {}", token);
        for (const auto &c : competitions) {
            Logger::info("Competition: {}", c);
        }
    }

    return EXIT_SUCCESS;
}
