#pragma once

#include <keety/pkg/manifest.hpp>
#include <keety/pkg/process.hpp>

#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keety::pkg {

/// Options for running a shell script or command line.
struct ScriptRunOptions {
    std::filesystem::path workingDir;
    std::string scriptCommand;
    std::vector<std::string> forwardedArgs;
    std::optional<std::filesystem::path> keetyDir;
    bool captureOutput{false};
};

/// Result of executing a script command.
struct ScriptRunResult {
    int exitCode{0};
    std::string stdoutText;
    std::string stderrText;

    [[nodiscard]] bool success() const noexcept { return exitCode == 0; }
};

/// Executes scripts and CLI commands (run, build, test) with PATH injection.
class ScriptRunner {
public:
    /// Locates the `keety` compiler binary:
    /// 1. Next to the running `kattery` binary (same directory).
    /// 2. In PATH via ProcessRunner::findExecutable.
    [[nodiscard]] static std::optional<std::filesystem::path> locateKeetyExecutable();

    /// Injects the directory containing `keety` into PATH in the provided env map.
    static void injectKeetyPath(
        std::map<std::string, std::string>& env,
        const std::optional<std::filesystem::path>& keetyDir = std::nullopt
    );

    /// Formats a shell command line appending forwarded arguments.
    [[nodiscard]] static std::string buildShellCommandLine(
        std::string_view command,
        const std::vector<std::string>& forwardedArgs
    );

    /// Executes a shell command line in the given directory with PATH injected.
    [[nodiscard]] static ScriptRunResult executeCommand(
        const ScriptRunOptions& options
    );

    /// Runs a named script from a manifest.
    /// Returns the script exit code, or 127 if script is not found.
    [[nodiscard]] static int runScript(
        const Manifest& manifest,
        const std::filesystem::path& manifestDir,
        const std::string& scriptName,
        const std::vector<std::string>& forwardedArgs = {},
        bool captureOutput = false,
        std::ostream& out = std::cout,
        std::ostream& err = std::cerr
    );

    /// Verifies manifest.keety version constraint against the compiler version.
    /// Emits a warning to `err` if the installed keety is older than required.
    static void checkKeetyVersionConstraint(
        const Manifest& manifest,
        std::ostream& err = std::cerr
    );
};

} // namespace keety::pkg
