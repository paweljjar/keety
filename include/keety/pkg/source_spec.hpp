#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace keety::pkg {

/// Kind of dependency source.
enum class SourceKind {
    Git,  // Git repository clone (URL + optional ref)
    Path, // Local directory reference (relative or absolute)
};

/// Validates that a package name:
///   - is 1..64 characters long,
///   - matches `^[a-z][a-z0-9_-]*$`,
///   - does not collide with a built-in KeetScript stdlib module (e.g. `io`, `fs`, `sys`, `json`, `net`).
/// Throws `PkgError(ErrorCode::InvalidPackageName)` on failure when `context` is provided.
[[nodiscard]] bool isValidPackageName(std::string_view name) noexcept;
void validatePackageName(std::string_view name, std::string_view context = {});

/// Validates a semver version string `MAJOR.MINOR.PATCH` with non-negative numeric components.
[[nodiscard]] bool isValidSemver(std::string_view version) noexcept;
void validateSemver(std::string_view version, std::string_view context = {});

/// Validates a minimum compiler version constraint `^>=[0-9]+\.[0-9]+\.[0-9]+$`.
[[nodiscard]] bool isValidKeetyConstraint(std::string_view constraint) noexcept;
void validateKeetyConstraint(std::string_view constraint, std::string_view context = {});

/// Parsed dependency source specification.
///
/// Supported textual forms:
///   - `git+https://github.com/u/repo.git#v1.0.0`
///   - `git+ssh://git@github.com/u/repo.git`
///   - `git+git@github.com:u/repo.git#main`
///   - `git+file:///path/to/repo#commit`
///   - `https://...` or `git@...` (normalized to `git+...` when parsed)
///   - `path:../sibling`
///   - `path:/absolute/path`
struct SourceSpec {
    SourceKind kind{SourceKind::Git};

    // For SourceKind::Git:
    std::string url;               // Bare repository URL without `git+` prefix and without `#ref`
    std::string ref;               // Optional tag, branch or commit SHA (empty means default branch)

    // For SourceKind::Path:
    std::string path;              // Stored path as written (normalized to forward slashes)

    /// Parses a source specification string. Throws `PkgError(ErrorCode::InvalidSourceSpec)` on malformed input.
    [[nodiscard]] static SourceSpec parse(std::string_view raw, std::string_view context = {});

    /// Converts back to canonical textual format:
    ///   - `git+<url>` or `git+<url>#<ref>`
    ///   - `path:<path>`
    [[nodiscard]] std::string toString() const;

    [[nodiscard]] bool isGit() const noexcept { return kind == SourceKind::Git; }
    [[nodiscard]] bool isPath() const noexcept { return kind == SourceKind::Path; }

    bool operator==(const SourceSpec& other) const = default;
};

} // namespace keety::pkg
