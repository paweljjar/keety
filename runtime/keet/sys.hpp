#pragma once

#include <keet/bool_byte.hpp>
#include <keet/error.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <variant>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#include <windows.h>
#define KEET_POPEN _popen
#define KEET_PCLOSE _pclose
#define KEET_GETPID _getpid
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define KEET_POPEN popen
#define KEET_PCLOSE pclose
#define KEET_GETPID getpid
#endif

namespace keet::sys {

namespace detail {
inline std::vector<std::string>& process_args_ref() noexcept {
    static std::vector<std::string> args;
    return args;
}
} // namespace detail

/// Initializes process command-line arguments. Called at main entry.
inline void initArgs(int argc, char* const argv[]) {
    auto& args = detail::process_args_ref();
    args.clear();
    if (argc > 0 && argv != nullptr) {
        args.reserve(static_cast<size_t>(argc));
        for (int i = 0; i < argc; ++i) {
            if (argv[i] != nullptr) {
                args.emplace_back(argv[i]);
            }
        }
    }
}

/// Returns the command-line arguments passed to the running process.
[[nodiscard]] inline std::vector<std::string> args() {
    return detail::process_args_ref();
}

/// Terminates the process immediately with the specified exit code.
[[noreturn]] inline void exit(int64_t code) {
    std::exit(static_cast<int>(code));
}

/// Returns the unique process ID (PID) of the current host process.
[[nodiscard]] inline int64_t pid() noexcept {
    return static_cast<int64_t>(KEET_GETPID());
}

/// Returns the value of an environment variable, or std::monostate{} (null) if unset.
[[nodiscard]] inline std::variant<std::string, std::monostate> getEnv(const std::string& key) {
    const char* val = std::getenv(key.c_str());
    if (val != nullptr) {
        return std::string(val);
    }
    return std::monostate{};
}

/// Returns the value of an environment variable, or `fallback` if unset.
[[nodiscard]] inline std::string getEnvOr(const std::string& key, const std::string& fallback) {
    const char* val = std::getenv(key.c_str());
    if (val != nullptr) {
        return std::string(val);
    }
    return fallback;
}

/// Sets or updates an environment variable for the current process. Throws SystemError on failure.
inline bool setEnv(const std::string& key, const std::string& value) {
#if defined(_WIN32)
    int res = _putenv_s(key.c_str(), value.c_str());
    if (res != 0) {
        throw keet::error("SystemError", "Failed to set environment variable '" + key + "'");
    }
#else
    int res = ::setenv(key.c_str(), value.c_str(), 1);
    if (res != 0) {
        throw keet::error("SystemError", "Failed to set environment variable '" + key + "'");
    }
#endif
    return true;
}

/// Removes an environment variable from the current process. Throws SystemError on failure.
inline bool unsetEnv(const std::string& key) {
#if defined(_WIN32)
    int res = _putenv_s(key.c_str(), "");
    if (res != 0) {
        throw keet::error("SystemError", "Failed to unset environment variable '" + key + "'");
    }
#else
    int res = ::unsetenv(key.c_str());
    if (res != 0) {
        throw keet::error("SystemError", "Failed to unset environment variable '" + key + "'");
    }
#endif
    return true;
}

/// Checks whether an environment variable is defined in the current process.
[[nodiscard]] inline bool hasEnv(const std::string& key) noexcept {
    return std::getenv(key.c_str()) != nullptr;
}

/// Returns normalized host OS name: "macos", "linux", "windows", or "unknown".
[[nodiscard]] inline std::string platform() {
#if defined(__APPLE__) && defined(__MACH__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#elif defined(_WIN32)
    return "windows";
#else
    return "unknown";
#endif
}

/// Returns normalized host CPU architecture: "x86_64", "arm64", "x86", or "unknown".
[[nodiscard]] inline std::string arch() {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

/// Returns the number of logical processor threads available (minimum 1).
[[nodiscard]] inline int64_t cpuCount() noexcept {
    unsigned int count = std::thread::hardware_concurrency();
    return count > 0 ? static_cast<int64_t>(count) : 1;
}

/// Returns the normalized absolute path of the current working directory.
[[nodiscard]] inline std::string cwd() {
    std::error_code ec;
    auto p = std::filesystem::current_path(ec);
    if (ec) {
        return ".";
    }
    return p.generic_string();
}

/// Changes the current working directory. Throws SystemError on failure.
inline bool chdir(const std::string& path) {
    std::error_code ec;
    std::filesystem::current_path(path, ec);
    if (ec) {
        throw keet::error("SystemError", "chdir failed for '" + path + "': " + ec.message());
    }
    return true;
}

/// Returns the current user's home directory path, or "" if unavailable.
[[nodiscard]] inline std::string homeDir() {
#if defined(_WIN32)
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile != nullptr && userProfile[0] != '\0') {
        return std::filesystem::path(userProfile).generic_string();
    }
    const char* homeDrive = std::getenv("HOMEDRIVE");
    const char* homePath = std::getenv("HOMEPATH");
    if (homeDrive != nullptr && homePath != nullptr) {
        return std::filesystem::path(std::string(homeDrive) + homePath).generic_string();
    }
    return "";
#else
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return std::filesystem::path(home).generic_string();
    }
    return "";
#endif
}

/// Returns the normalized path to the operating system's temporary directory.
[[nodiscard]] inline std::string tempDir() {
    std::error_code ec;
    auto p = std::filesystem::temp_directory_path(ec);
    if (ec) {
        return "/tmp";
    }
    return p.generic_string();
}

/// Executes a shell command synchronously and returns the exit status code.
inline int64_t exec(const std::string& command) {
    int status = std::system(command.c_str());
    if (status == -1) {
        throw keet::error("SystemError", "Failed to invoke command: '" + command + "'");
    }
#if !defined(_WIN32)
    if (WIFEXITED(status)) {
        return static_cast<int64_t>(WEXITSTATUS(status));
    }
#endif
    return static_cast<int64_t>(status);
}

/// Executes a shell command and captures its standard output. Throws SystemError on failure or non-zero exit code.
inline std::string execOutput(const std::string& command) {
    FILE* fp = KEET_POPEN(command.c_str(), "r");
    if (!fp) {
        throw keet::error("SystemError", "Failed to start subprocess for command: '" + command + "'");
    }

    std::string result;
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), fp) != nullptr) {
        result += buffer;
    }

    int rawStatus = KEET_PCLOSE(fp);
    int exitCode = rawStatus;
#if !defined(_WIN32)
    if (WIFEXITED(rawStatus)) {
        exitCode = WEXITSTATUS(rawStatus);
    }
#endif

    if (exitCode != 0) {
        throw keet::error("SystemError", "Command exited with non-zero status " + std::to_string(exitCode) + ": '" + command + "'");
    }

    return result;
}

} // namespace keet::sys
