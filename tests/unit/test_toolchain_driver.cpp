#include <catch2/catch_test_macros.hpp>
#include <keety/driver/toolchain_driver.hpp>

#include <filesystem>
#include <fstream>

using namespace keety::driver;

TEST_CASE("Optimization level conversions and compiler flag mappings", "[driver][toolchain]") {
    CHECK(to_string(OptimizationLevel::O0) == "O0");
    CHECK(to_string(OptimizationLevel::O1) == "O1");
    CHECK(to_string(OptimizationLevel::O2) == "O2");
    CHECK(to_string(OptimizationLevel::O3) == "O3");
    CHECK(to_string(OptimizationLevel::Os) == "Os");
    CHECK(to_string(OptimizationLevel::Oz) == "Oz");

    // Clang flags
    CHECK(to_compiler_flag(OptimizationLevel::O0, CompilerKind::Clang) == "-O0");
    CHECK(to_compiler_flag(OptimizationLevel::O1, CompilerKind::Clang) == "-O1");
    CHECK(to_compiler_flag(OptimizationLevel::O2, CompilerKind::Clang) == "-O2");
    CHECK(to_compiler_flag(OptimizationLevel::O3, CompilerKind::Clang) == "-O3");
    CHECK(to_compiler_flag(OptimizationLevel::Os, CompilerKind::Clang) == "-Os");
    CHECK(to_compiler_flag(OptimizationLevel::Oz, CompilerKind::Clang) == "-Oz");

    // GCC flags (Oz maps to -Os on GCC)
    CHECK(to_compiler_flag(OptimizationLevel::Oz, CompilerKind::GCC) == "-Os");

    // MSVC flags
    CHECK(to_compiler_flag(OptimizationLevel::O0, CompilerKind::MSVC) == "/Od");
    CHECK(to_compiler_flag(OptimizationLevel::O1, CompilerKind::MSVC) == "/O1");
    CHECK(to_compiler_flag(OptimizationLevel::O2, CompilerKind::MSVC) == "/O2");
    CHECK(to_compiler_flag(OptimizationLevel::O3, CompilerKind::MSVC) == "/O2");
    CHECK(to_compiler_flag(OptimizationLevel::Os, CompilerKind::MSVC) == "/O1");
    CHECK(to_compiler_flag(OptimizationLevel::Oz, CompilerKind::MSVC) == "/O1");
}

TEST_CASE("Toolchain driver builds Clang/GCC compilation arguments", "[driver][toolchain]") {
    ToolchainInfo info;
    info.kind = CompilerKind::Clang;
    info.compiler_path = "/usr/bin/clang++";

    ToolchainDriver driver(info);
    REQUIRE(driver.has_valid_toolchain());

    CompileOptions options;
    options.opt_level = OptimizationLevel::O3;
    options.wrapv = true;
    options.cpp_standard = "c++17";
    options.compile_only = true;
    options.position_independent_code = true;
    options.include_directories = {"/opt/keety/include", "/opt/keety/runtime"};
    options.defines = {"KEET_RELEASE=1", "NDEBUG"};
    options.extra_flags = {"-Wall"};

    auto args = driver.build_compile_args("main.cpp", "main.o", options);

    CHECK(args[0] == "/usr/bin/clang++");

    auto contains_arg = [&](const std::string& expected) {
        return std::find(args.begin(), args.end(), expected) != args.end();
    };

    CHECK(contains_arg("-std=c++17"));
    CHECK(contains_arg("-O3"));
    CHECK(contains_arg("-fwrapv"));
    CHECK(contains_arg("-fPIC"));
    CHECK(contains_arg("-c"));
    CHECK(contains_arg("-I/opt/keety/include"));
    CHECK(contains_arg("-I/opt/keety/runtime"));
    CHECK(contains_arg("-DKEET_RELEASE=1"));
    CHECK(contains_arg("-DNDEBUG"));
    CHECK(contains_arg("-Wall"));
    CHECK(contains_arg("main.cpp"));
    CHECK(contains_arg("-o"));
    CHECK(contains_arg("main.o"));
}

TEST_CASE("Toolchain driver builds MSVC compilation arguments", "[driver][toolchain]") {
    ToolchainInfo info;
    info.kind = CompilerKind::MSVC;
    info.compiler_path = "C:\\Program Files\\MSVC\\cl.exe";

    ToolchainDriver driver(info);
    REQUIRE(driver.has_valid_toolchain());

    CompileOptions options;
    options.opt_level = OptimizationLevel::O3;
    options.cpp_standard = "c++17";
    options.compile_only = true;
    options.include_directories = {"C:\\keety\\include"};
    options.defines = {"_CRT_SECURE_NO_WARNINGS"};

    auto args = driver.build_compile_args("test.cpp", "test.obj", options);

    auto contains_arg = [&](const std::string& expected) {
        return std::find(args.begin(), args.end(), expected) != args.end();
    };

    CHECK(contains_arg("/nologo"));
    CHECK(contains_arg("/EHsc"));
    CHECK(contains_arg("/std:c++17"));
    CHECK(contains_arg("/O2"));
    CHECK(contains_arg("/c"));
    CHECK(contains_arg("/IC:\\keety\\include"));
    CHECK(contains_arg("/D_CRT_SECURE_NO_WARNINGS"));
    CHECK(contains_arg("test.cpp"));
    CHECK(contains_arg("/Fo:test.obj"));
}

TEST_CASE("Toolchain driver quotes paths containing spaces", "[driver][toolchain]") {
    ToolchainInfo info;
    info.kind = CompilerKind::Clang;
    info.compiler_path = "/usr/local/bin/clang++";

    ToolchainDriver driver(info);

    CompileOptions options;
    options.include_directories = {"/path with spaces/include"};

    std::string cmd = driver.build_compile_command("source file.cpp", "output file.o", options);
    CHECK(cmd.find("\"-I/path with spaces/include\"") != std::string::npos);
    CHECK(cmd.find("\"source file.cpp\"") != std::string::npos);
    CHECK(cmd.find("\"output file.o\"") != std::string::npos);
}

TEST_CASE("Toolchain driver rejects compilation without valid toolchain", "[driver][toolchain]") {
    ToolchainDriver driver(ToolchainInfo{});
    CHECK_FALSE(driver.has_valid_toolchain());

    auto result = driver.compile("any.cpp", "any.o");
    CHECK_FALSE(result.success);
    CHECK(result.exit_code == -1);
    CHECK_FALSE(result.output.empty());
}

TEST_CASE("Toolchain driver compiles valid C++17 translation unit on host", "[driver][toolchain]") {
    ToolchainDriver driver;
    if (!driver.has_valid_toolchain()) {
        return; // Skip if no C++ toolchain is available on the machine
    }

    auto temp_dir = std::filesystem::temp_directory_path() / "keety_toolchain_test";
    std::filesystem::create_directories(temp_dir);

    auto cpp_file = temp_dir / "sample_test.cpp";
    auto obj_file = temp_dir / "sample_test.o";

    // Write a valid C++17 source file
    {
        std::ofstream out(cpp_file);
        out << "#include <string>\n"
            << "#include <vector>\n"
            << "#include <variant>\n"
            << "int compute_value(int x) {\n"
            << "    std::variant<int, std::string> v = x * 2;\n"
            << "    return std::get<int>(v);\n"
            << "}\n";
    }

    CompileOptions options;
    options.opt_level = OptimizationLevel::O3;
    options.wrapv = true;
    options.cpp_standard = "c++17";
    options.compile_only = true;

    auto result = driver.compile(cpp_file, obj_file, options);

    CHECK(result.success);
    CHECK(result.exit_code == 0);
    CHECK(std::filesystem::exists(obj_file));

    // Test failure on invalid code
    auto err_file = temp_dir / "invalid_syntax.cpp";
    auto err_obj = temp_dir / "invalid_syntax.o";
    {
        std::ofstream out(err_file);
        out << "this is completely invalid C++ syntax !!!;\n";
    }

    auto err_result = driver.compile(err_file, err_obj, options);
    CHECK_FALSE(err_result.success);
    CHECK(err_result.exit_code != 0);
    CHECK_FALSE(err_result.output.empty());

    // Test batch compilation
    auto batch_results = driver.compile_batch({cpp_file}, temp_dir, options);
    REQUIRE(batch_results.size() == 1);
    CHECK(batch_results[0].success);

    // Clean up
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}
