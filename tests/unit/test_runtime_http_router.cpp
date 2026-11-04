#include <catch2/catch_all.hpp>
#include <keet/http.hpp>

#include <string>
#include <vector>

using namespace keet::net::http;

TEST_CASE("Radix Tree Router - Static Matching & Normalization", "[runtime][http][router]") {
    HttpRouter router;

    router.get("/", [](const HttpRequestMessage&) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 200;
        res.body = "root";
        return res;
    });

    router.get("/api/v1/health", [](const HttpRequestMessage&) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 200;
        res.body = "healthy";
        return res;
    });

    router.post("/api/v1/users", [](const HttpRequestMessage&) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 201;
        res.body = "created user";
        return res;
    });

    SECTION("Exact static matches") {
        auto m1 = router.match("GET", "/");
        REQUIRE(m1.status == RouteMatchStatus::Found);
        HttpRequestMessage req1;
        CHECK(m1.handler(req1).body == "root");

        auto m2 = router.match("GET", "/api/v1/health");
        REQUIRE(m2.status == RouteMatchStatus::Found);
        HttpRequestMessage req2;
        CHECK(m2.handler(req2).body == "healthy");

        auto m3 = router.match("POST", "/api/v1/users");
        REQUIRE(m3.status == RouteMatchStatus::Found);
        HttpRequestMessage req3;
        CHECK(m3.handler(req3).body == "created user");
    }

    SECTION("Trailing and duplicate slash normalization") {
        auto m1 = router.match("GET", "///api//v1///health//");
        REQUIRE(m1.status == RouteMatchStatus::Found);
        HttpRequestMessage req;
        CHECK(m1.handler(req).body == "healthy");
    }

    SECTION("Method case-insensitivity in matching") {
        auto m = router.match("post", "/api/v1/users");
        REQUIRE(m.status == RouteMatchStatus::Found);
    }

    SECTION("Unmatched route returns NotFound") {
        auto m = router.match("GET", "/api/v2/unknown");
        CHECK(m.status == RouteMatchStatus::NotFound);
    }
}

TEST_CASE("Radix Tree Router - Dynamic Named Parameters", "[runtime][http][router]") {
    HttpRouter router;

    router.get("/users/:id", [](const HttpRequestMessage& req) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 200;
        res.body = "user:" + req.param("id");
        return res;
    });

    router.get("/users/me", [](const HttpRequestMessage&) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 200;
        res.body = "user:current_authenticated_user";
        return res;
    });

    router.get("/orgs/:orgId/teams/:teamId", [](const HttpRequestMessage& req) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 200;
        res.body = "org=" + req.param("orgId") + "&team=" + req.param("teamId");
        return res;
    });

    SECTION("Single parameter extraction") {
        auto m = router.match("GET", "/users/42");
        REQUIRE(m.status == RouteMatchStatus::Found);
        CHECK(m.params.at("id") == "42");

        HttpRequestMessage req;
        req.params = m.params;
        CHECK(m.handler(req).body == "user:42");
    }

    SECTION("Static route precedence over parameter route") {
        // /users/me must take precedence over /users/:id
        auto m = router.match("GET", "/users/me");
        REQUIRE(m.status == RouteMatchStatus::Found);
        CHECK(m.params.empty());

        HttpRequestMessage req;
        CHECK(m.handler(req).body == "user:current_authenticated_user");
    }

    SECTION("Multiple parameters in path") {
        auto m = router.match("GET", "/orgs/google/teams/deepmind");
        REQUIRE(m.status == RouteMatchStatus::Found);
        CHECK(m.params.at("orgId") == "google");
        CHECK(m.params.at("teamId") == "deepmind");

        HttpRequestMessage req;
        req.params = m.params;
        CHECK(m.handler(req).body == "org=google&team=deepmind");
    }
}

TEST_CASE("Radix Tree Router - Wildcard & Catch-All", "[runtime][http][router]") {
    HttpRouter router;

    router.get("/static/*filepath", [](const HttpRequestMessage& req) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 200;
        res.body = "asset:" + req.param("filepath");
        return res;
    });

    router.all("*", [](const HttpRequestMessage& req) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 404;
        res.body = "catch-all:" + req.param("*");
        return res;
    });

    SECTION("Trailing wildcard matching subpaths") {
        auto m = router.match("GET", "/static/css/theme/dark.min.css");
        REQUIRE(m.status == RouteMatchStatus::Found);
        CHECK(m.params.at("filepath") == "css/theme/dark.min.css");

        HttpRequestMessage req;
        req.params = m.params;
        CHECK(m.handler(req).body == "asset:css/theme/dark.min.css");
    }

    SECTION("Global catch-all wildcard for unmatched routes") {
        auto m = router.match("GET", "/some/random/missing/url");
        REQUIRE(m.status == RouteMatchStatus::Found);
        CHECK(m.params.at("*") == "some/random/missing/url");

        HttpRequestMessage req;
        req.params = m.params;
        CHECK(m.handler(req).body == "catch-all:some/random/missing/url");
    }
}

TEST_CASE("Radix Tree Router - HTTP Method Matrix & 405 Method Not Allowed", "[runtime][http][router]") {
    HttpRouter router;

    router.get("/items", [](const HttpRequestMessage&) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.body = "get_items";
        return res;
    });

    router.post("/items", [](const HttpRequestMessage&) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.body = "post_items";
        return res;
    });

    router.del("/items/:id", [](const HttpRequestMessage& req) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.body = "deleted:" + req.param("id");
        return res;
    });

    router.all("/webhook", [](const HttpRequestMessage& req) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.body = "all_webhook:" + req.line.method;
        return res;
    });

    SECTION("All standard HTTP methods match appropriately") {
        auto m1 = router.match("GET", "/items");
        REQUIRE(m1.status == RouteMatchStatus::Found);

        auto m2 = router.match("POST", "/items");
        REQUIRE(m2.status == RouteMatchStatus::Found);

        auto m3 = router.match("DELETE", "/items/99");
        REQUIRE(m3.status == RouteMatchStatus::Found);
        CHECK(m3.params.at("id") == "99");
    }

    SECTION("app.all() matches any method on that path") {
        auto m_get = router.match("GET", "/webhook");
        REQUIRE(m_get.status == RouteMatchStatus::Found);

        auto m_post = router.match("POST", "/webhook");
        REQUIRE(m_post.status == RouteMatchStatus::Found);

        auto m_put = router.match("PUT", "/webhook");
        REQUIRE(m_put.status == RouteMatchStatus::Found);

        auto m_patch = router.match("PATCH", "/webhook");
        REQUIRE(m_patch.status == RouteMatchStatus::Found);
    }

    SECTION("405 Method Not Allowed generated when path exists with different verbs") {
        auto m = router.match("PUT", "/items");
        REQUIRE(m.status == RouteMatchStatus::MethodNotAllowed);
        REQUIRE(m.allowed_methods.size() == 2);
        CHECK(m.allowed_methods[0] == "GET");
        CHECK(m.allowed_methods[1] == "POST");
    }
}

TEST_CASE("HTTP Server End-to-End Router Integration", "[runtime][http][server][router]") {
    HttpServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 0;
    cfg.worker_threads = 2;

    HttpServer server(cfg);

    server.get("/products/:id", [](const HttpRequestMessage& req) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 200;
        res.headers.set("Content-Type", "application/json");
        res.body = "{\"product_id\":\"" + req.param("id") + "\"}";
        return res;
    });

    server.post("/products", [](const HttpRequestMessage& req) -> HttpResponseMessage {
        HttpResponseMessage res;
        res.status = 201;
        res.body = "created: " + req.body;
        return res;
    });

    REQUIRE(server.start());
    int port = server.bound_port();

    keet::net::detail::ensure_winsock_initialized();
    auto connect_sock = [port]() {
        keet::net::detail::socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        ::connect(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        return s;
    };

    SECTION("GET /products/101 resolves route and returns 200 OK") {
        auto client = connect_sock();
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /products/101 HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
        detail::socket_send_all(client, req.data(), req.size());

        std::string resp;
        std::array<char, 1024> buf;
        while (true) {
            int pr = detail::socket_poll(client, POLLIN, 2000);
            if (pr <= 0) break;
            ssize_t n = detail::socket_recv(client, buf.data(), buf.size());
            if (n <= 0) break;
            resp.append(buf.data(), static_cast<size_t>(n));
        }
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 200 OK") != std::string::npos);
        CHECK(resp.find("{\"product_id\":\"101\"}") != std::string::npos);
    }

    SECTION("DELETE /products returns 405 Method Not Allowed with Allow header") {
        auto client = connect_sock();
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "DELETE /products HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
        detail::socket_send_all(client, req.data(), req.size());

        std::string resp;
        std::array<char, 1024> buf;
        while (true) {
            int pr = detail::socket_poll(client, POLLIN, 2000);
            if (pr <= 0) break;
            ssize_t n = detail::socket_recv(client, buf.data(), buf.size());
            if (n <= 0) break;
            resp.append(buf.data(), static_cast<size_t>(n));
        }
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 405 Method Not Allowed") != std::string::npos);
        CHECK(resp.find("Allow: POST") != std::string::npos);
    }

    SECTION("GET /nonexistent returns 404 Cannot GET /nonexistent") {
        auto client = connect_sock();
        REQUIRE(client != keet::net::detail::kInvalidSocket);

        std::string req = "GET /nonexistent HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
        detail::socket_send_all(client, req.data(), req.size());

        std::string resp;
        std::array<char, 1024> buf;
        while (true) {
            int pr = detail::socket_poll(client, POLLIN, 2000);
            if (pr <= 0) break;
            ssize_t n = detail::socket_recv(client, buf.data(), buf.size());
            if (n <= 0) break;
            resp.append(buf.data(), static_cast<size_t>(n));
        }
        keet::net::detail::close_raw_socket(client);

        CHECK(resp.find("HTTP/1.1 404 Not Found") != std::string::npos);
        CHECK(resp.find("Cannot GET /nonexistent") != std::string::npos);
    }

    server.stop();
}
