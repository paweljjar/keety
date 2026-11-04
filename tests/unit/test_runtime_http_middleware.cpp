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

TEST_CASE("HTTP Middleware - RFC 6265 Cookie Parsing & Formatting", "[runtime][http][middleware][cookie]") {
    SECTION("Format simple session cookie") {
        std::string set_cookie = detail::format_set_cookie("sessionId", "xyz123");
        CHECK(set_cookie == "sessionId=xyz123; Path=/; SameSite=Lax");
    }

    SECTION("Format comprehensive cookie with all security flags") {
        CookieOptions opt;
        opt.path = "/api";
        opt.domain = "example.com";
        opt.max_age = 3600;
        opt.secure = true;
        opt.http_only = true;
        opt.same_site = CookieOptions::SameSite::Strict;
        opt.expires = "Wed, 21 Oct 2026 07:28:00 GMT";

        std::string set_cookie = detail::format_set_cookie("auth_token", "secret%20val", opt);
        CHECK(set_cookie.find("auth_token=secret%20val") != std::string::npos);
        CHECK(set_cookie.find("Path=/api") != std::string::npos);
        CHECK(set_cookie.find("Domain=example.com") != std::string::npos);
        CHECK(set_cookie.find("Max-Age=3600") != std::string::npos);
        CHECK(set_cookie.find("Expires=Wed, 21 Oct 2026 07:28:00 GMT") != std::string::npos);
        CHECK(set_cookie.find("Secure") != std::string::npos);
        CHECK(set_cookie.find("HttpOnly") != std::string::npos);
        CHECK(set_cookie.find("SameSite=Strict") != std::string::npos);
    }

    SECTION("Parse incoming Cookie header values") {
        std::string cookie_hdr = "user=alice; theme=dark; token=\"quoted-val\"; encoded=hello%20world";
        auto cookies = detail::parse_cookies(cookie_hdr);

        CHECK(cookies["user"] == "alice");
        CHECK(cookies["theme"] == "dark");
        CHECK(cookies["token"] == "quoted-val");
        CHECK(cookies["encoded"] == "hello world");
    }

    SECTION("Parse empty or malformed cookie headers safely") {
        CHECK(detail::parse_cookies("").empty());
        CHECK(detail::parse_cookies("; ; = ;").empty());
        auto single = detail::parse_cookies("name_only");
        CHECK(single["name_only"].empty());
    }
}

TEST_CASE("HTTP Middleware - Path Prefix Matching", "[runtime][http][middleware][prefix]") {
    CHECK(detail::path_starts_with_prefix("/api/users", "/api"));
    CHECK(detail::path_starts_with_prefix("/api", "/api"));
    CHECK(detail::path_starts_with_prefix("/api/", "/api"));
    CHECK(detail::path_starts_with_prefix("/api/v1/data", "/api/v1"));
    CHECK(detail::path_starts_with_prefix("/anything", ""));

    // Boundary check: "/apix" must NOT match "/api"
    CHECK_FALSE(detail::path_starts_with_prefix("/apixyz", "/api"));
    CHECK_FALSE(detail::path_starts_with_prefix("/other", "/api"));
    CHECK_FALSE(detail::path_starts_with_prefix("/ap", "/api"));
}

TEST_CASE("HTTP Middleware - Request & Response Fluent API", "[runtime][http][middleware][fluent]") {
    HttpRequestMessage raw;
    raw.line.method = "POST";
    raw.line.path = "/users/42";
    raw.line.raw_target = "/users/42?active=true";
    raw.line.version = "HTTP/1.1";
    raw.client_ip = "192.168.1.10";
    raw.client_port = 54321;
    raw.params["id"] = "42";
    raw.query["active"] = "true";
    raw.headers.set("Host", "localhost:8080");
    raw.headers.set("Authorization", "Bearer tok-123");
    raw.headers.set("Cookie", "session=s123; user=bob");
    raw.body = "{\"name\":\"Bob\"}";

    Request req(raw);

    SECTION("Request getters and fallbacks") {
        CHECK(req.method == "POST");
        CHECK(req.path == "/users/42");
        CHECK(req.url == "/users/42?active=true");
        CHECK(req.protocol == "HTTP/1.1");
        CHECK(req.ip == "192.168.1.10");
        CHECK(req.port == 54321);
        CHECK(req.body == "{\"name\":\"Bob\"}");

        CHECK(req.param("id") == "42");
        CHECK(req.param("missing", "def") == "def");

        CHECK(req.query_param("active") == "true");
        CHECK(req.query_param("missing", "none") == "none");

        CHECK(req.header("authorization") == "Bearer tok-123");
        CHECK(req.header("x-nonexistent", "fallback") == "fallback");

        CHECK(req.cookie("session") == "s123");
        CHECK(req.cookie("user") == "bob");
        CHECK(req.cookie("missing", "none") == "none");
    }

    SECTION("Request locals context propagation") {
        req.locals["userId"] = "user_42";
        req.locals["role"] = "admin";

        HttpRequestMessage converted = req.to_raw_message();
        CHECK(converted.locals["userId"] == "user_42");
        CHECK(converted.locals["role"] == "admin");

        Request roundtrip(converted);
        CHECK(roundtrip.locals["userId"] == "user_42");
        CHECK(roundtrip.locals["role"] == "admin");
    }

    SECTION("Response fluent methods") {
        Response res;
        res.status(201)
           .setHeader("X-Custom", "Val1")
           .appendHeader("X-Multi", "A")
           .appendHeader("X-Multi", "B")
           .cookie("visited", "yes")
           .send("Created entity");

        CHECK(res.is_sent());
        CHECK(res.status_code == 201);
        CHECK(res.body == "Created entity");
        CHECK(res.headers.get_or("x-custom", "") == "Val1");
        CHECK(res.headers.get_or("content-type", "") == "text/plain; charset=utf-8");

        HttpResponseMessage msg = res.to_message();
        CHECK(msg.status == 201);
        CHECK(msg.body == "Created entity");
        CHECK(msg.headers.has("Set-Cookie"));
    }

    SECTION("Response json and html helpers") {
        Response res_json;
        res_json.json("{\"status\":\"ok\"}");
        CHECK(res_json.headers.get_or("content-type", "") == "application/json; charset=utf-8");
        CHECK(res_json.body == "{\"status\":\"ok\"}");

        Response res_html;
        res_html.html("<h1>Hello</h1>");
        CHECK(res_html.headers.get_or("content-type", "") == "text/html; charset=utf-8");
        CHECK(res_html.body == "<h1>Hello</h1>");
    }

    SECTION("Response redirect and clearCookie") {
        Response res;
        res.redirect("/login", 303);
        CHECK(res.status_code == 303);
        CHECK(res.headers.get_or("location", "") == "/login");

        Response res_clear;
        res_clear.clearCookie("session", "/api");
        CHECK(res_clear.headers.has("Set-Cookie"));
        auto set_cookie = res_clear.headers.get("set-cookie");
        REQUIRE(set_cookie.has_value());
        CHECK(set_cookie->find("Max-Age=0") != std::string::npos);
        CHECK(set_cookie->find("Path=/api") != std::string::npos);
    }
}

TEST_CASE("HTTP Server Middleware Pipeline - Global & Prefix Execution", "[runtime][http][middleware][pipeline]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);
    std::vector<std::string> execution_order;
    std::mutex order_mutex;

    // Global middleware: logs and enriches locals
    server.use([&](Request& req, Response&, NextFunction next) {
        {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back("global_mw:" + req.path);
        }
        req.locals["visited_global"] = "true";
        next();
    });

    // Prefix middleware mounted at /api
    server.use("/api", [&](Request& req, Response& res, NextFunction next) {
        {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back("api_prefix_mw:" + req.path);
        }
        if (req.header("authorization") != "Bearer secret") {
            res.status(401).json("{\"error\":\"unauthorized\"}");
            return;
        }
        req.locals["auth_user"] = "admin";
        next();
    });

    // Route handlers
    server.get("/api/dashboard", [](Request& req, Response& res) {
        res.json("{\"message\":\"welcome " + req.locals["auth_user"] + "\"}");
    });

    server.get("/public/info", [](Request& req, Response& res) {
        res.send("Public info, global=" + req.locals["visited_global"]);
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    SECTION("Public route executes global middleware only") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /public/info HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 200 OK") != std::string::npos);
        CHECK(resp.find("Public info, global=true") != std::string::npos);

        std::lock_guard<std::mutex> lock(order_mutex);
        REQUIRE_FALSE(execution_order.empty());
        CHECK(execution_order.back() == "global_mw:/public/info");
    }

    SECTION("Protected /api route rejected when unauthorized") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /api/dashboard HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 401 Unauthorized") != std::string::npos);
        CHECK(resp.find("{\"error\":\"unauthorized\"}") != std::string::npos);
    }

    SECTION("Protected /api route succeeds when authorized") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /api/dashboard HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Authorization: Bearer secret\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 200 OK") != std::string::npos);
        CHECK(resp.find("welcome admin") != std::string::npos);
    }

    server.stop();
}

TEST_CASE("HTTP Server Middleware Pipeline - Route-Level Middleware & Error Handling", "[runtime][http][middleware][error]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);

    // Route-level middleware interceptor
    server.get("/route/protected",
        [](Request& req, Response& res, NextFunction next) {
            if (req.query_param("key") != "valid") {
                res.status(403).send("Forbidden: invalid key");
                return;
            }
            next();
        },
        [](Request&, Response& res) {
            res.send("Access granted");
        }
    );

    // Route that triggers error via next("db failure")
    server.get("/cause-error",
        [](Request&, Response&, NextFunction next) {
            next("database disconnected");
        },
        [](Request&, Response& res) {
            res.send("Should never be reached");
        }
    );

    // Route that throws an exception
    server.get("/cause-exception", [](Request&, Response&) {
        throw std::runtime_error("unhandled panic in handler");
    });

    // Global error-handling middleware
    server.use([](const std::string& err, Request&, Response& res, NextFunction) {
        res.status(500).json("{\"error\":\"Caught: " + err + "\"}");
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    SECTION("Route middleware blocking") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /route/protected?key=invalid HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 403 Forbidden") != std::string::npos);
        CHECK(resp.find("Forbidden: invalid key") != std::string::npos);
    }

    SECTION("Route middleware allowing access") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /route/protected?key=valid HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 200 OK") != std::string::npos);
        CHECK(resp.find("Access granted") != std::string::npos);
    }

    SECTION("Error middleware catches next(err)") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /cause-error HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 500 Internal Server Error") != std::string::npos);
        CHECK(resp.find("database disconnected") != std::string::npos);
    }

    SECTION("Error middleware catches thrown exceptions") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /cause-exception HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 500 Internal Server Error") != std::string::npos);
        CHECK(resp.find("unhandled panic in handler") != std::string::npos);
    }

    server.stop();
}
