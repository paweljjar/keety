#include <catch2/catch_test_macros.hpp>
#include <keety/driver/cli_driver.hpp>
#include <keet/json.hpp>

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

        auto test_bench_opts = CliDriver::parse_args({"test", "--benchmarks"});
        CHECK(test_bench_opts.command == CliCommand::Test);
        CHECK(test_bench_opts.test_benchmarks);

        auto test_conf_opts = CliDriver::parse_args({"test", "--conformance"});
        CHECK(test_conf_opts.command == CliCommand::Test);
        CHECK(test_conf_opts.test_conformance);
    }

    SECTION("String conversions for commands and emit targets") {
        CHECK(to_string(CliCommand::Check) == "check");
        CHECK(to_string(CliCommand::Emit) == "emit");
        CHECK(to_string(CliCommand::Build) == "build");
        CHECK(to_string(CliCommand::Run) == "run");
        CHECK(to_string(CliCommand::Test) == "test");
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

TEST_CASE("CLI test runner options parsing and flag handling", "[driver][cli][test]") {
    SECTION("Parse test command with positional target path") {
        auto opts = CliDriver::parse_args({"test", "tests/my_suite.test.keet"});
        CHECK(opts.command == CliCommand::Test);
        CHECK(opts.input_file == "tests/my_suite.test.keet");
    }

    SECTION("Parse test command with -f and --filter flags") {
        auto opts1 = CliDriver::parse_args({"test", "-f", "Math"});
        CHECK(opts1.command == CliCommand::Test);
        CHECK(opts1.test_filter == "Math");

        auto opts2 = CliDriver::parse_args({"test", "--filter=Crypto", "tests/"});
        CHECK(opts2.command == CliCommand::Test);
        CHECK(opts2.test_filter == "Crypto");
        CHECK(opts2.input_file == "tests/");
    }

    SECTION("Parse test command with --fail-fast") {
        auto opts = CliDriver::parse_args({"test", "--fail-fast"});
        CHECK(opts.command == CliCommand::Test);
        CHECK(opts.test_fail_fast);
    }

    SECTION("Parse test command with --json and --tap") {
        auto opts1 = CliDriver::parse_args({"test", "--json"});
        CHECK(opts1.command == CliCommand::Test);
        CHECK(opts1.test_json);
        CHECK(!opts1.test_tap);

        auto opts2 = CliDriver::parse_args({"test", "--tap"});
        CHECK(opts2.command == CliCommand::Test);
        CHECK(!opts2.test_json);
        CHECK(opts2.test_tap);
    }
}

TEST_CASE("CLI test runner auto-discovery heuristics", "[driver][cli][test]") {
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "keety_test_discovery";
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
    std::filesystem::create_directories(temp_dir, ec);

    // 1. *.test.keet
    auto f1 = temp_dir / "math.test.keet";
    { std::ofstream out(f1); out << "import test;\n"; }

    // 2. test_*.keet
    auto f2 = temp_dir / "test_auth.keet";
    { std::ofstream out(f2); out << "import test;\n"; }

    // 3. Regular non-test file
    auto f3 = temp_dir / "app.keet";
    { std::ofstream out(f3); out << "fn main(): void {}\n"; }

    // 4. File in tests/ with import test
    auto tests_dir = temp_dir / "tests";
    std::filesystem::create_directories(tests_dir, ec);
    auto f4 = tests_dir / "custom.keet";
    { std::ofstream out(f4); out << "import test;\nfn run(): void {}\n"; }

    // 5. File in tests/ WITHOUT import test
    auto f5 = tests_dir / "helper.keet";
    { std::ofstream out(f5); out << "export fn help(): void {}\n"; }

    // 6. Hidden dir (should be skipped)
    auto hidden_dir = temp_dir / ".git";
    std::filesystem::create_directories(hidden_dir, ec);
    auto f6 = hidden_dir / "ignored.test.keet";
    { std::ofstream out(f6); out << "import test;\n"; }

    // 7. build dir (should be skipped)
    auto build_dir = temp_dir / "build";
    std::filesystem::create_directories(build_dir, ec);
    auto f7 = build_dir / "ignored.test.keet";
    { std::ofstream out(f7); out << "import test;\n"; }

    auto discovered = CliDriver::discover_test_files(temp_dir);
    REQUIRE(discovered.size() == 3);

    std::vector<std::string> filenames;
    for (const auto& p : discovered) {
        filenames.push_back(p.filename().string());
    }
    std::sort(filenames.begin(), filenames.end());

    CHECK(filenames[0] == "custom.keet");
    CHECK(filenames[1] == "math.test.keet");
    CHECK(filenames[2] == "test_auth.keet");

    SECTION("Direct single file target discovery") {
        auto single = CliDriver::discover_test_files(f1);
        REQUIRE(single.size() == 1);
        CHECK(single[0] == f1);
    }

    std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("CLI test runner end-to-end execution", "[driver][cli][test]") {
    ToolchainDriver driver;
    if (!driver.has_valid_toolchain()) {
        SUCCEED("Skipping CLI test runner execution test: no C++ toolchain found.");
        return;
    }

    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "keety_test_runner_e2e";
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
    std::filesystem::create_directories(temp_dir, ec);

    // Create a passing test file with test.case registration
    auto test_file = temp_dir / "sample.test.keet";
    {
        std::ofstream out(test_file);
        out << "import test;\n"
            << "fn main(): void {\n"
            << "    test.suite(\"Arithmetic\", fn(): void {\n"
            << "        test.case(\"addition check\", fn(): void {\n"
            << "            test.assertEq(10 + 5, 15);\n"
            << "            test.assertTrue(true);\n"
            << "        });\n"
            << "        test.case(\"inequality check\", fn(): void {\n"
            << "            test.assertNe(10, 20);\n"
            << "        });\n"
            << "    });\n"
            << "}\n";
    }

    SECTION("Default human-readable terminal reporter") {
        CliOptions opts;
        opts.command = CliCommand::Test;
        opts.input_file = test_file;
        opts.color_mode = keety::diag::ColorMode::Never;

        std::ostringstream out;
        std::ostringstream err;
        int code = CliDriver::execute_test(opts, out, err);

        CHECK(code == 0);
        std::string output = out.str();
        CHECK(output.find("PASS") != std::string::npos);
        CHECK(output.find("sample.test.keet (2 tests") != std::string::npos);
        CHECK(output.find("addition check") != std::string::npos);
        CHECK(output.find("inequality check") != std::string::npos);
        CHECK(output.find("Test Files") != std::string::npos);
        CHECK(output.find("1 passed (1 total)") != std::string::npos);
        CHECK(output.find("Tests") != std::string::npos);
        CHECK(output.find("2 passed (2 total)") != std::string::npos);
    }

    SECTION("Structured JSON reporter (--json)") {
        CliOptions opts;
        opts.command = CliCommand::Test;
        opts.input_file = test_file;
        opts.test_json = true;

        std::ostringstream out;
        std::ostringstream err;
        int code = CliDriver::execute_test(opts, out, err);

        CHECK(code == 0);
        std::string json_str = out.str();
        CHECK(json_str.find("PASS") == std::string::npos); // Suppresses ANSI output
        CHECK(json_str.find("Test Files") == std::string::npos);

        ::keet::json::detail::Parser parser(json_str);
        auto root = parser.parse_document();
        REQUIRE(root.is_object());
        const auto& obj = std::get<keet::json::detail::JsonNode::Object>(root.value);

        REQUIRE(obj.count("total"));
        CHECK(std::get<int64_t>(obj.at("total").value) == 2);
        REQUIRE(obj.count("passed"));
        CHECK(std::get<int64_t>(obj.at("passed").value) == 2);
        REQUIRE(obj.count("failed"));
        CHECK(std::get<int64_t>(obj.at("failed").value) == 0);

        REQUIRE(obj.count("testFiles"));
        const auto& tf_obj = std::get<keet::json::detail::JsonNode::Object>(obj.at("testFiles").value);
        CHECK(std::get<int64_t>(tf_obj.at("total").value) == 1);
        CHECK(std::get<int64_t>(tf_obj.at("passed").value) == 1);

        REQUIRE(obj.count("results"));
        const auto& res_arr = std::get<keet::json::detail::JsonNode::Array>(obj.at("results").value);
        REQUIRE(res_arr.size() == 2);
        const auto& r0 = std::get<keet::json::detail::JsonNode::Object>(res_arr[0].value);
        CHECK(std::get<std::string>(r0.at("name").value) == "addition check");
        CHECK(std::get<bool>(r0.at("passed").value) == true);
    }

    SECTION("TAP version 13 reporter (--tap)") {
        CliOptions opts;
        opts.command = CliCommand::Test;
        opts.input_file = test_file;
        opts.test_tap = true;

        std::ostringstream out;
        std::ostringstream err;
        int code = CliDriver::execute_test(opts, out, err);

        CHECK(code == 0);
        std::string tap_str = out.str();
        CHECK(tap_str.rfind("TAP version 13\n", 0) == 0);
        CHECK(tap_str.find("1..2\n") != std::string::npos);
        CHECK(tap_str.find("ok 1 - sample.test.keet > Arithmetic > addition check # time=") != std::string::npos);
        CHECK(tap_str.find("ok 2 - sample.test.keet > Arithmetic > inequality check # time=") != std::string::npos);
    }

    SECTION("Regex filtering (--filter)") {
        CliOptions opts;
        opts.command = CliCommand::Test;
        opts.input_file = test_file;
        opts.test_filter = "add.*"; // Regex matching addition check
        opts.test_json = true;

        std::ostringstream out;
        std::ostringstream err;
        int code = CliDriver::execute_test(opts, out, err);

        CHECK(code == 0);
        std::string json_str = out.str();
        ::keet::json::detail::Parser parser(json_str);
        auto root = parser.parse_document();
        REQUIRE(root.is_object());
        const auto& obj = std::get<keet::json::detail::JsonNode::Object>(root.value);

        CHECK(std::get<int64_t>(obj.at("total").value) == 1);
        CHECK(std::get<int64_t>(obj.at("passed").value) == 1);
        CHECK(std::get<int64_t>(obj.at("skipped").value) == 1);

        const auto& res_arr = std::get<keet::json::detail::JsonNode::Array>(obj.at("results").value);
        REQUIRE(res_arr.size() == 1);
        const auto& r0 = std::get<keet::json::detail::JsonNode::Object>(res_arr[0].value);
        CHECK(std::get<std::string>(r0.at("name").value) == "addition check");
    }

    SECTION("Fail-fast mode (--fail-fast)") {
        auto failing_file = temp_dir / "failing.test.keet";
        {
            std::ofstream f_out(failing_file);
            f_out << "import test;\n"
                  << "fn main(): void {\n"
                  << "    test.case(\"will fail\", fn(): void {\n"
                  << "        test.assertEq(1, 2);\n"
                  << "    });\n"
                  << "    test.case(\"should be skipped\", fn(): void {\n"
                  << "        test.assertTrue(true);\n"
                  << "    });\n"
                  << "}\n";
        }

        CliOptions opts;
        opts.command = CliCommand::Test;
        opts.input_file = failing_file;
        opts.test_fail_fast = true;
        opts.test_json = true;

        std::ostringstream out;
        std::ostringstream err;
        int code = CliDriver::execute_test(opts, out, err);

        CHECK(code == 1);
        std::string json_str = out.str();
        ::keet::json::detail::Parser parser(json_str);
        auto root = parser.parse_document();
        REQUIRE(root.is_object());
        const auto& obj = std::get<keet::json::detail::JsonNode::Object>(root.value);

        // Aborted on first failure: total executed 1, failed 1
        CHECK(std::get<int64_t>(obj.at("total").value) == 1);
        CHECK(std::get<int64_t>(obj.at("failed").value) == 1);
    }

    SECTION("Empty directory reporting for --json and --tap") {
        auto empty_dir = temp_dir / "empty_dir";
        std::filesystem::create_directories(empty_dir, ec);

        CliOptions json_opts;
        json_opts.command = CliCommand::Test;
        json_opts.input_file = empty_dir;
        json_opts.test_json = true;

        std::ostringstream json_out;
        std::ostringstream json_err;
        int code1 = CliDriver::execute_test(json_opts, json_out, json_err);
        CHECK(code1 == 0);
        CHECK(json_out.str().find("\"total\": 0") != std::string::npos);

        CliOptions tap_opts;
        tap_opts.command = CliCommand::Test;
        tap_opts.input_file = empty_dir;
        tap_opts.test_tap = true;

        std::ostringstream tap_out;
        std::ostringstream tap_err;
        int code2 = CliDriver::execute_test(tap_opts, tap_out, tap_err);
        CHECK(code2 == 0);
        CHECK(tap_out.str() == "TAP version 13\n1..0\n");
    }

    SECTION("BuildDir in pedigree.json routes build artifacts into buildDir and leaves src clean") {
        std::filesystem::path proj_dir = temp_dir / "proj_with_builddir";
        std::filesystem::create_directories(proj_dir / "src", ec);

        std::ofstream ped(proj_dir / "pedigree.json");
        ped << "{\n"
            << "  \"name\": \"builddir-test\",\n"
            << "  \"version\": \"0.1.0\",\n"
            << "  \"buildDir\": \"build\"\n"
            << "}\n";
        ped.close();

        std::filesystem::path src_test = proj_dir / "src" / "math.test.keet";
        std::ofstream test_fs(src_test);
        test_fs << "import test;\n"
                << "from \"test\" import assertEq;\n\n"
                << "fn test_simple(): void {\n"
                << "    assertEq(10, 10);\n"
                << "}\n";
        test_fs.close();

        CliOptions opts;
        opts.command = CliCommand::Test;
        opts.input_file = proj_dir / "src";
        opts.color_mode = keety::diag::ColorMode::Never;

        std::ostringstream out, err;
        int code = CliDriver::execute_test(opts, out, err);
        CHECK(code == 0);
        CHECK(out.str().find("PASS") != std::string::npos);

        // Verify that build artifacts are inside proj_dir / "build"
        CHECK(std::filesystem::exists(proj_dir / "build"));
        CHECK(std::filesystem::exists(proj_dir / "build" / ".keety_build"));

        // Verify that src/ has no .keety_build directory or compiled binary
        CHECK(!std::filesystem::exists(proj_dir / "src" / ".keety_build"));
        CHECK(!std::filesystem::exists(proj_dir / "src" / "math.test"));
    }

    std::filesystem::remove_all(temp_dir, ec);
}

