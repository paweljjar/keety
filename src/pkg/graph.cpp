#include <keety/pkg/graph.hpp>
#include <keety/pkg/diagnostics.hpp>
#include <keety/pkg/process.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace keety::pkg {

DependencyGraph::DependencyGraph(GitClient* gitClient) : gitClient_(gitClient) {}

void DependencyGraph::resolve(
    const Manifest& rootManifest,
    const std::filesystem::path& rootDir,
    const std::vector<std::pair<Manifest, std::filesystem::path>>& workspaceMembers,
    const std::optional<Lockfile>& existingLock,
    bool includeDevDependencies
) {
    packages_.clear();

    // 1. Collect all root dependencies (direct root declarations win over transitive ones)
    std::vector<std::pair<std::string, SourceSpec>> rootDeps;
    for (const auto& [name, spec] : rootManifest.dependencies) {
        rootDeps.emplace_back(name, spec);
    }
    if (includeDevDependencies) {
        for (const auto& [name, spec] : rootManifest.devDependencies) {
            rootDeps.emplace_back(name, spec);
        }
    }

    // 2. Resolve recursively from root dependencies
    std::vector<std::string> pathStack;
    for (const auto& [name, spec] : rootDeps) {
        pathStack.clear();
        resolveTransitive(name, spec, "root", rootDir, existingLock, pathStack);
    }

    // 3. Resolve member dependencies relative to each member's directory
    for (const auto& [member, memberDir] : workspaceMembers) {
        for (const auto& [name, spec] : member.dependencies) {
            bool isWsMember = false;
            for (const auto& [other, _] : workspaceMembers) {
                if (other.name == name) {
                    isWsMember = true;
                    break;
                }
            }
            if (!isWsMember) {
                pathStack.clear();
                resolveTransitive(name, spec, member.name, memberDir, existingLock, pathStack);
            }
        }
    }
}

void DependencyGraph::resolveTransitive(
    const std::string& name,
    const SourceSpec& spec,
    const std::string& requiredBy,
    const std::filesystem::path& rootDir,
    const std::optional<Lockfile>& existingLock,
    std::vector<std::string>& pathStack
) {
    // 1. Cycle detection
    auto cycleIt = std::find(pathStack.begin(), pathStack.end(), name);
    if (cycleIt != pathStack.end()) {
        std::string cycleStr;
        for (auto it = cycleIt; it != pathStack.end(); ++it) {
            cycleStr += *it + " -> ";
        }
        cycleStr += name;
        throw PkgError(
            ErrorCode::DependencyCycle,
            "cyclic dependency detected: " + cycleStr,
            "Refactor your dependencies to remove cyclic dependencies"
        );
    }

    // 2. Conflict detection against existing resolved packages
    auto existingIt = packages_.find(name);
    if (existingIt != packages_.end()) {
        const auto& existing = existingIt->second;
        if (existing.spec == spec) {
            return; // Identical specification already resolved
        }
        if (existing.requiredBy == "root") {
            // Root manifest declaration always wins over transitive requirement
            return;
        }
        throw PkgError(
            ErrorCode::DependencyConflict,
            "dependency conflict on package '" + name + "': requested with '" +
                existing.spec.toString() + "' by '" + existing.requiredBy +
                "' and with '" + spec.toString() + "' by '" + requiredBy + "'",
            "Declare '" + name + "' directly in your root pedigree.json to force a single version"
        );
    }

    pathStack.push_back(name);

    ResolvedPackage pkg;
    pkg.name = name;
    pkg.spec = spec;
    pkg.requiredBy = requiredBy;

    std::string manifestContent;
    std::filesystem::path pkgDir;

    if (spec.isGit()) {
        if (!gitClient_) {
            throw PkgError(ErrorCode::GitNotFound, "git client required to resolve git dependency '" + name + "'");
        }

        // Try lockfile cache reuse if spec matches
        bool reusedLock = false;
        if (existingLock.has_value()) {
            auto lockIt = existingLock->packages.find(name);
            if (lockIt != existingLock->packages.end()) {
                const auto& locked = lockIt->second;
                if (locked.isGit() && locked.url == spec.url && locked.ref == spec.ref) {
                    pkg.commit = locked.commit;
                    pkg.tree = locked.tree;
                    reusedLock = true;
                }
            }
        }

        if (!reusedLock) {
            pkg.commit = gitClient_->resolveRef(spec.url, spec.ref);
            pkg.tree = gitClient_->treeHash(spec.url, pkg.commit);
        }

        // Read pedigree.json from git mirror directly: git show <commit>:pedigree.json
        ProcessOptions showOpt;
        showOpt.env["GIT_TERMINAL_PROMPT"] = "0";
        showOpt.argv = {"git", "-C", gitClient_->getMirrorPath(spec.url).string(), "show", pkg.commit + ":pedigree.json"};
        auto showRes = ProcessRunner::run(showOpt);
        if (showRes.success()) {
            manifestContent = showRes.stdoutText;
        }
    } else {
        // Path dependency
        std::error_code ec;
        pkg.localPath = std::filesystem::absolute(rootDir / spec.path, ec);
        if (!std::filesystem::exists(pkg.localPath, ec)) {
            throw PkgError(
                ErrorCode::LocalPathNotFound,
                "local dependency directory '" + spec.path + "' does not exist (resolved to '" +
                    pkg.localPath.string() + "')"
            );
        }

        std::filesystem::path manifestFile = pkg.localPath / "pedigree.json";
        if (std::filesystem::is_regular_file(manifestFile, ec)) {
            std::ifstream ifs(manifestFile, std::ios::binary);
            if (ifs.is_open()) {
                std::ostringstream ss;
                ss << ifs.rdbuf();
                manifestContent = ss.str();
            }
        }
    }

    // If package has a pedigree.json, resolve its transitive dependencies
    if (!manifestContent.empty()) {
        std::filesystem::path mPath = pkgDir.empty() ?
            std::filesystem::path(name + "/pedigree.json") :
            (pkgDir / "pedigree.json");
        Manifest childManifest = Manifest::parse(manifestContent, mPath);
        for (const auto& [childName, childSpec] : childManifest.dependencies) {
            resolveTransitive(childName, childSpec, name, pkg.localPath.empty() ? rootDir : pkg.localPath, existingLock, pathStack);
            pkg.dependencies.push_back(childName);
        }
        std::sort(pkg.dependencies.begin(), pkg.dependencies.end());
    }

    pathStack.pop_back();
    packages_.emplace(name, std::move(pkg));
}

Lockfile DependencyGraph::toLockfile() const {
    Lockfile lock;
    lock.lockVersion = 1;

    for (const auto& [name, pkg] : packages_) {
        LockedPackage lp;
        if (pkg.spec.isGit()) {
            lp.source = SourceKind::Git;
            lp.url = pkg.spec.url;
            lp.ref = pkg.spec.ref;
            lp.commit = pkg.commit;
            lp.tree = pkg.tree;
        } else {
            lp.source = SourceKind::Path;
            lp.path = pkg.spec.path;
        }
        lp.dependencies = pkg.dependencies;
        lock.packages.emplace(name, std::move(lp));
    }

    return lock;
}

} // namespace keety::pkg
