#pragma once

#include <keety/pkg/git_client.hpp>
#include <keety/pkg/lockfile.hpp>
#include <keety/pkg/manifest.hpp>
#include <keety/pkg/source_spec.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace keety::pkg {

/// A fully resolved dependency node ready for installation and lockfile generation.
struct ResolvedPackage {
    std::string name;
    SourceSpec spec;
    std::string commit; // 40-char commit SHA for Git
    std::string tree;   // 40-char tree hash for Git
    std::filesystem::path localPath; // Absolute path for Path
    std::vector<std::string> dependencies; // Sorted names of direct dependencies
    std::string requiredBy; // Name of package that requested this dependency (or "root")
};

/// Resolution graph that performs transitive walking, cycle detection, and hoisting.
class DependencyGraph {
public:
    explicit DependencyGraph(GitClient* gitClient = nullptr);

    /// Resolves all dependencies of `rootManifest` (and optionally workspace members).
    /// If an existing lockfile is provided and still satisfies declared specs, reuse locked SHAs.
    void resolve(
        const Manifest& rootManifest,
        const std::filesystem::path& rootDir,
        const std::vector<std::pair<Manifest, std::filesystem::path>>& workspaceMembers = {},
        const std::optional<Lockfile>& existingLock = std::nullopt,
        bool includeDevDependencies = true
    );

    /// Returns the resolved packages in alphabetical order.
    [[nodiscard]] const std::map<std::string, ResolvedPackage>& getPackages() const noexcept {
        return packages_;
    }

    /// Generates a `Lockfile` from the resolved packages.
    [[nodiscard]] Lockfile toLockfile() const;

private:
    GitClient* gitClient_{nullptr};
    std::map<std::string, ResolvedPackage> packages_;

    void resolveTransitive(
        const std::string& name,
        const SourceSpec& spec,
        const std::string& requiredBy,
        const std::filesystem::path& rootDir,
        const std::optional<Lockfile>& existingLock,
        std::vector<std::string>& pathStack
    );
};

} // namespace keety::pkg
