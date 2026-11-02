#include <keety/pkg/source_spec.hpp>
#include <keety/pkg/diagnostics.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <unordered_set>

namespace keety::pkg {

namespace {

bool isReservedStdlibName(std::string_view name) noexcept {
    static const std::unordered_set<std::string_view> stdlibModules = {
        "io", "fs", "sys", "math", "time", "json", "net",
        "crypto", "encoding", "test", "os"
    };
    if (stdlibModules.find(name) != stdlibModules.end()) {
        return true;
    }
    if (name.rfind("std", 0) == 0 || name.rfind("keet", 0) == 0) {
        return true;
    }
    return false;
}

} // namespace

bool isValidPackageName(std::string_view name) noexcept {
    if (name.empty() || name.size() > 64) {
        return false;
    }
    // Must start with lowercase ASCII letter
    char first = name.front();
    if (first < 'a' || first > 'z') {
        return false;
    }
    for (char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            return false;
        }
    }
    // Must not collide with standard library modules
    if (isReservedStdlibName(name)) {
        return false;
    }
    return true;
}

void validatePackageName(std::string_view name, std::string_view context) {
    if (!isValidPackageName(name)) {
        std::string ctx = context.empty() ? "" : std::string(context) + ": ";
        if (isReservedStdlibName(name)) {
            throw PkgError(
                ErrorCode::InvalidPackageName,
                ctx + "package name '" + std::string(name) + "' collides with a KeetScript standard library module",
                "Choose a package name that does not collide with built-in modules (e.g., 'io', 'net', 'json')"
            );
        }
        throw PkgError(
            ErrorCode::InvalidPackageName,
            ctx + "invalid package name '" + std::string(name) + "'",
            "Package names must match '^[a-z][a-z0-9_-]{0,63}$'"
        );
    }
}

static bool parseNonNegativeInt(std::string_view s) noexcept {
    if (s.empty()) return false;
    // Disallow leading zeros for multi-digit numbers (strict semver)
    if (s.size() > 1 && s[0] == '0') return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

bool isValidSemver(std::string_view version) noexcept {
    // Expect MAJOR.MINOR.PATCH (prerelease/build metadata omitted for v1)
    size_t firstDot = version.find('.');
    if (firstDot == std::string_view::npos || firstDot == 0) return false;
    size_t secondDot = version.find('.', firstDot + 1);
    if (secondDot == std::string_view::npos || secondDot == firstDot + 1) return false;
    if (secondDot == version.size() - 1) return false;

    // Check no further dots
    if (version.find('.', secondDot + 1) != std::string_view::npos) return false;

    std::string_view major = version.substr(0, firstDot);
    std::string_view minor = version.substr(firstDot + 1, secondDot - firstDot - 1);
    std::string_view patch = version.substr(secondDot + 1);

    return parseNonNegativeInt(major) && parseNonNegativeInt(minor) && parseNonNegativeInt(patch);
}

void validateSemver(std::string_view version, std::string_view context) {
    if (!isValidSemver(version)) {
        std::string ctx = context.empty() ? "" : std::string(context) + ": ";
        throw PkgError(
            ErrorCode::InvalidVersion,
            ctx + "invalid semver version '" + std::string(version) + "'",
            "Version must follow 'MAJOR.MINOR.PATCH' (e.g., '1.0.0')"
        );
    }
}

bool isValidKeetyConstraint(std::string_view constraint) noexcept {
    if (constraint.rfind(">=", 0) != 0) {
        return false;
    }
    return isValidSemver(constraint.substr(2));
}

void validateKeetyConstraint(std::string_view constraint, std::string_view context) {
    if (!isValidKeetyConstraint(constraint)) {
        std::string ctx = context.empty() ? "" : std::string(context) + ": ";
        throw PkgError(
            ErrorCode::InvalidKeetyConstraint,
            ctx + "invalid keety constraint '" + std::string(constraint) + "'",
            "The 'keety' field must use '>=X.Y.Z' (e.g., '>=0.1.0')"
        );
    }
}

SourceSpec SourceSpec::parse(std::string_view raw, std::string_view context) {
    std::string ctx = context.empty() ? "" : std::string(context) + ": ";

    if (raw.empty()) {
        throw PkgError(ErrorCode::InvalidSourceSpec, ctx + "source specification cannot be empty");
    }

    // Path kind: starts with "path:"
    if (raw.rfind("path:", 0) == 0) {
        std::string_view target = raw.substr(5);
        if (target.empty()) {
            throw PkgError(ErrorCode::InvalidSourceSpec, ctx + "empty path in 'path:' specification");
        }
        std::string normalized(target);
        // Normalize backslashes to forward slashes for cross-platform portability
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        SourceSpec spec;
        spec.kind = SourceKind::Path;
        spec.path = std::move(normalized);
        return spec;
    }

    // Git kind: either explicit "git+" prefix or infer from URL scheme
    std::string_view gitPayload = raw;
    bool explicitGit = false;
    if (raw.rfind("git+", 0) == 0) {
        gitPayload = raw.substr(4);
        explicitGit = true;
    } else {
        // Accept bare git URLs like https://... or git@...
        bool isHttps = raw.rfind("https://", 0) == 0 || raw.rfind("http://", 0) == 0;
        bool isSsh = raw.rfind("ssh://", 0) == 0 || raw.rfind("git@", 0) == 0;
        bool isFile = raw.rfind("file://", 0) == 0;
        if (!isHttps && !isSsh && !isFile) {
            throw PkgError(
                ErrorCode::InvalidSourceSpec,
                ctx + "unrecognized source spec '" + std::string(raw) + "'",
                "Use 'git+https://...#ref' or 'path:./local-dir'"
            );
        }
    }

    if (gitPayload.empty()) {
        throw PkgError(ErrorCode::InvalidSourceSpec, ctx + "empty URL after 'git+' prefix");
    }

    // Split #ref if present
    size_t hashPos = gitPayload.find('#');
    std::string_view urlPart = gitPayload.substr(0, hashPos);
    std::string_view refPart = (hashPos == std::string_view::npos) ? "" : gitPayload.substr(hashPos + 1);

    if (urlPart.empty()) {
        throw PkgError(ErrorCode::InvalidSourceSpec, ctx + "empty URL in git specification");
    }
    if (hashPos != std::string_view::npos && refPart.empty()) {
        throw PkgError(
            ErrorCode::InvalidSourceSpec,
            ctx + "empty ref after '#' in git specification '" + std::string(raw) + "'",
            "Omit '#' entirely to track the default branch, or specify a tag/branch/commit"
        );
    }

    SourceSpec spec;
    spec.kind = SourceKind::Git;
    spec.url = std::string(urlPart);
    if (!refPart.empty()) {
        spec.ref = std::string(refPart);
    }
    (void)explicitGit;
    return spec;
}

std::string SourceSpec::toString() const {
    if (kind == SourceKind::Path) {
        return "path:" + path;
    }
    std::string out = "git+" + url;
    if (!ref.empty()) {
        out.push_back('#');
        out += ref;
    }
    return out;
}

} // namespace keety::pkg
