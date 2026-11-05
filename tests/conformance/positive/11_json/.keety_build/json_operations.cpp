// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "json_operations.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_json_operations {
#line 7 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
    int64_t _keet_main() {
#line 8 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        ::keet::io::println("Running KeetScript json conformance tests...");
#line 11 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::string sample = "{\"server\":{\"host\":\"127.0.0.1\",\"port\":8080,\"tls\":true,\"ratio\":0.75,\"cert\":null},\"users\":[{\"name\":\"Alice\",\"score\":100},{\"name\":\"Bob\",\"score\":85}]}";
#line 12 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (!::keet::json::isValid(sample)) {
#line 13 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 1;
        }
#line 15 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::isValid("{broken_json")) {
#line 16 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 2;
        }
#line 18 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::typeOf(sample) != "object") {
#line 19 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 3;
        }
#line 21 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (!::keet::json::isObject(sample)) {
#line 22 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 4;
        }
#line 24 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::isArray(sample)) {
#line 25 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 5;
        }
#line 27 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::size(sample) != 2) {
#line 28 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 6;
        }
#line 30 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::vector<std::string> top_keys = ::keet::json::keys(sample);
#line 31 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (top_keys[0] != "server" || top_keys[1] != "users") {
#line 32 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 7;
        }
#line 36 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (!::keet::json::hasKey(sample, "server.host") || ::keet::json::hasKey(sample, "server.missing")) {
#line 37 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 8;
        }
#line 39 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (!::keet::json::isNull(sample, "server.cert") || ::keet::json::isNull(sample, "server.host")) {
#line 40 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 9;
        }
#line 42 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::getString(sample, "server.host") != "127.0.0.1") {
#line 43 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 10;
        }
#line 45 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::getInt(sample, "server.port") != 8080) {
#line 46 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 11;
        }
#line 48 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (!::keet::json::getBool(sample, "server.tls")) {
#line 49 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 12;
        }
#line 51 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        double ratio = ::keet::json::getFloat(sample, "server.ratio");
#line 52 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (ratio < 0.74 || ratio > 0.76) {
#line 53 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 13;
        }
#line 55 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::getString(sample, "users.0.name") != "Alice") {
#line 56 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 14;
        }
#line 58 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::getInt(sample, "users.1.score") != 85) {
#line 59 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 15;
        }
#line 61 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::getStringOr(sample, "server.missing", "fallback") != "fallback") {
#line 62 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 16;
        }
#line 64 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::getIntOr(sample, "server.missing", 9000) != 9000) {
#line 65 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 17;
        }
#line 67 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::getBoolOr(sample, "server.missing", false) != false) {
#line 68 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 18;
        }
#line 70 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        double fallback_f = ::keet::json::getFloatOr(sample, "server.missing", 2.5);
#line 71 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (fallback_f < 2.49 || fallback_f > 2.51) {
#line 72 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 19;
        }
#line 76 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        int64_t bad_port = ([&]() -> std::decay_t<decltype(::keet::json::getInt(sample, "server.nonexistent"))> { try { return ::keet::json::getInt(sample, "server.nonexistent"); } catch (...) { return -1; } })();
#line 77 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (bad_port != -1) {
#line 78 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 20;
        }
#line 80 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        int64_t bad_parse = ([&]() -> std::decay_t<decltype(::keet::json::size("{not_valid"))> { try { return ::keet::json::size("{not_valid"); } catch (...) { return -2; } })();
#line 81 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (bad_parse != -2) {
#line 82 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 21;
        }
#line 86 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::string built = ::keet::json::emptyObject();
#line 87 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        built = ::keet::json::setString(built, "app.name", "keety");
#line 88 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        built = ::keet::json::setInt(built, "app.version", 1);
#line 89 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        built = ::keet::json::setFloat(built, "app.load", 0.5);
#line 90 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        built = ::keet::json::setBool(built, "app.production", true);
#line 91 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        built = ::keet::json::setNull(built, "app.legacy");
#line 92 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        built = ::keet::json::setRaw(built, "app.tags", "[\"compiler\",\"cpp20\"]");
#line 94 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::getString(built, "app.name") != "keety") {
#line 95 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 22;
        }
#line 97 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::getInt(built, "app.version") != 1) {
#line 98 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 23;
        }
#line 100 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::getString(built, "app.tags.1") != "cpp20") {
#line 101 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 24;
        }
#line 103 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        built = ::keet::json::removeKey(built, "app.legacy");
#line 104 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::hasKey(built, "app.legacy")) {
#line 105 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 25;
        }
#line 109 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::vector<int64_t> nums = ::keet::json::parseIntArray("[10, 20, 30]");
#line 110 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (nums[0] + nums[1] + nums[2] != 60) {
#line 111 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 26;
        }
#line 113 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::vector<bool> flags = ::keet::json::parseBoolArray("[true, false, true]");
#line 114 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (!flags[0] || flags[1] || !flags[2]) {
#line 115 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 27;
        }
#line 117 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::vector<double> floats = ::keet::json::parseFloatArray("[1.5, 2.5]");
#line 118 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (floats[0] + floats[1] < 3.99 || floats[0] + floats[1] > 4.01) {
#line 119 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 28;
        }
#line 121 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::unordered_map<std::string, std::string> str_map = ::keet::json::parseMap("{\"k1\":\"v1\",\"k2\":\"v2\"}");
#line 122 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (str_map["k1"] != "v1" || str_map["k2"] != "v2") {
#line 123 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 29;
        }
#line 127 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::stringify(42) != "42") {
#line 128 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 30;
        }
#line 130 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::stringify(true) != "true") {
#line 131 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 31;
        }
#line 133 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::stringify("hi") != "\"hi\"") {
#line 134 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 32;
        }
#line 136 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::stringify(nums) != "[10,20,30]") {
#line 137 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 33;
        }
#line 139 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::variant<std::string, std::monostate> maybe_text = std::monostate{};
#line 140 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::stringify(maybe_text) != "null") {
#line 141 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 34;
        }
#line 145 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::string pretty = ::keet::json::format(built, 2);
#line 146 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::string minified = ::keet::json::minify(pretty);
#line 147 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (minified != built) {
#line 148 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 35;
        }
#line 150 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::string escaped = ::keet::json::escape("a\"b");
#line 151 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (::keet::json::unescape(escaped) != "a\"b") {
#line 152 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 36;
        }
#line 155 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::string tmp_file = ::keet::fs::join(::keet::fs::cwd(), "temp_json_conformance_test.json");
#line 156 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        bool wrote = ::keet::json::writeFile(tmp_file, built, 2);
#line 157 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (!wrote) {
#line 158 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 37;
        }
#line 160 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        std::string loaded = ::keet::json::readFile(tmp_file);
#line 161 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        ::keet::fs::remove(tmp_file);
#line 162 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        if (loaded != built) {
#line 163 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
            return 38;
        }
#line 166 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        ::keet::io::println("All KeetScript json conformance tests passed successfully!");
#line 167 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/11_json/json_operations.keet"
        return 0;
    }
} // namespace keet_module_json_operations

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_json_operations::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
