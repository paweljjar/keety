#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/git_client.hpp>
#include <keety/pkg/diagnostics.hpp>
#include <keety/pkg/process.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace keety::pkg;
namespace fs = std::filesystem;

namespace {

struct TempDirectory {
    fs::path path;

    explicit TempDirectory(const std::string& prefix) {
        path = fs::temp_directory_path() / (prefix + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        fs::create_directories(path);
    }

    ~TempDirectory() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    fs::path createFile(const fs::path& relPath, const std::string& content = "") {
        fs::path fullPath = path / relPath;
        fs::create_directories(fullPath.parent_path());
        std::ofstream ofs(fullPath, std::ios::binary);
        ofs << content;
        return fullPath;
    }
};

struct LocalGitRepo {
    TempDirectory temp;
    std::string repoUrl;

    explicit LocalGitRepo(const std::string& prefix) : temp(prefix) {
        (void)ProcessRunner::run({"git", "init", temp.path.string()});
        (void)ProcessRunner::run({"git", "-C", temp.path.string(), "config", "user.name", "KeetyTest"});
        (void)ProcessRunner::run({"git", "-C", temp.path.string(), "config", "user.email", "test@keety.org"});
        (void)ProcessRunner::run({"git", "-C", temp.path.string(), "config", "commit.gpgsign", "false"});
        temp.createFile("index.keet", "export let hello: string = \"world\";");
        temp.createFile("pedigree.json", R"({"name":"sample","version":"1.0.0"})");
        (void)ProcessRunner::run({"git", "-C", temp.path.string(), "add", "."});
        (void)ProcessRunner::run({"git", "-C", temp.path.string(), "commit", "-m", "Initial commit"});
        (void)ProcessRunner::run({"git", "-C", temp.path.string(), "tag", "v1.0.0"});
        repoUrl = "file://" + temp.path.string();
    }
};

} // namespace

TEST_CASE("GitClient local repository operations", "[kattery][git]") {
    LocalGitRepo repo("kattery_git_repo");
    TempDirectory clientHome("kattery_git_client");

    GitClient client(clientHome.path);
    REQUIRE(client.isAvailable());

    SECTION("Resolve tag and HEAD to commit SHA") {
        std::string tagSha = client.resolveRef(repo.repoUrl, "v1.0.0");
        REQUIRE(tagSha.size() == 40);

        std::string headSha = client.resolveRef(repo.repoUrl, "");
        REQUIRE(headSha.size() == 40);
        REQUIRE(tagSha == headSha);

        std::string tree = client.treeHash(repo.repoUrl, tagSha);
        REQUIRE(tree.size() == 40);
    }

    SECTION("Resolve invalid ref throws KAT2003") {
        try {
            (void)client.resolveRef(repo.repoUrl, "non-existent-tag");
            FAIL("expected throw");
        } catch (const PkgError& err) {
            REQUIRE(err.code() == ErrorCode::GitRefNotFound);
        }
    }

    SECTION("Checkout materializes source snapshot without .git") {
        std::string sha = client.resolveRef(repo.repoUrl, "v1.0.0");
        fs::path dest = clientHome.path / "dest";

        client.checkout(repo.repoUrl, sha, dest);
        REQUIRE(fs::exists(dest / "index.keet"));
        REQUIRE(fs::exists(dest / "pedigree.json"));
        REQUIRE_FALSE(fs::exists(dest / ".git"));
    }
}
