#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <keet/json.hpp>

#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace {

void require_json_error(const std::function<void()>& fn, const std::string& expected_substr = "") {
    bool caught = false;
    try {
        fn();
    } catch (const keet::error& e) {
        caught = true;
        REQUIRE(e.name == "JsonError");
        if (!expected_substr.empty()) {
            REQUIRE(e.message.find(expected_substr) != std::string::npos);
        }
    }
    REQUIRE(caught);
}

} // namespace

TEST_CASE("Runtime JSON - RFC 8259 Validation & Type Inspection", "[runtime][json][validation]") {
    SECTION("isValid accepts valid RFC 8259 documents") {
        REQUIRE(keet::json::isValid("{}"));
        REQUIRE(keet::json::isValid("[]"));
        REQUIRE(keet::json::isValid("{\"name\":\"KeetScript\",\"version\":1,\"active\":true,\"meta\":null}"));
        REQUIRE(keet::json::isValid("[1, -42, 3.14, 1.5e-3, true, false, null, \"hello\"]"));
        REQUIRE(keet::json::isValid("  \n\t \"standalone string\" \r\n "));
        REQUIRE(keet::json::isValid("42"));
        REQUIRE(keet::json::isValid("-0.5e+2"));
        REQUIRE(keet::json::isValid("true"));
        REQUIRE(keet::json::isValid("false"));
        REQUIRE(keet::json::isValid("null"));
    }

    SECTION("isValid rejects malformed JSON") {
        REQUIRE_FALSE(keet::json::isValid(""));
        REQUIRE_FALSE(keet::json::isValid("   "));
        REQUIRE_FALSE(keet::json::isValid("{"));
        REQUIRE_FALSE(keet::json::isValid("}"));
        REQUIRE_FALSE(keet::json::isValid("{\"a\":1,}")); // trailing comma in object
        REQUIRE_FALSE(keet::json::isValid("[1, 2,]"));    // trailing comma in array
        REQUIRE_FALSE(keet::json::isValid("{a: 1}"));     // unquoted key
        REQUIRE_FALSE(keet::json::isValid("'single'"));   // single-quoted string
        REQUIRE_FALSE(keet::json::isValid("01"));         // leading zero
        REQUIRE_FALSE(keet::json::isValid("-01"));        // negative leading zero
        REQUIRE_FALSE(keet::json::isValid("1."));         // trailing decimal point
        REQUIRE_FALSE(keet::json::isValid(".5"));         // leading decimal point
        REQUIRE_FALSE(keet::json::isValid("1e"));         // incomplete exponent
        REQUIRE_FALSE(keet::json::isValid("1e+"));        // incomplete exponent sign
        REQUIRE_FALSE(keet::json::isValid("True"));       // wrong case
        REQUIRE_FALSE(keet::json::isValid("NaN"));        // non-standard number
        REQUIRE_FALSE(keet::json::isValid("Infinity"));
        REQUIRE_FALSE(keet::json::isValid("{} []"));      // trailing tokens
        REQUIRE_FALSE(keet::json::isValid("\"unterminated"));
        REQUIRE_FALSE(keet::json::isValid("\"raw\nnewline\"")); // unescaped control char < 0x20
        REQUIRE_FALSE(keet::json::isValid("\"bad\\xescape\""));
    }

    SECTION("typeOf classifies all JSON value categories and invalid inputs") {
        REQUIRE(keet::json::typeOf("{\"a\":1}") == "object");
        REQUIRE(keet::json::typeOf("[1,2,3]") == "array");
        REQUIRE(keet::json::typeOf("\"hello\"") == "string");
        REQUIRE(keet::json::typeOf("42") == "number");
        REQUIRE(keet::json::typeOf("-3.14") == "number");
        REQUIRE(keet::json::typeOf("true") == "bool");
        REQUIRE(keet::json::typeOf("false") == "bool");
        REQUIRE(keet::json::typeOf("null") == "null");
        REQUIRE(keet::json::typeOf("{bad}") == "invalid");
        REQUIRE(keet::json::typeOf("") == "invalid");
    }

    SECTION("isObject and isArray") {
        REQUIRE(keet::json::isObject("{}"));
        REQUIRE(keet::json::isObject("{\"k\":\"v\"}"));
        REQUIRE_FALSE(keet::json::isObject("[]"));
        REQUIRE_FALSE(keet::json::isObject("\"str\""));
        REQUIRE_FALSE(keet::json::isObject("invalid"));

        REQUIRE(keet::json::isArray("[]"));
        REQUIRE(keet::json::isArray("[1, 2]"));
        REQUIRE_FALSE(keet::json::isArray("{}"));
        REQUIRE_FALSE(keet::json::isArray("42"));
        REQUIRE_FALSE(keet::json::isArray("invalid"));
    }

    SECTION("size and keys") {
        REQUIRE(keet::json::size("{}") == 0);
        REQUIRE(keet::json::size("[]") == 0);
        REQUIRE(keet::json::size("{\"a\":1,\"b\":2,\"c\":3}") == 3);
        REQUIRE(keet::json::size("[10, 20, 30, 40]") == 4);

        auto k = keet::json::keys("{\"zebra\":1,\"alpha\":2,\"middle\":3}");
        REQUIRE(k == std::vector<std::string>{"alpha", "middle", "zebra"});

        require_json_error([]() { (void)keet::json::size("42"); }, "size:");
        require_json_error([]() { (void)keet::json::keys("[1, 2]"); }, "keys:");
    }
}

TEST_CASE("Runtime JSON - Unicode Escapes & Surrogate Pairs", "[runtime][json][unicode]") {
    SECTION("Basic BMP \\uXXXX escapes and standard escapes") {
        std::string doc = R"({"msg":"Line1\nLine2\t\"Quote\"\\Slash\/\b\f\r","euro":"\u20AC","ascii":"\u0041"})";
        REQUIRE(keet::json::getString(doc, "msg") == "Line1\nLine2\t\"Quote\"\\Slash/\b\f\r");
        REQUIRE(keet::json::getString(doc, "euro") == "\xE2\x82\xAC"); // U+20AC in UTF-8
        REQUIRE(keet::json::getString(doc, "ascii") == "A");
    }

    SECTION("UTF-16 surrogate pair decoding (U+1F600 Grinning Face & U+1D11E G Clef)") {
        std::string doc = R"({"emoji":"\uD83D\uDE00","clef":"\uD834\uDD1E"})";
        REQUIRE(keet::json::getString(doc, "emoji") == "\xF0\x9F\x98\x80");
        REQUIRE(keet::json::getString(doc, "clef") == "\xF0\x9D\x84\x9E");
    }

    SECTION("Invalid surrogate pairs throw JsonError") {
        // High surrogate without low surrogate
        require_json_error([]() { (void)keet::json::getString(R"({"bad":"\uD83Dabc"})", "bad"); });
        // High surrogate followed by non-low surrogate \u0041
        require_json_error([]() { (void)keet::json::getString(R"({"bad":"\uD83D\u0041"})", "bad"); });
        // Unpaired low surrogate
        require_json_error([]() { (void)keet::json::getString(R"({"bad":"\uDE00"})", "bad"); });
        // Incomplete \u escape
        require_json_error([]() { (void)keet::json::getString(R"({"bad":"\u12"})", "bad"); });
    }
}

TEST_CASE("Runtime JSON - Dot-Path Queries & Fallback Extractors", "[runtime][json][dotpath]") {
    const std::string doc = R"({
        "server": {
            "host": "127.0.0.1",
            "port": 8080,
            "tls": true,
            "timeout": 2.5,
            "cert": null
        },
        "users": [
            {"name": "Alice", "score": 100, "tags": ["admin", "core"]},
            {"name": "Bob", "score": 75, "tags": ["user"]}
        ]
    })";

    SECTION("hasKey and isNull") {
        REQUIRE(keet::json::hasKey(doc, "server"));
        REQUIRE(keet::json::hasKey(doc, "server.host"));
        REQUIRE(keet::json::hasKey(doc, "server.cert"));
        REQUIRE(keet::json::hasKey(doc, "users.0.name"));
        REQUIRE(keet::json::hasKey(doc, "users.1.tags.0"));
        REQUIRE_FALSE(keet::json::hasKey(doc, "server.missing"));
        REQUIRE_FALSE(keet::json::hasKey(doc, "users.5.name"));
        REQUIRE_FALSE(keet::json::hasKey(doc, "users.-1"));
        REQUIRE_FALSE(keet::json::hasKey("{invalid", "server"));

        REQUIRE(keet::json::isNull(doc, "server.cert"));
        REQUIRE_FALSE(keet::json::isNull(doc, "server.host"));
        REQUIRE_FALSE(keet::json::isNull(doc, "server.missing"));
    }

    SECTION("Typed dot-path getters") {
        REQUIRE(keet::json::getString(doc, "server.host") == "127.0.0.1");
        REQUIRE(keet::json::getInt(doc, "server.port") == 8080);
        REQUIRE(keet::json::getBool(doc, "server.tls") == true);
        REQUIRE_THAT(keet::json::getFloat(doc, "server.timeout"), Catch::Matchers::WithinRel(2.5));
        // getFloat also accepts integer JSON numbers
        REQUIRE_THAT(keet::json::getFloat(doc, "server.port"), Catch::Matchers::WithinRel(8080.0));

        REQUIRE(keet::json::getString(doc, "users.0.name") == "Alice");
        REQUIRE(keet::json::getInt(doc, "users.1.score") == 75);
        REQUIRE(keet::json::getString(doc, "users.0.tags.1") == "core");

        auto tags = keet::json::getArray(doc, "users.0.tags");
        REQUIRE(tags == std::vector<std::string>{"admin", "core"});

        std::string srv = keet::json::getObject(doc, "server");
        REQUIRE(keet::json::getString(srv, "host") == "127.0.0.1");
        REQUIRE(keet::json::getInt(srv, "port") == 8080);
    }

    SECTION("Typed getters throw JsonError on missing path or type mismatch") {
        require_json_error([&]() { (void)keet::json::getString(doc, "server.port"); }, "expected string");
        require_json_error([&]() { (void)keet::json::getInt(doc, "server.timeout"); }, "not an exact integer");
        require_json_error([&]() { (void)keet::json::getBool(doc, "server.host"); }, "expected bool");
        require_json_error([&]() { (void)keet::json::getFloat(doc, "server.host"); }, "expected number");
        require_json_error([&]() { (void)keet::json::getArray(doc, "server.host"); }, "expected array");
        require_json_error([&]() { (void)keet::json::getObject(doc, "users"); }, "expected object");
        require_json_error([&]() { (void)keet::json::getString(doc, "server.nonexistent"); }, "not found in JSON document");
        require_json_error([&]() { (void)keet::json::getString(doc, "users.99.name"); }, "not found in JSON document");
    }

    SECTION("Non-throwing *Or fallback extractors") {
        REQUIRE(keet::json::getStringOr(doc, "server.host", "0.0.0.0") == "127.0.0.1");
        REQUIRE(keet::json::getStringOr(doc, "server.missing", "0.0.0.0") == "0.0.0.0");
        REQUIRE(keet::json::getStringOr(doc, "server.port", "fallback") == "fallback");
        REQUIRE(keet::json::getStringOr("{broken", "server.host", "fallback") == "fallback");

        REQUIRE(keet::json::getIntOr(doc, "server.port", 80) == 8080);
        REQUIRE(keet::json::getIntOr(doc, "server.missing", 80) == 80);
        REQUIRE(keet::json::getIntOr(doc, "server.timeout", 80) == 80);

        REQUIRE_THAT(keet::json::getFloatOr(doc, "server.timeout", 1.0), Catch::Matchers::WithinRel(2.5));
        REQUIRE_THAT(keet::json::getFloatOr(doc, "server.missing", 1.0), Catch::Matchers::WithinRel(1.0));

        REQUIRE(keet::json::getBoolOr(doc, "server.tls", false) == true);
        REQUIRE(keet::json::getBoolOr(doc, "server.missing", false) == false);
        REQUIRE(keet::json::getBoolOr(doc, "server.port", true) == true);
    }
}

TEST_CASE("Runtime JSON - Typed Collection Parsers", "[runtime][json][collections]") {
    SECTION("parseMap") {
        auto m = keet::json::parseMap(R"({"name":"keet","count":3,"flag":true})");
        REQUIRE(m.size() == 3);
        REQUIRE(m.at("name") == "keet");
        REQUIRE(m.at("count") == "3");
        REQUIRE(m.at("flag") == "true");

        require_json_error([]() { (void)keet::json::parseMap("[1,2,3]"); }, "parseMap");
    }

    SECTION("parseArray") {
        auto arr = keet::json::parseArray(R"(["alpha","beta","gamma"])");
        REQUIRE(arr == std::vector<std::string>{"alpha", "beta", "gamma"});

        auto mixed = keet::json::parseArray(R"(["text", 42, true, {"k":"v"}])");
        REQUIRE(mixed == std::vector<std::string>{"text", "42", "true", "{\"k\":\"v\"}"});

        require_json_error([]() { (void)keet::json::parseArray("{}"); }, "parseArray");
    }

    SECTION("parseIntArray") {
        auto ints = keet::json::parseIntArray("[10, -20, 0, 999999999999]");
        REQUIRE(ints == std::vector<int64_t>{10, -20, 0, 999999999999LL});

        require_json_error([]() { (void)keet::json::parseIntArray("[1, 2.5, 3]"); }, "parseIntArray");
        require_json_error([]() { (void)keet::json::parseIntArray("[\"1\"]"); }, "parseIntArray");
    }

    SECTION("parseFloatArray") {
        auto floats = keet::json::parseFloatArray("[1, 2.5, -3.25, 1e2]");
        REQUIRE(floats.size() == 4);
        REQUIRE_THAT(floats[0], Catch::Matchers::WithinRel(1.0));
        REQUIRE_THAT(floats[1], Catch::Matchers::WithinRel(2.5));
        REQUIRE_THAT(floats[2], Catch::Matchers::WithinRel(-3.25));
        REQUIRE_THAT(floats[3], Catch::Matchers::WithinRel(100.0));

        require_json_error([]() { (void)keet::json::parseFloatArray("[1.0, \"bad\"]"); }, "parseFloatArray");
    }

    SECTION("parseBoolArray") {
        auto bools = keet::json::parseBoolArray("[true, false, true]");
        REQUIRE(bools == std::vector<bool>{true, false, true});

        require_json_error([]() { (void)keet::json::parseBoolArray("[true, 1]"); }, "parseBoolArray");
    }
}

TEST_CASE("Runtime JSON - Immutable Object & Array Builders", "[runtime][json][builders]") {
    SECTION("emptyObject, emptyArray, and nested set* operations") {
        std::string obj = keet::json::emptyObject();
        REQUIRE(obj == "{}");

        obj = keet::json::setString(obj, "service.name", "keety-api");
        obj = keet::json::setInt(obj, "service.port", 9000);
        obj = keet::json::setFloat(obj, "service.ratio", 0.75);
        obj = keet::json::setBool(obj, "service.enabled", true);
        obj = keet::json::setNull(obj, "service.deprecated");
        obj = keet::json::setRaw(obj, "service.tags", R"(["fast","safe"])");

        REQUIRE(keet::json::getString(obj, "service.name") == "keety-api");
        REQUIRE(keet::json::getInt(obj, "service.port") == 9000);
        REQUIRE_THAT(keet::json::getFloat(obj, "service.ratio"), Catch::Matchers::WithinRel(0.75));
        REQUIRE(keet::json::getBool(obj, "service.enabled") == true);
        REQUIRE(keet::json::isNull(obj, "service.deprecated"));
        REQUIRE(keet::json::getString(obj, "service.tags.0") == "fast");
        REQUIRE(keet::json::getString(obj, "service.tags.1") == "safe");

        // Update existing array index and append at index == size
        obj = keet::json::setString(obj, "service.tags.1", "strict");
        obj = keet::json::setString(obj, "service.tags.2", "typed");
        REQUIRE(keet::json::getArray(obj, "service.tags") ==
                std::vector<std::string>{"fast", "strict", "typed"});

        // Remove key and remove array element
        obj = keet::json::removeKey(obj, "service.deprecated");
        REQUIRE_FALSE(keet::json::hasKey(obj, "service.deprecated"));

        obj = keet::json::removeKey(obj, "service.tags.0");
        REQUIRE(keet::json::getArray(obj, "service.tags") ==
                std::vector<std::string>{"strict", "typed"});
    }

    SECTION("Builder error conditions") {
        require_json_error([]() {
            (void)keet::json::setFloat("{}", "nanVal", std::numeric_limits<double>::quiet_NaN());
        }, "NaN or Infinity");

        require_json_error([]() {
            (void)keet::json::setRaw("{}", "bad", "{invalid");
        });

        require_json_error([]() {
            (void)keet::json::removeKey("{}", "");
        }, "empty");
    }
}

TEST_CASE("Runtime JSON - Polymorphic stringify, Formatting, Escaping & File I/O", "[runtime][json][stringify][io]") {
    SECTION("Polymorphic stringify on primitives, collections, optionals, and variants") {
        REQUIRE(keet::json::stringify(int64_t{42}) == "42");
        REQUIRE(keet::json::stringify(int64_t{-100}) == "-100");
        REQUIRE(keet::json::stringify(3.5) == "3.5");
        REQUIRE(keet::json::stringify(true) == "true");
        REQUIRE(keet::json::stringify(false) == "false");
        REQUIRE(keet::json::stringify('A') == "\"A\"");
        REQUIRE(keet::json::stringify(std::string("hello \"world\"")) == "\"hello \\\"world\\\"\"");
        REQUIRE(keet::json::stringify(std::monostate{}) == "null");
        REQUIRE(keet::json::stringify(nullptr) == "null");

        std::vector<int64_t> nums = {1, 2, 3};
        REQUIRE(keet::json::stringify(nums) == "[1,2,3]");

        std::vector<bool> flags = {true, false, true};
        REQUIRE(keet::json::stringify(flags) == "[true,false,true]");

        std::unordered_map<std::string, int64_t> scores = {{"bob", 20}, {"alice", 30}};
        // Keys are sorted deterministically
        REQUIRE(keet::json::stringify(scores) == R"({"alice":30,"bob":20})");

        std::optional<std::string> opt_some = "present";
        std::optional<std::string> opt_none = std::nullopt;
        REQUIRE(keet::json::stringify(opt_some) == "\"present\"");
        REQUIRE(keet::json::stringify(opt_none) == "null");

        std::variant<int64_t, std::string, std::monostate> v1 = int64_t{99};
        std::variant<int64_t, std::string, std::monostate> v2 = std::string("text");
        std::variant<int64_t, std::string, std::monostate> v3 = std::monostate{};
        REQUIRE(keet::json::stringify(v1) == "99");
        REQUIRE(keet::json::stringify(v2) == "\"text\"");
        REQUIRE(keet::json::stringify(v3) == "null");

        require_json_error([]() {
            (void)keet::json::stringify(std::numeric_limits<double>::infinity());
        }, "NaN or Infinity");
    }

    SECTION("format, minify, escape, and unescape") {
        std::string messy = " { \n \"b\" : [ 1 , 2 ] , \"a\" : true } ";
        std::string minified = keet::json::minify(messy);
        REQUIRE(minified == R"({"a":true,"b":[1,2]})");

        std::string pretty = keet::json::format(minified, 2);
        std::string expected_pretty =
            "{\n"
            "  \"a\": true,\n"
            "  \"b\": [\n"
            "    1,\n"
            "    2\n"
            "  ]\n"
            "}";
        REQUIRE(pretty == expected_pretty);

        require_json_error([&]() { (void)keet::json::format(minified, -1); }, "indent");
        require_json_error([&]() { (void)keet::json::format(minified, 17); }, "indent");

        std::string raw = "line1\n\"quoted\"\t\\slash";
        std::string escaped = keet::json::escape(raw);
        REQUIRE(escaped == R"("line1\n\"quoted\"\t\\slash")");
        REQUIRE(keet::json::unescape(escaped) == raw);
        // Also works on unquoted interior string content
        REQUIRE(keet::json::unescape(R"(hello\nworld)") == "hello\nworld");
    }

    SECTION("readFile and writeFile round-trip") {
        auto tmp_path = (std::filesystem::temp_directory_path() / "keety_test_runtime_json.json").string();
        std::string payload = R"({"app":"keety","items":[1,2,3],"ok":true})";

        REQUIRE(keet::json::writeFile(tmp_path, payload, 2));
        std::string loaded = keet::json::readFile(tmp_path);
        REQUIRE(loaded == payload);

        std::filesystem::remove(tmp_path);

        require_json_error([]() {
            (void)keet::json::readFile("/nonexistent_dir_keety/missing.json");
        }, "readFile");
    }
}
