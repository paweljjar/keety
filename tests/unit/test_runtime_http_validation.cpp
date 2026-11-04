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

TEST_CASE("HTTP Validation - Schema Definition DSL", "[runtime][http][validation][dsl]") {
    auto s = schema();
    s.body("email", FieldType::String).required().pattern(R"(^[^@]+@[^@]+\.[^@]+$)");
    s.body("age", FieldType::Int).required().min(18).max(120);
    s.body("score", FieldType::Float).optional().min(0.0).max(100.0);
    s.body("verified", FieldType::Bool).required();
    s.body("roles", FieldType::Array).min(1).max(5);
    s.body("meta", FieldType::Object).required();
    s.query("page", FieldType::Int).optional().default_value("1").min(1);
    s.param("id", FieldType::Int).required().min(1);

    REQUIRE(s.fields.size() == 8);
    CHECK(s.fields[0].name == "email");
    CHECK(s.fields[0].target == SchemaTarget::Body);
    CHECK(s.fields[0].type == SchemaType::String);
    CHECK(s.fields[0].is_required == true);
    CHECK(s.fields[0].pattern_regex.has_value());

    CHECK(s.fields[1].name == "age");
    CHECK(s.fields[1].type == SchemaType::Int);
    CHECK(s.fields[1].min_value == 18.0);
    CHECK(s.fields[1].max_value == 120.0);

    CHECK(s.fields[6].name == "page");
    CHECK(s.fields[6].target == SchemaTarget::Query);
    CHECK(s.fields[6].default_value_str == "1");
    CHECK(s.fields[6].is_required == false);

    CHECK(s.fields[7].name == "id");
    CHECK(s.fields[7].target == SchemaTarget::Params);
    CHECK(s.fields[7].is_required == true);
}

TEST_CASE("HTTP Validation - Body Validation with Type & Constraints", "[runtime][http][validation][body]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);
    server.use(jsonParser());

    auto user_schema = schema();
    user_schema.body("email", FieldType::String).required().pattern(R"(^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+$)");
    user_schema.body("age", FieldType::Int).required().min(18).max(120);
    user_schema.body("role", FieldType::String).optional().default_value("member").enum_values({"member", "admin", "moderator"});

    server.post("/api/users", validate(user_schema), [](Request& req, Response& res) {
        std::string email = req.validated("email");
        int64_t age = req.validated_int("age");
        std::string role = req.validated("role");

        res.json("{\"status\":\"ok\",\"email\":\"" + email + "\",\"age\":" + std::to_string(age) + ",\"role\":\"" + role + "\"}");
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    SECTION("Valid request passes validation and injects validated data") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = R"({"email":"alice@example.com","age":25,"role":"admin"})";
        std::string raw_req = "POST /api/users HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("200 OK") != std::string::npos);
        CHECK(resp.find("\"status\":\"ok\"") != std::string::npos);
        CHECK(resp.find("\"email\":\"alice@example.com\"") != std::string::npos);
        CHECK(resp.find("\"age\":25") != std::string::npos);
        CHECK(resp.find("\"role\":\"admin\"") != std::string::npos);
    }

    SECTION("Default value is applied when optional field is omitted") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = R"({"email":"bob@example.com","age":30})";
        std::string raw_req = "POST /api/users HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("200 OK") != std::string::npos);
        CHECK(resp.find("\"role\":\"member\"") != std::string::npos);
    }

    SECTION("Missing required field returns 400 Bad Request with structured error") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = R"({"age":25})";
        std::string raw_req = "POST /api/users HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("\"error\":\"ValidationError\"") != std::string::npos);
        CHECK(resp.find("\"field\":\"body.email\"") != std::string::npos);
        CHECK(resp.find("Field 'email' is required") != std::string::npos);
    }

    SECTION("Regex pattern violation returns 400 Bad Request") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = R"({"email":"invalid-email-address","age":25})";
        std::string raw_req = "POST /api/users HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("\"error\":\"ValidationError\"") != std::string::npos);
        CHECK(resp.find("\"field\":\"body.email\"") != std::string::npos);
    }

    SECTION("Type mismatch returns 400 Bad Request") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = R"({"email":"alice@example.com","age":"twenty-five"})";
        std::string raw_req = "POST /api/users HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("\"error\":\"ValidationError\"") != std::string::npos);
        CHECK(resp.find("\"field\":\"body.age\"") != std::string::npos);
        CHECK(resp.find("Field 'age' must be an integer") != std::string::npos);
    }

    SECTION("Min constraint violation returns 400 Bad Request") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = R"({"email":"alice@example.com","age":15})";
        std::string raw_req = "POST /api/users HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("\"field\":\"body.age\"") != std::string::npos);
        CHECK(resp.find("Field 'age' must be at least 18") != std::string::npos);
    }

    SECTION("Enum constraint violation returns 400 Bad Request") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = R"({"email":"alice@example.com","age":30,"role":"superadmin"})";
        std::string raw_req = "POST /api/users HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("\"field\":\"body.role\"") != std::string::npos);
        CHECK(resp.find("must be one of allowed values") != std::string::npos);
    }

    server.stop();
}

TEST_CASE("HTTP Validation - Query & Path Parameter Validation", "[runtime][http][validation][query_params]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);

    auto list_schema = schema();
    list_schema.param("category", FieldType::String).required().min(2);
    list_schema.query("page", FieldType::Int).optional().default_value("1").min(1);
    list_schema.query("limit", FieldType::Int).optional().default_value("20").min(1).max(100);
    list_schema.query("active", FieldType::Bool).optional().default_value("true");

    server.get("/api/catalog/:category", validate(list_schema), [](Request& req, Response& res) {
        std::string cat = req.validated("category");
        int64_t page = req.validated_int("page");
        int64_t limit = req.validated_int("limit");
        bool active = req.validated_bool("active");

        res.json("{\"category\":\"" + cat + "\",\"page\":" + std::to_string(page) +
                 ",\"limit\":" + std::to_string(limit) + ",\"active\":" + (active ? "true" : "false") + "}");
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    SECTION("Valid params and query with default values applied") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string raw_req = "GET /api/catalog/electronics HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Connection: close\r\n\r\n";

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("200 OK") != std::string::npos);
        CHECK(resp.find("\"category\":\"electronics\"") != std::string::npos);
        CHECK(resp.find("\"page\":1") != std::string::npos);
        CHECK(resp.find("\"limit\":20") != std::string::npos);
        CHECK(resp.find("\"active\":true") != std::string::npos);
    }

    SECTION("Explicit query parameters parsed and validated") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string raw_req = "GET /api/catalog/books?page=3&limit=50&active=false HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Connection: close\r\n\r\n";

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("200 OK") != std::string::npos);
        CHECK(resp.find("\"category\":\"books\"") != std::string::npos);
        CHECK(resp.find("\"page\":3") != std::string::npos);
        CHECK(resp.find("\"limit\":50") != std::string::npos);
        CHECK(resp.find("\"active\":false") != std::string::npos);
    }

    SECTION("Invalid query integer returns 400 Bad Request") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string raw_req = "GET /api/catalog/books?page=not_a_number HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Connection: close\r\n\r\n";

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("\"field\":\"query.page\"") != std::string::npos);
        CHECK(resp.find("Field 'page' must be an integer") != std::string::npos);
    }

    SECTION("Query max constraint violation returns 400 Bad Request") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string raw_req = "GET /api/catalog/books?limit=500 HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Connection: close\r\n\r\n";

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("\"field\":\"query.limit\"") != std::string::npos);
        CHECK(resp.find("Field 'limit' must be at most 100") != std::string::npos);
    }

    SECTION("Invalid boolean query parameter returns 400 Bad Request") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string raw_req = "GET /api/catalog/books?active=maybe HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Connection: close\r\n\r\n";

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("\"field\":\"query.active\"") != std::string::npos);
        CHECK(resp.find("Field 'active' must be a boolean") != std::string::npos);
    }

    server.stop();
}

TEST_CASE("HTTP Validation - Nested JSON Dotted Fields & Array/Object Support", "[runtime][http][validation][nested]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);
    server.use(jsonParser());

    auto complex_schema = schema();
    complex_schema.body("user.name", FieldType::String).required().min(2);
    complex_schema.body("user.profile.age", FieldType::Int).required().min(18);
    complex_schema.body("tags", FieldType::Array).required().min(1).max(3);
    complex_schema.body("config", FieldType::Object).required();

    server.post("/api/complex", validate(complex_schema), [](Request& req, Response& res) {
        std::string name = req.validated("user.name");
        int64_t age = req.validated_int("user.profile.age");
        res.json("{\"name\":\"" + name + "\",\"age\":" + std::to_string(age) + "}");
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    SECTION("Valid nested JSON structure passes validation") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = R"({
            "user": {
                "name": "Bob",
                "profile": {
                    "age": 28
                }
            },
            "tags": ["alpha", "beta"],
            "config": {"theme": "dark"}
        })";

        std::string raw_req = "POST /api/complex HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("200 OK") != std::string::npos);
        CHECK(resp.find("\"name\":\"Bob\"") != std::string::npos);
        CHECK(resp.find("\"age\":28") != std::string::npos);
    }

    SECTION("Missing nested field fails with dotted path") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = R"({
            "user": {
                "name": "Bob"
            },
            "tags": ["alpha"],
            "config": {}
        })";

        std::string raw_req = "POST /api/complex HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("\"field\":\"body.user.profile.age\"") != std::string::npos);
    }

    SECTION("Array size limit violation fails validation") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = R"({
            "user": {
                "name": "Bob",
                "profile": { "age": 28 }
            },
            "tags": ["a", "b", "c", "d"],
            "config": {}
        })";

        std::string raw_req = "POST /api/complex HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("\"field\":\"body.tags\"") != std::string::npos);
        CHECK(resp.find("items") != std::string::npos);
    }

    server.stop();
}

TEST_CASE("HTTP Validation - Custom Error Messages & Auto JSON Parsing", "[runtime][http][validation][custom_msg]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);
    // Notice: NO jsonParser() middleware registered on server.
    // validate() should automatically parse the JSON body!

    auto s = schema();
    s.body("token", FieldType::String).required().min(10).message("Authentication token must be at least 10 chars");

    server.post("/api/auth", validate(s), [](Request& req, Response& res) {
        res.json("{\"authenticated\":true,\"token\":\"" + req.validated("token") + "\"}");
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    SECTION("Custom error message is emitted on constraint violation") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = R"({"token":"short"})";
        std::string raw_req = "POST /api/auth HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("\"field\":\"body.token\"") != std::string::npos);
        CHECK(resp.find("\"message\":\"Authentication token must be at least 10 chars\"") != std::string::npos);
    }

    SECTION("Auto JSON parsing succeeds without explicit jsonParser() middleware") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = R"({"token":"secret-token-12345"})";
        std::string raw_req = "POST /api/auth HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("200 OK") != std::string::npos);
        CHECK(resp.find("\"authenticated\":true") != std::string::npos);
        CHECK(resp.find("\"token\":\"secret-token-12345\"") != std::string::npos);
    }

    SECTION("Malformed JSON body returns 400 Bad Request") {
        auto client = connect_client(port);
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string payload = "{ not valid json";
        std::string raw_req = "POST /api/auth HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + payload;

        REQUIRE(detail::socket_send_all(client, raw_req.data(), raw_req.size()));
        std::string resp = read_http_response(client);
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("400 Bad Request") != std::string::npos);
        CHECK(resp.find("\"field\":\"body\"") != std::string::npos);
    }

    server.stop();
}
