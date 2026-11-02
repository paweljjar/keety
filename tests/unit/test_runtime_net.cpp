#include <catch2/catch_test_macros.hpp>
#include <keet/fs.hpp>
#include <keet/json.hpp>
#include <keet/net.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

void require_net_error(const std::function<void()>& fn, const std::string& expected_substr = "") {
    bool caught = false;
    try {
        fn();
    } catch (const keet::error& e) {
        caught = true;
        REQUIRE(e.name == "NetError");
        if (!expected_substr.empty()) {
            REQUIRE(e.message.find(expected_substr) != std::string::npos);
        }
    }
    REQUIRE(caught);
}

} // namespace

TEST_CASE("Runtime NET - URL Encoding, Decoding, Parsing & Query Strings", "[runtime][net][url]") {
    SECTION("urlEncode and urlDecode follow RFC 3986") {
        std::string raw = "hello world! @#$%^&*()+=/\\?~-_.\"<>";
        std::string encoded = keet::net::urlEncode(raw);
        REQUIRE(encoded.find(' ') == std::string::npos);
        REQUIRE(encoded.find("%20") != std::string::npos);
        REQUIRE(keet::net::urlDecode(encoded) == raw);

        // Plus decoding in query component compatibility
        REQUIRE(keet::net::urlDecode("first+last%21") == "first last!");

        require_net_error([]() { (void)keet::net::urlDecode("%ZZ"); }, "urlDecode");
        require_net_error([]() { (void)keet::net::urlDecode("%2"); }, "urlDecode");
    }

    SECTION("parseUrl and URL component extractors") {
        std::string url = "https://api.example.com:8443/v1/users?page=2&limit=10#results";
        auto parts = keet::net::parseUrl(url);
        REQUIRE(parts.at("scheme") == "https");
        REQUIRE(parts.at("host") == "api.example.com");
        REQUIRE(parts.at("port") == "8443");
        REQUIRE(parts.at("path") == "/v1/users");
        REQUIRE(parts.at("query") == "page=2&limit=10");
        REQUIRE(parts.at("fragment") == "results");

        REQUIRE(keet::net::urlScheme(url) == "https");
        REQUIRE(keet::net::urlHost(url) == "api.example.com");
        REQUIRE(keet::net::urlPort(url) == 8443);
        REQUIRE(keet::net::urlPath(url) == "/v1/users");
        REQUIRE(keet::net::urlQuery(url) == "page=2&limit=10");

        // Default ports for http and https
        REQUIRE(keet::net::urlPort("http://example.com/test") == 80);
        REQUIRE(keet::net::urlPort("https://example.com") == 443);
        REQUIRE(keet::net::urlPath("https://example.com") == "/");

        // Bracketed IPv6 URL
        std::string v6_url = "http://[::1]:9090/health?ok=1";
        REQUIRE(keet::net::urlScheme(v6_url) == "http");
        REQUIRE(keet::net::urlHost(v6_url) == "::1");
        REQUIRE(keet::net::urlPort(v6_url) == 9090);
        REQUIRE(keet::net::urlPath(v6_url) == "/health");
        REQUIRE(keet::net::urlQuery(v6_url) == "ok=1");

        // Malformed URLs
        require_net_error([]() { (void)keet::net::parseUrl("not_a_url"); });
        require_net_error([]() { (void)keet::net::parseUrl("http:///empty-host"); });
        require_net_error([]() { (void)keet::net::parseUrl("http://example.com:99999/bad-port"); });
    }

    SECTION("buildQuery and parseQuery round-trip deterministically") {
        std::unordered_map<std::string, std::string> params = {
            {"q", "KeetScript compiler"},
            {"lang", "en"},
            {"tag", "c++20 & net"}
        };
        std::string query = keet::net::buildQuery(params);
        // Sorted keys: lang, q, tag
        REQUIRE(query == "lang=en&q=KeetScript%20compiler&tag=c%2B%2B20%20%26%20net");

        auto parsed = keet::net::parseQuery("?" + query);
        REQUIRE(parsed.size() == 3);
        REQUIRE(parsed.at("q") == "KeetScript compiler");
        REQUIRE(parsed.at("lang") == "en");
        REQUIRE(parsed.at("tag") == "c++20 & net");
    }
}

TEST_CASE("Runtime NET - IP Validation, Loopback Detection & DNS Resolution", "[runtime][net][dns]") {
    SECTION("isIpv4, isIpv6, isIp, and isLoopback") {
        REQUIRE(keet::net::isIpv4("127.0.0.1"));
        REQUIRE(keet::net::isIpv4("192.168.1.254"));
        REQUIRE_FALSE(keet::net::isIpv4("999.1.1.1"));
        REQUIRE_FALSE(keet::net::isIpv4("::1"));
        REQUIRE_FALSE(keet::net::isIpv4("example.com"));

        REQUIRE(keet::net::isIpv6("::1"));
        REQUIRE(keet::net::isIpv6("2001:db8::1"));
        REQUIRE(keet::net::isIpv6("[::1]"));
        REQUIRE_FALSE(keet::net::isIpv6("127.0.0.1"));
        REQUIRE_FALSE(keet::net::isIpv6("not:an:ipv6:address:at:all:xyz"));

        REQUIRE(keet::net::isIp("127.0.0.1"));
        REQUIRE(keet::net::isIp("::1"));
        REQUIRE_FALSE(keet::net::isIp("localhost"));

        REQUIRE(keet::net::isLoopback("127.0.0.1"));
        REQUIRE(keet::net::isLoopback("127.0.1.1"));
        REQUIRE(keet::net::isLoopback("::1"));
        REQUIRE(keet::net::isLoopback("[::1]"));
        REQUIRE(keet::net::isLoopback("localhost"));
        REQUIRE_FALSE(keet::net::isLoopback("8.8.8.8"));
        REQUIRE_FALSE(keet::net::isLoopback("2001:db8::1"));
    }

    SECTION("hostname and localhost DNS resolution") {
        std::string host = keet::net::hostname();
        REQUIRE_FALSE(host.empty());

        auto addrs = keet::net::resolveHost("127.0.0.1");
        REQUIRE_FALSE(addrs.empty());
        REQUIRE(addrs[0] == "127.0.0.1");

        std::string ipv4 = keet::net::resolveIpv4("127.0.0.1");
        REQUIRE(ipv4 == "127.0.0.1");
    }
}

TEST_CASE("Runtime NET - Handle-Based TCP Loopback Sockets", "[runtime][net][tcp]") {
    int64_t listener = keet::net::tcpListen("127.0.0.1", 0, 8);
    REQUIRE(listener > 0);

    int64_t port = keet::net::tcpLocalPort(listener);
    REQUIRE(port > 0);
    REQUIRE(port <= 65535);

    std::thread server_thread([listener]() {
        int64_t conn = keet::net::tcpAccept(listener);
        REQUIRE(conn > 0);
        std::string peer = keet::net::tcpPeerAddr(conn);
        REQUIRE(peer.find("127.0.0.1:") == 0);

        std::string line = keet::net::tcpRecvLine(conn, 1024);
        REQUIRE(line == "PING keety");

        int64_t sent = keet::net::tcpSend(conn, "PONG keety\r\nPAYLOAD");
        REQUIRE(sent > 0);
        keet::net::tcpClose(conn);
    });

    int64_t client = keet::net::tcpConnectTimeout("127.0.0.1", port, 3000);
    REQUIRE(client > 0);
    keet::net::tcpSetTimeout(client, 3000);

    int64_t sent = keet::net::tcpSend(client, "PING keety\n");
    REQUIRE(sent == 11);

    std::string reply_line = keet::net::tcpRecvLine(client, 1024);
    REQUIRE(reply_line == "PONG keety");

    std::string rest = keet::net::tcpRecv(client, 1024);
    REQUIRE(rest == "PAYLOAD");

    keet::net::tcpClose(client);
    server_thread.join();
    keet::net::tcpClose(listener);

    // Closing an already-closed handle throws NetError
    require_net_error([listener]() { keet::net::tcpClose(listener); }, "tcpClose");
}

TEST_CASE("Runtime NET - Handle-Based UDP Loopback Datagrams", "[runtime][net][udp]") {
    int64_t server_sock = keet::net::udpBind("127.0.0.1", 0);
    int64_t client_sock = keet::net::udpBind("127.0.0.1", 0);
    REQUIRE(server_sock > 0);
    REQUIRE(client_sock > 0);

    int64_t server_port = keet::net::udpLocalPort(server_sock);
    int64_t client_port = keet::net::udpLocalPort(client_sock);
    REQUIRE(server_port > 0);
    REQUIRE(client_port > 0);

    int64_t bytes = keet::net::udpSendTo(client_sock, "127.0.0.1", server_port, "hello-udp");
    REQUIRE(bytes == 9);

    std::string packet_json = keet::net::udpRecvFrom(server_sock, 1024);
    REQUIRE(keet::json::isValid(packet_json));
    REQUIRE(keet::json::getString(packet_json, "data") == "hello-udp");
    REQUIRE(keet::json::getString(packet_json, "host") == "127.0.0.1");
    REQUIRE(keet::json::getInt(packet_json, "port") == client_port);

    keet::net::udpClose(client_sock);
    keet::net::udpClose(server_sock);
    require_net_error([server_sock]() { keet::net::udpClose(server_sock); }, "udpClose");
}

TEST_CASE("Runtime NET - Native HTTP/1.1 Client over Loopback Server", "[runtime][net][http]") {
    int64_t listener = keet::net::tcpListen("127.0.0.1", 0, 16);
    int64_t port = keet::net::tcpLocalPort(listener);

    // Serve 4 sequential requests: GET, POST JSON, PUT, and chunked response
    std::thread http_server([listener]() {
        for (int req_idx = 0; req_idx < 4; ++req_idx) {
            int64_t conn = keet::net::tcpAccept(listener);
            std::string req_line = keet::net::tcpRecvLine(conn, 4096);
            int64_t content_len = 0;
            while (true) {
                std::string hdr = keet::net::tcpRecvLine(conn, 4096);
                if (hdr.empty()) break;
                if (hdr.rfind("Content-Length:", 0) == 0 || hdr.rfind("content-length:", 0) == 0) {
                    content_len = std::stoll(hdr.substr(15));
                }
            }
            std::string body;
            while (static_cast<int64_t>(body.size()) < content_len) {
                std::string chunk = keet::net::tcpRecv(conn, content_len - static_cast<int64_t>(body.size()));
                if (chunk.empty()) break;
                body += chunk;
            }

            if (req_idx == 0) {
                std::string resp_body = "{\"ok\":true,\"endpoint\":\"get\"}";
                std::string resp =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "X-Custom: KeetServer\r\n"
                    "Content-Length: " + std::to_string(resp_body.size()) + "\r\n"
                    "Connection: close\r\n\r\n" + resp_body;
                keet::net::tcpSend(conn, resp);
            } else if (req_idx == 1) {
                std::string resp_body = "ECHO:" + body;
                std::string resp =
                    "HTTP/1.1 201 Created\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: " + std::to_string(resp_body.size()) + "\r\n"
                    "Connection: close\r\n\r\n" + resp_body;
                keet::net::tcpSend(conn, resp);
            } else if (req_idx == 2) {
                std::string resp =
                    "HTTP/1.1 204 No Content\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n\r\n";
                keet::net::tcpSend(conn, resp);
            } else if (req_idx == 3) {
                std::string resp =
                    "HTTP/1.1 200 OK\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "Connection: close\r\n\r\n"
                    "4\r\nKeet\r\n"
                    "6\r\nScript\r\n"
                    "0\r\n\r\n";
                keet::net::tcpSend(conn, resp);
            }
            keet::net::tcpClose(conn);
        }
    });

    std::string base_url = "http://127.0.0.1:" + std::to_string(port);

    // 1. net.get
    std::string get_body = keet::net::get(base_url + "/status");
    REQUIRE(keet::json::getBool(get_body, "ok") == true);
    REQUIRE(keet::json::getString(get_body, "endpoint") == "get");

    // 2. net.request with POST & headers
    std::unordered_map<std::string, std::string> hdrs = {{"Content-Type", "application/json"}};
    std::string req_resp = keet::net::request("POST", base_url + "/items", hdrs, "{\"id\":7}", 3000);
    REQUIRE(keet::json::isValid(req_resp));
    REQUIRE(keet::json::getInt(req_resp, "status") == 201);
    REQUIRE(keet::json::getBool(req_resp, "ok") == true);
    REQUIRE(keet::json::getString(req_resp, "body") == "ECHO:{\"id\":7}");

    // 3. net.getStatus
    int64_t status = keet::net::getStatus(base_url + "/no-content");
    REQUIRE(status == 204);

    // 4. Chunked transfer-encoding download to file
    auto tmp_path = (std::filesystem::temp_directory_path() / "keety_net_download_test.txt").string();
    REQUIRE(keet::net::download(base_url + "/chunked", tmp_path));
    REQUIRE(keet::fs::readText(tmp_path) == "KeetScript");
    keet::fs::remove(tmp_path);

    http_server.join();
    keet::net::tcpClose(listener);
}
