#pragma once

#include <keety/pkg/source_spec.hpp>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace keety::pkg {

/// In-memory representation of a validated `pedigree.json` package manifest.
struct Manifest {
    std::string name;
    std::string version;
    std::string description;
    std::string entry;
    std::string buildDir; // Configured build output directory, e.g. "build"
    std::string keety; // Minimum compiler constraint, e.g. ">=0.1.0"

    // Lexicographically sorted dependencies
    std::map<std::string, SourceSpec> dependencies;
    std::map<std::string, SourceSpec> devDependencies;

    // Project scripts (e.g. "start", "test", "build")
    std::map<std::string, std::string> scripts;

    // Monorepo member directory globs/paths (e.g. ["packages/*", "apps/cli"])
    std::vector<std::string> workspaces;

    /// Parses and strictly validates manifest JSON text.
    /// If `sourcePath` is provided, diagnostic messages include it.
    [[nodiscard]] static Manifest parse(
        std::string_view jsonText,
        const std::filesystem::path& sourcePath = {}
    );

    /// Loads, parses and validates a `pedigree.json` file from disk.
    [[nodiscard]] static Manifest load(const std::filesystem::path& manifestPath);

    /// Serializes to formatted JSON with canonical 2-space indentation and fixed field ordering.
    [[nodiscard]] std::string toJsonString(int indent = 2) const;

    /// Writes this manifest to disk at the given path (atomic write via temp file).
    void save(const std::filesystem::path& manifestPath) const;

    /// Checks if a dependency (runtime or dev) is declared under `depName`.
    [[nodiscard]] bool hasDependency(const std::string& depName) const noexcept;
};

} // namespace keety::pkg
