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

                // Inspect Content-Length
                std::string lower_headers = detail::to_lower_ascii(out.substr(0, header_end));
                size_t cl_pos = lower_headers.find("content-length:");
                if (cl_pos != std::string::npos) {
                    size_t val_start = cl_pos + 15;
                    size_t val_end = lower_headers.find("\r\n", val_start);
                    std::string cl_str = std::string(detail::trim_ascii(lower_headers.substr(val_start, val_end - val_start)));
                    size_t cl = 0;
                    std::from_chars(cl_str.data(), cl_str.data() + cl_str.size(), cl);
                    expected_len = body_start + cl;
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

TEST_CASE("HTTP Server Lifecycle & Port Binding", "[runtime][http][server]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0; // Bind ephemeral free port
    cfg.worker_threads = 2;

    HttpServer server(cfg);
    server.set_handler([](const HttpRequestMessage& req) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 200;
        res.headers.set("Content-Type", "text/plain");
        res.body = "Hello from Keet HTTP Server: " + req.line.path;
        return res;
    });

    REQUIRE(server.start());
    REQUIRE(server.is_running());
    int bound = server.bound_port();
    REQUIRE(bound > 0);

    // Send a client request
    keet::net::detail::socket_t client = connect_client(bound);
    REQUIRE(client != keet::net::detail::kInvalidSocket);

    std::string req = "GET /api/test HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "Connection: close\r\n"
                      "\r\n";
    REQUIRE(detail::socket_send_all(client, req.data(), req.size()));

    std::string response = read_http_response(client);
    keet::net::detail::close_raw_socket(client);

    CHECK(response.find("HTTP/1.1 200 OK") != std::string::npos);
    CHECK(response.find("Server: keety-http/1.0") != std::string::npos);
    CHECK(response.find("Content-Type: text/plain") != std::string::npos);
    CHECK(response.find("Hello from Keet HTTP Server: /api/test") != std::string::npos);

    // Stop server
    server.stop();
    CHECK_FALSE(server.is_running());

    // Subsequent connection should fail
    keet::net::detail::socket_t dead_client = connect_client(bound);
    CHECK(dead_client == keet::net::detail::kInvalidSocket);
}

TEST_CASE("HTTP Server Persistent Connection & Keep-Alive", "[runtime][http][server]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;
    cfg.keep_alive_timeout_ms = 2000;
    cfg.max_keep_alive_requests = 10;

    HttpServer server(cfg);
    server.set_handler([](const HttpRequestMessage& req) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 200;
        res.body = "path=" + req.line.path;
        return res;
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    keet::net::detail::socket_t client = connect_client(port);
    REQUIRE(client != keet::net::detail::kInvalidSocket);

    // Request 1 over persistent connection
    std::string req1 = "GET /first HTTP/1.1\r\n"
                       "Host: localhost\r\n"
                       "Connection: keep-alive\r\n"
                       "\r\n";
    REQUIRE(detail::socket_send_all(client, req1.data(), req1.size()));

    std::string resp1 = read_http_response(client);
    CHECK(resp1.find("HTTP/1.1 200 OK") != std::string::npos);
    CHECK(resp1.find("path=/first") != std::string::npos);
    CHECK(resp1.find("Connection: keep-alive") != std::string::npos);
    CHECK(resp1.find("Keep-Alive:") != std::string::npos);

    // Request 2 over SAME persistent connection
    std::string req2 = "GET /second HTTP/1.1\r\n"
                       "Host: localhost\r\n"
                       "Connection: keep-alive\r\n"
                       "\r\n";
    REQUIRE(detail::socket_send_all(client, req2.data(), req2.size()));

    std::string resp2 = read_http_response(client);
    CHECK(resp2.find("HTTP/1.1 200 OK") != std::string::npos);
    CHECK(resp2.find("path=/second") != std::string::npos);

    // Request 3 closing connection
    std::string req3 = "GET /final HTTP/1.1\r\n"
                       "Host: localhost\r\n"
                       "Connection: close\r\n"
                       "\r\n";
    REQUIRE(detail::socket_send_all(client, req3.data(), req3.size()));

    std::string resp3 = read_http_response(client);
    CHECK(resp3.find("HTTP/1.1 200 OK") != std::string::npos);
    CHECK(resp3.find("path=/final") != std::string::npos);
    CHECK(resp3.find("Connection: close") != std::string::npos);

    // Socket should now be closed by server
    std::array<char, 16> eof_buf;
    ssize_t n = detail::socket_recv(client, eof_buf.data(), eof_buf.size());
    CHECK(n <= 0);

    keet::net::detail::close_raw_socket(client);
    server.stop();
}

TEST_CASE("HTTP Server Worker Pool Concurrency", "[runtime][http][server]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 4;

    HttpServer server(cfg);
    std::atomic<int> handled_requests{0};

    server.set_handler([&](const HttpRequestMessage& req) -> HttpResponseMessage {
        handled_requests.fetch_add(1);
        HttpResponseMessage res;
        res.status = 200;
        res.body = "client=" + req.query.at("id");
        return res;
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    constexpr int kClientCount = 8;
    std::vector<std::thread> clients;
    std::vector<std::string> results(kClientCount);

    for (int i = 0; i < kClientCount; ++i) {
        clients.emplace_back([i, port, &results]() {
            keet::net::detail::socket_t s = connect_client(port);
            if (s == keet::net::detail::kInvalidSocket) {
                return;
            }
            std::string req = "GET /items?id=" + std::to_string(i) + " HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Connection: close\r\n"
                              "\r\n";
            detail::socket_send_all(s, req.data(), req.size());
            results[static_cast<size_t>(i)] = read_http_response(s);
            keet::net::detail::close_raw_socket(s);
        });
    }

    for (auto& t : clients) {
        t.join();
    }

    CHECK(handled_requests.load() == kClientCount);
    for (int i = 0; i < kClientCount; ++i) {
        CHECK(results[static_cast<size_t>(i)].find("client=" + std::to_string(i)) != std::string::npos);
    }

    server.stop();
}

TEST_CASE("HTTP Server Graceful Shutdown & Drain", "[runtime][http][server]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;
    cfg.drain_timeout_ms = 2000;

    HttpServer server(cfg);
    std::atomic<bool> in_flight_started{false};
    std::atomic<bool> in_flight_completed{false};

    server.set_handler([&](const HttpRequestMessage&) -> HttpResponseMessage {
        in_flight_started.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        in_flight_completed.store(true);

        HttpResponseMessage res;
        res.status = 200;
        res.body = "done";
        return res;
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    std::string client_response;
    std::thread client_thread([port, &client_response]() {
        keet::net::detail::socket_t s = connect_client(port);
        if (s == keet::net::detail::kInvalidSocket) return;
        std::string req = "GET /slow HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
        detail::socket_send_all(s, req.data(), req.size());
        client_response = read_http_response(s);
        keet::net::detail::close_raw_socket(s);
    });

    // Wait until handler starts executing
    while (!in_flight_started.load()) {
        std::this_thread::yield();
    }

    // Now stop server while request is in-flight
    server.stop();
    CHECK_FALSE(server.is_running());

    client_thread.join();

    // Verify the in-flight request was allowed to drain and finish
    CHECK(in_flight_completed.load());
    CHECK(client_response.find("HTTP/1.1 200 OK") != std::string::npos);
    CHECK(client_response.find("done") != std::string::npos);
}

TEST_CASE("HTTP Server Error Handling & Limits", "[runtime][http][server]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;
    cfg.limits.max_body_size = 50; // Strict limit: 50 bytes

    HttpServer server(cfg);
    server.set_handler([](const HttpRequestMessage& req) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 200;
        res.body = "received: " + req.body;
        return res;
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    SECTION("Payload exceeds max_body_size returns 413") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string huge_body(100, 'A');
        std::string req = "POST /upload HTTP/1.1\r\n"
                          "Host: localhost\r\n"
                          "Content-Length: 100\r\n"
                          "\r\n" +
                          huge_body;

        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 413") != std::string::npos);
    }

    SECTION("Malformed request line returns 400") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string bad_req = "NOT_A_VALID_HTTP_REQUEST\r\n\r\n";
        REQUIRE(detail::socket_send_all(client, bad_req.data(), bad_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 400") != std::string::npos);
    }

    server.stop();
}

TEST_CASE("HTTP Server Port Binding Conflict and Optional SO_REUSEPORT", "[runtime][http][server]") {
    HttpServerConfig cfg1;
    cfg1.host = "127.0.0.1";
    cfg1.port = 0; // Ephemeral port
    cfg1.reuse_port = false;

    HttpServer server1(cfg1);
    REQUIRE(server1.start());
    int bound_port = server1.bound_port();
    REQUIRE(bound_port > 0);

    SECTION("Default configuration throws NetError when port is already taken") {
        HttpServerConfig cfg2;
        cfg2.host = "127.0.0.1";
        cfg2.port = bound_port;
        cfg2.reuse_port = false;

        HttpServer server2(cfg2);
        CHECK_THROWS_AS(server2.start(), keet::error);
        try {
            server2.start();
        } catch (const keet::error& err) {
            CHECK(err.name == "NetError");
            CHECK(err.message.find("failed to bind") != std::string::npos);
        }
    }

#if defined(SO_REUSEPORT)
    SECTION("Optional reuse_port allows shared port binding when both servers enable it") {
        server1.stop();

        HttpServerConfig cfg_shared1;
        cfg_shared1.host = "127.0.0.1";
        cfg_shared1.port = 0;
        cfg_shared1.reuse_port = true;

        HttpServer server_shared1(cfg_shared1);
        REQUIRE(server_shared1.start());
        int shared_port = server_shared1.bound_port();
        REQUIRE(shared_port > 0);

        HttpServerConfig cfg_shared2;
        cfg_shared2.host = "127.0.0.1";
        cfg_shared2.port = shared_port;
        cfg_shared2.reuse_port = true;

        HttpServer server_shared2(cfg_shared2);
        CHECK_NOTHROW(server_shared2.start());
        CHECK(server_shared2.is_running());

        server_shared1.stop();
        server_shared2.stop();
    }
#endif

    server1.stop();
}
