#pragma once

#include <keety/pkg/manifest.hpp>
#include <keety/pkg/source_spec.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace keety::pkg {

/// Resolved package record in `pedigree.lock`.
struct LockedPackage {
    SourceKind source{SourceKind::Git};

    // For Git:
    std::string url;
    std::string ref;    // User-declared ref (e.g. tag/branch) or empty
    std::string commit; // Full 40-hex commit SHA
    std::string tree;   // Full 40-hex tree hash

    // For Path:
    std::string path;

    // Direct dependencies of this package (sorted names)
    std::vector<std::string> dependencies;

    [[nodiscard]] bool isGit() const noexcept { return source == SourceKind::Git; }
    [[nodiscard]] bool isPath() const noexcept { return source == SourceKind::Path; }

    bool operator==(const LockedPackage& other) const = default;
};

/// In-memory representation of `pedigree.lock`.
struct Lockfile {
    static constexpr int CURRENT_LOCK_VERSION = 1;

    int lockVersion{CURRENT_LOCK_VERSION};

    // Sorted lexicographically by package name
    std::map<std::string, LockedPackage> packages;

    /// Parses and validates a `pedigree.lock` JSON string.
    [[nodiscard]] static Lockfile parse(
        std::string_view jsonText,
        const std::filesystem::path& sourcePath = {}
    );

    /// Loads and parses a `pedigree.lock` file from disk.
    [[nodiscard]] static Lockfile load(const std::filesystem::path& lockfilePath);

    /// Serializes with canonical key order and 2-space indentation.
    [[nodiscard]] std::string toJsonString(int indent = 2) const;

    /// Writes `pedigree.lock` to disk atomically.
    void save(const std::filesystem::path& lockfilePath) const;

    /// Returns true if all dependencies declared in the manifest are present in the
    /// lockfile with matching source parameters (url/ref/path).
    [[nodiscard]] bool satisfies(const Manifest& manifest) const;
};

} // namespace keety::pkg
