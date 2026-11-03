#include <keety/driver/toolchain_driver.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace keety::driver {

namespace {

std::string quote_arg(const std::string& arg) {
    if (arg.empty()) {
        return "\"\"";
    }
    bool needs_quote = false;
    for (char c : arg) {
        if (c == ' ' || c == '\t' || c == '\"' || c == '(' || c == ')') {
            needs_quote = true;
            break;
        }
    }
    if (!needs_quote) {
        return arg;
    }
    std::string result = "\"";
    for (char c : arg) {
        if (c == '\"') {
            result += "\\\"";
        } else {
            result += c;
        }
    }
    result += "\"";
    return result;
}

} // namespace

std::string_view to_string(OptimizationLevel level) noexcept {
    switch (level) {
    case OptimizationLevel::O0: return "O0";
    case OptimizationLevel::O1: return "O1";
    case OptimizationLevel::O2: return "O2";
    case OptimizationLevel::O3: return "O3";
    case OptimizationLevel::Os: return "Os";
    case OptimizationLevel::Oz: return "Oz";
    }
    return "O3";
}

std::string_view to_string(BinaryKind kind) noexcept {
    switch (kind) {
    case BinaryKind::Executable:   return "Executable";
    case BinaryKind::SharedLibrary: return "SharedLibrary";
    }
    return "Executable";
}

std::string_view to_compiler_flag(OptimizationLevel level, CompilerKind kind) noexcept {
    if (kind == CompilerKind::MSVC) {
        switch (level) {
        case OptimizationLevel::O0: return "/Od";
        case OptimizationLevel::O1: return "/O1";
        case OptimizationLevel::O2: return "/O2";
        case OptimizationLevel::O3: return "/O2";
        case OptimizationLevel::Os: return "/O1";
        case OptimizationLevel::Oz: return "/O1";
        }
        return "/O2";
    }

    switch (level) {
    case OptimizationLevel::O0: return "-O0";
    case OptimizationLevel::O1: return "-O1";
    case OptimizationLevel::O2: return "-O2";
    case OptimizationLevel::O3: return "-O3";
    case OptimizationLevel::Os: return "-Os";
    case OptimizationLevel::Oz:
        return (kind == CompilerKind::GCC) ? "-Os" : "-Oz";
    }
    return "-O3";
}

ToolchainDriver::ToolchainDriver() {
    auto default_toolchain = ToolchainDetector::detect_default();
    if (default_toolchain.has_value()) {
        toolchain_ = std::move(*default_toolchain);
    }
}

ToolchainDriver::ToolchainDriver(ToolchainInfo info)
    : toolchain_(std::move(info)) {}

bool ToolchainDriver::has_valid_toolchain() const noexcept {
    return toolchain_.is_valid();
}

const ToolchainInfo& ToolchainDriver::get_toolchain() const noexcept {
    return toolchain_;
}

void ToolchainDriver::set_toolchain(ToolchainInfo info) {
    toolchain_ = std::move(info);
}

std::string ToolchainDriver::get_executable_extension() noexcept {
#if defined(_WIN32)
    return ".exe";
#else
    return "";
#endif
}

std::string ToolchainDriver::get_shared_library_extension() noexcept {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

std::string ToolchainDriver::get_object_file_extension(CompilerKind kind) noexcept {
    return (kind == CompilerKind::MSVC) ? ".obj" : ".o";
}

std::vector<std::string> ToolchainDriver::build_compile_args(
    const std::filesystem::path& source_file,
    const std::filesystem::path& output_file,
    const CompileOptions& options) const {

    std::vector<std::string> args;
    args.push_back(toolchain_.compiler_path.string());

    if (toolchain_.kind == CompilerKind::MSVC) {
        args.push_back("/nologo");
        args.push_back("/EHsc");
        args.push_back("/std:" + options.cpp_standard);
        args.push_back(std::string(to_compiler_flag(options.opt_level, CompilerKind::MSVC)));

        if (options.debug_symbols) {
            args.push_back("/Z7");
        }

        if (options.compile_only) {
            args.push_back("/c");
        }

        for (const auto& inc : options.include_directories) {
            args.push_back("/I" + inc.string());
        }

        for (const auto& def : options.defines) {
            args.push_back("/D" + def);
        }

        for (const auto& flag : options.extra_flags) {
            args.push_back(flag);
        }

        args.push_back(source_file.string());

        if (options.compile_only) {
            args.push_back("/Fo:" + output_file.string());
        } else {
            args.push_back("/Fe:" + output_file.string());
        }
    } else {
        // Clang, GCC, and compatible toolchains
        args.push_back("-std=" + options.cpp_standard);
        args.push_back(std::string(to_compiler_flag(options.opt_level, toolchain_.kind)));

        if (options.debug_symbols) {
            args.push_back("-g");
        }

        if (options.wrapv) {
            args.push_back("-fwrapv");
        }

#if !defined(_WIN32)
        if (options.position_independent_code) {
            args.push_back("-fPIC");
        }
#endif

        if (options.compile_only) {
            args.push_back("-c");
        }

        for (const auto& inc : options.include_directories) {
            args.push_back("-I" + inc.string());
        }

        for (const auto& def : options.defines) {
            args.push_back("-D" + def);
        }

        for (const auto& flag : options.extra_flags) {
            args.push_back(flag);
        }

        args.push_back(source_file.string());
        args.push_back("-o");
        args.push_back(output_file.string());
    }

    return args;
}

std::string ToolchainDriver::build_compile_command(
    const std::filesystem::path& source_file,
    const std::filesystem::path& output_file,
    const CompileOptions& options) const {

    auto args = build_compile_args(source_file, output_file, options);
    std::ostringstream ss;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            ss << ' ';
        }
        ss << quote_arg(args[i]);
    }
    return ss.str();
}

CompilationResult ToolchainDriver::compile(
    const std::filesystem::path& source_file,
    const std::filesystem::path& output_file,
    const CompileOptions& options) const {

    CompilationResult result;
    result.output_file = output_file;

    if (!has_valid_toolchain()) {
        result.success = false;
        result.exit_code = -1;
        result.output = "No valid C++ toolchain configured for compilation.";
        return result;
    }

    // Ensure parent directory of output_file exists
    if (output_file.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(output_file.parent_path(), ec);
    }

    std::string cmd = build_compile_command(source_file, output_file, options);
    result.command_line = cmd;

#if defined(_WIN32)
    std::string full_cmd = "\"" + cmd + " 2>&1\"";
    FILE* pipe = _popen(full_cmd.c_str(), "r");
#else
    std::string full_cmd = cmd + " 2>&1";
    FILE* pipe = popen(full_cmd.c_str(), "r");
#endif

    if (!pipe) {
        result.success = false;
        result.exit_code = -1;
        result.output = "Failed to launch compiler process.";
        return result;
    }

    std::array<char, 512> buffer;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.output += buffer.data();
    }

#if defined(_WIN32)
    int raw_status = _pclose(pipe);
    result.exit_code = raw_status;
#else
    int raw_status = pclose(pipe);
    if (WIFEXITED(raw_status)) {
        result.exit_code = WEXITSTATUS(raw_status);
    } else {
        result.exit_code = -1;
    }
#endif

    result.success = (result.exit_code == 0) && std::filesystem::exists(output_file);
    return result;
}

std::vector<CompilationResult> ToolchainDriver::compile_batch(
    const std::vector<std::filesystem::path>& source_files,
    const std::filesystem::path& output_dir,
    const CompileOptions& options) const {

    std::vector<CompilationResult> results;
    results.reserve(source_files.size());

    const std::string ext = get_object_file_extension(toolchain_.kind);

    for (const auto& src : source_files) {
        std::filesystem::path obj_path = output_dir / (src.stem().string() + ext);
        results.push_back(compile(src, obj_path, options));
    }

    return results;
}

std::vector<std::string> ToolchainDriver::build_link_args(
    const std::vector<std::filesystem::path>& object_files,
    const std::filesystem::path& output_file,
    const LinkOptions& options) const {

    std::vector<std::string> args;
    args.push_back(toolchain_.compiler_path.string());

    if (toolchain_.kind == CompilerKind::MSVC) {
        args.push_back("/nologo");
        if (options.kind == BinaryKind::SharedLibrary) {
            args.push_back("/LD");
        }

        for (const auto& obj : object_files) {
            args.push_back(obj.string());
        }

        args.push_back("/Fe:" + output_file.string());

        args.push_back("/link");
        if (options.debug_symbols) {
            args.push_back("/DEBUG");
        }

        for (const auto& dir : options.library_directories) {
            args.push_back("/LIBPATH:" + dir.string());
        }

        for (const auto& lib : options.libraries) {
            if (lib.size() >= 4 && lib.substr(lib.size() - 4) == ".lib") {
                args.push_back(lib);
            } else {
                args.push_back(lib + ".lib");
            }
        }

        for (const auto& flag : options.extra_linker_flags) {
            args.push_back(flag);
        }
    } else {
        // Clang, GCC, and compatible toolchains
        if (options.debug_symbols) {
            args.push_back("-g");
        }

        if (options.kind == BinaryKind::SharedLibrary) {
#if defined(__APPLE__)
            args.push_back("-dynamiclib");
#else
            args.push_back("-shared");
#endif
#if !defined(_WIN32)
            args.push_back("-fPIC");
#endif
        }

        for (const auto& obj : object_files) {
            args.push_back(obj.string());
        }

        for (const auto& dir : options.library_directories) {
            args.push_back("-L" + dir.string());
        }

        for (const auto& lib : options.libraries) {
            if (lib.rfind("-l", 0) == 0 || lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                args.push_back(lib);
            } else if (lib.size() >= 2 && (lib.substr(lib.size() - 2) == ".a" || lib.substr(lib.size() - 3) == ".so")) {
                args.push_back(lib);
            } else {
                args.push_back("-l" + lib);
            }
        }

        args.push_back("-o");
        args.push_back(output_file.string());

        for (const auto& flag : options.extra_linker_flags) {
            args.push_back(flag);
        }
    }

    return args;
}

std::string ToolchainDriver::build_link_command(
    const std::vector<std::filesystem::path>& object_files,
    const std::filesystem::path& output_file,
    const LinkOptions& options) const {

    auto args = build_link_args(object_files, output_file, options);
    std::ostringstream ss;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            ss << ' ';
        }
        ss << quote_arg(args[i]);
    }
    return ss.str();
}

LinkResult ToolchainDriver::link(
    const std::vector<std::filesystem::path>& object_files,
    const std::filesystem::path& output_file,
    const LinkOptions& options) const {

    LinkResult result;
    result.output_file = output_file;

    if (!has_valid_toolchain()) {
        result.success = false;
        result.exit_code = -1;
        result.output = "No valid C++ toolchain configured for linking.";
        return result;
    }

    if (object_files.empty()) {
        result.success = false;
        result.exit_code = -1;
        result.output = "No object files provided for linking.";
        return result;
    }

    // Ensure parent directory of output_file exists
    if (output_file.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(output_file.parent_path(), ec);
    }

    std::string cmd = build_link_command(object_files, output_file, options);
    result.command_line = cmd;

#if defined(_WIN32)
    std::string full_cmd = "\"" + cmd + " 2>&1\"";
    FILE* pipe = _popen(full_cmd.c_str(), "r");
#else
    std::string full_cmd = cmd + " 2>&1";
    FILE* pipe = popen(full_cmd.c_str(), "r");
#endif

    if (!pipe) {
        result.success = false;
        result.exit_code = -1;
        result.output = "Failed to launch linker process.";
        return result;
    }

    std::array<char, 512> buffer;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.output += buffer.data();
    }

#if defined(_WIN32)
    int raw_status = _pclose(pipe);
    result.exit_code = raw_status;
#else
    int raw_status = pclose(pipe);
    if (WIFEXITED(raw_status)) {
        result.exit_code = WEXITSTATUS(raw_status);
    } else {
        result.exit_code = -1;
    }
#endif

    result.success = (result.exit_code == 0) && std::filesystem::exists(output_file);
    return result;
}

LinkResult ToolchainDriver::link_executable(
    const std::vector<std::filesystem::path>& object_files,
    const std::filesystem::path& output_file,
    const LinkOptions& options) const {

    LinkOptions exec_opts = options;
    exec_opts.kind = BinaryKind::Executable;
    return link(object_files, output_file, exec_opts);
}

LinkResult ToolchainDriver::link_shared_library(
    const std::vector<std::filesystem::path>& object_files,
    const std::filesystem::path& output_file,
    const LinkOptions& options) const {

    LinkOptions sh_opts = options;
    sh_opts.kind = BinaryKind::SharedLibrary;
    return link(object_files, output_file, sh_opts);
}

LinkResult ToolchainDriver::compile_and_link(
    const std::vector<std::filesystem::path>& source_files,
    const std::filesystem::path& output_binary,
    const CompileOptions& compile_opts,
    const LinkOptions& link_opts) const {

    LinkResult final_result;
    final_result.output_file = output_binary;

    if (!has_valid_toolchain()) {
        final_result.success = false;
        final_result.exit_code = -1;
        final_result.output = "No valid C++ toolchain configured.";
        return final_result;
    }

    if (source_files.empty()) {
        final_result.success = false;
        final_result.exit_code = -1;
        final_result.output = "No source files provided for compilation.";
        return final_result;
    }

    std::filesystem::path temp_dir = output_binary.has_parent_path()
        ? output_binary.parent_path() / ".keety_tmp"
        : std::filesystem::temp_directory_path() / "keety_tmp";

    std::error_code ec;
    std::filesystem::create_directories(temp_dir, ec);

    CompileOptions effective_compile_opts = compile_opts;
    effective_compile_opts.compile_only = true;
    if (link_opts.kind == BinaryKind::SharedLibrary) {
        effective_compile_opts.position_independent_code = true;
    }

    std::vector<std::filesystem::path> object_files;
    object_files.reserve(source_files.size());

    const std::string ext = get_object_file_extension(toolchain_.kind);

    for (const auto& src : source_files) {
        std::filesystem::path obj_path = temp_dir / (src.stem().string() + ext);
        auto comp_res = compile(src, obj_path, effective_compile_opts);
        if (!comp_res.success) {
            final_result.success = false;
            final_result.exit_code = comp_res.exit_code;
            final_result.command_line = comp_res.command_line;
            final_result.output = comp_res.output;
            std::filesystem::remove_all(temp_dir, ec);
            return final_result;
        }
        object_files.push_back(obj_path);
    }

    final_result = link(object_files, output_binary, link_opts);

    // Clean up temporary object directory
    std::filesystem::remove_all(temp_dir, ec);

    return final_result;
}

} // namespace keety::driver
