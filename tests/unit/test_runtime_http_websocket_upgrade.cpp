#include <catch2/catch_all.hpp>
#include <keet/http.hpp>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace keet::net::http;

namespace {

keet::net::detail::socket_t connect_client(int port) {
    keet::net::detail::ensure_winsock_initialized();
    keet::net::detail::socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == keet::net::detail::kInvalidSocket) {
        return keet::net::detail::kInvalidSocket;
    }
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        keet::net::detail::close_raw_socket(s);
        return keet::net::detail::kInvalidSocket;
    }
    return s;
}

std::string read_http_response(keet::net::detail::socket_t s, int timeout_ms = 3000) {
    std::string out;
    std::array<char, 1024> buf;
    size_t expected_len = std::string::npos;
    size_t body_start = std::string::npos;

    while (true) {
        int pr = detail::socket_poll(s, POLLIN, timeout_ms);
        if (pr <= 0) {
            break;
        }
        ssize_t n = detail::socket_recv(s, buf.data(), buf.size());
        if (n <= 0) {
            break;
        }
        out.append(buf.data(), static_cast<size_t>(n));

        if (body_start == std::string::npos) {
            size_t header_end = out.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                body_start = header_end + 4;

                std::string lower_headers = detail::to_lower_ascii(out.substr(0, header_end));
                size_t cl_pos = lower_headers.find("content-length:");
                if (cl_pos != std::string::npos) {
                    size_t val_start = cl_pos + 15;
                    size_t val_end = lower_headers.find("\r\n", val_start);
                    std::string cl_str = std::string(detail::trim_ascii(lower_headers.substr(val_start, val_end - val_start)));
                    size_t cl = 0;
                    std::from_chars(cl_str.data(), cl_str.data() + cl_str.size(), cl);
                    expected_len = body_start + cl;
                } else if (out.find("101 Switching Protocols") != std::string::npos) {
                    // Handshake 101 has no body; headers end with \r\n\r\n
                    expected_len = body_start;
                }
            }
        }

        if (expected_len != std::string::npos && out.size() >= expected_len) {
            break;
        }
    }
    return out;
}

} // namespace

TEST_CASE("WebSocket - RFC 6455 Cryptographic Accept Key Computation", "[runtime][http][websocket][crypto]") {
    SECTION("RFC 6455 Section 1.3 standard sample nonce test vector") {
        std::string client_key = "dGhlIHNhbXBsZSBub25jZQ==";
        std::string accept = compute_websocket_accept(client_key);
        CHECK(accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

    SECTION("Additional nonce accept key calculation") {
        std::string client_key = "x3JJHMbDL1EzLkh9GBhXDw==";
        std::string accept = compute_websocket_accept(client_key);
        CHECK(!accept.empty());
        CHECK(accept.size() == 28);
        CHECK(accept.back() == '=');
    }
}

TEST_CASE("WebSocket - Upgrade Header Detection & Validation", "[runtime][http][websocket][validation]") {
    SECTION("Valid RFC 6455 Upgrade request headers pass validation") {
        HttpRequestMessage msg;
        msg.line.method = "GET";
        msg.line.version = "HTTP/1.1";
        msg.line.path = "/chat";
        msg.headers.set("Host", "server.example.com");
        msg.headers.set("Upgrade", "websocket");
        msg.headers.set("Connection", "Upgrade");
        msg.headers.set("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
        msg.headers.set("Sec-WebSocket-Version", "13");

        Request req(msg);
        CHECK(is_websocket_upgrade(req) == true);

        auto val = validate_websocket_upgrade(req);
        CHECK(val.status == WebSocketUpgradeStatus::Success);
        CHECK(val.accept_key == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");

        auto resp = make_websocket_upgrade_response(req);
        CHECK(resp.status == 101);
        CHECK(resp.headers.get_or("Upgrade", "") == "websocket");
        CHECK(resp.headers.get_or("Connection", "") == "Upgrade");
        CHECK(resp.headers.get_or("Sec-WebSocket-Accept", "") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

    SECTION("Case-insensitivity of Upgrade and Connection tokens") {
        HttpRequestMessage msg;
        msg.line.method = "GET";
        msg.line.version = "HTTP/1.1";
        msg.line.path = "/chat";
        msg.headers.set("Upgrade", "WebSocket");
        msg.headers.set("Connection", "keep-alive, Upgrade");
        msg.headers.set("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
        msg.headers.set("Sec-WebSocket-Version", "13");

        Request req(msg);
        CHECK(is_websocket_upgrade(req) == true);
        auto val = validate_websocket_upgrade(req);
        CHECK(val.status == WebSocketUpgradeStatus::Success);
    }

    SECTION("Non-GET method rejects upgrade") {
        HttpRequestMessage msg;
        msg.line.method = "POST";
        msg.line.version = "HTTP/1.1";
        msg.headers.set("Upgrade", "websocket");
        msg.headers.set("Connection", "Upgrade");
        msg.headers.set("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
        msg.headers.set("Sec-WebSocket-Version", "13");

        Request req(msg);
        CHECK(is_websocket_upgrade(req) == false);
        auto val = validate_websocket_upgrade(req);
        CHECK(val.status == WebSocketUpgradeStatus::NotAnUpgrade);
    }

    SECTION("Unsupported version returns 426 Upgrade Required with Sec-WebSocket-Version: 13") {
        HttpRequestMessage msg;
        msg.line.method = "GET";
        msg.line.version = "HTTP/1.1";
        msg.headers.set("Upgrade", "websocket");
        msg.headers.set("Connection", "Upgrade");
        msg.headers.set("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
        msg.headers.set("Sec-WebSocket-Version", "8");

        Request req(msg);
        CHECK(is_websocket_upgrade(req) == true);
        auto val = validate_websocket_upgrade(req);
        CHECK(val.status == WebSocketUpgradeStatus::VersionMismatch);

        auto resp = make_websocket_upgrade_response(req);
        CHECK(resp.status == 426);
        CHECK(resp.headers.get_or("Sec-WebSocket-Version", "") == "13");
    }

    SECTION("Missing Sec-WebSocket-Key returns 400 Bad Request") {
        HttpRequestMessage msg;
        msg.line.method = "GET";
        msg.line.version = "HTTP/1.1";
        msg.headers.set("Upgrade", "websocket");
        msg.headers.set("Connection", "Upgrade");
        msg.headers.set("Sec-WebSocket-Version", "13");

        Request req(msg);
        auto val = validate_websocket_upgrade(req);
        CHECK(val.status == WebSocketUpgradeStatus::InvalidKey);

        auto resp = make_websocket_upgrade_response(req);
        CHECK(resp.status == 400);
    }

    SECTION("Invalid non-16-byte nonce returns 400 Bad Request") {
        HttpRequestMessage msg;
        msg.line.method = "GET";
        msg.line.version = "HTTP/1.1";
        msg.headers.set("Upgrade", "websocket");
        msg.headers.set("Connection", "Upgrade");
        msg.headers.set("Sec-WebSocket-Key", "c2hvcnQ="); // Decodes to only 5 bytes "short"
        msg.headers.set("Sec-WebSocket-Version", "13");

        Request req(msg);
        auto val = validate_websocket_upgrade(req);
        CHECK(val.status == WebSocketUpgradeStatus::InvalidKey);

        auto resp = make_websocket_upgrade_response(req);
        CHECK(resp.status == 400);
    }

    SECTION("Subprotocol negotiation selects matching client protocol") {
        HttpRequestMessage msg;
        msg.line.method = "GET";
        msg.line.version = "HTTP/1.1";
        msg.headers.set("Upgrade", "websocket");
        msg.headers.set("Connection", "Upgrade");
        msg.headers.set("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
        msg.headers.set("Sec-WebSocket-Version", "13");
        msg.headers.set("Sec-WebSocket-Protocol", "json-rpc, binary-stream, custom-chat");

        Request req(msg);
        std::vector<std::string> server_subprotocols = {"custom-chat", "binary-stream"};

        auto val = validate_websocket_upgrade(req, server_subprotocols);
        CHECK(val.status == WebSocketUpgradeStatus::Success);
        CHECK(val.selected_protocol == "binary-stream");

        auto resp = make_websocket_upgrade_response(req, server_subprotocols);
        CHECK(resp.status == 101);
        CHECK(resp.headers.get_or("Sec-WebSocket-Protocol", "") == "binary-stream");
    }
}

TEST_CASE("WebSocket - End-to-End Upgrade Handshake & Server Integration", "[runtime][http][websocket][server]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);

    std::atomic<bool> ws_connected{false};
    std::string captured_path;
    std::string captured_channel;
    std::string captured_protocol;

    server.ws("/ws/:channel", {"v1.chat"}, [&](std::shared_ptr<WebSocket> ws) {
        ws_connected.store(true);
        captured_path = ws->path;
        captured_channel = ws->param("channel");
        captured_protocol = ws->protocol;
    });

    server.get("/api/status", [](Request&, Response& res) {
        res.send("HTTP OK");
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    SECTION("Successful RFC 6455 handshake upgrade on server.ws route") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string raw_req = "GET /ws/lobby HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                              "Sec-WebSocket-Version: 13\r\n"
                              "Sec-WebSocket-Protocol: v1.chat\r\n"
                              "\r\n";

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);

        CHECK(resp.find("HTTP/1.1 101 Switching Protocols") != std::string::npos);
        CHECK(resp.find("Upgrade: websocket") != std::string::npos);
        CHECK(resp.find("Connection: Upgrade") != std::string::npos);
        CHECK(resp.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);
        CHECK(resp.find("Sec-WebSocket-Protocol: v1.chat") != std::string::npos);

        // Verify WebSocket session was established
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CHECK(ws_connected.load() == true);
        CHECK(captured_path == "/ws/lobby");
        CHECK(captured_channel == "lobby");
        CHECK(captured_protocol == "v1.chat");

        keet::net::detail::close_raw_socket(client);
    }

    SECTION("Non-upgrade GET request to WebSocket route returns 400 Bad Request") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string raw_req = "GET /ws/lobby HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Connection: close\r\n\r\n";

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("Expected WebSocket Upgrade") != std::string::npos);
    }

    SECTION("Invalid Sec-WebSocket-Version returns 426 Upgrade Required") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string raw_req = "GET /ws/lobby HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                              "Sec-WebSocket-Version: 8\r\n\r\n";

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("426 Upgrade Required") != std::string::npos);
        CHECK(resp.find("Sec-WebSocket-Version: 13") != std::string::npos);
    }

    SECTION("Normal HTTP routes continue to operate alongside WebSocket routes") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string raw_req = "GET /api/status HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Connection: close\r\n\r\n";

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("200 OK") != std::string::npos);
        CHECK(resp.find("HTTP OK") != std::string::npos);
    }

    server.stop();
}
