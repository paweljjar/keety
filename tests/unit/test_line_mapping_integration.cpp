#include <catch2/catch_test_macros.hpp>
#include <keety/driver/cli_driver.hpp>
#include <keety/driver/toolchain_driver.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace keety::driver;

TEST_CASE("Host compiler diagnostics report originating .keet source lines", "[line_mapping][integration]") {
    ToolchainDriver driver;
    if (!driver.has_valid_toolchain()) {
        return;
    }

    auto temp_dir = std::filesystem::temp_directory_path() / "keety_diag_line_map_test";
    std::filesystem::create_directories(temp_dir);

    // Create a generated C++ source that has a #line directive pointing to app.keet:42,
    // followed immediately by a static_assert failure to test compiler line reporting.
    auto cpp_file = temp_dir / "diag_test.cpp";
    auto obj_file = temp_dir / ("diag_test" + ToolchainDriver::get_object_file_extension(driver.get_toolchain().kind));
    {
        std::ofstream out(cpp_file);
        out << "#line 42 \"app.keet\"\n"
            << "static_assert(false, \"line mapping error verification\");\n";
    }

    CompileOptions comp_opts;
    comp_opts.opt_level = OptimizationLevel::O0;
    comp_opts.compile_only = true;

    auto comp_res = driver.compile(cpp_file, obj_file, comp_opts);
    CHECK(comp_res.success == false);
    CHECK(comp_res.exit_code != 0);

    // Host compiler (Clang, GCC, MSVC) should report "app.keet" and line "42" in its error output
    CHECK(comp_res.output.find("app.keet") != std::string::npos);
    CHECK(comp_res.output.find("42") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("Native debugger LLDB resolves breakpoints in .keet source files", "[line_mapping][integration]") {
    ToolchainDriver driver;
    if (!driver.has_valid_toolchain()) {
        return;
    }

    // Check if lldb is installed on the current platform
    bool has_lldb = false;
#if !defined(_WIN32)
    has_lldb = (std::system("which lldb > /dev/null 2>&1") == 0);
#endif

    if (!has_lldb) {
        SUCCEED("LLDB is not available in the current environment; skipping debugger verification.");
        return;
    }

    auto temp_dir = std::filesystem::temp_directory_path() / "keety_lldb_breakpoint_test";
    std::filesystem::create_directories(temp_dir);

    auto src_file = temp_dir / "target.keet";
    {
        std::ofstream out(src_file);
        out << "fn compute(x: int): int {\n"
            << "    let y: int = x * 3;\n"
            << "    return y + 10;\n"
            << "}\n\n"
            << "fn main(): int {\n"
            << "    let result: int = compute(7);\n"
            << "    return result;\n"
            << "}\n";
    }

    auto bin_path = temp_dir / ("target_app" + ToolchainDriver::get_executable_extension());

    // Build with debug symbols enabled
    std::ostringstream build_out, build_err;
    CliOptions opts;
    opts.command = CliCommand::Build;
    opts.input_file = src_file;
    opts.output_dir = bin_path;
    opts.debug_info = true;
    opts.opt_level = OptimizationLevel::O0;

    int build_rc = CliDriver::run(opts, build_out, build_err);
    REQUIRE(build_rc == 0);
    REQUIRE(std::filesystem::exists(bin_path));

    // Verify breakpoint on line 3 (return y + 10;) resolves via LLDB batch mode
    std::string lldb_cmd = "lldb -b -o \"breakpoint set -f target.keet -l 3\" \"" + bin_path.string() + "\" 2>&1";
    FILE* pipe = popen(lldb_cmd.c_str(), "r");
    REQUIRE(pipe != nullptr);

    char buffer[256];
    std::string lldb_output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        lldb_output += buffer;
    }
    int raw_status = pclose(pipe);
    int status = 0;
#if !defined(_WIN32)
    if (WIFEXITED(raw_status)) {
        status = WEXITSTATUS(raw_status);
    }
#else
    status = raw_status;
#endif

    CHECK(status == 0);
    // LLDB must report resolving a breakpoint at target.keet:3
    CHECK(lldb_output.find("Breakpoint 1:") != std::string::npos);
    CHECK(lldb_output.find("target.keet:3") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}
