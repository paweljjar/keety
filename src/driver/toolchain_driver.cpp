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

        if (options.wrapv) {
            args.push_back("-fwrapv");
        }

        if (options.position_independent_code) {
            args.push_back("-fPIC");
        }

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

    std::string full_cmd = cmd + " 2>&1";

#if defined(_WIN32)
    FILE* pipe = _popen(full_cmd.c_str(), "r");
#else
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

    const std::string ext = (toolchain_.kind == CompilerKind::MSVC) ? ".obj" : ".o";

    for (const auto& src : source_files) {
        std::filesystem::path obj_path = output_dir / (src.stem().string() + ext);
        results.push_back(compile(src, obj_path, options));
    }

    return results;
}

} // namespace keety::driver
