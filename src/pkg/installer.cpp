#include <keety/pkg/installer.hpp>
#include <keety/pkg/diagnostics.hpp>
#include <keety/pkg/fetcher.hpp>
#include <keety/pkg/graph.hpp>

#include <keet/json.hpp>

#include <chrono>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_set>

namespace keety::pkg {

namespace {

using JsonNode = keet::json::detail::JsonNode;

std::unordered_set<std::string> loadStateFile(const std::filesystem::path& statePath) {
    std::unordered_set<std::string> pkgs;
    std::ifstream ifs(statePath, std::ios::binary);
    if (!ifs.is_open()) return pkgs;

    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string jsonText = ss.str();

    try {
        keet::json::detail::Parser parser(jsonText);
        JsonNode root = parser.parse_document();
        if (root.is_object()) {
            const auto& obj = std::get<JsonNode::Object>(root.value);
            auto it = obj.find("packages");
            if (it != obj.end() && it->second.is_array()) {
                const auto& arr = std::get<JsonNode::Array>(it->second.value);
                for (const auto& item : arr) {
                    if (item.is_string()) {
                        pkgs.insert(std::get<std::string>(item.value));
                    }
                }
            }
        }
    } catch (...) {
        // Corrupted state file will be overwritten on install
    }
    return pkgs;
}

void saveStateFile(
    const std::filesystem::path& statePath,
    const std::unordered_set<std::string>& managed
) {
    JsonNode::Object root;
    JsonNode::Array arr;
    std::vector<std::string> sorted(managed.begin(), managed.end());
    std::sort(sorted.begin(), sorted.end());
    for (const auto& s : sorted) {
        arr.push_back(JsonNode(s));
    }
    root["packages"] = JsonNode(std::move(arr));

    JsonNode top(std::move(root));
    std::string text = keet::json::detail::dump_node(top, 2);

    std::error_code ec;
    std::filesystem::create_directories(statePath.parent_path(), ec);
    std::ofstream ofs(statePath, std::ios::binary | std::ios::trunc);
    if (ofs.is_open()) {
        ofs << text;
    }
}

} // namespace

Installer::Installer(GitClient* gitClient) : gitClient_(gitClient) {}

InstallResult Installer::install(
    const Workspace& workspace,
    const InstallOptions& options
) {
    const auto& rootDir = workspace.rootDir;
    std::filesystem::path lockfilePath = rootDir / "pedigree.lock";
    std::filesystem::path keetModulesDir = rootDir / "keet_modules";
    std::filesystem::path stateFilePath = keetModulesDir / ".kattery.json";

    std::error_code ec;
    std::filesystem::create_directories(keetModulesDir, ec);

    // 1. Check existing lockfile
    std::optional<Lockfile> existingLock;
    if (std::filesystem::is_regular_file(lockfilePath, ec)) {
        try {
            existingLock = Lockfile::load(lockfilePath);
        } catch (...) {
            if (options.frozen) {
                throw PkgError(
                    ErrorCode::LockfileMismatch,
                    "existing pedigree.lock is corrupted and --frozen was specified",
                    "Run 'kattery install' without --frozen to regenerate the lockfile"
                );
            }
        }
    }

    if (options.frozen) {
        if (!existingLock.has_value() || !existingLock->satisfies(workspace.rootManifest)) {
            throw PkgError(
                ErrorCode::LockfileMismatch,
                "pedigree.lock is missing or does not satisfy declared dependencies, but --frozen was specified",
                "Run 'kattery install' without --frozen to update pedigree.lock"
            );
        }
    }

    // 2. Resolve dependency graph
    DependencyGraph graph(gitClient_);
    std::vector<Manifest> memberManifests;
    memberManifests.reserve(workspace.members.size());
    for (const auto& m : workspace.members) {
        memberManifests.push_back(m.manifest);
    }

    graph.resolve(
        workspace.rootManifest,
        rootDir,
        memberManifests,
        existingLock,
        !options.production
    );

    const auto& resolvedPkgs = graph.getPackages();

    // 3. Staging directory
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::filesystem::path stagingDir = keetModulesDir / (".staging_" + std::to_string(now));
    std::filesystem::create_directories(stagingDir, ec);

    InstallResult result;
    result.lockfile = graph.toLockfile();

    try {
        // 4. Materialize packages into staging
        for (const auto& [name, pkg] : resolvedPkgs) {
            std::filesystem::path dest = stagingDir / name;
            if (pkg.spec.isGit()) {
                if (!gitClient_) {
                    throw PkgError(ErrorCode::GitNotFound, "git client required to install '" + name + "'");
                }
                gitClient_->checkout(pkg.spec.url, pkg.commit, dest);

                // Verify integrity tree hash against lock
                std::string actualTree = gitClient_->treeHash(pkg.spec.url, pkg.commit);
                if (actualTree != pkg.tree) {
                    throw PkgError(
                        ErrorCode::TreeHashMismatch,
                        "integrity verification failed for package '" + name + "': expected tree " +
                            pkg.tree + " but git computed " + actualTree
                    );
                }
            } else {
                Fetcher::linkOrCopy(pkg.localPath, dest);
            }
        }

        // 5. Read previous managed packages
        auto previouslyManaged = loadStateFile(stateFilePath);
        std::unordered_set<std::string> newlyManaged;

        // 6. Atomically move staged packages into keet_modules/
        for (const auto& [name, _] : resolvedPkgs) {
            std::filesystem::path target = keetModulesDir / name;
            Fetcher::remove(target);
            std::filesystem::rename(stagingDir / name, target, ec);
            if (ec) {
                // If rename across filesystems fails, fallback to link/copy
                Fetcher::linkOrCopy(stagingDir / name, target);
            }
            newlyManaged.insert(name);
            result.installedPackages.push_back(name);
        }

        // 7. Link workspace members into root keet_modules/
        for (const auto& member : workspace.members) {
            std::filesystem::path target = keetModulesDir / member.name;
            Fetcher::linkOrCopy(member.directory, target);
            newlyManaged.insert(member.name);
            result.linkedWorkspaceMembers.push_back(member.name);
        }

        // 8. Prune unneeded previously-managed packages
        for (const auto& oldPkg : previouslyManaged) {
            if (newlyManaged.find(oldPkg) == newlyManaged.end()) {
                Fetcher::remove(keetModulesDir / oldPkg);
                result.prunedPackages.push_back(oldPkg);
            }
        }

        // 9. Save state file
        saveStateFile(stateFilePath, newlyManaged);

        // 10. Save lockfile unless --frozen
        if (!options.frozen) {
            result.lockfile.save(lockfilePath);
        }

        // Clean staging directory
        std::filesystem::remove_all(stagingDir, ec);

    } catch (...) {
        // Rollback staging directory on failure
        std::filesystem::remove_all(stagingDir, ec);
        throw;
    }

    return result;
}

} // namespace keety::pkg
