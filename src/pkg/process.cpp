#include <keety/pkg/process.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace keety::pkg {

#if defined(_WIN32)

namespace {

std::wstring toWide(std::string_view str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wstr.data(), sizeNeeded);
    return wstr;
}

std::string toUtf8(const wchar_t* wstr, int len = -1) {
    if (!wstr || len == 0) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr, len, nullptr, 0, nullptr, nullptr);
    std::string str(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, len, str.data(), sizeNeeded, nullptr, nullptr);
    if (len == -1 && !str.empty() && str.back() == '\0') {
        str.pop_back();
    }
    return str;
}

// Quotes an argument per Windows CommandLineToArgvW rules
std::wstring quoteArgWindows(std::string_view arg) {
    std::wstring warg = toWide(arg);
    if (!warg.empty() && warg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return warg;
    }

    std::wstring result = L"\"";
    for (size_t i = 0; i < warg.size(); ++i) {
        size_t backslashes = 0;
        while (i < warg.size() && warg[i] == L'\\') {
            ++backslashes;
            ++i;
        }

        if (i == warg.size()) {
            result.append(backslashes * 2, L'\\');
            break;
        } else if (warg[i] == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
        } else {
            result.append(backslashes, L'\\');
            result.push_back(warg[i]);
        }
    }
    result.push_back(L'"');
    return result;
}

} // namespace

ProcessResult ProcessRunner::run(const ProcessOptions& options) {
    ProcessResult result;
    if (options.argv.empty()) {
        result.exitCode = -1;
        result.stderrText = "empty argv list";
        return result;
    }

    std::wstring cmdLine;
    for (size_t i = 0; i < options.argv.size(); ++i) {
        if (i > 0) cmdLine += L" ";
        cmdLine += quoteArgWindows(options.argv[i]);
    }

    HANDLE outRead = nullptr, outWrite = nullptr;
    HANDLE errRead = nullptr, errWrite = nullptr;

    if (options.captureOutput) {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        if (!CreatePipe(&outRead, &outWrite, &sa, 0)) {
            result.exitCode = -1;
            result.stderrText = "failed to create stdout pipe";
            return result;
        }
        SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);

        if (!CreatePipe(&errRead, &errWrite, &sa, 0)) {
            CloseHandle(outRead);
            CloseHandle(outWrite);
            result.exitCode = -1;
            result.stderrText = "failed to create stderr pipe";
            return result;
        }
        SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    if (options.captureOutput) {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = outWrite;
        si.hStdError = errWrite;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring wCwd;
    const wchar_t* pCwd = nullptr;
    if (options.cwd.has_value()) {
        wCwd = toWide(options.cwd->string());
        pCwd = wCwd.c_str();
    }

    // Apply environment variables
    for (const auto& [k, v] : options.env) {
        SetEnvironmentVariableW(toWide(k).c_str(), toWide(v).c_str());
    }

    BOOL ok = CreateProcessW(
        nullptr,
        cmdLine.data(),
        nullptr,
        nullptr,
        options.captureOutput ? TRUE : FALSE,
        0,
        nullptr,
        pCwd,
        &si,
        &pi
    );

    if (options.captureOutput) {
        CloseHandle(outWrite);
        CloseHandle(errWrite);
    }

    if (!ok) {
        if (options.captureOutput) {
            CloseHandle(outRead);
            CloseHandle(errRead);
        }
        result.exitCode = 127;
        result.stderrText = "failed to spawn process: " + options.argv[0];
        return result;
    }

    if (options.captureOutput) {
        auto readPipe = [](HANDLE h) -> std::string {
            std::string out;
            char buf[4096];
            DWORD bytesRead = 0;
            while (ReadFile(h, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
                out.append(buf, bytesRead);
            }
            return out;
        };

        result.stdoutText = readPipe(outRead);
        result.stderrText = readPipe(errRead);
        CloseHandle(outRead);
        CloseHandle(errRead);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    result.exitCode = static_cast<int>(exitCode);
    return result;
}

std::optional<std::filesystem::path> ProcessRunner::findExecutable(std::string_view name) {
    std::filesystem::path candidate(name);
    if (candidate.has_parent_path()) {
        if (std::filesystem::exists(candidate)) return candidate;
        return std::nullopt;
    }

    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return std::nullopt;

    std::vector<std::string> exts = {"", ".exe", ".cmd", ".bat"};
    std::stringstream ss(pathEnv);
    std::string item;
    while (std::getline(ss, item, ';')) {
        for (const auto& ext : exts) {
            std::filesystem::path full = std::filesystem::path(item) / (std::string(name) + ext);
            std::error_code ec;
            if (std::filesystem::is_regular_file(full, ec)) {
                return full;
            }
        }
    }
    return std::nullopt;
}

#else // POSIX (macOS, Linux)

ProcessResult ProcessRunner::run(const ProcessOptions& options) {
    ProcessResult result;
    if (options.argv.empty()) {
        result.exitCode = -1;
        result.stderrText = "empty argv list";
        return result;
    }

    int outPipe[2] = {-1, -1};
    int errPipe[2] = {-1, -1};

    if (options.captureOutput) {
        if (pipe(outPipe) != 0 || pipe(errPipe) != 0) {
            result.exitCode = -1;
            result.stderrText = "failed to create pipes";
            return result;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        if (options.captureOutput) {
            close(outPipe[0]); close(outPipe[1]);
            close(errPipe[0]); close(errPipe[1]);
        }
        result.exitCode = -1;
        result.stderrText = "failed to fork";
        return result;
    }

    if (pid == 0) {
        // Child process
        if (options.cwd.has_value()) {
            if (chdir(options.cwd->c_str()) != 0) {
                _exit(127);
            }
        }

        if (options.captureOutput) {
            close(outPipe[0]);
            dup2(outPipe[1], STDOUT_FILENO);
            close(outPipe[1]);

            close(errPipe[0]);
            dup2(errPipe[1], STDERR_FILENO);
            close(errPipe[1]);
        }

        for (const auto& [k, v] : options.env) {
            setenv(k.c_str(), v.c_str(), 1);
        }

        std::vector<char*> c_argv;
        c_argv.reserve(options.argv.size() + 1);
        for (const auto& a : options.argv) {
            c_argv.push_back(const_cast<char*>(a.c_str()));
        }
        c_argv.push_back(nullptr);

        execvp(c_argv[0], c_argv.data());
        _exit(127);
    }

    // Parent process
    if (options.captureOutput) {
        close(outPipe[1]);
        close(errPipe[1]);

        auto readAll = [](int fd) -> std::string {
            std::string out;
            char buf[4096];
            ssize_t n = 0;
            while ((n = read(fd, buf, sizeof(buf))) > 0) {
                out.append(buf, static_cast<size_t>(n));
            }
            return out;
        };

        result.stdoutText = readAll(outPipe[0]);
        result.stderrText = readAll(errPipe[0]);

        close(outPipe[0]);
        close(errPipe[0]);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            result.exitCode = -1;
            return result;
        }
    }

    if (WIFEXITED(status)) {
        result.exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exitCode = 128 + WTERMSIG(status);
    } else {
        result.exitCode = -1;
    }

    return result;
}

std::optional<std::filesystem::path> ProcessRunner::findExecutable(std::string_view name) {
    std::filesystem::path candidate(name);
    if (candidate.has_parent_path()) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec)) return candidate;
        return std::nullopt;
    }

    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return std::nullopt;

    std::stringstream ss(pathEnv);
    std::string item;
    while (std::getline(ss, item, ':')) {
        std::filesystem::path full = std::filesystem::path(item) / name;
        std::error_code ec;
        if (std::filesystem::is_regular_file(full, ec) && access(full.c_str(), X_OK) == 0) {
            return full;
        }
    }
    return std::nullopt;
}

#endif

ProcessResult ProcessRunner::run(
    std::vector<std::string> argv,
    std::optional<std::filesystem::path> cwd,
    bool captureOutput
) {
    ProcessOptions opt;
    opt.argv = std::move(argv);
    opt.cwd = std::move(cwd);
    opt.captureOutput = captureOutput;
    return run(opt);
}

} // namespace keety::pkg
