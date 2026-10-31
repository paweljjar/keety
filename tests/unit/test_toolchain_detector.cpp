#include <catch2/catch_test_macros.hpp>
#include <keety/driver/toolchain_detector.hpp>

using namespace keety::driver;

TEST_CASE("Toolchain detector classifies compiler kind from path", "[driver][toolchain]") {
    CHECK(ToolchainDetector::classify_from_path("/usr/bin/clang++") == CompilerKind::Clang);
    CHECK(ToolchainDetector::classify_from_path("clang++-14") == CompilerKind::Clang);
    CHECK(ToolchainDetector::classify_from_path("clang") == CompilerKind::Clang);

    CHECK(ToolchainDetector::classify_from_path("/usr/bin/g++") == CompilerKind::GCC);
    CHECK(ToolchainDetector::classify_from_path("/usr/local/bin/g++-11") == CompilerKind::GCC);
    CHECK(ToolchainDetector::classify_from_path("gcc") == CompilerKind::GCC);

    CHECK(ToolchainDetector::classify_from_path("C:\\Program Files\\MSVC\\cl.exe") == CompilerKind::MSVC);
    CHECK(ToolchainDetector::classify_from_path("cl") == CompilerKind::MSVC);

    CHECK(ToolchainDetector::classify_from_path("/bin/ls") == CompilerKind::Unknown);
}

TEST_CASE("Toolchain detector classifies compiler kind from version banner", "[driver][toolchain]") {
    std::string_view apple_clang_banner =
        "Apple clang version 21.0.0 (clang-2100.3.34.2)\n"
        "Target: arm64-apple-darwin27.0.0\n"
        "Thread model: posix\n";
    CHECK(ToolchainDetector::classify_from_banner(apple_clang_banner) == CompilerKind::Clang);

    std::string_view llvm_clang_banner =
        "clang version 15.0.7\n"
        "Target: x86_64-pc-linux-gnu\n"
        "Thread model: posix\n";
    CHECK(ToolchainDetector::classify_from_banner(llvm_clang_banner) == CompilerKind::Clang);

    std::string_view gcc_banner =
        "g++ (Ubuntu 11.2.0-19ubuntu1) 11.2.0\n"
        "Copyright (C) 2021 Free Software Foundation, Inc.\n"
        "This is free software; see the source for copying conditions.\n";
    CHECK(ToolchainDetector::classify_from_banner(gcc_banner) == CompilerKind::GCC);

    std::string_view msvc_banner =
        "Microsoft (R) C/C++ Optimizing Compiler Version 19.29.30133 for x64\n"
        "Copyright (C) Microsoft Corporation. All rights reserved.\n";
    CHECK(ToolchainDetector::classify_from_banner(msvc_banner) == CompilerKind::MSVC);

    std::string_view unknown_banner = "some custom compiler v1.0\n";
    CHECK(ToolchainDetector::classify_from_banner(unknown_banner) == CompilerKind::Unknown);
}

TEST_CASE("Toolchain detector parses version numbers correctly", "[driver][toolchain]") {
    int major = 0, minor = 0, patch = 0;

    REQUIRE(ToolchainDetector::parse_version_string("14.0.6", major, minor, patch));
    CHECK(major == 14);
    CHECK(minor == 0);
    CHECK(patch == 6);

    REQUIRE(ToolchainDetector::parse_version_string("Apple clang version 21.0.0", major, minor, patch));
    CHECK(major == 21);
    CHECK(minor == 0);
    CHECK(patch == 0);

    REQUIRE(ToolchainDetector::parse_version_string("Compiler Version 19.29.30133 for x64", major, minor, patch));
    CHECK(major == 19);
    CHECK(minor == 29);
    CHECK(patch == 30133);

    CHECK_FALSE(ToolchainDetector::parse_version_string("no digits here", major, minor, patch));
}

TEST_CASE("Toolchain compiler kind string conversions", "[driver][toolchain]") {
    CHECK(to_string(CompilerKind::Clang) == "Clang");
    CHECK(to_string(CompilerKind::GCC) == "GCC");
    CHECK(to_string(CompilerKind::MSVC) == "MSVC");
    CHECK(to_string(CompilerKind::Unknown) == "Unknown");
}

TEST_CASE("Toolchain candidate discovery and path lookup", "[driver][toolchain]") {
    auto candidates = ToolchainDetector::get_candidate_names();
    CHECK_FALSE(candidates.empty());

    auto non_existent = ToolchainDetector::find_in_path("definitely_nonexistent_compiler_xyz_404");
    CHECK_FALSE(non_existent.has_value());

    auto existing_sh = ToolchainDetector::find_in_path("sh");
    CHECK(existing_sh.has_value());
    if (existing_sh) {
        CHECK(std::filesystem::exists(*existing_sh));
    }
}

TEST_CASE("Toolchain detector probes default host C++ compiler", "[driver][toolchain]") {
    auto default_toolchain = ToolchainDetector::detect_default();
    REQUIRE(default_toolchain.has_value());
    CHECK(default_toolchain->is_valid());
    CHECK(default_toolchain->kind != CompilerKind::Unknown);
    CHECK(default_toolchain->major_version > 0);
    CHECK(default_toolchain->supports_cpp17);
    CHECK(std::filesystem::exists(default_toolchain->compiler_path));
}
