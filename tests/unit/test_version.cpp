#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/cli.hpp>
#include <keety/version.hpp>

#include <sstream>

TEST_CASE("Compiler and package manager version checks", "[version]") {
    REQUIRE(keety::getVersionString() == "0.1.0");
    REQUIRE_FALSE(keety::getVersionString().empty());

    SECTION("kattery CLI reports identical version string to keety") {
        std::ostringstream out;
        std::ostringstream err;
        int code = keety::pkg::Cli::run({"--version"}, out, err);

        REQUIRE(code == 0);
        std::string versionStr = std::string(keety::getVersionString());
        REQUIRE(out.str().find("v" + versionStr) != std::string::npos);
    }
}

