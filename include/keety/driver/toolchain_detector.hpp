#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keety::driver {

/// Underlying native C++ compiler family
enum class CompilerKind {
    Clang,   ///< Clang / Apple Clang (`clang++`)
    GCC,     ///< GNU Compiler Collection (`g++`)
    MSVC,    ///< Microsoft Visual C++ (`cl.exe`)
    Unknown  ///< Unrecognized or unsupported toolchain
};

[[nodiscard]] std::string_view to_string(CompilerKind kind) noexcept;

/// Metadata describing a detected platform C++ toolchain
struct ToolchainInfo {
    CompilerKind kind{CompilerKind::Unknown};
    std::filesystem::path compiler_path{};
    std::string version{};
    int major_version{0};
    int minor_version{0};
    int patch_version{0};
    std::string target_triple{};
    bool supports_cpp17{false};

    [[nodiscard]] bool is_valid() const noexcept {
        return kind != CompilerKind::Unknown && !compiler_path.empty();
    }
};

/// Service for locating and identifying platform C++ compilers
class ToolchainDetector {
public:
    /// Detects the default platform C++ compiler by inspecting PATH and standard install locations
    [[nodiscard]] static std::optional<ToolchainInfo> detect_default();

    /// Probes a specific compiler command or path (e.g. "clang++", "/usr/bin/g++", "cl.exe")
    [[nodiscard]] static std::optional<ToolchainInfo> probe(const std::filesystem::path& compiler_path_or_cmd);

    /// Classifies compiler kind from executable filename
    [[nodiscard]] static CompilerKind classify_from_path(const std::filesystem::path& path);

    /// Classifies compiler kind from --version or -dumpversion output banner
    [[nodiscard]] static CompilerKind classify_from_banner(std::string_view banner);

    /// Parses version components from compiler version output
    [[nodiscard]] static bool parse_version_string(std::string_view version_str,
                                                   int& major, int& minor, int& patch);

    /// Finds an executable in system PATH
    [[nodiscard]] static std::optional<std::filesystem::path> find_in_path(std::string_view command_name);

    /// Returns default candidate executable names for the current platform
    [[nodiscard]] static std::vector<std::string> get_candidate_names();
};

} // namespace keety::driver
