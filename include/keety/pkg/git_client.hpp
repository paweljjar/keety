#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace keety::pkg {

/// Client for interacting with the system `git` executable and managing bare mirrors.
class GitClient {
public:
    explicit GitClient(std::optional<std::filesystem::path> customHome = std::nullopt);

    /// Checks if the git executable is available on PATH.
    [[nodiscard]] bool isAvailable() const;

    /// Returns the global cache directory (~/.kattery/cache/git or $KATTERY_HOME/cache/git).
    [[nodiscard]] const std::filesystem::path& getCacheDir() const noexcept {
        return gitCacheDir_;
    }

    /// Resolves a Git reference (tag, branch, commit, or empty for HEAD) to a full 40-hex commit SHA.
    /// Clones or updates the bare mirror in the cache as needed.
    [[nodiscard]] std::string resolveRef(const std::string& url, const std::string& ref);

    /// Computes the git tree hash for a resolved commit SHA.
    [[nodiscard]] std::string treeHash(const std::string& url, const std::string& commitSha);

    /// Materializes the repository at `commitSha` into `destDir` without `.git` metadata.
    void checkout(
        const std::string& url,
        const std::string& commitSha,
        const std::filesystem::path& destDir
    );

    /// Returns the bare mirror path for a given git repository URL.
    [[nodiscard]] std::filesystem::path getMirrorPath(const std::string& url) const;

private:
    std::filesystem::path katteryHome_;
    std::filesystem::path gitCacheDir_;
    mutable std::optional<bool> isGitAvailable_;

    void requireGit() const;
    void ensureMirror(const std::string& url);
};

} // namespace keety::pkg
