#include <catch2/catch_test_macros.hpp>
#include <keety/driver/cli_driver.hpp>

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

TEST_CASE("Positive conformance: Control flow and error recovery", "[conformance][control_flow]") {
    ToolchainDriver driver;
    if (!driver.has_valid_toolchain()) {
        return;
    }

    SECTION("Verify switch pattern matching and named loop break in switch_named_loop_break") {
        auto test_file = locate_conformance_test("positive/05_control_flow/switch_named_loop_break.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"run", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 0);
    }

    SECTION("Verify scoped zonk error recovery block and yield in zonk_block_yield") {
        auto test_file = locate_conformance_test("positive/05_control_flow/zonk_block_yield.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"run", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 0);
    }
}
