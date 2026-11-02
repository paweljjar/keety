#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/process.hpp>

#include <chrono>
#include <filesystem>

using namespace keety::pkg;
namespace fs = std::filesystem;

TEST_CASE("ProcessRunner execution and output capture", "[kattery][process]") {
    SECTION("Find executable in PATH") {
        auto gitExe = ProcessRunner::findExecutable("git");
        REQUIRE(gitExe.has_value());
        REQUIRE(fs::exists(*gitExe));

        auto nonExistent = ProcessRunner::findExecutable("non_existent_binary_xyz_123");
        REQUIRE_FALSE(nonExistent.has_value());
    }

    SECTION("Run process and capture stdout") {
        auto res = ProcessRunner::run({"git", "--version"});
        REQUIRE(res.success());
        REQUIRE(res.exitCode == 0);
        REQUIRE(res.stdoutText.find("git version") != std::string::npos);
    }

    SECTION("Exit code propagation on command failure") {
        auto res = ProcessRunner::run({"git", "invalid-subcommand-xyz"});
        REQUIRE_FALSE(res.success());
        REQUIRE(res.exitCode != 0);
    }

    SECTION("Process cwd option") {
        fs::path tmp = fs::temp_directory_path();
        auto res = ProcessRunner::run({"git", "rev-parse", "--is-inside-work-tree"}, tmp);
        // temp dir is not a git worktree, so git exits non-zero
        REQUIRE_FALSE(res.success());
    }
}
