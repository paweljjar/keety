#include <keety/pkg/lockfile.hpp>
#include <keety/pkg/diagnostics.hpp>

#include <keet/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace keety::pkg {

namespace {

using JsonNode = keet::json::detail::JsonNode;

std::string formatContext(const std::filesystem::path& path, std::string_view field) {
    std::string prefix = path.empty() ? "" : path.filename().string() + ": ";
    if (field.empty()) return prefix;
    return prefix + std::string(field);
}

bool isHexSha(std::string_view s, size_t expectedLen = 40) noexcept {
    if (s.size() != expectedLen) return false;
    for (char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

} // namespace

Lockfile Lockfile::parse(std::string_view jsonText, const std::filesystem::path& sourcePath) {
    JsonNode rootNode;
    try {
        keet::json::detail::Parser parser(jsonText);
        rootNode = parser.parse_document();
    } catch (const keet::error& err) {
        throw PkgError(
            ErrorCode::InvalidJson,
            (sourcePath.empty() ? "" : sourcePath.filename().string() + ": ") + err.message
        );
    }

    if (!rootNode.is_object()) {
        throw PkgError(
            ErrorCode::InvalidLockfile,
            formatContext(sourcePath, "") + "root must be a JSON object"
        );
    }

    const auto& root = std::get<JsonNode::Object>(rootNode.value);

    // lockVersion (required int)
    auto verIt = root.find("lockVersion");
    if (verIt == root.end() || !verIt->second.is_int()) {
        throw PkgError(
            ErrorCode::InvalidLockfile,
            formatContext(sourcePath, "lockVersion") + " must be an integer"
        );
    }
    int64_t v = std::get<int64_t>(verIt->second.value);
    if (v != CURRENT_LOCK_VERSION) {
        throw PkgError(
            ErrorCode::UnsupportedLockVersion,
            formatContext(sourcePath, "lockVersion") + " " + std::to_string(v) +
                " is not supported (expected " + std::to_string(CURRENT_LOCK_VERSION) + ")",
            "Update kattery or regenerate the lockfile with 'kattery install'"
        );
    }

    Lockfile lock;
    lock.lockVersion = static_cast<int>(v);

    // packages (required object)
    auto pkgsIt = root.find("packages");
    if (pkgsIt == root.end() || !pkgsIt->second.is_object()) {
        throw PkgError(
            ErrorCode::InvalidLockfile,
            formatContext(sourcePath, "packages") + " must be a JSON object"
        );
    }

    const auto& pkgsObj = std::get<JsonNode::Object>(pkgsIt->second.value);
    for (const auto& [pkgName, pkgNode] : pkgsObj) {
        validatePackageName(pkgName, formatContext(sourcePath, "packages." + pkgName));
        if (!pkgNode.is_object()) {
            throw PkgError(
                ErrorCode::InvalidLockfile,
                formatContext(sourcePath, "packages." + pkgName) + " must be an object"
            );
        }

        const auto& entryObj = std::get<JsonNode::Object>(pkgNode.value);
        auto srcIt = entryObj.find("source");
        if (srcIt == entryObj.end() || !srcIt->second.is_string()) {
            throw PkgError(
                ErrorCode::InvalidLockfile,
                formatContext(sourcePath, "packages." + pkgName + ".source") + " must be 'git' or 'path'"
            );
        }

        std::string sourceStr = std::get<std::string>(srcIt->second.value);
        LockedPackage lp;

        if (sourceStr == "git") {
            lp.source = SourceKind::Git;

            auto urlIt = entryObj.find("url");
            if (urlIt == entryObj.end() || !urlIt->second.is_string()) {
                throw PkgError(
                    ErrorCode::InvalidLockfile,
                    formatContext(sourcePath, "packages." + pkgName + ".url") + " is required for git package"
                );
            }
            lp.url = std::get<std::string>(urlIt->second.value);

            auto refIt = entryObj.find("ref");
            if (refIt != entryObj.end() && refIt->second.is_string()) {
                lp.ref = std::get<std::string>(refIt->second.value);
            }

            auto commitIt = entryObj.find("commit");
            if (commitIt == entryObj.end() || !commitIt->second.is_string()) {
                throw PkgError(
                    ErrorCode::InvalidLockfile,
                    formatContext(sourcePath, "packages." + pkgName + ".commit") + " is required for git package"
                );
            }
            lp.commit = std::get<std::string>(commitIt->second.value);
            if (!isHexSha(lp.commit)) {
                throw PkgError(
                    ErrorCode::InvalidLockfile,
                    formatContext(sourcePath, "packages." + pkgName + ".commit") + " must be a 40-character hex SHA"
                );
            }

            auto treeIt = entryObj.find("tree");
            if (treeIt == entryObj.end() || !treeIt->second.is_string()) {
                throw PkgError(
                    ErrorCode::InvalidLockfile,
                    formatContext(sourcePath, "packages." + pkgName + ".tree") + " is required for git package"
                );
            }
            lp.tree = std::get<std::string>(treeIt->second.value);
            if (!isHexSha(lp.tree)) {
                throw PkgError(
                    ErrorCode::InvalidLockfile,
                    formatContext(sourcePath, "packages." + pkgName + ".tree") + " must be a 40-character hex SHA"
                );
            }
        } else if (sourceStr == "path") {
            lp.source = SourceKind::Path;

            auto pathIt = entryObj.find("path");
            if (pathIt == entryObj.end() || !pathIt->second.is_string()) {
                throw PkgError(
                    ErrorCode::InvalidLockfile,
                    formatContext(sourcePath, "packages." + pkgName + ".path") + " is required for path package"
                );
            }
            lp.path = std::get<std::string>(pathIt->second.value);
        } else {
            throw PkgError(
                ErrorCode::InvalidLockfile,
                formatContext(sourcePath, "packages." + pkgName + ".source") + " must be 'git' or 'path'"
            );
        }

        // dependencies (optional array of package name strings)
        auto depsIt = entryObj.find("dependencies");
        if (depsIt != entryObj.end()) {
            if (!depsIt->second.is_array()) {
                throw PkgError(
                    ErrorCode::InvalidLockfile,
                    formatContext(sourcePath, "packages." + pkgName + ".dependencies") + " must be an array"
                );
            }
            const auto& depsArr = std::get<JsonNode::Array>(depsIt->second.value);
            for (const auto& d : depsArr) {
                if (!d.is_string()) {
                    throw PkgError(
                        ErrorCode::InvalidLockfile,
                        formatContext(sourcePath, "packages." + pkgName + ".dependencies") + " must contain strings"
                    );
                }
                lp.dependencies.push_back(std::get<std::string>(d.value));
            }
            std::sort(lp.dependencies.begin(), lp.dependencies.end());
        }

        lock.packages.emplace(pkgName, std::move(lp));
    }

    return lock;
}

Lockfile Lockfile::load(const std::filesystem::path& lockfilePath) {
    std::ifstream ifs(lockfilePath, std::ios::binary);
    if (!ifs.is_open()) {
        throw PkgError(
            ErrorCode::ManifestNotFound,
            "lockfile not found at '" + lockfilePath.string() + "'"
        );
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return parse(ss.str(), lockfilePath);
}

std::string Lockfile::toJsonString(int indent) const {
    JsonNode::Object root;
    root["lockVersion"] = JsonNode(static_cast<int64_t>(lockVersion));

    JsonNode::Object pkgsObj;
    for (const auto& [pkgName, pkg] : packages) {
        JsonNode::Object entry;
        if (pkg.isGit()) {
            entry["source"] = JsonNode(std::string("git"));
            entry["url"] = JsonNode(pkg.url);
            if (!pkg.ref.empty()) {
                entry["ref"] = JsonNode(pkg.ref);
            }
            entry["commit"] = JsonNode(pkg.commit);
            entry["tree"] = JsonNode(pkg.tree);
        } else {
            entry["source"] = JsonNode(std::string("path"));
            entry["path"] = JsonNode(pkg.path);
        }

        JsonNode::Array depsArr;
        depsArr.reserve(pkg.dependencies.size());
        for (const auto& d : pkg.dependencies) {
            depsArr.push_back(JsonNode(d));
        }
        entry["dependencies"] = JsonNode(std::move(depsArr));

        pkgsObj[pkgName] = JsonNode(std::move(entry));
    }

    root["packages"] = JsonNode(std::move(pkgsObj));

    JsonNode top(std::move(root));
    std::string out = keet::json::detail::dump_node(top, indent);
    if (out.empty() || out.back() != '\n') {
        out.push_back('\n');
    }
    return out;
}

void Lockfile::save(const std::filesystem::path& lockfilePath) const {
    std::string content = toJsonString(2);

    std::filesystem::path parent = lockfilePath.parent_path();
    if (parent.empty()) parent = ".";
    std::filesystem::create_directories(parent);

    std::filesystem::path tmpPath = parent / (lockfilePath.filename().string() + ".tmp." + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

    {
        std::ofstream ofs(tmpPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            throw PkgError(
                ErrorCode::ManifestNotFound,
                "failed to write lockfile to '" + tmpPath.string() + "'"
            );
        }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    std::error_code ec;
    std::filesystem::rename(tmpPath, lockfilePath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        throw PkgError(
            ErrorCode::ManifestNotFound,
            "failed to replace '" + lockfilePath.string() + "': " + ec.message()
        );
    }
}

bool Lockfile::satisfies(const Manifest& manifest) const {
    auto checkMap = [&](const std::map<std::string, SourceSpec>& declared) -> bool {
        for (const auto& [name, spec] : declared) {
            auto it = packages.find(name);
            if (it == packages.end()) {
                return false;
            }
            const auto& locked = it->second;
            if (spec.isGit()) {
                if (!locked.isGit()) return false;
                if (locked.url != spec.url) return false;
                if (locked.ref != spec.ref) return false;
            } else {
                if (!locked.isPath()) return false;
                if (locked.path != spec.path) return false;
            }
        }
        return true;
    };

    return checkMap(manifest.dependencies) && checkMap(manifest.devDependencies);
}

} // namespace keety::pkg
