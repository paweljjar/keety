#include <catch2/catch_test_macros.hpp>
#include <keet/http.hpp>

#include <chrono>
#include <string>
#include <vector>

using namespace keet::net::http;

TEST_CASE("HTTP HeaderMap - Case-Insensitive Storage & Mutation", "[runtime][http][headers]") {
    HeaderMap headers;
    REQUIRE(headers.empty());
    REQUIRE(headers.size() == 0);

    SECTION("Case-insensitive lookup and modification") {
        headers.set("Content-Type", "application/json");
        headers.set("authorization", "Bearer token123");

        CHECK(headers.has("Content-Type"));
        CHECK(headers.has("content-type"));
        CHECK(headers.has("CONTENT-TYPE"));
        CHECK(headers.has("Authorization"));
        CHECK(headers.has("AUTHORIZATION"));
        CHECK(!headers.has("Accept"));

        CHECK(headers.get("content-type") == "application/json");
        CHECK(headers.get("Authorization") == "Bearer token123");
        CHECK(headers.get_or("accept", "text/plain") == "text/plain");

        // Overwrite existing header
        headers.set("content-type", "text/html; charset=utf-8");
        CHECK(headers.size() == 2);
        CHECK(headers.get("Content-Type") == "text/html; charset=utf-8");

        // Remove header
        CHECK(headers.remove("AUTHORIZATION") == true);
        CHECK(!headers.has("Authorization"));
        CHECK(headers.size() == 1);
        CHECK(headers.remove("Authorization") == false);
    }

    SECTION("Header list appending and Set-Cookie preservation") {
        headers.add("Accept", "text/html");
        headers.add("accept", "application/xhtml+xml");
        headers.add("ACCEPT", "application/xml;q=0.9");

        CHECK(headers.get("Accept") == "text/html, application/xhtml+xml, application/xml;q=0.9");

        // Set-Cookie must create separate entries
        headers.add("Set-Cookie", "session=abc; HttpOnly");
        headers.add("set-cookie", "theme=dark; Secure");
        CHECK(headers.size() == 3); // 1 Accept + 2 Set-Cookie entries
    }
}

TEST_CASE("HTTP URL Decoding & Query String Parser", "[runtime][http][url]") {
    SECTION("Percent decoding with plus-as-space support") {
        CHECK(url_decode("hello%20world") == "hello world");
        CHECK(url_decode("key%3Dvalue%26test", false) == "key=value&test");
        CHECK(url_decode("search+term%21", true) == "search term!");
        CHECK(url_decode("search+term%21", false) == "search+term!");
        CHECK(url_decode("no_escapes") == "no_escapes");
    }

    SECTION("Query string parsing into key-value map") {
        auto q1 = parse_query_string("?name=Alice&age=30&active=true");
        CHECK(q1.size() == 3);
        CHECK(q1.at("name") == "Alice");
        CHECK(q1.at("age") == "30");
        CHECK(q1.at("active") == "true");

        // Semicolon separator and empty values
        auto q2 = parse_query_string("tag=compiler;debug&empty=&novalue");
        CHECK(q2.at("tag") == "compiler");
        CHECK(q2.at("debug") == "");
        CHECK(q2.at("empty") == "");
        CHECK(q2.at("novalue") == "");

        // Percent-encoded keys and values
        auto q3 = parse_query_string("filter%20name=John%20Doe&email=john%2Btest%40example.com");
        CHECK(q3.at("filter name") == "John Doe");
        CHECK(q3.at("email") == "john+test@example.com");
    }
}

TEST_CASE("HTTP Status Codes & Reason Phrases", "[runtime][http][status]") {
    CHECK(status_reason_phrase(200) == "OK");
    CHECK(status_reason_phrase(201) == "Created");
    CHECK(status_reason_phrase(204) == "No Content");
    CHECK(status_reason_phrase(301) == "Moved Permanently");
    CHECK(status_reason_phrase(304) == "Not Modified");
    CHECK(status_reason_phrase(400) == "Bad Request");
    CHECK(status_reason_phrase(401) == "Unauthorized");
    CHECK(status_reason_phrase(403) == "Forbidden");
    CHECK(status_reason_phrase(404) == "Not Found");
    CHECK(status_reason_phrase(405) == "Method Not Allowed");
    CHECK(status_reason_phrase(413) == "Payload Too Large");
    CHECK(status_reason_phrase(431) == "Request Header Fields Too Large");
    CHECK(status_reason_phrase(500) == "Internal Server Error");
    CHECK(status_reason_phrase(503) == "Service Unavailable");

    // RFC 1123 Date Format verification
    std::string date = format_http_date();
    CHECK(date.size() == 29);
    CHECK(date.find("GMT") == 26);
    CHECK(date.find(", ") == 3);
}

TEST_CASE("HTTP Request Parser - Request Lines & Target Parsing", "[runtime][http][parser]") {
    HttpRequestParser parser;

    SECTION("Valid GET request line with query string") {
        std::string raw = "GET /api/users?active=1&role=admin HTTP/1.1\r\n"
                          "Host: localhost:8080\r\n"
                          "\r\n";
        bool ok = parser.feed(raw);
        REQUIRE(ok);
        REQUIRE(parser.is_complete());

        const auto& req = parser.message();
        CHECK(req.line.method == "GET");
        CHECK(req.line.path == "/api/users");
        CHECK(req.line.query_string == "active=1&role=admin");
        CHECK(req.line.version == "HTTP/1.1");
        CHECK(req.query.at("active") == "1");
        CHECK(req.query.at("role") == "admin");
        CHECK(req.headers.get("host") == "localhost:8080");
        CHECK(req.keep_alive == true);
    }

    SECTION("HTTP/1.0 Connection default is close unless keep-alive requested") {
        std::string raw = "GET / HTTP/1.0\r\n"
                          "Host: example.com\r\n"
                          "\r\n";
        REQUIRE(parser.feed(raw));
        REQUIRE(parser.is_complete());
        CHECK(parser.message().keep_alive == false);

        parser.reset();
        std::string raw_keepalive = "GET / HTTP/1.0\r\n"
                                    "Host: example.com\r\n"
                                    "Connection: keep-alive\r\n"
                                    "\r\n";
        REQUIRE(parser.feed(raw_keepalive));
        REQUIRE(parser.is_complete());
        CHECK(parser.message().keep_alive == true);
    }

    SECTION("WebSocket Upgrade request detection") {
        std::string raw = "GET /chat HTTP/1.1\r\n"
                          "Host: server.example.com\r\n"
                          "Upgrade: websocket\r\n"
                          "Connection: Upgrade\r\n"
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                          "Sec-WebSocket-Version: 13\r\n"
                          "\r\n";
        REQUIRE(parser.feed(raw));
        REQUIRE(parser.is_complete());
        CHECK(parser.message().upgrade_websocket == true);
        CHECK(parser.message().headers.get("sec-websocket-key") == "dGhlIHNhbXBsZSBub25jZQ==");
    }

    SECTION("Reject malformed request line") {
        std::string bad_req = "NOT_A_VALID_HTTP_REQUEST\r\n\r\n";
        bool ok = parser.feed(bad_req);
        CHECK(!ok);
        CHECK(parser.is_error());
        CHECK(parser.error_code() == 400);

        parser.reset();
        std::string bad_ver = "GET / HTTP/2.0\r\n\r\n";
        bool ver_ok = parser.feed(bad_ver);
        CHECK(!ver_ok);
        CHECK(parser.is_error());
        CHECK(parser.error_code() == 505);
    }
}

TEST_CASE("HTTP Request Parser - Fixed Body & Content-Length", "[runtime][http][parser]") {
    HttpRequestParser parser;

    std::string headers = "POST /api/items HTTP/1.1\r\n"
                          "Host: localhost\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: 26\r\n"
                          "\r\n";
    std::string body = "{\"name\":\"keet\",\"count\":42}";

    SECTION("All data fed in single buffer") {
        REQUIRE(parser.feed(headers + body));
        REQUIRE(parser.is_complete());
        CHECK(parser.message().body == body);
        CHECK(parser.message().headers.get("content-type") == "application/json");
    }

    SECTION("Incremental byte-by-byte streaming feed") {
        std::string full = headers + body;
        for (char c : full) {
            std::string_view byte(&c, 1);
            parser.feed(byte);
        }
        REQUIRE(parser.is_complete());
        CHECK(parser.message().body == body);
    }

    SECTION("Reject payload exceeding max_body_size limit") {
        HttpParserLimits limits;
        limits.max_body_size = 100; // 100 bytes limit
        HttpRequestParser strict_parser(limits);

        std::string oversized_header = "POST /upload HTTP/1.1\r\n"
                                       "Host: localhost\r\n"
                                       "Content-Length: 1000\r\n"
                                       "\r\n";
        bool ok = strict_parser.feed(oversized_header);
        CHECK(!ok);
        CHECK(strict_parser.is_error());
        CHECK(strict_parser.error_code() == 413);
    }
}

TEST_CASE("HTTP Request Parser - Chunked Transfer Encoding", "[runtime][http][parser]") {
    HttpRequestParser parser;

    SECTION("Multi-chunk payload with chunk extensions and final empty chunk") {
        std::string raw = "POST /stream HTTP/1.1\r\n"
                          "Host: localhost\r\n"
                          "Transfer-Encoding: chunked\r\n"
                          "\r\n"
                          "5;ext=foo\r\n"
                          "hello\r\n"
                          "6\r\n"
                          " world\r\n"
                          "1\r\n"
                          "!\r\n"
                          "0\r\n"
                          "X-Checksum: 12345\r\n"
                          "\r\n";

        REQUIRE(parser.feed(raw));
        REQUIRE(parser.is_complete());
        CHECK(parser.message().body == "hello world!");
        // Verify trailer header was captured
        CHECK(parser.message().headers.get("x-checksum") == "12345");
    }

    SECTION("Incremental chunk feeding") {
        std::string raw = "POST /data HTTP/1.1\r\n"
                          "Host: localhost\r\n"
                          "Transfer-Encoding: chunked\r\n"
                          "\r\n"
                          "4\r\n"
                          "Wiki\r\n"
                          "5\r\n"
                          "pedia\r\n"
                          "0\r\n"
                          "\r\n";

        // Feed in small arbitrary byte chunks
        size_t step = 3;
        for (size_t i = 0; i < raw.size(); i += step) {
            parser.feed(raw.substr(i, std::min(step, raw.size() - i)));
        }

        REQUIRE(parser.is_complete());
        CHECK(parser.message().body == "Wikipedia");
    }
}

TEST_CASE("HTTP Response Serializer - Headers & Formatting", "[runtime][http][serializer]") {
    SECTION("Standard 200 OK JSON response serialization") {
        HttpResponseMessage resp;
        resp.status = 200;
        resp.headers.set("Content-Type", "application/json");
        resp.body = "{\"success\":true}";

        std::string raw = HttpResponseSerializer::serialize(resp, true);

        CHECK(raw.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
        CHECK(raw.find("Content-Type: application/json\r\n") != std::string::npos);
        CHECK(raw.find("Content-Length: 16\r\n") != std::string::npos);
        CHECK(raw.find("Server: keety-http/1.0\r\n") != std::string::npos);
        CHECK(raw.find("Date: ") != std::string::npos);
        CHECK(raw.find("\r\n\r\n{\"success\":true}") != std::string::npos);
    }

    SECTION("Chunked response formatting") {
        HttpResponseMessage resp;
        resp.status = 200;
        resp.chunked = true;
        resp.headers.set("Content-Type", "text/plain");

        std::string header_bytes = HttpResponseSerializer::serialize(resp, true);
        CHECK(header_bytes.find("Transfer-Encoding: chunked\r\n") != std::string::npos);
        CHECK(header_bytes.find("Content-Length:") == std::string::npos);

        std::string chunk1 = HttpResponseSerializer::format_chunk("First chunk");
        CHECK(chunk1 == "b\r\nFirst chunk\r\n"); // 11 in hex is 'b'

        std::string chunk2 = HttpResponseSerializer::format_chunk("Second");
        CHECK(chunk2 == "6\r\nSecond\r\n");

        std::string last = HttpResponseSerializer::format_last_chunk();
        CHECK(last == "0\r\n\r\n");
    }
}
