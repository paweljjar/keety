#include <catch2/catch_all.hpp>
#include <keet/http.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
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

TEST_CASE("HTTP Middleware - CORS Interception & Preflight", "[runtime][http][middleware][cors]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);

    CorsOptions cors_opts;
    cors_opts.origin = "https://app.example.com, https://admin.example.com";
    cors_opts.credentials = true;
    cors_opts.allowed_headers = {"Content-Type", "Authorization", "X-Api-Key"};
    cors_opts.exposed_headers = {"X-Total-Count"};
    cors_opts.max_age = 86400;

    server.use(cors(cors_opts));

    server.post("/api/items", [](Request&, Response& res) {
        res.setHeader("X-Total-Count", "42");
        res.json("{\"created\":true}");
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    SECTION("Preflight OPTIONS returns 204 with CORS headers") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "OPTIONS /api/items HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Origin: https://app.example.com\r\n"
                          "Access-Control-Request-Method: POST\r\n"
                          "Access-Control-Request-Headers: Content-Type, Authorization\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 204 No Content") != std::string::npos);
        CHECK(resp.find("Access-Control-Allow-Origin: https://app.example.com") != std::string::npos);
        CHECK(resp.find("Access-Control-Allow-Credentials: true") != std::string::npos);
        CHECK(resp.find("Access-Control-Allow-Methods: GET, HEAD, PUT, PATCH, POST, DELETE") != std::string::npos);
        CHECK(resp.find("Access-Control-Allow-Headers: Content-Type, Authorization, X-Api-Key") != std::string::npos);
        CHECK(resp.find("Access-Control-Max-Age: 86400") != std::string::npos);
    }

    SECTION("Standard request sets CORS and exposed headers") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "POST /api/items HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Origin: https://admin.example.com\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 200 OK") != std::string::npos);
        CHECK(resp.find("Access-Control-Allow-Origin: https://admin.example.com") != std::string::npos);
        CHECK(resp.find("Access-Control-Allow-Credentials: true") != std::string::npos);
        CHECK(resp.find("Access-Control-Expose-Headers: X-Total-Count") != std::string::npos);
        CHECK(resp.find("X-Total-Count: 42") != std::string::npos);
        CHECK(resp.find("{\"created\":true}") != std::string::npos);
    }

    SECTION("Disallowed origin does not receive Access-Control-Allow-Origin") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "POST /api/items HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Origin: https://malicious.example.com\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("Access-Control-Allow-Origin: https://malicious.example.com") == std::string::npos);
    }

    server.stop();
}

TEST_CASE("HTTP Middleware - Body Parsers (json, urlencoded, raw)", "[runtime][http][middleware][body]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);
    server.use(jsonParser(1024));
    server.use(urlencodedParser(1024));
    server.use(rawParser(1024));

    server.post("/api/json-echo", [](Request& req, Response& res) {
        if (!req.has_json()) {
            res.status(400).send("No json");
            return;
        }
        std::string name = req.json_field("name");
        std::string age = req.json_field("age");
        res.json("{\"greeting\":\"hello " + name + "\",\"age\":" + age + "}");
    });

    server.post("/api/form-submit", [](Request& req, Response& res) {
        std::string username = req.body_param("username");
        std::string email = req.body_param("email");
        res.send("Received user=" + username + ", email=" + email);
    });

    server.post("/api/raw-echo", [](Request& req, Response& res) {
        res.setHeader("X-Raw-Length", std::to_string(req.raw_body.size()));
        res.send(req.raw_body);
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    SECTION("jsonParser parses valid JSON body") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = "{\"name\":\"Keety\",\"age\":2}";
        std::string req = "POST /api/json-echo HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                          "Connection: close\r\n"
                          "\r\n" + payload;
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 200 OK") != std::string::npos);
        CHECK(resp.find("{\"greeting\":\"hello Keety\",\"age\":2}") != std::string::npos);
    }

    SECTION("jsonParser rejects malformed JSON with 400 Bad Request") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = "{\"invalid_json: ...";
        std::string req = "POST /api/json-echo HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                          "Connection: close\r\n"
                          "\r\n" + payload;
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 400 Bad Request") != std::string::npos);
        CHECK(resp.find("Bad Request") != std::string::npos);
    }

    SECTION("urlencodedParser parses form-urlencoded body") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = "username=admin_user&email=admin%40example.com";
        std::string req = "POST /api/form-submit HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Content-Type: application/x-www-form-urlencoded\r\n"
                          "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                          "Connection: close\r\n"
                          "\r\n" + payload;
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 200 OK") != std::string::npos);
        CHECK(resp.find("Received user=admin_user, email=admin@example.com") != std::string::npos);
    }

    SECTION("rawParser captures raw binary buffer") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = "raw_binary_content_12345";
        std::string req = "POST /api/raw-echo HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Content-Type: application/octet-stream\r\n"
                          "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                          "Connection: close\r\n"
                          "\r\n" + payload;
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 200 OK") != std::string::npos);
        CHECK(resp.find("X-Raw-Length: " + std::to_string(payload.size())) != std::string::npos);
        CHECK(resp.find("raw_binary_content_12345") != std::string::npos);
    }

    server.stop();
}

TEST_CASE("HTTP Middleware - Operational (logger, staticDir, recover)", "[runtime][http][middleware][ops]") {
    // Set up a temporary static directory
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / ("keet_http_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp_dir);

    // Create static test files
    {
        std::ofstream index_file(temp_dir / "index.html");
        index_file << "<html><body><h1>Hello Static</h1></body></html>";
    }
    {
        std::ofstream css_file(temp_dir / "style.css");
        css_file << "body { background: #333; color: white; }";
    }
    {
        std::ofstream json_file(temp_dir / "manifest.json");
        json_file << "{\"name\":\"test-app\",\"version\":\"1.0.0\"}";
    }

    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);

    // Capture logs via custom sink
    std::vector<std::string> logged_lines;
    std::mutex log_mutex;
    LoggerOptions log_opts;
    log_opts.color = false;
    log_opts.sink = [&](std::string_view line) {
        std::lock_guard<std::mutex> lock(log_mutex);
        logged_lines.emplace_back(line);
    };

    server.use(logger(log_opts));
    server.use(recover());
    server.use(staticDir("/static", temp_dir.string()));

    server.get("/api/panic", [](Request&, Response&) {
        throw std::runtime_error("simulated critical crash in handler");
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    SECTION("staticDir serves index.html and sets correct MIME type and caching headers") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /static/index.html HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 200 OK") != std::string::npos);
        CHECK(resp.find("Content-Type: text/html; charset=utf-8") != std::string::npos);
        CHECK(resp.find("ETag: \"") != std::string::npos);
        CHECK(resp.find("Last-Modified: ") != std::string::npos);
        CHECK(resp.find("<h1>Hello Static</h1>") != std::string::npos);

        // Extract ETag
        size_t etag_pos = resp.find("ETag: ");
        REQUIRE(etag_pos != std::string::npos);
        size_t etag_end = resp.find("\r\n", etag_pos);
        std::string etag = resp.substr(etag_pos + 6, etag_end - (etag_pos + 6));

        // Test 304 Not Modified caching with If-None-Match
        keet::net::detail::socket_t client2 = connect_client(port);
        REQUIRE(client2 != keet::net::detail::kInvalidSocket);
        std::string req2 = "GET /static/index.html HTTP/1.1\r\n"
                           "Host: 127.0.0.1\r\n"
                           "If-None-Match: " + etag + "\r\n"
                           "Connection: close\r\n"
                           "\r\n";
        REQUIRE(detail::socket_send_all(client2, req2.data(), req2.size()));
        std::string resp2 = read_http_response(client2);
        keet::net::detail::close_raw_socket(client2);

        CHECK(resp2.find("HTTP/1.1 304 Not Modified") != std::string::npos);
    }

    SECTION("staticDir serves css with text/css MIME type") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /static/style.css HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 200 OK") != std::string::npos);
        CHECK(resp.find("Content-Type: text/css; charset=utf-8") != std::string::npos);
        CHECK(resp.find("background: #333") != std::string::npos);
    }

    SECTION("staticDir blocks path traversal with 403 Forbidden") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /static/../etc/passwd HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 403 Forbidden") != std::string::npos);
    }

    SECTION("recover middleware prevents uncaught panic from crashing server and returns 500 JSON") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /api/panic HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 500 Internal Server Error") != std::string::npos);
        CHECK(resp.find("simulated critical crash in handler") != std::string::npos);

        // Server is still alive and healthy for subsequent requests!
        keet::net::detail::socket_t client_alive = connect_client(port);
        REQUIRE(client_alive != keet::net::detail::kInvalidSocket);
        std::string req_alive = "GET /static/manifest.json HTTP/1.1\r\n"
                                "Host: 127.0.0.1\r\n"
                                "Connection: close\r\n"
                                "\r\n";
        REQUIRE(detail::socket_send_all(client_alive, req_alive.data(), req_alive.size()));
        std::string resp_alive = read_http_response(client_alive);
        keet::net::detail::close_raw_socket(client_alive);

        CHECK(resp_alive.find("HTTP/1.1 200 OK") != std::string::npos);
        CHECK(resp_alive.find("test-app") != std::string::npos);
    }

    SECTION("logger captures requests and response status codes") {
        keet::net::detail::socket_t client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /static/index.html HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Connection: close\r\n"
                          "\r\n";
        REQUIRE(detail::socket_send_all(client, req.data(), req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        std::lock_guard<std::mutex> lock(log_mutex);
        REQUIRE_FALSE(logged_lines.empty());
        bool found_200 = false;
        for (const auto& l : logged_lines) {
            if (l.find("GET") != std::string::npos && l.find("/static/index.html") != std::string::npos) {
                found_200 = true;
                break;
            }
        }
        CHECK(found_200);
    }

    server.stop();
    std::filesystem::remove_all(temp_dir);
}
