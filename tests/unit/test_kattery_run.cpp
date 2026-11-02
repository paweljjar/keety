#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/cli.hpp>
#include <keety/pkg/manifest.hpp>
#include <keety/pkg/script_runner.hpp>
#include <keety/version.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using namespace keety::pkg;

namespace {

struct TempDirectory {
    fs::path path;

    explicit TempDirectory(const std::string& prefix) {
        path = fs::temp_directory_path() / (prefix + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        fs::create_directories(path);
    }

    ~TempDirectory() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    void createFile(const fs::path& relPath, const std::string& content = "") {
        fs::path fullPath = path / relPath;
        fs::create_directories(fullPath.parent_path());
        std::ofstream ofs(fullPath);
        ofs << content;
    }
};

} // namespace

TEST_CASE("ScriptRunner shell command building and PATH injection", "[pkg][script_runner]") {
    SECTION("Command line without extra arguments returns exact command") {
        std::string cmd = ScriptRunner::buildShellCommandLine("keety build src/main.keet", {});
        REQUIRE(cmd == "keety build src/main.keet");
    }

    SECTION("Command line with forwarded arguments quotes correctly") {
        std::string cmd = ScriptRunner::buildShellCommandLine("echo", {"hello", "world with space"});
#if defined(_WIN32)
        REQUIRE(cmd == "echo hello \"world with space\"");
#else
        REQUIRE(cmd == "echo hello 'world with space'");
#endif
    }

    SECTION("PATH injection adds directory to env") {
        std::map<std::string, std::string> env;
        fs::path mockDir = "/fake/toolchain/bin";
        ScriptRunner::injectKeetyPath(env, mockDir);

        REQUIRE(env.find("PATH") != env.end());
        REQUIRE(env["PATH"].rfind(mockDir.string(), 0) == 0);
    }

    SECTION("Compiler version constraint check emits warning if older") {
        Manifest m;
        m.name = "futuristic_pkg";
        m.version = "1.0.0";
        m.keety = ">=999.0.0";

        std::ostringstream err;
        ScriptRunner::checkKeetyVersionConstraint(m, err);
        std::string errText = err.str();

        REQUIRE(errText.find("warning: package 'futuristic_pkg' requires keety >=999.0.0") != std::string::npos);
    }

    SECTION("Compiler version constraint check emits no warning if satisfied") {
        Manifest m;
        m.name = "current_pkg";
        m.version = "1.0.0";
        m.keety = ">=0.1.0";

        std::ostringstream err;
        ScriptRunner::checkKeetyVersionConstraint(m, err);
        REQUIRE(err.str().empty());
    }
}

TEST_CASE("kattery CLI run, build, and test commands", "[pkg][cli][run]") {
    TempDirectory temp("test_kattery_run");

    SECTION("kattery run without args lists available scripts") {
        temp.createFile("pedigree.json", R"({
  "name": "my-app",
  "version": "1.0.0",
  "scripts": {
    "start": "echo start_app",
    "test": "echo test_app"
  }
})");

        std::ostringstream out;
        std::ostringstream err;
        int code = Cli::run({"--cwd", temp.path.string(), "run"}, out, err);

        REQUIRE(code == 0);
        std::string text = out.str();
        REQUIRE(text.find("Available scripts:") != std::string::npos);
        REQUIRE(text.find("start: echo start_app") != std::string::npos);
        REQUIRE(text.find("test: echo test_app") != std::string::npos);
    }

    SECTION("kattery run executes defined script and captures exit code 0") {
        temp.createFile("pedigree.json", R"({
  "name": "my-app",
  "version": "1.0.0",
  "scripts": {
    "greet": "echo ScriptRanSuccessfully"
  }
})");

        std::ostringstream out;
        std::ostringstream err;
        int code = Cli::run({"--cwd", temp.path.string(), "run", "greet"}, out, err);

        REQUIRE(code == 0);
        REQUIRE(out.str().find("ScriptRanSuccessfully") != std::string::npos);
    }

    SECTION("kattery run forwards arguments after --") {
        temp.createFile("pedigree.json", R"({
  "name": "my-app",
  "version": "1.0.0",
  "scripts": {
    "echo_args": "echo"
  }
})");

        std::ostringstream out;
        std::ostringstream err;
        int code = Cli::run({"--cwd", temp.path.string(), "run", "echo_args", "--", "alpha", "beta"}, out, err);

        REQUIRE(code == 0);
        std::string text = out.str();
        REQUIRE(text.find("alpha") != std::string::npos);
        REQUIRE(text.find("beta") != std::string::npos);
    }

    SECTION("kattery run unknown script returns exit code 127") {
        temp.createFile("pedigree.json", R"({
  "name": "my-app",
  "version": "1.0.0",
  "scripts": {}
})");

        std::ostringstream out;
        std::ostringstream err;
        int code = Cli::run({"--cwd", temp.path.string(), "run", "nonexistent"}, out, err);

        REQUIRE(code == 127);
        REQUIRE(err.str().find("script 'nonexistent' not found") != std::string::npos);
    }

    SECTION("kattery run propagates non-zero exit code") {
        temp.createFile("pedigree.json", R"({
  "name": "my-app",
  "version": "1.0.0",
  "scripts": {
    "fail": "exit 42"
  }
})");

        std::ostringstream out;
        std::ostringstream err;
        int code = Cli::run({"--cwd", temp.path.string(), "run", "fail"}, out, err);

        REQUIRE(code == 42);
    }

    SECTION("kattery test executes scripts.test when defined") {
        temp.createFile("pedigree.json", R"({
  "name": "my-app",
  "version": "1.0.0",
  "scripts": {
    "test": "echo RunningTestSuite"
  }
})");

        std::ostringstream out;
        std::ostringstream err;
        int code = Cli::run({"--cwd", temp.path.string(), "test"}, out, err);

        REQUIRE(code == 0);
        REQUIRE(out.str().find("RunningTestSuite") != std::string::npos);
    }

    SECTION("kattery test fails with KAT4003 when scripts.test is missing") {
        temp.createFile("pedigree.json", R"({
  "name": "my-app",
  "version": "1.0.0",
  "scripts": {}
})");

        std::ostringstream out;
        std::ostringstream err;
        int code = Cli::run({"--cwd", temp.path.string(), "test"}, out, err);

        REQUIRE(code != 0);
        REQUIRE(err.str().find("KAT4003") != std::string::npos);
    }

    SECTION("kattery run with --workspaces runs in each member that defines the script") {
        temp.createFile("pedigree.json", R"({
  "name": "mono-root",
  "version": "1.0.0",
  "workspaces": ["packages/*"]
})");
        temp.createFile("packages/pkg_a/pedigree.json", R"({
  "name": "pkg_a",
  "version": "1.0.0",
  "scripts": {
    "check": "echo RunningPkgA"
  }
})");
        temp.createFile("packages/pkg_b/pedigree.json", R"({
  "name": "pkg_b",
  "version": "1.0.0",
  "scripts": {
    "check": "echo RunningPkgB"
  }
})");

        std::ostringstream out;
        std::ostringstream err;
        int code = Cli::run({"--cwd", temp.path.string(), "run", "check", "--workspaces"}, out, err);

        REQUIRE(code == 0);
        std::string text = out.str();
        REQUIRE(text.find("pkg_a") != std::string::npos);
        REQUIRE(text.find("RunningPkgA") != std::string::npos);
        REQUIRE(text.find("pkg_b") != std::string::npos);
        REQUIRE(text.find("RunningPkgB") != std::string::npos);
    }
}
