// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "net_operations.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_net_operations {
#line 7 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
    int64_t _keet_main() {
#line 8 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        ::keet::io::println("Running KeetScript net conformance tests...");
#line 11 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::string raw_text = "keety net & json!";
#line 12 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::string encoded = ::keet::net::urlEncode(raw_text);
#line 13 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (encoded != "keety%20net%20%26%20json%21") {
#line 14 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 1;
        }
#line 16 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (::keet::net::urlDecode(encoded) != raw_text) {
#line 17 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 2;
        }
#line 21 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::string url = "https://api.keetscript.dev:8443/v2/search?q=compiler&sort=asc#top";
#line 22 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (::keet::net::urlScheme(url) != "https") {
#line 23 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 3;
        }
#line 25 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (::keet::net::urlHost(url) != "api.keetscript.dev") {
#line 26 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 4;
        }
#line 28 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (::keet::net::urlPort(url) != 8443) {
#line 29 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 5;
        }
#line 31 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (::keet::net::urlPath(url) != "/v2/search") {
#line 32 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 6;
        }
#line 34 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (::keet::net::urlQuery(url) != "q=compiler&sort=asc") {
#line 35 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 7;
        }
#line 37 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::unordered_map<std::string, std::string> parts = ::keet::net::parseUrl(url);
#line 38 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (parts["fragment"] != "top" || parts["port"] != "8443") {
#line 39 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 8;
        }
#line 43 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::unordered_map<std::string, std::string> q_params = ::keet::net::parseQuery("?page=1&tag=c%2B%2B20");
#line 44 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (q_params["page"] != "1" || q_params["tag"] != "c++20") {
#line 45 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 9;
        }
#line 47 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::string rebuilt = ::keet::net::buildQuery(q_params);
#line 48 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (rebuilt != "page=1&tag=c%2B%2B20") {
#line 49 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 10;
        }
#line 53 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (!::keet::net::isIpv4("127.0.0.1") || ::keet::net::isIpv4("999.0.0.1")) {
#line 54 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 11;
        }
#line 56 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (!::keet::net::isIpv6("::1") || ::keet::net::isIpv6("127.0.0.1")) {
#line 57 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 12;
        }
#line 59 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (!::keet::net::isIp("127.0.0.1") || !::keet::net::isIp("::1") || ::keet::net::isIp("not-an-ip")) {
#line 60 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 13;
        }
#line 62 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (!::keet::net::isLoopback("127.0.0.1") || !::keet::net::isLoopback("::1") || !::keet::net::isLoopback("localhost") || ::keet::net::isLoopback("8.8.8.8")) {
#line 63 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 14;
        }
#line 65 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::string hname = ::keet::net::hostname();
#line 66 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (hname == "") {
#line 67 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 15;
        }
#line 69 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::string resolved_ip = ::keet::net::resolveIpv4("127.0.0.1");
#line 70 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (resolved_ip != "127.0.0.1") {
#line 71 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 16;
        }
#line 73 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::vector<std::string> resolved_all = ::keet::net::resolveHost("127.0.0.1");
#line 74 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (resolved_all[0] != "127.0.0.1") {
#line 75 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 17;
        }
#line 79 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        int64_t bad_port = ([&]() -> std::decay_t<decltype(::keet::net::urlPort("not_a_valid_url"))> { try { return ::keet::net::urlPort("not_a_valid_url"); } catch (...) { return -1; } })();
#line 80 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (bad_port != -1) {
#line 81 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 18;
        }
#line 83 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::string bad_decode = ([&]() -> std::decay_t<decltype(::keet::net::urlDecode("%GG"))> { try { return ::keet::net::urlDecode("%GG"); } catch (...) { return "fallback"; } })();
#line 84 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (bad_decode != "fallback") {
#line 85 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 19;
        }
#line 89 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        int64_t listener = ::keet::net::tcpListen("127.0.0.1", 0, 8);
#line 90 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        int64_t tcp_port = ::keet::net::tcpLocalPort(listener);
#line 91 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (tcp_port <= 0) {
#line 92 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 20;
        }
#line 94 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        int64_t client = ::keet::net::tcpConnectTimeout("127.0.0.1", tcp_port, 3000);
#line 95 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        int64_t server_conn = ::keet::net::tcpAccept(listener);
#line 96 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        ::keet::net::tcpSetTimeout(client, 3000);
#line 97 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        ::keet::net::tcpSetTimeout(server_conn, 3000);
#line 99 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        int64_t sent_bytes = ::keet::net::tcpSend(client, "HELLO_TCP\r\nWORLD");
#line 100 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (sent_bytes != 16) {
#line 101 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 22;
        }
#line 103 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::string line = ::keet::net::tcpRecvLine(server_conn, 256);
#line 104 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (line != "HELLO_TCP") {
#line 105 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 23;
        }
#line 107 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::string rem = ::keet::net::tcpRecv(server_conn, 256);
#line 108 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (rem != "WORLD") {
#line 109 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 24;
        }
#line 111 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::string peer = ::keet::net::tcpPeerAddr(server_conn);
#line 112 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (peer == "") {
#line 113 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 25;
        }
#line 116 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        ::keet::net::tcpClose(client);
#line 117 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        ::keet::net::tcpClose(server_conn);
#line 118 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        ::keet::net::tcpClose(listener);
#line 121 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        int64_t u_recv = ::keet::net::udpBind("127.0.0.1", 0);
#line 122 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        int64_t u_send = ::keet::net::udpBind("127.0.0.1", 0);
#line 123 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        int64_t u_port = ::keet::net::udpLocalPort(u_recv);
#line 124 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        int64_t u_send_port = ::keet::net::udpLocalPort(u_send);
#line 125 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (u_port <= 0 || u_send_port <= 0) {
#line 126 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 28;
        }
#line 128 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        int64_t u_bytes = ::keet::net::udpSendTo(u_send, "127.0.0.1", u_port, "KEET_UDP_DATAGRAM");
#line 129 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (u_bytes != 17) {
#line 130 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 29;
        }
#line 132 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        std::string pkt_json = ::keet::net::udpRecvFrom(u_recv, 512);
#line 133 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (::keet::json::getString(pkt_json, "data") != "KEET_UDP_DATAGRAM") {
#line 134 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 30;
        }
#line 136 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        if (::keet::json::getString(pkt_json, "host") != "127.0.0.1" || ::keet::json::getInt(pkt_json, "port") != u_send_port) {
#line 137 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
            return 31;
        }
#line 139 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        ::keet::net::udpClose(u_send);
#line 140 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        ::keet::net::udpClose(u_recv);
#line 142 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        ::keet::io::println("All KeetScript net conformance tests passed successfully!");
#line 143 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/12_net/net_operations.keet"
        return 0;
    }
} // namespace keet_module_net_operations

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_net_operations::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
