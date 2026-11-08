#pragma once

#include <keety/driver/toolchain_detector.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keety::driver {

/// Compilation optimization tier
enum class OptimizationLevel {
    O0, ///< No optimizations (debug / fast build)
    O1, ///< Basic optimizations
    O2, ///< Standard optimizations
    O3, ///< Aggressive optimizations (default for KeetScript release builds)
    Os, ///< Optimize for binary size
    Oz  ///< Optimize aggressively for binary size
};

[[nodiscard]] std::string_view to_string(OptimizationLevel level) noexcept;

[[nodiscard]] std::string_view to_compiler_flag(OptimizationLevel level, CompilerKind kind) noexcept;

/// Output binary artifact kind
enum class BinaryKind {
    Executable,   ///< Standalone executable binary
    SharedLibrary ///< Dynamic / shared library (.so, .dylib, .dll)
};

[[nodiscard]] std::string_view to_string(BinaryKind kind) noexcept;

/// Configuration options for native C++ compiler invocation
struct CompileOptions {
    OptimizationLevel opt_level{OptimizationLevel::O3};
    bool wrapv{true};                          ///< Enforce two's-complement overflow semantics (-fwrapv)
    std::string cpp_standard{"c++20"};         ///< Target C++ standard (default: c++20)
    bool compile_only{true};                   ///< Compile to object file without linking (-c / /c)
    bool position_independent_code{false};     ///< Position-independent code (-fPIC)
    bool debug_symbols{false};                 ///< Generate debug symbols (-g / /Z7)
    std::vector<std::filesystem::path> include_directories{};
    std::vector<std::string> defines{};
    std::vector<std::string> extra_flags{};
    std::optional<std::filesystem::path> working_directory{std::nullopt};
    bool verbose{false};
};

/// Configuration options for native toolchain linker invocation
struct LinkOptions {
    BinaryKind kind{BinaryKind::Executable};
    bool debug_symbols{false};                 ///< Generate debug symbols (-g / /DEBUG)
    std::vector<std::filesystem::path> library_directories{};
    std::vector<std::string> libraries{};
    std::vector<std::string> extra_linker_flags{};
    std::optional<std::filesystem::path> working_directory{std::nullopt};
    bool verbose{false};
};

/// Result of a native compiler invocation
struct CompilationResult {
    bool success{false};
    int exit_code{-1};
    std::string command_line{};
    std::string output{};
    std::filesystem::path output_file{};
};

/// Result of a native linker invocation
struct LinkResult {
    bool success{false};
    int exit_code{-1};
    std::string command_line{};
    std::string output{};
    std::filesystem::path output_file{};
};

/// High-level native toolchain driver for invoking platform C++ compilers and linkers
class ToolchainDriver {
public:
    /// Constructs driver with automatically detected default platform toolchain
    ToolchainDriver();

    /// Constructs driver with an explicit toolchain configuration
    explicit ToolchainDriver(ToolchainInfo info);

    /// Checks if a valid compiler toolchain is configured
    [[nodiscard]] bool has_valid_toolchain() const noexcept;

    /// Returns the active toolchain configuration
    [[nodiscard]] const ToolchainInfo& get_toolchain() const noexcept;

    /// Sets or updates the active toolchain configuration
    void set_toolchain(ToolchainInfo info);

    /// Returns platform-specific executable binary extension (".exe" on Windows, "" on POSIX)
    [[nodiscard]] static std::string get_executable_extension() noexcept;

    /// Returns platform-specific shared library extension (".dylib" on macOS, ".so" on Linux, ".dll" on Windows)
    [[nodiscard]] static std::string get_shared_library_extension() noexcept;

    /// Returns platform-specific object file extension (".obj" on Windows, ".o" on POSIX)
    [[nodiscard]] static std::string get_object_file_extension(CompilerKind kind) noexcept;

    /// Assembles command-line argument list for compilation
    [[nodiscard]] std::vector<std::string> build_compile_args(
        const std::filesystem::path& source_file,
        const std::filesystem::path& output_file,
        const CompileOptions& options = {}) const;

    /// Assembles formatted single-string shell command for compilation
    [[nodiscard]] std::string build_compile_command(
        const std::filesystem::path& source_file,
        const std::filesystem::path& output_file,
        const CompileOptions& options = {}) const;

    /// Executes the native compiler synchronously for a single source file
    [[nodiscard]] CompilationResult compile(
        const std::filesystem::path& source_file,
        const std::filesystem::path& output_file,
        const CompileOptions& options = {}) const;

    /// Compiles multiple source files into corresponding object files in the target directory
    [[nodiscard]] std::vector<CompilationResult> compile_batch(
        const std::vector<std::filesystem::path>& source_files,
        const std::filesystem::path& output_dir,
        const CompileOptions& options = {}) const;

    /// Assembles command-line argument list for linking object files
    [[nodiscard]] std::vector<std::string> build_link_args(
        const std::vector<std::filesystem::path>& object_files,
        const std::filesystem::path& output_file,
        const LinkOptions& options = {}) const;

    /// Assembles formatted single-string shell command for linking object files
    [[nodiscard]] std::string build_link_command(
        const std::vector<std::filesystem::path>& object_files,
        const std::filesystem::path& output_file,
        const LinkOptions& options = {}) const;

    /// Executes the native linker synchronously to create an executable binary or shared library
    [[nodiscard]] LinkResult link(
        const std::vector<std::filesystem::path>& object_files,
        const std::filesystem::path& output_file,
        const LinkOptions& options = {}) const;

    /// Convenience method to link object files into an executable binary
    [[nodiscard]] LinkResult link_executable(
        const std::vector<std::filesystem::path>& object_files,
        const std::filesystem::path& output_file,
        const LinkOptions& options = {}) const;

    /// Convenience method to link object files into a shared library
    [[nodiscard]] LinkResult link_shared_library(
        const std::vector<std::filesystem::path>& object_files,
        const std::filesystem::path& output_file,
        const LinkOptions& options = {}) const;

    /// Compiles source files and links them into the final binary artifact in a unified pipeline
    [[nodiscard]] LinkResult compile_and_link(
        const std::vector<std::filesystem::path>& source_files,
        const std::filesystem::path& output_binary,
        const CompileOptions& compile_opts = {},
        const LinkOptions& link_opts = {}) const;

private:
    ToolchainInfo toolchain_{};
};

} // namespace keety::driver
