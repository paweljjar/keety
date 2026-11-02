#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace keety::pkg {

/// Stable `kattery` diagnostic codes.
///   KAT1xxx - manifest / lockfile / source-spec validation
///   KAT2xxx - fetch / git
///   KAT3xxx - resolve / install / workspaces
///   KAT4xxx - scripts / CLI
enum class ErrorCode {
    // --- KAT1xxx: pedigree.json / pedigree.lock ---
    ManifestNotFound = 1001,       // File missing or unreadable
    InvalidJson = 1002,            // Malformed JSON document
    InvalidFieldType = 1003,       // Field has the wrong JSON type
    MissingField = 1004,           // Required field absent
    InvalidPackageName = 1005,     // Violates name rules or collides with stdlib
    InvalidVersion = 1006,         // Not MAJOR.MINOR.PATCH
    InvalidSourceSpec = 1007,      // Not `git+<url>[#ref]` or `path:<dir>`
    InvalidKeetyConstraint = 1008, // Not `>=X.Y.Z`
    InvalidWorkspaceEntry = 1009,  // Unsupported workspace path or glob
    InvalidLockfile = 1010,        // Malformed or inconsistent pedigree.lock
    UnsupportedLockVersion = 1011, // Unknown `lockVersion`
    SelfDependency = 1012,         // Package lists itself as a dependency

    // --- KAT2xxx: fetch / git ---
    GitNotFound = 2001,            // git executable missing from PATH
    GitCloneFailed = 2002,         // git clone or fetch failed
    GitRefNotFound = 2003,         // git ref could not be resolved
    GitCheckoutFailed = 2004,      // git checkout failed
    TreeHashMismatch = 2005,       // Tree hash does not match lockfile integrity
    LocalPathNotFound = 2006,      // Local path does not exist
    LocalLinkFailed = 2007,        // Symlink / junction / copy failed

    // --- KAT3xxx: resolve / install / workspaces ---
    DependencyCycle = 3001,          // Cyclic dependency detected (A -> B -> A)
    DependencyConflict = 3002,       // Two dependencies require different commits for same package
    LockfileMismatch = 3003,         // Lockfile stale under --frozen
    DuplicateWorkspaceMember = 3004, // Two members (or root + member) share a name
    WorkspaceMemberMissing = 3005,   // Listed member has no pedigree.json
    InstallationFailed = 3006,       // File I/O or staging failure during install
};

/// Returns the printable code, e.g. `KAT1005`.
[[nodiscard]] std::string codeString(ErrorCode code);

/// A single kattery diagnostic: stable code, message and optional hint.
struct Diagnostic {
    ErrorCode code{ErrorCode::InvalidJson};
    std::string message;
    std::string hint;

    /// Formats as `error[KAT1005]: <message>` plus an indented `hint:` line when present.
    [[nodiscard]] std::string format() const;
};

/// Exception thrown by kattery library code; carries a structured Diagnostic.
class PkgError : public std::runtime_error {
public:
    explicit PkgError(Diagnostic diagnostic);
    PkgError(ErrorCode code, std::string message, std::string hint = {});

    [[nodiscard]] const Diagnostic& diagnostic() const noexcept { return diagnostic_; }
    [[nodiscard]] ErrorCode code() const noexcept { return diagnostic_.code; }

private:
    Diagnostic diagnostic_;
};

} // namespace keety::pkg
