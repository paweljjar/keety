#include <catch2/catch_test_macros.hpp>
#include <keety/driver/cli_driver.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace keety::driver;

TEST_CASE("CLI argument parser extracts commands and options", "[driver][cli]") {
    SECTION("Parse check command with options") {
        std::vector<std::string> args = {"check", "main.keet", "--verbose", "-O2", "--no-color"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Check);
        CHECK(opts.input_file == "main.keet");
        CHECK(opts.verbose == true);
        CHECK(opts.opt_level == OptimizationLevel::O2);
        CHECK(opts.color_mode == keety::diag::ColorMode::Never);
    }

    SECTION("Parse help and version commands") {
        auto help_opts = CliDriver::parse_args({"--help"});
        CHECK(help_opts.command == CliCommand::Help);

        auto ver_opts = CliDriver::parse_args({"-V"});
        CHECK(ver_opts.command == CliCommand::Version);
    }

    SECTION("Parse emit, build and run stubs with flags") {
        auto emit_opts = CliDriver::parse_args({"emit", "app.keet", "--cpp", "-o", "build/out"});
        CHECK(emit_opts.command == CliCommand::Emit);
        CHECK(emit_opts.input_file == "app.keet");
        CHECK(emit_opts.emit_target == EmitTarget::Cpp);
        CHECK(emit_opts.output_dir == "build/out");

        auto run_opts = CliDriver::parse_args({"run", "script.keet", "--", "arg1", "arg2"});
        CHECK(run_opts.command == CliCommand::Run);
        CHECK(run_opts.input_file == "script.keet");
        REQUIRE(run_opts.run_args.size() == 2);
        CHECK(run_opts.run_args[0] == "arg1");
        CHECK(run_opts.run_args[1] == "arg2");
    }

    SECTION("String conversions for commands and emit targets") {
        CHECK(to_string(CliCommand::Check) == "check");
        CHECK(to_string(CliCommand::Emit) == "emit");
        CHECK(to_string(CliCommand::Build) == "build");
        CHECK(to_string(CliCommand::Run) == "run");
        CHECK(to_string(CliCommand::Version) == "version");
        CHECK(to_string(CliCommand::Help) == "help");

        CHECK(to_string(EmitTarget::None) == "none");
        CHECK(to_string(EmitTarget::Cpp) == "cpp");
        CHECK(to_string(EmitTarget::Hpp) == "hpp");
        CHECK(to_string(EmitTarget::Ast) == "ast");
        CHECK(to_string(EmitTarget::Ir) == "ir");
    }
}

TEST_CASE("CLI driver executes help and version commands", "[driver][cli]") {
    std::ostringstream out;
    std::ostringstream err;

    CliOptions help_opts;
    help_opts.command = CliCommand::Help;
    int help_res = CliDriver::run(help_opts, out, err);
    CHECK(help_res == 0);
    CHECK(out.str().find("Usage: keety") != std::string::npos);

    out.str("");
    out.clear();
    CliOptions ver_opts;
    ver_opts.command = CliCommand::Version;
    int ver_res = CliDriver::run(ver_opts, out, err);
    CHECK(ver_res == 0);
    CHECK(out.str().find("keety - KeetScript compiler") != std::string::npos);
}

TEST_CASE("CLI check validates missing or non-existent files", "[driver][cli]") {
    std::ostringstream out;
    std::ostringstream err;

    CliOptions empty_opts;
    empty_opts.command = CliCommand::Check;
    int res1 = CliDriver::execute_check(empty_opts, out, err);
    CHECK(res1 == 1);
    CHECK(err.str().find("requires a target input file") != std::string::npos);

    err.str("");
    err.clear();
    CliOptions non_existent;
    non_existent.command = CliCommand::Check;
    non_existent.input_file = "non_existent_file_xyz_123.keet";
    int res2 = CliDriver::execute_check(non_existent, out, err);
    CHECK(res2 == 1);
    CHECK(err.str().find("input file not found") != std::string::npos);
}

TEST_CASE("CLI check detects syntax and type errors", "[driver][cli]") {
    auto temp_dir = std::filesystem::temp_directory_path() / "keety_cli_check_errors";
    std::filesystem::create_directories(temp_dir);

    SECTION("Syntax error detection") {
        auto file = temp_dir / "syntax_err.keet";
        {
            std::ofstream out(file);
            out << "fn compute(x: int) int {\n" // missing colon before return type
                << "    return x;\n"
                << "}\n";
        }

        std::ostringstream out;
        std::ostringstream err;
        CliOptions opts;
        opts.command = CliCommand::Check;
        opts.input_file = file;

        int code = CliDriver::execute_check(opts, out, err);
        CHECK(code == 1);
        CHECK_FALSE(err.str().empty());
    }

    SECTION("Type annotation error detection (E3001)") {
        auto file = temp_dir / "type_err.keet";
        {
            std::ofstream out(file);
            out << "let x = 10;\n"; // missing explicit type annotation
        }

        std::ostringstream out;
        std::ostringstream err;
        CliOptions opts;
        opts.command = CliCommand::Check;
        opts.input_file = file;

        int code = CliDriver::execute_check(opts, out, err);
        CHECK(code == 1);
        CHECK(err.str().find("E3001") != std::string::npos);
    }

    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("CLI check validates correct KeetScript sources and multi-module imports", "[driver][cli]") {
    auto temp_dir = std::filesystem::temp_directory_path() / "keety_cli_check_success";
    std::filesystem::create_directories(temp_dir);

    auto math_file = temp_dir / "math.keet";
    {
        std::ofstream out(math_file);
        out << "fn add(a: int, b: int): int {\n"
            << "    return a + b;\n"
            << "}\n";
    }

    auto main_file = temp_dir / "main.keet";
    {
        std::ofstream out(main_file);
        out << "import \"./math\";\n"
            << "fn run(): int {\n"
            << "    let total: int = 42;\n"
            << "    return total;\n"
            << "}\n";
    }

    std::ostringstream out;
    std::ostringstream err;
    CliOptions opts;
    opts.command = CliCommand::Check;
    opts.input_file = main_file;
    opts.verbose = true;

    int code = CliDriver::execute_check(opts, out, err);
    CHECK(code == 0);
    CHECK(out.str().find("successfully") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("CLI check detects circular dependency cycles", "[driver][cli]") {
    auto temp_dir = std::filesystem::temp_directory_path() / "keety_cli_check_cycles";
    std::filesystem::create_directories(temp_dir);

    auto a_file = temp_dir / "a.keet";
    auto b_file = temp_dir / "b.keet";

    {
        std::ofstream out(a_file);
        out << "import \"./b\";\n"
            << "let valA: int = 1;\n";
    }
    {
        std::ofstream out(b_file);
        out << "import \"./a\";\n"
            << "let valB: int = 2;\n";
    }

    std::ostringstream out;
    std::ostringstream err;
    CliOptions opts;
    opts.command = CliCommand::Check;
    opts.input_file = a_file;

    int code = CliDriver::execute_check(opts, out, err);
    CHECK(code == 1);
    CHECK(err.str().find("E1001") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("CLI emit rejects invalid arguments or missing targets", "[driver][cli]") {
    std::ostringstream out;
    std::ostringstream err;

    SECTION("Emit without target specified") {
        CliOptions opts;
        opts.command = CliCommand::Emit;
        opts.input_file = "valid.keet";
        opts.emit_target = EmitTarget::None;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 1);
        CHECK(err.str().find("requires an emission target") != std::string::npos);
    }

    SECTION("Emit without input file") {
        CliOptions opts;
        opts.command = CliCommand::Emit;
        opts.emit_target = EmitTarget::Cpp;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 1);
        CHECK(err.str().find("requires a target input file") != std::string::npos);
    }

    SECTION("Emit with non-existent input file") {
        CliOptions opts;
        opts.command = CliCommand::Emit;
        opts.input_file = "not_found.keet";
        opts.emit_target = EmitTarget::Cpp;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 1);
        CHECK(err.str().find("input file not found") != std::string::npos);
    }
}

TEST_CASE("CLI emit generates AST, IR, HPP and CPP to stream and directory", "[driver][cli]") {
    auto temp_dir = std::filesystem::temp_directory_path() / "keety_cli_emit_test";
    std::filesystem::create_directories(temp_dir);

    auto src_file = temp_dir / "calculator.keet";
    {
        std::ofstream out(src_file);
        out << "let globalOffset: int = 100;\n"
            << "fn calculate(x: int, y: int): int {\n"
            << "    let result: int = x + y + globalOffset;\n"
            << "    return result;\n"
            << "}\n";
    }

    SECTION("Emit AST to stream") {
        std::ostringstream out;
        std::ostringstream err;
        CliOptions opts;
        opts.command = CliCommand::Emit;
        opts.input_file = src_file;
        opts.emit_target = EmitTarget::Ast;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);
        std::string ast_str = out.str();
        CHECK(ast_str.find("ModuleAST \"calculator\"") != std::string::npos);
        CHECK(ast_str.find("VarDecl globalOffset") != std::string::npos);
        CHECK(ast_str.find("FnDecl calculate") != std::string::npos);
    }

    SECTION("Emit IR to stream") {
        std::ostringstream out;
        std::ostringstream err;
        CliOptions opts;
        opts.command = CliCommand::Emit;
        opts.input_file = src_file;
        opts.emit_target = EmitTarget::Ir;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);
        std::string ir_str = out.str();
        CHECK(ir_str.find("calculate") != std::string::npos);
        CHECK(ir_str.find("alloca") != std::string::npos);
    }

    SECTION("Emit HPP to designated directory") {
        auto out_dir = temp_dir / "output_hpp/";
        std::ostringstream out;
        std::ostringstream err;
        CliOptions opts;
        opts.command = CliCommand::Emit;
        opts.input_file = src_file;
        opts.emit_target = EmitTarget::Hpp;
        opts.output_dir = out_dir;
        opts.verbose = true;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);

        auto expected_hpp = out_dir / "calculator.hpp";
        REQUIRE(std::filesystem::exists(expected_hpp));

        std::ifstream hpp_in(expected_hpp);
        std::stringstream ss;
        ss << hpp_in.rdbuf();
        std::string hpp_str = ss.str();

        CHECK(hpp_str.find("#pragma once") != std::string::npos);
        CHECK(hpp_str.find("namespace keet_module_calculator") != std::string::npos);
        CHECK(hpp_str.find("calculate") != std::string::npos);
    }

    SECTION("Emit CPP to designated directory") {
        auto out_dir = temp_dir / "output_cpp/";
        std::ostringstream out;
        std::ostringstream err;
        CliOptions opts;
        opts.command = CliCommand::Emit;
        opts.input_file = src_file;
        opts.emit_target = EmitTarget::Cpp;
        opts.output_dir = out_dir;
        opts.verbose = true;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);

        auto expected_cpp = out_dir / "calculator.cpp";
        REQUIRE(std::filesystem::exists(expected_cpp));

        std::ifstream cpp_in(expected_cpp);
        std::stringstream ss;
        ss << cpp_in.rdbuf();
        std::string cpp_str = ss.str();

        CHECK(cpp_str.find("namespace keet_module_calculator") != std::string::npos);
        CHECK(cpp_str.find("_internal_globalOffset") != std::string::npos);
        CHECK(cpp_str.find("calculate") != std::string::npos);
    }

    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("CLI build compiles native executable and CLI run executes it", "[driver][cli]") {
    ToolchainDriver driver;
    if (!driver.has_valid_toolchain()) {
        return;
    }

    auto temp_dir = std::filesystem::temp_directory_path() / "keety_cli_build_run_test";
    std::filesystem::create_directories(temp_dir);

    auto src_file = temp_dir / "runner.keet";
    {
        std::ofstream out(src_file);
        out << "fn main(): int {\n"
            << "    return 42;\n"
            << "}\n";
    }

    SECTION("Build produces executable binary") {
        auto bin_path = temp_dir / ("runner_app" + ToolchainDriver::get_executable_extension());
        std::ostringstream out;
        std::ostringstream err;
        CliOptions opts;
        opts.command = CliCommand::Build;
        opts.input_file = src_file;
        opts.output_dir = bin_path;
        opts.verbose = true;

        int build_res = CliDriver::run(opts, out, err);
        CHECK(build_res == 0);
        REQUIRE(std::filesystem::exists(bin_path));

        std::string run_cmd = "\"" + bin_path.string() + "\"";
        int sys_code = std::system(run_cmd.c_str());
#if !defined(_WIN32)
        if (WIFEXITED(sys_code)) {
            sys_code = WEXITSTATUS(sys_code);
        }
#endif
        CHECK(sys_code == 42);
    }

    SECTION("Run compiles and directly executes program") {
        std::ostringstream out;
        std::ostringstream err;
        CliOptions opts;
        opts.command = CliCommand::Run;
        opts.input_file = src_file;

        int run_res = CliDriver::run(opts, out, err);
        CHECK(run_res == 42);
    }

    SECTION("Build handles multi-module imports") {
        auto helper_file = temp_dir / "helper.keet";
        {
            std::ofstream out(helper_file);
            out << "fn get_code(): int {\n"
                << "    return 10;\n"
                << "}\n";
        }

        auto entry_file = temp_dir / "entry.keet";
        {
            std::ofstream out(entry_file);
            out << "import \"./helper\";\n"
                << "fn main(): int {\n"
                << "    return 0;\n"
                << "}\n";
        }

        auto bin_path = temp_dir / ("multi_app" + ToolchainDriver::get_executable_extension());
        std::ostringstream out;
        std::ostringstream err;
        CliOptions opts;
        opts.command = CliCommand::Build;
        opts.input_file = entry_file;
        opts.output_dir = bin_path;

        int build_res = CliDriver::run(opts, out, err);
        CHECK(build_res == 0);
        REQUIRE(std::filesystem::exists(bin_path));

        std::string run_cmd = "\"" + bin_path.string() + "\"";
        int sys_code = std::system(run_cmd.c_str());
#if !defined(_WIN32)
        if (WIFEXITED(sys_code)) {
            sys_code = WEXITSTATUS(sys_code);
        }
#endif
        CHECK(sys_code == 0);
    }

    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("CLI configuration flags argument parsing", "[driver][cli]") {
    SECTION("Parse output-dir variations") {
        auto opts1 = CliDriver::parse_args({"build", "main.keet", "--output-dir", "custom_build"});
        CHECK(opts1.output_dir == "custom_build");
        CHECK(opts1.output_is_dir == true);

        auto opts2 = CliDriver::parse_args({"emit", "main.keet", "--cpp", "--output-dir=build_out"});
        CHECK(opts2.output_dir == "build_out");
        CHECK(opts2.output_is_dir == true);

        auto opts3 = CliDriver::parse_args({"build", "main.keet", "--output=dist/bin"});
        CHECK(opts3.output_dir == "dist/bin");
        CHECK(opts3.output_is_dir == false);

        auto opts4 = CliDriver::parse_args({"build", "main.keet", "-o=dist/app"});
        CHECK(opts4.output_dir == "dist/app");
    }

    SECTION("Parse cxx compiler option") {
        auto opts1 = CliDriver::parse_args({"build", "main.keet", "--cxx", "/usr/bin/clang++"});
        CHECK(opts1.cxx_compiler == "/usr/bin/clang++");

        auto opts2 = CliDriver::parse_args({"build", "main.keet", "--cxx=g++-12"});
        CHECK(opts2.cxx_compiler == "g++-12");
    }

    SECTION("Parse optimization level options") {
        CHECK(CliDriver::parse_args({"build", "main.keet", "-O0"}).opt_level == OptimizationLevel::O0);
        CHECK(CliDriver::parse_args({"build", "main.keet", "-O1"}).opt_level == OptimizationLevel::O1);
        CHECK(CliDriver::parse_args({"build", "main.keet", "-O2"}).opt_level == OptimizationLevel::O2);
        CHECK(CliDriver::parse_args({"build", "main.keet", "-O3"}).opt_level == OptimizationLevel::O3);
        CHECK(CliDriver::parse_args({"build", "main.keet", "-Os"}).opt_level == OptimizationLevel::Os);
        CHECK(CliDriver::parse_args({"build", "main.keet", "-Oz"}).opt_level == OptimizationLevel::Oz);

        CHECK(CliDriver::parse_args({"build", "main.keet", "--opt", "2"}).opt_level == OptimizationLevel::O2);
        CHECK(CliDriver::parse_args({"build", "main.keet", "--opt=s"}).opt_level == OptimizationLevel::Os);
        CHECK(CliDriver::parse_args({"build", "main.keet", "-O=1"}).opt_level == OptimizationLevel::O1);
        CHECK(CliDriver::parse_args({"build", "main.keet", "--opt", "O0"}).opt_level == OptimizationLevel::O0);
    }

    SECTION("Parse verbose flag") {
        auto opts1 = CliDriver::parse_args({"check", "main.keet", "-v"});
        CHECK(opts1.verbose == true);

        auto opts2 = CliDriver::parse_args({"build", "main.keet", "--verbose"});
        CHECK(opts2.verbose == true);
    }
}

TEST_CASE("CLI configuration flags behavior in compilation and execution", "[driver][cli]") {
    ToolchainDriver driver;
    if (!driver.has_valid_toolchain()) {
        return;
    }

    auto temp_dir = std::filesystem::temp_directory_path() / "keety_cli_flags_test";
    std::filesystem::create_directories(temp_dir);

    auto src_file = temp_dir / "calc.keet";
    {
        std::ofstream out(src_file);
        out << "fn main(): int {\n"
            << "    return 7;\n"
            << "}\n";
    }

    SECTION("Output directory flag creates destination folder and houses binary") {
        auto out_dir = temp_dir / "target_out";
        std::ostringstream out;
        std::ostringstream err;

        auto opts = CliDriver::parse_args({"build", src_file.string(), "--output-dir", out_dir.string(), "--opt=2"});
        int code = CliDriver::run(opts, out, err);
        CHECK(code == 0);
        REQUIRE(std::filesystem::is_directory(out_dir));

        std::string exec_ext = ToolchainDriver::get_executable_extension();
        auto expected_bin = out_dir / ("calc" + exec_ext);
        REQUIRE(std::filesystem::exists(expected_bin));

        std::string run_cmd = "\"" + expected_bin.string() + "\"";
        int sys_code = std::system(run_cmd.c_str());
#if !defined(_WIN32)
        if (WIFEXITED(sys_code)) {
            sys_code = WEXITSTATUS(sys_code);
        }
#endif
        CHECK(sys_code == 7);
    }

    SECTION("Specifying an invalid C++ compiler produces diagnostic error") {
        std::ostringstream out;
        std::ostringstream err;

        auto opts = CliDriver::parse_args({"build", src_file.string(), "--cxx=nonexistent_compiler_xyz_123"});
        int code = CliDriver::run(opts, out, err);
        CHECK(code == 1);
        CHECK(err.str().find("invalid or not found") != std::string::npos);
    }

    SECTION("Explicit valid C++ compiler flag compiles successfully") {
        const auto& tc = driver.get_toolchain();
        std::ostringstream out;
        std::ostringstream err;

        auto opts = CliDriver::parse_args({"build", src_file.string(), "--cxx=" + tc.compiler_path.string(), "-O1"});
        int code = CliDriver::run(opts, out, err);
        CHECK(code == 0);
    }

    SECTION("Verbose mode streams phase details during build and execution") {
        std::ostringstream build_out;
        std::ostringstream build_err;

        auto build_opts = CliDriver::parse_args({"build", src_file.string(), "--verbose", "-O2"});
        int b_code = CliDriver::run(build_opts, build_out, build_err);
        CHECK(b_code == 0);

        std::string b_log = build_out.str();
        CHECK(b_log.find("[verbose] Building project from entry:") != std::string::npos);
        CHECK(b_log.find("[verbose] Target binary:") != std::string::npos);
        CHECK(b_log.find("[verbose] Optimization level: O2") != std::string::npos);
        CHECK(b_log.find("[verbose] Compiling:") != std::string::npos);
        CHECK(b_log.find("[verbose] Linking") != std::string::npos);

        std::ostringstream run_out;
        std::ostringstream run_err;
        auto run_opts = CliDriver::parse_args({"run", src_file.string(), "--verbose", "--", "alpha", "beta"});
        int r_code = CliDriver::run(run_opts, run_out, run_err);
        CHECK(r_code == 7);

        std::string r_log = run_out.str();
        CHECK(r_log.find("[verbose] Executing:") != std::string::npos);
        CHECK(r_log.find("[verbose] Process exited with status code: 7") != std::string::npos);
    }

    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}
