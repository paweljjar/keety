#pragma once

#include <keety/pkg/manifest.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keety::pkg {

/// A member package inside a monorepo workspace.
struct WorkspaceMember {
    std::string name;
    std::filesystem::path relativePath; // Relative to workspace root, using '/'
    std::filesystem::path directory;    // Absolute path on disk
    Manifest manifest;
};

/// Represents a loaded workspace consisting of a root project and zero or more member packages.
struct Workspace {
    std::filesystem::path rootDir;
    Manifest rootManifest;
    std::vector<WorkspaceMember> members; // Sorted lexicographically by member name

    /// Finds the workspace root directory starting at `startDir` and walking upward.
    /// Returns empty optional if no `pedigree.json` is found in the ancestor tree.
    [[nodiscard]] static std::optional<std::filesystem::path> findRootDirectory(
        const std::filesystem::path& startDir
    );

    /// Discovers and loads the workspace containing `startDir`.
    /// Throws `PkgError(ErrorCode::ManifestNotFound)` if no manifest is found.
    /// Validates all member manifests, glob expansions, and duplicate member names.
    [[nodiscard]] static Workspace discover(const std::filesystem::path& startDir);

    /// Finds a member by package name. Returns nullptr if not found.
    [[nodiscard]] const WorkspaceMember* findMember(std::string_view name) const noexcept;

    /// Finds a member whose directory contains the given path.
    [[nodiscard]] const WorkspaceMember* findMemberByPath(
        const std::filesystem::path& path
    ) const noexcept;

    /// Returns true if this workspace declares at least one member.
    [[nodiscard]] bool hasMembers() const noexcept { return !members.empty(); }
};

} // namespace keety::pkg
