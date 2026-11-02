// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "net_operations.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_net_operations {
    int64_t _keet_main() {
        ::keet::io::println("Running KeetScript net conformance tests...");
        std::string raw_text = "keety net & json!";
        std::string encoded = ::keet::net::urlEncode(raw_text);
        if (encoded != "keety%20net%20%26%20json%21") {
            return 1;
        }
        if (::keet::net::urlDecode(encoded) != raw_text) {
            return 2;
        }
        std::string url = "https://api.keetscript.dev:8443/v2/search?q=compiler&sort=asc#top";
        if (::keet::net::urlScheme(url) != "https") {
            return 3;
        }
        if (::keet::net::urlHost(url) != "api.keetscript.dev") {
            return 4;
        }
        if (::keet::net::urlPort(url) != 8443) {
            return 5;
        }
        if (::keet::net::urlPath(url) != "/v2/search") {
            return 6;
        }
        if (::keet::net::urlQuery(url) != "q=compiler&sort=asc") {
            return 7;
        }
        std::unordered_map<std::string, std::string> parts = ::keet::net::parseUrl(url);
        if (parts["fragment"] != "top" || parts["port"] != "8443") {
            return 8;
        }
        std::unordered_map<std::string, std::string> q_params = ::keet::net::parseQuery("?page=1&tag=c%2B%2B20");
        if (q_params["page"] != "1" || q_params["tag"] != "c++20") {
            return 9;
        }
        std::string rebuilt = ::keet::net::buildQuery(q_params);
        if (rebuilt != "page=1&tag=c%2B%2B20") {
            return 10;
        }
        if (!::keet::net::isIpv4("127.0.0.1") || ::keet::net::isIpv4("999.0.0.1")) {
            return 11;
        }
        if (!::keet::net::isIpv6("::1") || ::keet::net::isIpv6("127.0.0.1")) {
            return 12;
        }
        if (!::keet::net::isIp("127.0.0.1") || !::keet::net::isIp("::1") || ::keet::net::isIp("not-an-ip")) {
            return 13;
        }
        if (!::keet::net::isLoopback("127.0.0.1") || !::keet::net::isLoopback("::1") || !::keet::net::isLoopback("localhost") || ::keet::net::isLoopback("8.8.8.8")) {
            return 14;
        }
        std::string hname = ::keet::net::hostname();
        if (hname == "") {
            return 15;
        }
        std::string resolved_ip = ::keet::net::resolveIpv4("127.0.0.1");
        if (resolved_ip != "127.0.0.1") {
            return 16;
        }
        std::vector<std::string> resolved_all = ::keet::net::resolveHost("127.0.0.1");
        if (resolved_all[0] != "127.0.0.1") {
            return 17;
        }
        int64_t bad_port = ([&]() -> std::decay_t<decltype(::keet::net::urlPort("not_a_valid_url"))> { try { return ::keet::net::urlPort("not_a_valid_url"); } catch (...) { return -1; } })();
        if (bad_port != -1) {
            return 18;
        }
        std::string bad_decode = ([&]() -> std::decay_t<decltype(::keet::net::urlDecode("%GG"))> { try { return ::keet::net::urlDecode("%GG"); } catch (...) { return "fallback"; } })();
        if (bad_decode != "fallback") {
            return 19;
        }
        int64_t listener = ::keet::net::tcpListen("127.0.0.1", 0, 8);
        int64_t tcp_port = ::keet::net::tcpLocalPort(listener);
        if (tcp_port <= 0) {
            return 20;
        }
        int64_t client = ::keet::net::tcpConnectTimeout("127.0.0.1", tcp_port, 3000);
        int64_t server_conn = ::keet::net::tcpAccept(listener);
        ::keet::net::tcpSetTimeout(client, 3000);
        ::keet::net::tcpSetTimeout(server_conn, 3000);
        int64_t sent_bytes = ::keet::net::tcpSend(client, "HELLO_TCP\r\nWORLD");
        if (sent_bytes != 16) {
            return 22;
        }
        std::string line = ::keet::net::tcpRecvLine(server_conn, 256);
        if (line != "HELLO_TCP") {
            return 23;
        }
        std::string rem = ::keet::net::tcpRecv(server_conn, 256);
        if (rem != "WORLD") {
            return 24;
        }
        std::string peer = ::keet::net::tcpPeerAddr(server_conn);
        if (peer == "") {
            return 25;
        }
        ::keet::net::tcpClose(client);
        ::keet::net::tcpClose(server_conn);
        ::keet::net::tcpClose(listener);
        int64_t u_recv = ::keet::net::udpBind("127.0.0.1", 0);
        int64_t u_send = ::keet::net::udpBind("127.0.0.1", 0);
        int64_t u_port = ::keet::net::udpLocalPort(u_recv);
        int64_t u_send_port = ::keet::net::udpLocalPort(u_send);
        if (u_port <= 0 || u_send_port <= 0) {
            return 28;
        }
        int64_t u_bytes = ::keet::net::udpSendTo(u_send, "127.0.0.1", u_port, "KEET_UDP_DATAGRAM");
        if (u_bytes != 17) {
            return 29;
        }
        std::string pkt_json = ::keet::net::udpRecvFrom(u_recv, 512);
        if (::keet::json::getString(pkt_json, "data") != "KEET_UDP_DATAGRAM") {
            return 30;
        }
        if (::keet::json::getString(pkt_json, "host") != "127.0.0.1" || ::keet::json::getInt(pkt_json, "port") != u_send_port) {
            return 31;
        }
        ::keet::net::udpClose(u_send);
        ::keet::net::udpClose(u_recv);
        ::keet::io::println("All KeetScript net conformance tests passed successfully!");
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
