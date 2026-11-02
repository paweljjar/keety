#include <keety/pkg/workspace.hpp>
#include <keety/pkg/diagnostics.hpp>

#include <algorithm>
#include <system_error>
#include <unordered_set>

namespace keety::pkg {

namespace {

bool isSubpath(const std::filesystem::path& child, const std::filesystem::path& parent) {
    auto rel = std::filesystem::relative(child, parent);
    if (rel.empty()) return true;
    std::string s = rel.string();
    return s != ".." && s.rfind("../", 0) != 0 && s.rfind("..\\", 0) != 0;
}

std::vector<std::filesystem::path> expandPattern(
    const std::filesystem::path& rootDir,
    const std::string& pattern
) {
    std::vector<std::filesystem::path> matched;

    if (pattern.ends_with("/*")) {
        std::string baseRel = pattern.substr(0, pattern.size() - 2);
        std::filesystem::path baseDir = rootDir / baseRel;

        std::error_code ec;
        if (!std::filesystem::exists(baseDir, ec) || !std::filesystem::is_directory(baseDir, ec)) {
            // Non-existent base glob directory matches 0 members (per npm convention)
            return matched;
        }

        std::vector<std::filesystem::path> subdirs;
        for (const auto& entry : std::filesystem::directory_iterator(baseDir, ec)) {
            if (entry.is_directory(ec)) {
                // Ignore hidden directories like .git
                std::string fname = entry.path().filename().string();
                if (!fname.empty() && fname.front() == '.') continue;
                subdirs.push_back(entry.path());
            }
        }
        std::sort(subdirs.begin(), subdirs.end());
        return subdirs;
    }

    if (pattern.find('*') != std::string::npos) {
        throw PkgError(
            ErrorCode::InvalidWorkspaceEntry,
            "invalid workspace pattern '" + pattern + "': only trailing '/*' globs are supported",
            "Use literal directories (e.g. 'packages/cli') or single-level globs (e.g. 'packages/*')"
        );
    }

    std::filesystem::path dir = rootDir / pattern;
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        throw PkgError(
            ErrorCode::WorkspaceMemberMissing,
            "workspace member directory '" + pattern + "' does not exist or is not a directory"
        );
    }
    matched.push_back(dir);
    return matched;
}

} // namespace

std::optional<std::filesystem::path> Workspace::findRootDirectory(
    const std::filesystem::path& startDir
) {
    std::error_code ec;
    std::filesystem::path curr = std::filesystem::absolute(startDir, ec);

    std::vector<std::filesystem::path> manifests;
    while (!curr.empty()) {
        std::filesystem::path cand = curr / "pedigree.json";
        if (std::filesystem::is_regular_file(cand, ec)) {
            manifests.push_back(curr);
        }
        std::filesystem::path parent = curr.parent_path();
        if (parent == curr) break;
        curr = parent;
    }

    if (manifests.empty()) {
        return std::nullopt;
    }

    // Check if an ancestor manifest defines workspaces that include startDir
    for (auto it = manifests.rbegin(); it != manifests.rend(); ++it) {
        try {
            Manifest m = Manifest::load(*it / "pedigree.json");
            if (!m.workspaces.empty()) {
                // Verify that startDir is under this root
                if (isSubpath(startDir, *it)) {
                    return *it;
                }
            }
        } catch (...) {
            // Ignore parse errors during upward probe; nearest will handle reporting
        }
    }

    // Default to the nearest manifest directory
    return manifests.front();
}

Workspace Workspace::discover(const std::filesystem::path& startDir) {
    auto rootOpt = findRootDirectory(startDir);
    if (!rootOpt.has_value()) {
        throw PkgError(
            ErrorCode::ManifestNotFound,
            "no pedigree.json found in '" + startDir.string() + "' or any parent directory",
            "Run 'kattery init' to scaffold a new project"
        );
    }

    std::filesystem::path rootDir = *rootOpt;
    Manifest rootManifest = Manifest::load(rootDir / "pedigree.json");

    Workspace ws;
    ws.rootDir = rootDir;
    ws.rootManifest = std::move(rootManifest);

    std::unordered_set<std::string> seenNames;
    seenNames.insert(ws.rootManifest.name);

    for (const auto& pattern : ws.rootManifest.workspaces) {
        auto memberDirs = expandPattern(rootDir, pattern);
        for (const auto& mdir : memberDirs) {
            std::filesystem::path manifestFile = mdir / "pedigree.json";
            std::error_code ec;
            if (!std::filesystem::is_regular_file(manifestFile, ec)) {
                throw PkgError(
                    ErrorCode::WorkspaceMemberMissing,
                    "workspace member directory '" + mdir.string() + "' is missing pedigree.json",
                    "Add a pedigree.json to the member directory or remove it from 'workspaces'"
                );
            }

            Manifest memberManifest = Manifest::load(manifestFile);

            if (seenNames.find(memberManifest.name) != seenNames.end()) {
                throw PkgError(
                    ErrorCode::DuplicateWorkspaceMember,
                    "duplicate workspace package name '" + memberManifest.name + "' at '" + mdir.string() + "'",
                    "All workspace members and the workspace root must have unique package names"
                );
            }
            seenNames.insert(memberManifest.name);

            std::filesystem::path rel = std::filesystem::relative(mdir, rootDir, ec);
            std::string relStr = rel.string();
            std::replace(relStr.begin(), relStr.end(), '\\', '/');

            WorkspaceMember member;
            member.name = memberManifest.name;
            member.relativePath = relStr;
            member.directory = mdir;
            member.manifest = std::move(memberManifest);

            ws.members.push_back(std::move(member));
        }
    }

    // Sort members lexicographically by name
    std::sort(ws.members.begin(), ws.members.end(), [](const WorkspaceMember& a, const WorkspaceMember& b) {
        return a.name < b.name;
    });

    return ws;
}

const WorkspaceMember* Workspace::findMember(std::string_view name) const noexcept {
    for (const auto& m : members) {
        if (m.name == name) return &m;
    }
    return nullptr;
}

const WorkspaceMember* Workspace::findMemberByPath(const std::filesystem::path& path) const noexcept {
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(path, ec);
    for (const auto& m : members) {
        if (abs == m.directory || isSubpath(abs, m.directory)) {
            return &m;
        }
    }
    return nullptr;
}

} // namespace keety::pkg
