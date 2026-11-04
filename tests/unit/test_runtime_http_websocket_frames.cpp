#include <catch2/catch_all.hpp>
#include <keet/http.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <future>
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
    while (true) {
        int pr = detail::socket_poll(s, POLLIN, timeout_ms);
        if (pr <= 0) break;
        ssize_t n = detail::socket_recv(s, buf.data(), buf.size());
        if (n <= 0) break;
        out.append(buf.data(), static_cast<size_t>(n));
        if (out.find("\r\n\r\n") != std::string::npos) break;
    }
    return out;
}

bool perform_handshake(keet::net::detail::socket_t s, const std::string& path, const std::string& subproto = "") {
    std::string req = "GET " + path + " HTTP/1.1\r\n"
                      "Host: localhost\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n";
    if (!subproto.empty()) {
        req += "Sec-WebSocket-Protocol: " + subproto + "\r\n";
    }
    req += "\r\n";
    if (!detail::socket_send_all(s, req.data(), req.size())) return false;
    std::string resp = read_http_response(s);
    return resp.find("101 Switching Protocols") != std::string::npos;
}

std::optional<WebSocketFrame> read_one_frame(keet::net::detail::socket_t s, std::string& buffer, int timeout_ms = 3000) {
    std::array<char, 4096> chunk;
    while (true) {
        if (!buffer.empty()) {
            auto res = parse_websocket_frame(buffer);
            if (res.status == FrameParseStatus::Ok) {
                buffer.erase(0, res.bytes_consumed);
                return res.frame;
            }
            if (res.status == FrameParseStatus::ProtocolError) {
                return std::nullopt;
            }
        }
        int pr = detail::socket_poll(s, POLLIN, timeout_ms);
        if (pr <= 0) return std::nullopt;
        ssize_t n = detail::socket_recv(s, chunk.data(), chunk.size());
        if (n <= 0) return std::nullopt;
        buffer.append(chunk.data(), static_cast<size_t>(n));
    }
}

} // namespace

// ============================================================================
// Unit Tests: WebSocket Framing Engine (RFC 6455 Serialization & Parsing)
// ============================================================================

TEST_CASE("WebSocket Framing - Basic Frame Serialization and Parsing", "[runtime][http][websocket][framing]") {
    SECTION("Unmasked text frame with payload <= 125 bytes") {
        std::string payload = "Hello WebSocket!";
        std::string wire = serialize_websocket_frame(WebSocketOpcode::Text, payload, /*masked=*/false);

        REQUIRE(wire.size() == 2 + payload.size());
        // Byte 0: FIN=1, Opcode=0x1 -> 0x81
        CHECK(static_cast<uint8_t>(wire[0]) == 0x81);
        // Byte 1: Mask=0, Len=16 -> 0x10
        CHECK(static_cast<uint8_t>(wire[1]) == static_cast<uint8_t>(payload.size()));

        auto parsed = parse_websocket_frame(wire);
        REQUIRE(parsed.status == FrameParseStatus::Ok);
        CHECK(parsed.frame.fin == true);
        CHECK(parsed.frame.opcode == WebSocketOpcode::Text);
        CHECK(parsed.frame.masked == false);
        CHECK(parsed.frame.payload == payload);
        CHECK(parsed.bytes_consumed == wire.size());
    }

    SECTION("Masked text frame with custom 4-byte mask key") {
        std::string payload = "The quick brown fox jumps over the lazy dog";
        std::array<uint8_t, 4> mask = {0xAA, 0xBB, 0xCC, 0xDD};
        std::string wire = serialize_websocket_frame(WebSocketOpcode::Text, payload, /*masked=*/true, mask);

        REQUIRE(wire.size() == 2 + 4 + payload.size());
        // Byte 1: Mask=1 (0x80) | Len
        CHECK((static_cast<uint8_t>(wire[1]) & 0x80) == 0x80);
        // Mask key in header
        CHECK(static_cast<uint8_t>(wire[2]) == 0xAA);
        CHECK(static_cast<uint8_t>(wire[3]) == 0xBB);
        CHECK(static_cast<uint8_t>(wire[4]) == 0xCC);
        CHECK(static_cast<uint8_t>(wire[5]) == 0xDD);

        auto parsed = parse_websocket_frame(wire);
        REQUIRE(parsed.status == FrameParseStatus::Ok);
        CHECK(parsed.frame.fin == true);
        CHECK(parsed.frame.masked == true);
        CHECK(parsed.frame.mask_key == mask);
        CHECK(parsed.frame.payload == payload); // Unmasked matches original!
    }

    SECTION("Extended payload length 126 (16-bit integer, e.g. 1000 bytes)") {
        std::string payload(1000, 'K');
        std::string wire = serialize_websocket_frame(WebSocketOpcode::Binary, payload, /*masked=*/false);

        REQUIRE(wire.size() == 2 + 2 + payload.size());
        CHECK(static_cast<uint8_t>(wire[1] & 0x7F) == 126);
        uint16_t parsed_len = (static_cast<uint8_t>(wire[2]) << 8) | static_cast<uint8_t>(wire[3]);
        CHECK(parsed_len == 1000);

        auto parsed = parse_websocket_frame(wire);
        REQUIRE(parsed.status == FrameParseStatus::Ok);
        CHECK(parsed.frame.opcode == WebSocketOpcode::Binary);
        CHECK(parsed.frame.payload.size() == 1000);
        CHECK(parsed.frame.payload == payload);
    }

    SECTION("Extended payload length 127 (64-bit integer, e.g. 70000 bytes)") {
        std::string payload(70000, 'X');
        std::string wire = serialize_websocket_frame(WebSocketOpcode::Binary, payload, /*masked=*/false);

        REQUIRE(wire.size() == 2 + 8 + payload.size());
        CHECK(static_cast<uint8_t>(wire[1] & 0x7F) == 127);

        auto parsed = parse_websocket_frame(wire);
        REQUIRE(parsed.status == FrameParseStatus::Ok);
        CHECK(parsed.frame.opcode == WebSocketOpcode::Binary);
        CHECK(parsed.frame.payload.size() == 70000);
        CHECK(parsed.frame.payload == payload);
    }

    SECTION("Control frames: Ping, Pong, and Close") {
        // Ping
        std::string ping_payload = "ping-data";
        std::string ping_wire = serialize_websocket_frame(WebSocketOpcode::Ping, ping_payload);
        auto ping_res = parse_websocket_frame(ping_wire);
        REQUIRE(ping_res.status == FrameParseStatus::Ok);
        CHECK(ping_res.frame.opcode == WebSocketOpcode::Ping);
        CHECK(ping_res.frame.payload == ping_payload);

        // Pong
        std::string pong_wire = serialize_websocket_frame(WebSocketOpcode::Pong, ping_payload);
        auto pong_res = parse_websocket_frame(pong_wire);
        REQUIRE(pong_res.status == FrameParseStatus::Ok);
        CHECK(pong_res.frame.opcode == WebSocketOpcode::Pong);
        CHECK(pong_res.frame.payload == ping_payload);

        // Close with 2-byte status code 1000 + reason
        std::string close_body;
        close_body.push_back(static_cast<char>(1000 >> 8));
        close_body.push_back(static_cast<char>(1000 & 0xFF));
        close_body.append("Normal Closure");
        std::string close_wire = serialize_websocket_frame(WebSocketOpcode::Close, close_body);
        auto close_res = parse_websocket_frame(close_wire);
        REQUIRE(close_res.status == FrameParseStatus::Ok);
        CHECK(close_res.frame.opcode == WebSocketOpcode::Close);
        uint16_t code = (static_cast<uint8_t>(close_res.frame.payload[0]) << 8) |
                        static_cast<uint8_t>(close_res.frame.payload[1]);
        CHECK(code == 1000);
        CHECK(close_res.frame.payload.substr(2) == "Normal Closure");
    }
}

TEST_CASE("WebSocket Framing - Protocol Error Detection (RFC 6455)", "[runtime][http][websocket][framing][errors]") {
    using namespace std::string_literals;

    SECTION("Non-zero RSV bit returns ProtocolError (code 1002)") {
        std::string wire = "\xC1\x00"s; // RSV1=1, FIN=1, Text
        auto res = parse_websocket_frame(wire);
        CHECK(res.status == FrameParseStatus::ProtocolError);
        CHECK(res.error_code == 1002);
    }

    SECTION("Unknown opcode returns ProtocolError (code 1002)") {
        std::string wire = "\x8F\x00"s; // Opcode 0xF
        auto res = parse_websocket_frame(wire);
        CHECK(res.status == FrameParseStatus::ProtocolError);
        CHECK(res.error_code == 1002);
    }

    SECTION("Fragmented control frame returns ProtocolError (code 1002)") {
        std::string wire = "\x09\x00"s; // FIN=0, Ping (0x9)
        auto res = parse_websocket_frame(wire);
        CHECK(res.status == FrameParseStatus::ProtocolError);
        CHECK(res.error_code == 1002);
    }

    SECTION("Control frame payload exceeding 125 bytes returns ProtocolError (code 1002)") {
        std::string wire = "\x89\x7E\x00\x7E"s; // Ping with extended length 126 (126 bytes)
        wire.append(126, 'P');
        auto res = parse_websocket_frame(wire);
        CHECK(res.status == FrameParseStatus::ProtocolError);
        CHECK(res.error_code == 1002);
    }

    SECTION("Non-minimal length encoding returns ProtocolError (code 1002)") {
        // Length 100 encoded as 16-bit extended length 126
        std::string wire = "\x81\x7E\x00\x64"s;
        wire.append(100, 'A');
        auto res = parse_websocket_frame(wire);
        CHECK(res.status == FrameParseStatus::ProtocolError);
        CHECK(res.error_code == 1002);
    }

    SECTION("Truncated buffer returns NeedMoreData") {
        std::string wire = "\x81\x05Hel"s; // Expects 5 bytes, only 3 present
        auto res = parse_websocket_frame(wire);
        CHECK(res.status == FrameParseStatus::NeedMoreData);
    }

    SECTION("UTF-8 validator correctly detects valid and invalid sequences") {
        CHECK(is_valid_utf8("Hello, world! 123"));
        CHECK(is_valid_utf8("Zażółć gęślą jaźń"));
        CHECK(is_valid_utf8("こんにちは世界"));
        CHECK(is_valid_utf8("\xF0\x9F\x9A\x80")); // Rocket emoji 🚀

        // Invalid: overlong ASCII NUL
        CHECK_FALSE(is_valid_utf8(std::string_view("\xC0\x80", 2)));
        // Invalid: lone continuation byte
        CHECK_FALSE(is_valid_utf8(std::string_view("\x80", 1)));
        // Invalid: UTF-16 surrogate
        CHECK_FALSE(is_valid_utf8(std::string_view("\xED\xA0\x80", 3)));
    }
}

// ============================================================================
// End-to-End Tests: WebSocket Client Abstraction & Server Integration
// ============================================================================

TEST_CASE("WebSocket Server - Bidirectional Text & Binary Messaging", "[runtime][http][websocket][e2e]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);

    std::promise<std::string> text_promise;
    auto text_future = text_promise.get_future();

    server.ws("/ws/echo", [&](std::shared_ptr<WebSocket> ws) {
        ws->onMessage([ws](const std::string& msg) {
            ws->send("ECHO: " + msg);
        });

        ws->onBinary([ws](const std::string& bytes) {
            std::string reversed = bytes;
            std::reverse(reversed.begin(), reversed.end());
            ws->sendBinary(reversed);
        });
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    auto client = connect_client(port);
    REQUIRE(client != keet::net::detail::kInvalidSocket);
    REQUIRE(perform_handshake(client, "/ws/echo"));

    std::string client_buf;

    SECTION("Text message echo round-trip") {
        std::string msg = "Hello from client!";
        std::string masked_frame = serialize_websocket_frame(WebSocketOpcode::Text, msg, /*masked=*/true);
        REQUIRE(detail::socket_send_all(client, masked_frame.data(), masked_frame.size()));

        auto resp_frame = read_one_frame(client, client_buf);
        REQUIRE(resp_frame.has_value());
        CHECK(resp_frame->opcode == WebSocketOpcode::Text);
        CHECK(resp_frame->masked == false); // Server frames MUST NOT be masked
        CHECK(resp_frame->payload == "ECHO: Hello from client!");
    }

    SECTION("Binary message echo round-trip") {
        std::string raw_bytes = "\x01\x02\x03\x04\x05\x06\x07\x08";
        std::string masked_frame = serialize_websocket_frame(WebSocketOpcode::Binary, raw_bytes, /*masked=*/true);
        REQUIRE(detail::socket_send_all(client, masked_frame.data(), masked_frame.size()));

        auto resp_frame = read_one_frame(client, client_buf);
        REQUIRE(resp_frame.has_value());
        CHECK(resp_frame->opcode == WebSocketOpcode::Binary);
        CHECK(resp_frame->masked == false);
        CHECK(resp_frame->payload == "\x08\x07\x06\x05\x04\x03\x02\x01");
    }

    keet::net::detail::close_raw_socket(client);
    server.stop();
}

TEST_CASE("WebSocket Server - Automatic Ping-Pong Handling", "[runtime][http][websocket][pingpong]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);

    std::atomic<bool> ping_received{false};
    std::string captured_ping_payload;

    server.ws("/ws/pingpong", [&](std::shared_ptr<WebSocket> ws) {
        ws->onPing([&](const std::string& p) {
            ping_received.store(true);
            captured_ping_payload = p;
        });
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    auto client = connect_client(port);
    REQUIRE(client != keet::net::detail::kInvalidSocket);
    REQUIRE(perform_handshake(client, "/ws/pingpong"));

    std::string client_buf;

    // Client sends Ping with payload
    std::string ping_payload = "heartbeat-test-nonce-42";
    std::string ping_frame = serialize_websocket_frame(WebSocketOpcode::Ping, ping_payload, /*masked=*/true);
    REQUIRE(detail::socket_send_all(client, ping_frame.data(), ping_frame.size()));

    // Server MUST automatically reply with Pong with identical payload (RFC 6455 §5.5.2)
    auto pong_frame = read_one_frame(client, client_buf);
    REQUIRE(pong_frame.has_value());
    CHECK(pong_frame->opcode == WebSocketOpcode::Pong);
    CHECK(pong_frame->payload == ping_payload);
    CHECK(ping_received.load() == true);
    CHECK(captured_ping_payload == ping_payload);

    keet::net::detail::close_raw_socket(client);
    server.stop();
}

TEST_CASE("WebSocket Server - Fragmented Message Assembly", "[runtime][http][websocket][fragmentation]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);

    std::promise<std::string> assembled_promise;
    auto assembled_future = assembled_promise.get_future();

    server.ws("/ws/fragmented", [&](std::shared_ptr<WebSocket> ws) {
        ws->onMessage([&assembled_promise](const std::string& msg) {
            assembled_promise.set_value(msg);
        });
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    auto client = connect_client(port);
    REQUIRE(client != keet::net::detail::kInvalidSocket);
    REQUIRE(perform_handshake(client, "/ws/fragmented"));

    // Send 3 fragments:
    // Frame 1: FIN=0, Text, "Hello "
    // Frame 2: FIN=0, Continuation, "beautiful "
    // Frame 3: FIN=1, Continuation, "world!"
    std::string f1 = serialize_websocket_frame(WebSocketOpcode::Text, "Hello ", /*masked=*/true, std::nullopt, /*fin=*/false);
    std::string f2 = serialize_websocket_frame(WebSocketOpcode::Continuation, "beautiful ", /*masked=*/true, std::nullopt, /*fin=*/false);
    std::string f3 = serialize_websocket_frame(WebSocketOpcode::Continuation, "world!", /*masked=*/true, std::nullopt, /*fin=*/true);

    REQUIRE(detail::socket_send_all(client, f1.data(), f1.size()));
    REQUIRE(detail::socket_send_all(client, f2.data(), f2.size()));
    REQUIRE(detail::socket_send_all(client, f3.data(), f3.size()));

    REQUIRE(assembled_future.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
    CHECK(assembled_future.get() == "Hello beautiful world!");

    keet::net::detail::close_raw_socket(client);
    server.stop();
}

TEST_CASE("WebSocket Server - Interleaved Control Frame During Fragmentation", "[runtime][http][websocket][interleaving]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);

    std::promise<std::string> msg_promise;
    auto msg_future = msg_promise.get_future();

    server.ws("/ws/interleave", [&](std::shared_ptr<WebSocket> ws) {
        ws->onMessage([&msg_promise](const std::string& msg) {
            msg_promise.set_value(msg);
        });
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    auto client = connect_client(port);
    REQUIRE(client != keet::net::detail::kInvalidSocket);
    REQUIRE(perform_handshake(client, "/ws/interleave"));

    std::string client_buf;

    // Send fragment 1
    std::string f1 = serialize_websocket_frame(WebSocketOpcode::Text, "Part 1, ", /*masked=*/true, std::nullopt, /*fin=*/false);
    REQUIRE(detail::socket_send_all(client, f1.data(), f1.size()));

    // Interleave a Ping control frame (RFC 6455 §5.4 allows control frames within fragmented messages)
    std::string ping_f = serialize_websocket_frame(WebSocketOpcode::Ping, "mid-stream-ping", /*masked=*/true, std::nullopt, /*fin=*/true);
    REQUIRE(detail::socket_send_all(client, ping_f.data(), ping_f.size()));

    // Verify Pong was received immediately
    auto pong_f = read_one_frame(client, client_buf);
    REQUIRE(pong_f.has_value());
    CHECK(pong_f->opcode == WebSocketOpcode::Pong);
    CHECK(pong_f->payload == "mid-stream-ping");

    // Send final continuation fragment
    std::string f2 = serialize_websocket_frame(WebSocketOpcode::Continuation, "Part 2!", /*masked=*/true, std::nullopt, /*fin=*/true);
    REQUIRE(detail::socket_send_all(client, f2.data(), f2.size()));

    // Verify full assembled message
    REQUIRE(msg_future.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
    CHECK(msg_future.get() == "Part 1, Part 2!");

    keet::net::detail::close_raw_socket(client);
    server.stop();
}

TEST_CASE("WebSocket Server - Clean Close Handshake & Client Mask Enforcement", "[runtime][http][websocket][close]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);

    std::atomic<bool> close_fired{false};
    uint16_t captured_close_code = 0;
    std::string captured_close_reason;

    server.ws("/ws/closure", [&](std::shared_ptr<WebSocket> ws) {
        ws->onClose([&](uint16_t code, const std::string& reason) {
            close_fired.store(true);
            captured_close_code = code;
            captured_close_reason = reason;
        });
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    SECTION("Client initiates clean closure with status code 1000") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);
        REQUIRE(perform_handshake(client, "/ws/closure"));

        std::string client_buf;

        // Client sends Close frame: status 1000 + "Normal bye"
        std::string close_body;
        close_body.push_back(static_cast<char>(1000 >> 8));
        close_body.push_back(static_cast<char>(1000 & 0xFF));
        close_body.append("Normal bye");

        std::string close_wire = serialize_websocket_frame(WebSocketOpcode::Close, close_body, /*masked=*/true);
        REQUIRE(detail::socket_send_all(client, close_wire.data(), close_wire.size()));

        // Server echoes Close frame
        auto srv_close = read_one_frame(client, client_buf);
        REQUIRE(srv_close.has_value());
        CHECK(srv_close->opcode == WebSocketOpcode::Close);
        uint16_t resp_code = (static_cast<uint8_t>(srv_close->payload[0]) << 8) |
                             static_cast<uint8_t>(srv_close->payload[1]);
        CHECK(resp_code == 1000);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CHECK(close_fired.load() == true);
        CHECK(captured_close_code == 1000);
        CHECK(captured_close_reason == "Normal bye");

        keet::net::detail::close_raw_socket(client);
    }

    SECTION("Unmasked client frame triggers Protocol Error (1002)") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);
        REQUIRE(perform_handshake(client, "/ws/closure"));

        std::string client_buf;

        // Client illegally sends UNMASKED frame to server (RFC 6455 §5.1 requires mask)
        std::string unmasked_frame = serialize_websocket_frame(WebSocketOpcode::Text, "Sneaky Unmasked", /*masked=*/false);
        REQUIRE(detail::socket_send_all(client, unmasked_frame.data(), unmasked_frame.size()));

        // Server MUST close connection with 1002
        auto srv_close = read_one_frame(client, client_buf);
        REQUIRE(srv_close.has_value());
        CHECK(srv_close->opcode == WebSocketOpcode::Close);
        uint16_t resp_code = (static_cast<uint8_t>(srv_close->payload[0]) << 8) |
                             static_cast<uint8_t>(srv_close->payload[1]);
        CHECK(resp_code == 1002);

        keet::net::detail::close_raw_socket(client);
    }

    SECTION("Invalid UTF-8 in text frame triggers status 1007") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);
        REQUIRE(perform_handshake(client, "/ws/closure"));

        std::string client_buf;

        // Send text frame with invalid UTF-8 (e.g. overlong 0xC0 0x80)
        std::string invalid_utf8 = "\xC0\x80";
        std::string frame = serialize_websocket_frame(WebSocketOpcode::Text, invalid_utf8, /*masked=*/true);
        REQUIRE(detail::socket_send_all(client, frame.data(), frame.size()));

        // Server MUST close connection with 1007
        auto srv_close = read_one_frame(client, client_buf);
        REQUIRE(srv_close.has_value());
        CHECK(srv_close->opcode == WebSocketOpcode::Close);
        uint16_t resp_code = (static_cast<uint8_t>(srv_close->payload[0]) << 8) |
                             static_cast<uint8_t>(srv_close->payload[1]);
        CHECK(resp_code == 1007);

        keet::net::detail::close_raw_socket(client);
    }

    server.stop();
}

TEST_CASE("WebSocket Server - Concurrent Thread Transmission & Broadcast", "[runtime][http][websocket][concurrency]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 4;

    HttpServer server(cfg);

    std::shared_ptr<WebSocket> saved_ws;
    std::mutex ws_mutex;
    std::condition_variable ws_cv;

    server.ws("/ws/broadcast", [&](std::shared_ptr<WebSocket> ws) {
        {
            std::lock_guard<std::mutex> lock(ws_mutex);
            saved_ws = ws;
        }
        ws_cv.notify_one();
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    auto client = connect_client(port);
    REQUIRE(client != keet::net::detail::kInvalidSocket);
    REQUIRE(perform_handshake(client, "/ws/broadcast"));

    // Wait until server handler captured ws instance
    {
        std::unique_lock<std::mutex> lock(ws_mutex);
        ws_cv.wait_for(lock, std::chrono::seconds(2), [&saved_ws]() {
            return saved_ws != nullptr;
        });
    }
    REQUIRE(saved_ws != nullptr);

    // Broadcast 5 messages from background thread concurrently
    std::thread broadcaster([saved_ws]() {
        for (int i = 1; i <= 5; ++i) {
            saved_ws->send("Tick #" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    std::string client_buf;
    for (int i = 1; i <= 5; ++i) {
        auto frame = read_one_frame(client, client_buf);
        REQUIRE(frame.has_value());
        CHECK(frame->opcode == WebSocketOpcode::Text);
        CHECK(frame->payload == "Tick #" + std::to_string(i));
    }

    broadcaster.join();

    keet::net::detail::close_raw_socket(client);
    server.stop();
}
