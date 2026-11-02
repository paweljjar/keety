#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keety::pkg {

/// Options for launching an external subprocess.
struct ProcessOptions {
    std::vector<std::string> argv;
    std::optional<std::filesystem::path> cwd;
    std::map<std::string, std::string> env; // Additional / overridden environment variables
    bool captureOutput{true};                // If false, stdout/stderr are inherited from parent
};

/// Result of executing a subprocess.
struct ProcessResult {
    int exitCode{-1};
    std::string stdoutText;
    std::string stderrText;

    [[nodiscard]] bool success() const noexcept { return exitCode == 0; }
};

/// Cross-platform process runner with argv semantics (no shell interpretation).
class ProcessRunner {
public:
    /// Executes a process synchronously and returns its exit code and captured output.
    [[nodiscard]] static ProcessResult run(const ProcessOptions& options);

    /// Convenience wrapper for argv list.
    [[nodiscard]] static ProcessResult run(
        std::vector<std::string> argv,
        std::optional<std::filesystem::path> cwd = std::nullopt,
        bool captureOutput = true
    );

    /// Searches PATH for an executable name.
    /// On Windows, checks extensions (.exe, .cmd, .bat).
    [[nodiscard]] static std::optional<std::filesystem::path> findExecutable(
        std::string_view name
    );
};

} // namespace keety::pkg
