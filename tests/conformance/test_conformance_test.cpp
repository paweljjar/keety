#include <catch2/catch_test_macros.hpp>
#include <keety/driver/cli_driver.hpp>
#include <keet/json.hpp>

#include <filesystem>
#include <sstream>
#include <string>

using namespace keety::driver;

namespace {

std::filesystem::path locate_conformance_test(const std::string& rel_path) {
    std::filesystem::path curr = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i) {
        auto candidate = curr / "tests" / "conformance" / rel_path;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (!curr.has_parent_path() || curr.parent_path() == curr) break;
        curr = curr.parent_path();
    }
    return "";
}

} // namespace

TEST_CASE("Positive conformance: Standard Test module and assertion runner", "[conformance][test]") {
    ToolchainDriver driver;
    if (!driver.has_valid_toolchain()) {
        return;
    }

    auto test_file = locate_conformance_test("positive/13_test/test_framework_operations.keet");
    REQUIRE(!test_file.empty());
    REQUIRE(std::filesystem::exists(test_file));

    SECTION("Execute test file via keety run") {
        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"run", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 0);
    }

    SECTION("Execute test file via keety test with human-readable reporter") {
        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"test", test_file.string(), "--no-color"});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 0);
        std::string text = out.str();
        CHECK(text.find("PASS") != std::string::npos);
        CHECK(text.find("primitive equality and inequality") != std::string::npos);
        CHECK(text.find("assertZonks catches errors") != std::string::npos);
        CHECK(text.find("Test Files") != std::string::npos);
        CHECK(text.find("Tests") != std::string::npos);
    }

    SECTION("Execute test file via keety test with --json reporter") {
        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"test", test_file.string(), "--json"});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 0);

        std::string json_str = out.str();
        CHECK(json_str.find("PASS") == std::string::npos);

        ::keet::json::detail::Parser parser(json_str);
        auto root = parser.parse_document();
        REQUIRE(root.is_object());
        const auto& obj = std::get<keet::json::detail::JsonNode::Object>(root.value);

        REQUIRE(obj.count("total"));
        CHECK(std::get<int64_t>(obj.at("total").value) == 4);
        REQUIRE(obj.count("passed"));
        CHECK(std::get<int64_t>(obj.at("passed").value) == 4);
        REQUIRE(obj.count("failed"));
        CHECK(std::get<int64_t>(obj.at("failed").value) == 0);
    }

    SECTION("Execute test file via keety test with --tap reporter") {
        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"test", test_file.string(), "--tap"});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 0);

        std::string tap_str = out.str();
        CHECK(tap_str.rfind("TAP version 13\n", 0) == 0);
        CHECK(tap_str.find("1..4\n") != std::string::npos);
        CHECK(tap_str.find("ok 1 - test_framework_operations.keet > Core Assertions > primitive equality and inequality") != std::string::npos);
        CHECK(tap_str.find("ok 4 - test_framework_operations.keet > Error Handling > assertZonks catches errors") != std::string::npos);
    }

    SECTION("Execute test file with regex filter") {
        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"test", test_file.string(), "--filter=boolean.*", "--json"});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 0);

        std::string json_str = out.str();
        ::keet::json::detail::Parser parser(json_str);
        auto root = parser.parse_document();
        REQUIRE(root.is_object());
        const auto& obj = std::get<keet::json::detail::JsonNode::Object>(root.value);

        CHECK(std::get<int64_t>(obj.at("total").value) == 1);
        CHECK(std::get<int64_t>(obj.at("passed").value) == 1);
        CHECK(std::get<int64_t>(obj.at("skipped").value) == 3);
    }
}
