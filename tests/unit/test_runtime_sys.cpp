#include <catch2/catch_test_macros.hpp>
#include <keet/sys.hpp>

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

TEST_CASE("keet::sys process arguments and lifecycle", "[runtime][sys]") {
    SECTION("initArgs and args roundtrip") {
        char arg0[] = "keety_app";
        char arg1[] = "--verbose";
        char arg2[] = "input.keet";
        char* test_argv[] = {arg0, arg1, arg2, nullptr};

        keet::sys::initArgs(3, test_argv);
        auto args = keet::sys::args();

        REQUIRE(args.size() == 3);
        REQUIRE(args[0] == "keety_app");
        REQUIRE(args[1] == "--verbose");
        REQUIRE(args[2] == "input.keet");
    }

    SECTION("pid returns positive process ID") {
        int64_t pid = keet::sys::pid();
        REQUIRE(pid > 0);
    }
}

TEST_CASE("keet::sys environment variable operations", "[runtime][sys]") {
    const std::string test_key = "KEET_TEST_ENV_VAR_SYS_MOD";
    const std::string test_val = "keet_runtime_value_42";

    SECTION("setEnv, getEnv, hasEnv, getEnvOr, and unsetEnv workflow") {
        // Ensure clean initial state
        keet::sys::unsetEnv(test_key);
        REQUIRE_FALSE(keet::sys::hasEnv(test_key));

        // getEnv on missing returns null (std::monostate)
        auto missing = keet::sys::getEnv(test_key);
        REQUIRE(std::holds_alternative<std::monostate>(missing));

        // getEnvOr returns fallback
        REQUIRE(keet::sys::getEnvOr(test_key, "fallback_default") == "fallback_default");

        // Set variable
        REQUIRE(keet::sys::setEnv(test_key, test_val));
        REQUIRE(keet::sys::hasEnv(test_key));

        // Verify with getEnv
        auto present = keet::sys::getEnv(test_key);
        REQUIRE(std::holds_alternative<std::string>(present));
        REQUIRE(std::get<std::string>(present) == test_val);

        // Verify with getEnvOr
        REQUIRE(keet::sys::getEnvOr(test_key, "unexpected") == test_val);

        // Unset variable
        REQUIRE(keet::sys::unsetEnv(test_key));
        REQUIRE_FALSE(keet::sys::hasEnv(test_key));

        auto after_unset = keet::sys::getEnv(test_key);
        REQUIRE(std::holds_alternative<std::monostate>(after_unset));
    }
}

TEST_CASE("keet::sys platform, arch, and system hardware queries", "[runtime][sys]") {
    SECTION("platform returns recognized OS identifier") {
        std::string platform = keet::sys::platform();
        REQUIRE_FALSE(platform.empty());
#if defined(__APPLE__) && defined(__MACH__)
        REQUIRE(platform == "macos");
#elif defined(__linux__)
        REQUIRE(platform == "linux");
#elif defined(_WIN32)
        REQUIRE(platform == "windows");
#endif
    }

    SECTION("arch returns recognized architecture identifier") {
        std::string arch = keet::sys::arch();
        REQUIRE_FALSE(arch.empty());
#if defined(__x86_64__) || defined(_M_X64)
        REQUIRE(arch == "x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
        REQUIRE(arch == "arm64");
#endif
    }

    SECTION("cpuCount returns at least 1") {
        int64_t cpus = keet::sys::cpuCount();
        REQUIRE(cpus >= 1);
    }
}

TEST_CASE("keet::sys working directory and system paths", "[runtime][sys]") {
    SECTION("cwd returns non-empty path matching std::filesystem") {
        std::string cwd = keet::sys::cwd();
        REQUIRE_FALSE(cwd.empty());
        REQUIRE(std::filesystem::exists(cwd));
    }

    SECTION("tempDir returns existing directory path") {
        std::string temp = keet::sys::tempDir();
        REQUIRE_FALSE(temp.empty());
        REQUIRE(std::filesystem::exists(temp));
        REQUIRE(std::filesystem::is_directory(temp));
    }

    SECTION("homeDir returns non-empty string when environment is set") {
        std::string home = keet::sys::homeDir();
        // In typical Unix/macOS/Windows environments HOME / USERPROFILE is present
        if (!home.empty()) {
            REQUIRE(std::filesystem::exists(home));
        }
    }

    SECTION("chdir changes working directory and throws on invalid path") {
        std::string original_cwd = keet::sys::cwd();
        std::string temp = keet::sys::tempDir();

        REQUIRE(keet::sys::chdir(temp));
        // Verify cwd updated (canonical paths match)
        std::error_code ec;
        REQUIRE(std::filesystem::equivalent(keet::sys::cwd(), temp, ec));

        // Restore original cwd
        REQUIRE(keet::sys::chdir(original_cwd));

        // Attempt chdir into invalid path throws keet::error
        REQUIRE_THROWS_AS(keet::sys::chdir("/path/that/definitely/does/not/exist_keety_sys_test"), keet::error);
        try {
            keet::sys::chdir("/path/that/definitely/does/not/exist_keety_sys_test");
        } catch (const keet::error& err) {
            REQUIRE(err.name == "SystemError");
        }
    }
}

TEST_CASE("keet::sys subprocess execution", "[runtime][sys]") {
    SECTION("exec runs simple command successfully") {
        int64_t code = keet::sys::exec("echo sys_test_exec > /dev/null");
        REQUIRE(code == 0);
    }

    SECTION("execOutput captures standard output") {
        std::string out = keet::sys::execOutput("echo hello_keety_sys");
        REQUIRE(out.find("hello_keety_sys") != std::string::npos);
    }

    SECTION("execOutput throws keet::error on non-zero exit status") {
        REQUIRE_THROWS_AS(keet::sys::execOutput("false || exit 1"), keet::error);
        try {
            keet::sys::execOutput("false || exit 1");
        } catch (const keet::error& err) {
            REQUIRE(err.name == "SystemError");
        }
    }
}
