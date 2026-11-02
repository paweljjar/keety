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

TEST_CASE("Binary kind string and platform extension conversions", "[driver][toolchain]") {
    CHECK(to_string(BinaryKind::Executable) == "Executable");
    CHECK(to_string(BinaryKind::SharedLibrary) == "SharedLibrary");

    CHECK(ToolchainDriver::get_object_file_extension(CompilerKind::MSVC) == ".obj");
    CHECK(ToolchainDriver::get_object_file_extension(CompilerKind::Clang) == ".o");
    CHECK(ToolchainDriver::get_object_file_extension(CompilerKind::GCC) == ".o");

    std::string exec_ext = ToolchainDriver::get_executable_extension();
    std::string shlib_ext = ToolchainDriver::get_shared_library_extension();
#if defined(_WIN32)
    CHECK(exec_ext == ".exe");
    CHECK(shlib_ext == ".dll");
#elif defined(__APPLE__)
    CHECK(exec_ext == "");
    CHECK(shlib_ext == ".dylib");
#else
    CHECK(exec_ext == "");
    CHECK(shlib_ext == ".so");
#endif
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
#if !defined(_WIN32)
    CHECK(contains_arg("-fPIC"));
#endif
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

TEST_CASE("Toolchain driver builds linker arguments for Clang and GCC", "[driver][toolchain]") {
    ToolchainInfo info;
    info.kind = CompilerKind::Clang;
    info.compiler_path = "/usr/bin/clang++";

    ToolchainDriver driver(info);

    LinkOptions exec_opts;
    exec_opts.kind = BinaryKind::Executable;
    exec_opts.library_directories = {"/usr/local/lib"};
    exec_opts.libraries = {"pthread", "libz.a"};
    exec_opts.extra_linker_flags = {"-Wl,-rpath,/usr/local/lib"};

    auto exec_args = driver.build_link_args({"a.o", "b.o"}, "bin/app", exec_opts);

    auto contains_arg = [&](const std::vector<std::string>& vec, const std::string& expected) {
        return std::find(vec.begin(), vec.end(), expected) != vec.end();
    };

    CHECK(exec_args[0] == "/usr/bin/clang++");
    CHECK(contains_arg(exec_args, "a.o"));
    CHECK(contains_arg(exec_args, "b.o"));
    CHECK(contains_arg(exec_args, "-L/usr/local/lib"));
    CHECK(contains_arg(exec_args, "-lpthread"));
    CHECK(contains_arg(exec_args, "libz.a"));
    CHECK(contains_arg(exec_args, "-o"));
    CHECK(contains_arg(exec_args, "bin/app"));
    CHECK(contains_arg(exec_args, "-Wl,-rpath,/usr/local/lib"));

    LinkOptions sh_opts;
    sh_opts.kind = BinaryKind::SharedLibrary;
    auto sh_args = driver.build_link_args({"plugin.o"}, "libplugin.dylib", sh_opts);

#if defined(__APPLE__)
    CHECK(contains_arg(sh_args, "-dynamiclib"));
#else
    CHECK(contains_arg(sh_args, "-shared"));
#endif
#if !defined(_WIN32)
    CHECK(contains_arg(sh_args, "-fPIC"));
#endif
    CHECK(contains_arg(sh_args, "plugin.o"));
    CHECK(contains_arg(sh_args, "-o"));
    CHECK(contains_arg(sh_args, "libplugin.dylib"));
}

TEST_CASE("Toolchain driver builds linker arguments for MSVC", "[driver][toolchain]") {
    ToolchainInfo info;
    info.kind = CompilerKind::MSVC;
    info.compiler_path = "C:\\Program Files\\MSVC\\cl.exe";

    ToolchainDriver driver(info);

    LinkOptions exec_opts;
    exec_opts.kind = BinaryKind::Executable;
    exec_opts.library_directories = {"C:\\libs"};
    exec_opts.libraries = {"ws2_32", "user32.lib"};

    auto exec_args = driver.build_link_args({"main.obj"}, "app.exe", exec_opts);

    auto contains_arg = [&](const std::vector<std::string>& vec, const std::string& expected) {
        return std::find(vec.begin(), vec.end(), expected) != vec.end();
    };

    CHECK(contains_arg(exec_args, "/nologo"));
    CHECK(contains_arg(exec_args, "main.obj"));
    CHECK(contains_arg(exec_args, "/Fe:app.exe"));
    CHECK(contains_arg(exec_args, "/link"));
    CHECK(contains_arg(exec_args, "/LIBPATH:C:\\libs"));
    CHECK(contains_arg(exec_args, "ws2_32.lib"));
    CHECK(contains_arg(exec_args, "user32.lib"));

    LinkOptions sh_opts;
    sh_opts.kind = BinaryKind::SharedLibrary;
    auto sh_args = driver.build_link_args({"lib.obj"}, "lib.dll", sh_opts);
    CHECK(contains_arg(sh_args, "/LD"));
    CHECK(contains_arg(sh_args, "/Fe:lib.dll"));
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

    LinkOptions link_opts;
    link_opts.library_directories = {"/lib path with spaces"};
    std::string link_cmd = driver.build_link_command({"obj file.o"}, "bin output", link_opts);
    CHECK(link_cmd.find("\"-L/lib path with spaces\"") != std::string::npos);
    CHECK(link_cmd.find("\"obj file.o\"") != std::string::npos);
    CHECK(link_cmd.find("\"bin output\"") != std::string::npos);
}

TEST_CASE("Toolchain driver rejects compilation and linking without valid toolchain or objects", "[driver][toolchain]") {
    ToolchainDriver driver(ToolchainInfo{});
    CHECK_FALSE(driver.has_valid_toolchain());

    auto comp_res = driver.compile("any.cpp", "any.o");
    CHECK_FALSE(comp_res.success);
    CHECK(comp_res.exit_code == -1);
    CHECK_FALSE(comp_res.output.empty());

    auto link_res = driver.link({"any.o"}, "any.bin");
    CHECK_FALSE(link_res.success);
    CHECK(link_res.exit_code == -1);

    ToolchainDriver valid_driver;
    if (valid_driver.has_valid_toolchain()) {
        auto empty_link_res = valid_driver.link({}, "empty.bin");
        CHECK_FALSE(empty_link_res.success);
        CHECK(empty_link_res.output == "No object files provided for linking.");
    }
}

TEST_CASE("Toolchain driver compiles and links executable and shared library on host", "[driver][toolchain]") {
    ToolchainDriver driver;
    if (!driver.has_valid_toolchain()) {
        return; // Skip if no C++ toolchain is available on the machine
    }

    auto temp_dir = std::filesystem::temp_directory_path() / "keety_toolchain_link_test";
    std::filesystem::create_directories(temp_dir);

    // 1. Standalone executable compilation and linking
    auto main_cpp = temp_dir / "main.cpp";
    auto main_obj = temp_dir / ("main" + ToolchainDriver::get_object_file_extension(driver.get_toolchain().kind));
    auto main_exe = temp_dir / ("sample_app" + ToolchainDriver::get_executable_extension());

    {
        std::ofstream out(main_cpp);
        out << "#include <iostream>\n"
            << "int main(int argc, char* argv[]) {\n"
            << "    (void)argc; (void)argv;\n"
            << "    std::cout << \"KeetScript native host execution test\" << std::endl;\n"
            << "    return 0;\n"
            << "}\n";
    }

    CompileOptions comp_opts;
    comp_opts.opt_level = OptimizationLevel::O3;
    comp_opts.wrapv = true;
    comp_opts.cpp_standard = "c++17";
    comp_opts.compile_only = true;

    auto comp_res = driver.compile(main_cpp, main_obj, comp_opts);
    REQUIRE(comp_res.success);
    REQUIRE(std::filesystem::exists(main_obj));

    LinkOptions link_opts;
    link_opts.kind = BinaryKind::Executable;
    auto link_res = driver.link_executable({main_obj}, main_exe, link_opts);
    REQUIRE(link_res.success);
    REQUIRE(std::filesystem::exists(main_exe));

    // Verify generated executable runs successfully
    std::string run_cmd = "\"" + main_exe.string() + "\"";
    int run_status = std::system(run_cmd.c_str());
    CHECK(run_status == 0);

    // 2. Shared library compilation and linking
    auto lib_cpp = temp_dir / "math_plugin.cpp";
    auto lib_obj = temp_dir / ("math_plugin" + ToolchainDriver::get_object_file_extension(driver.get_toolchain().kind));
    auto lib_shlib = temp_dir / ("libmath_plugin" + ToolchainDriver::get_shared_library_extension());

    {
        std::ofstream out(lib_cpp);
        out << "extern \"C\" int calculate_sum(int a, int b) {\n"
            << "    return a + b;\n"
            << "}\n";
    }

    CompileOptions lib_comp_opts = comp_opts;
    lib_comp_opts.position_independent_code = true;

    auto lib_comp_res = driver.compile(lib_cpp, lib_obj, lib_comp_opts);
    REQUIRE(lib_comp_res.success);

    auto sh_link_res = driver.link_shared_library({lib_obj}, lib_shlib);
    REQUIRE(sh_link_res.success);
    CHECK(std::filesystem::exists(lib_shlib));

    // 3. Unified compile_and_link pipeline
    auto pipeline_bin = temp_dir / ("pipeline_app" + ToolchainDriver::get_executable_extension());
    auto pipeline_res = driver.compile_and_link({main_cpp}, pipeline_bin, comp_opts, link_opts);
    REQUIRE(pipeline_res.success);
    CHECK(std::filesystem::exists(pipeline_bin));

    // Clean up
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}
