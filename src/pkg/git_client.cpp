#include <keety/pkg/git_client.hpp>
#include <keety/pkg/diagnostics.hpp>
#include <keety/pkg/process.hpp>

#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace keety::pkg {

namespace {

std::string hashUrl(const std::string& url) {
    // 64-bit FNV-1a hash for deterministic mirror directory naming
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : url) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << hash;

    // Sanitize leading prefix for human readability
    std::string clean;
    for (char c : url) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            clean.push_back(c);
        } else {
            clean.push_back('_');
        }
        if (clean.size() >= 24) break;
    }
    return clean + "_" + ss.str() + ".git";
}

std::filesystem::path defaultKatteryHome() {
    const char* env = std::getenv("KATTERY_HOME");
    if (env && env[0] != '\0') {
        return std::filesystem::path(env);
    }
#if defined(_WIN32)
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile && userProfile[0] != '\0') {
        return std::filesystem::path(userProfile) / ".kattery";
    }
#else
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::filesystem::path(home) / ".kattery";
    }
#endif
    return std::filesystem::temp_directory_path() / ".kattery";
}

std::string trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.remove_suffix(1);
    }
    return std::string(s);
}

} // namespace

GitClient::GitClient(std::optional<std::filesystem::path> customHome) {
    katteryHome_ = customHome.has_value() ? *customHome : defaultKatteryHome();
    gitCacheDir_ = katteryHome_ / "cache" / "git";
}

bool GitClient::isAvailable() const {
    if (!isGitAvailable_.has_value()) {
        auto exe = ProcessRunner::findExecutable("git");
        if (!exe.has_value()) {
            isGitAvailable_ = false;
        } else {
            auto res = ProcessRunner::run({"git", "--version"});
            isGitAvailable_ = res.success();
        }
    }
    return *isGitAvailable_;
}

void GitClient::requireGit() const {
    if (!isAvailable()) {
        throw PkgError(
            ErrorCode::GitNotFound,
            "git executable not found on PATH",
            "Install git or ensure it is accessible in your PATH environment variable"
        );
    }
}

std::filesystem::path GitClient::getMirrorPath(const std::string& url) const {
    return gitCacheDir_ / hashUrl(url);
}

void GitClient::ensureMirror(const std::string& url) {
    requireGit();

    std::filesystem::path mirror = getMirrorPath(url);
    std::filesystem::create_directories(mirror.parent_path());

    ProcessOptions opt;
    opt.env["GIT_TERMINAL_PROMPT"] = "0";

    std::error_code ec;
    if (std::filesystem::exists(mirror / "HEAD", ec)) {
        // Fetch updates into existing mirror
        opt.argv = {"git", "-C", mirror.string(), "fetch", "--prune", "--tags", "origin"};
        auto res = ProcessRunner::run(opt);
        if (!res.success()) {
            // Non-fatal if offline and mirror already has commits, but if empty, throw
        }
    } else {
        // Clone new bare mirror
        opt.argv = {"git", "clone", "--mirror", url, mirror.string()};
        auto res = ProcessRunner::run(opt);
        if (!res.success()) {
            std::filesystem::remove_all(mirror, ec);
            throw PkgError(
                ErrorCode::GitCloneFailed,
                "failed to clone git repository '" + url + "': " + trim(res.stderrText),
                "Check the repository URL, access permissions, and network connectivity"
            );
        }
    }
}

std::string GitClient::resolveRef(const std::string& url, const std::string& ref) {
    ensureMirror(url);

    std::filesystem::path mirror = getMirrorPath(url);

    auto tryRevParse = [&](const std::string& target) -> std::optional<std::string> {
        ProcessOptions opt;
        opt.env["GIT_TERMINAL_PROMPT"] = "0";
        opt.argv = {"git", "-C", mirror.string(), "rev-parse", "--verify", target};
        auto res = ProcessRunner::run(opt);
        if (res.success()) {
            std::string sha = trim(res.stdoutText);
            if (sha.size() == 40) {
                return sha;
            }
        }
        return std::nullopt;
    };

    if (ref.empty()) {
        if (auto sha = tryRevParse("HEAD^{commit}")) {
            return *sha;
        }
    } else {
        // Try exact commit/tag/branch
        if (auto sha = tryRevParse(ref + "^{commit}")) {
            return *sha;
        }
        if (auto sha = tryRevParse("refs/tags/" + ref + "^{commit}")) {
            return *sha;
        }
        if (auto sha = tryRevParse("refs/heads/" + ref + "^{commit}")) {
            return *sha;
        }
        if (auto sha = tryRevParse("refs/remotes/origin/" + ref + "^{commit}")) {
            return *sha;
        }
    }

    throw PkgError(
        ErrorCode::GitRefNotFound,
        "failed to resolve ref '" + (ref.empty() ? "HEAD" : ref) + "' in repository '" + url + "'",
        "Verify that the tag, branch, or commit SHA exists in the remote repository"
    );
}

std::string GitClient::treeHash(const std::string& url, const std::string& commitSha) {
    ensureMirror(url);

    std::filesystem::path mirror = getMirrorPath(url);
    ProcessOptions opt;
    opt.env["GIT_TERMINAL_PROMPT"] = "0";
    opt.argv = {"git", "-C", mirror.string(), "rev-parse", commitSha + "^{tree}"};
    auto res = ProcessRunner::run(opt);
    if (!res.success()) {
        throw PkgError(
            ErrorCode::GitRefNotFound,
            "failed to compute tree hash for commit '" + commitSha + "'"
        );
    }
    std::string th = trim(res.stdoutText);
    if (th.size() != 40) {
        throw PkgError(
            ErrorCode::GitRefNotFound,
            "invalid tree hash returned by git: '" + th + "'"
        );
    }
    return th;
}

void GitClient::checkout(
    const std::string& url,
    const std::string& commitSha,
    const std::filesystem::path& destDir
) {
    ensureMirror(url);

    std::filesystem::path mirror = getMirrorPath(url);
    std::error_code ec;
    std::filesystem::create_directories(destDir.parent_path(), ec);

    // Clone without checkout from local mirror
    ProcessOptions cloneOpt;
    cloneOpt.env["GIT_TERMINAL_PROMPT"] = "0";
    cloneOpt.argv = {"git", "clone", "--no-checkout", mirror.string(), destDir.string()};
    auto cloneRes = ProcessRunner::run(cloneOpt);
    if (!cloneRes.success()) {
        std::filesystem::remove_all(destDir, ec);
        throw PkgError(
            ErrorCode::GitCheckoutFailed,
            "failed to clone from local mirror to '" + destDir.string() + "': " + trim(cloneRes.stderrText)
        );
    }

    // Checkout specific commit
    ProcessOptions checkoutOpt;
    checkoutOpt.env["GIT_TERMINAL_PROMPT"] = "0";
    checkoutOpt.argv = {"git", "-C", destDir.string(), "checkout", "--detach", commitSha};
    auto checkoutRes = ProcessRunner::run(checkoutOpt);
    if (!checkoutRes.success()) {
        std::filesystem::remove_all(destDir, ec);
        throw PkgError(
            ErrorCode::GitCheckoutFailed,
            "failed to checkout commit '" + commitSha + "': " + trim(checkoutRes.stderrText)
        );
    }

    // Remove .git directory so the package is a pure source directory
    std::filesystem::remove_all(destDir / ".git", ec);
}

} // namespace keety::pkg
