#pragma once

#include <keety/pkg/git_client.hpp>
#include <keety/pkg/lockfile.hpp>
#include <keety/pkg/workspace.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace keety::pkg {

/// Options controlling installation behavior.
struct InstallOptions {
    bool frozen{false};     // If true, fails if pedigree.lock is missing or out of date
    bool production{false}; // If true, ignores devDependencies
    bool verbose{false};    // If true, logs step details
};

/// Result summary of an installation execution.
struct InstallResult {
    std::vector<std::string> installedPackages;
    std::vector<std::string> linkedWorkspaceMembers;
    std::vector<std::string> prunedPackages;
    Lockfile lockfile;
};

/// High-level package installer that populates `keet_modules/` and maintains `pedigree.lock`.
class Installer {
public:
    explicit Installer(GitClient* gitClient = nullptr);

    /// Installs dependencies for a given workspace (or standalone project).
    InstallResult install(
        const Workspace& workspace,
        const InstallOptions& options = {}
    );

private:
    GitClient* gitClient_{nullptr};
};

} // namespace keety::pkg
