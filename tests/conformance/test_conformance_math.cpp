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

TEST_CASE("Positive conformance: Standard Math module and numerical operations", "[conformance][math]") {
    ToolchainDriver driver;
    if (!driver.has_valid_toolchain()) {
        return;
    }

    SECTION("Compile and run math_operations program with constants, functions, PRNG, and zonk fallback") {
        auto test_file = locate_conformance_test("positive/09_math/math_operations.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"run", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 0);
    }
}
