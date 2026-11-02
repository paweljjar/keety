#pragma once

#include <filesystem>
#include <string>

namespace keety::pkg {

/// Link kind used when materializing local packages or workspace members.
enum class LinkMethod {
    Symlink,
    Junction,
    Copy,
};

/// Cross-platform filesystem linking helpers for local packages and workspace members.
class Fetcher {
public:
    /// Creates a directory symlink, junction, or recursive copy at `dest` pointing to `target`.
    /// On POSIX, creates a directory symlink.
    /// On Windows, attempts symlink -> junction (mklink /J) -> recursive copy.
    static LinkMethod linkOrCopy(
        const std::filesystem::path& target,
        const std::filesystem::path& dest
    );

    /// Safely removes a link or directory (handles symlinks, junctions, and directories).
    static void remove(const std::filesystem::path& path);
};

} // namespace keety::pkg
