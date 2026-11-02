#include <keety/pkg/script_runner.hpp>
#include <keety/version.hpp>

#include <cstdlib>
#include <system_error>
#include <tuple>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace keety::pkg {

namespace {

std::filesystem::path getSelfExecutablePath() {
    std::error_code ec;
#if defined(__APPLE__)
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return std::filesystem::weakly_canonical(path, ec);
    }
    std::vector<char> dynamic_path(size);
    if (_NSGetExecutablePath(dynamic_path.data(), &size) == 0) {
        return std::filesystem::weakly_canonical(dynamic_path.data(), ec);
    }
#elif defined(_WIN32)
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len > 0) {
        return std::filesystem::weakly_canonical(std::filesystem::path(path), ec);
    }
#elif defined(__linux__)
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        return std::filesystem::weakly_canonical(path, ec);
    }
#endif
    return {};
}

#if defined(_WIN32)
std::string quoteWindowsArg(std::string_view arg) {
    if (arg.empty()) return "\"\"";
    if (arg.find_first_of(" \t\n\v\"") == std::string_view::npos) {
        return std::string(arg);
    }
    std::string result = "\"";
    for (char c : arg) {
        if (c == '"') result += "\\\"";
        else result += c;
    }
    result += "\"";
    return result;
}
#else
std::string quotePosixArg(std::string_view arg) {
    if (arg.empty()) return "''";
    if (arg.find_first_of(" \t\n\r\"'$&*?()[]{}|<>~;^\\") == std::string_view::npos) {
        return std::string(arg);
    }
    std::string result = "'";
    for (char c : arg) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result.push_back(c);
        }
    }
    result += "'";
    return result;
}
#endif

std::optional<std::tuple<int, int, int>> parseSemverComponents(std::string_view s) {
    size_t d1 = s.find('.');
    if (d1 == std::string_view::npos) return std::nullopt;
    size_t d2 = s.find('.', d1 + 1);
    if (d2 == std::string_view::npos) return std::nullopt;
    try {
        int maj = std::stoi(std::string(s.substr(0, d1)));
        int min = std::stoi(std::string(s.substr(d1 + 1, d2 - d1 - 1)));
        int pat = std::stoi(std::string(s.substr(d2 + 1)));
        return std::make_tuple(maj, min, pat);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

std::optional<std::filesystem::path> ScriptRunner::locateKeetyExecutable() {
    std::filesystem::path selfExe = getSelfExecutablePath();
    std::error_code ec;

    if (!selfExe.empty() && selfExe.has_parent_path()) {
        std::filesystem::path parentDir = selfExe.parent_path();
#if defined(_WIN32)
        std::filesystem::path cand = parentDir / "keety.exe";
#else
        std::filesystem::path cand = parentDir / "keety";
#endif
        if (std::filesystem::is_regular_file(cand, ec)) {
            return std::filesystem::weakly_canonical(cand, ec);
        }
    }

    // Fall back to PATH search
    return ProcessRunner::findExecutable("keety");
}

void ScriptRunner::injectKeetyPath(
    std::map<std::string, std::string>& env,
    const std::optional<std::filesystem::path>& keetyDir
) {
    std::optional<std::filesystem::path> dir = keetyDir;
    if (!dir.has_value()) {
        auto exe = locateKeetyExecutable();
        if (exe.has_value() && exe->has_parent_path()) {
            dir = exe->parent_path();
        }
    }
    if (!dir.has_value()) return;

    std::string dirStr = dir->string();
    const char* curPath = std::getenv("PATH");
#if defined(_WIN32)
    char delim = ';';
#else
    char delim = ':';
#endif

    std::string newPath;
    if (auto it = env.find("PATH"); it != env.end() && !it->second.empty()) {
        newPath = dirStr + delim + it->second;
    } else if (curPath && *curPath) {
        newPath = dirStr + delim + std::string(curPath);
    } else {
        newPath = dirStr;
    }

    env["PATH"] = newPath;
#if defined(_WIN32)
    env["Path"] = newPath;
#endif
}

std::string ScriptRunner::buildShellCommandLine(
    std::string_view command,
    const std::vector<std::string>& forwardedArgs
) {
    if (forwardedArgs.empty()) {
        return std::string(command);
    }

    std::string full(command);
    for (const auto& a : forwardedArgs) {
        full.push_back(' ');
#if defined(_WIN32)
        full += quoteWindowsArg(a);
#else
        full += quotePosixArg(a);
#endif
    }
    return full;
}

ScriptRunResult ScriptRunner::executeCommand(const ScriptRunOptions& options) {
    std::string cmdLine = buildShellCommandLine(options.scriptCommand, options.forwardedArgs);
    ProcessOptions popt;
#if defined(_WIN32)
    popt.argv = {"cmd.exe", "/C", cmdLine};
#else
    popt.argv = {"/bin/sh", "-c", cmdLine};
#endif
    if (!options.workingDir.empty()) {
        popt.cwd = options.workingDir;
    }
    popt.captureOutput = options.captureOutput;
    injectKeetyPath(popt.env, options.keetyDir);

    ProcessResult pres = ProcessRunner::run(popt);
    ScriptRunResult sres;
    sres.exitCode = pres.exitCode;
    sres.stdoutText = std::move(pres.stdoutText);
    sres.stderrText = std::move(pres.stderrText);
    return sres;
}

int ScriptRunner::runScript(
    const Manifest& manifest,
    const std::filesystem::path& manifestDir,
    const std::string& scriptName,
    const std::vector<std::string>& forwardedArgs,
    bool captureOutput,
    std::ostream& out,
    std::ostream& err
) {
    auto it = manifest.scripts.find(scriptName);
    if (it == manifest.scripts.end()) {
        err << "error: script '" << scriptName << "' not found in pedigree.json\n";
        return 127;
    }

    checkKeetyVersionConstraint(manifest, err);

    ScriptRunOptions opts;
    opts.workingDir = manifestDir;
    opts.scriptCommand = it->second;
    opts.forwardedArgs = forwardedArgs;
    opts.captureOutput = captureOutput;

    ScriptRunResult res = executeCommand(opts);
    if (captureOutput) {
        out << res.stdoutText;
        err << res.stderrText;
    }
    return res.exitCode;
}

void ScriptRunner::checkKeetyVersionConstraint(
    const Manifest& manifest,
    std::ostream& err
) {
    if (manifest.keety.empty()) return;
    std::string_view c = manifest.keety;
    if (c.rfind(">=", 0) != 0) return;
    std::string_view req = c.substr(2);

    auto reqVer = parseSemverComponents(req);
    auto curVer = parseSemverComponents(keety::getVersionString());
    if (reqVer.has_value() && curVer.has_value()) {
        if (*curVer < *reqVer) {
            err << "warning: package '" << manifest.name << "' requires keety "
                << manifest.keety << ", but installed keety is "
                << keety::getVersionString() << "\n";
        }
    }
}

} // namespace keety::pkg
