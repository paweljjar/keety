// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "json_operations.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_json_operations {
    int64_t _keet_main() {
        ::keet::io::println("Running KeetScript json conformance tests...");
        std::string sample = "{\"server\":{\"host\":\"127.0.0.1\",\"port\":8080,\"tls\":true,\"ratio\":0.75,\"cert\":null},\"users\":[{\"name\":\"Alice\",\"score\":100},{\"name\":\"Bob\",\"score\":85}]}";
        if (!::keet::json::isValid(sample)) {
            return 1;
        }
        if (::keet::json::isValid("{broken_json")) {
            return 2;
        }
        if (::keet::json::typeOf(sample) != "object") {
            return 3;
        }
        if (!::keet::json::isObject(sample)) {
            return 4;
        }
        if (::keet::json::isArray(sample)) {
            return 5;
        }
        if (::keet::json::size(sample) != 2) {
            return 6;
        }
        std::vector<std::string> top_keys = ::keet::json::keys(sample);
        if (top_keys[0] != "server" || top_keys[1] != "users") {
            return 7;
        }
        if (!::keet::json::hasKey(sample, "server.host") || ::keet::json::hasKey(sample, "server.missing")) {
            return 8;
        }
        if (!::keet::json::isNull(sample, "server.cert") || ::keet::json::isNull(sample, "server.host")) {
            return 9;
        }
        if (::keet::json::getString(sample, "server.host") != "127.0.0.1") {
            return 10;
        }
        if (::keet::json::getInt(sample, "server.port") != 8080) {
            return 11;
        }
        if (!::keet::json::getBool(sample, "server.tls")) {
            return 12;
        }
        double ratio = ::keet::json::getFloat(sample, "server.ratio");
        if (ratio < 0.74 || ratio > 0.76) {
            return 13;
        }
        if (::keet::json::getString(sample, "users.0.name") != "Alice") {
            return 14;
        }
        if (::keet::json::getInt(sample, "users.1.score") != 85) {
            return 15;
        }
        if (::keet::json::getStringOr(sample, "server.missing", "fallback") != "fallback") {
            return 16;
        }
        if (::keet::json::getIntOr(sample, "server.missing", 9000) != 9000) {
            return 17;
        }
        if (::keet::json::getBoolOr(sample, "server.missing", false) != false) {
            return 18;
        }
        double fallback_f = ::keet::json::getFloatOr(sample, "server.missing", 2.5);
        if (fallback_f < 2.49 || fallback_f > 2.51) {
            return 19;
        }
        int64_t bad_port = ([&]() -> std::decay_t<decltype(::keet::json::getInt(sample, "server.nonexistent"))> { try { return ::keet::json::getInt(sample, "server.nonexistent"); } catch (...) { return -1; } })();
        if (bad_port != -1) {
            return 20;
        }
        int64_t bad_parse = ([&]() -> std::decay_t<decltype(::keet::json::size("{not_valid"))> { try { return ::keet::json::size("{not_valid"); } catch (...) { return -2; } })();
        if (bad_parse != -2) {
            return 21;
        }
        std::string built = ::keet::json::emptyObject();
        built = ::keet::json::setString(built, "app.name", "keety");
        built = ::keet::json::setInt(built, "app.version", 1);
        built = ::keet::json::setFloat(built, "app.load", 0.5);
        built = ::keet::json::setBool(built, "app.production", true);
        built = ::keet::json::setNull(built, "app.legacy");
        built = ::keet::json::setRaw(built, "app.tags", "[\"compiler\",\"cpp20\"]");
        if (::keet::json::getString(built, "app.name") != "keety") {
            return 22;
        }
        if (::keet::json::getInt(built, "app.version") != 1) {
            return 23;
        }
        if (::keet::json::getString(built, "app.tags.1") != "cpp20") {
            return 24;
        }
        built = ::keet::json::removeKey(built, "app.legacy");
        if (::keet::json::hasKey(built, "app.legacy")) {
            return 25;
        }
        std::vector<int64_t> nums = ::keet::json::parseIntArray("[10, 20, 30]");
        if (nums[0] + nums[1] + nums[2] != 60) {
            return 26;
        }
        std::vector<bool> flags = ::keet::json::parseBoolArray("[true, false, true]");
        if (!flags[0] || flags[1] || !flags[2]) {
            return 27;
        }
        std::vector<double> floats = ::keet::json::parseFloatArray("[1.5, 2.5]");
        if (floats[0] + floats[1] < 3.99 || floats[0] + floats[1] > 4.01) {
            return 28;
        }
        std::unordered_map<std::string, std::string> str_map = ::keet::json::parseMap("{\"k1\":\"v1\",\"k2\":\"v2\"}");
        if (str_map["k1"] != "v1" || str_map["k2"] != "v2") {
            return 29;
        }
        if (::keet::json::stringify(42) != "42") {
            return 30;
        }
        if (::keet::json::stringify(true) != "true") {
            return 31;
        }
        if (::keet::json::stringify("hi") != "\"hi\"") {
            return 32;
        }
        if (::keet::json::stringify(nums) != "[10,20,30]") {
            return 33;
        }
        std::variant<std::string, std::monostate> maybe_text = std::monostate{};
        if (::keet::json::stringify(maybe_text) != "null") {
            return 34;
        }
        std::string pretty = ::keet::json::format(built, 2);
        std::string minified = ::keet::json::minify(pretty);
        if (minified != built) {
            return 35;
        }
        std::string escaped = ::keet::json::escape("a\"b");
        if (::keet::json::unescape(escaped) != "a\"b") {
            return 36;
        }
        std::string tmp_file = ::keet::fs::join(::keet::fs::cwd(), "temp_json_conformance_test.json");
        bool wrote = ::keet::json::writeFile(tmp_file, built, 2);
        if (!wrote) {
            return 37;
        }
        std::string loaded = ::keet::json::readFile(tmp_file);
        ::keet::fs::remove(tmp_file);
        if (loaded != built) {
            return 38;
        }
        ::keet::io::println("All KeetScript json conformance tests passed successfully!");
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
