#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/installer.hpp>
#include <keety/pkg/git_client.hpp>
#include <keety/pkg/workspace.hpp>
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
        temp.createFile("index.keet", "export let gitPkg: string = \"from_git\";");
        temp.createFile("pedigree.json", R"({"name":"gitpkg","version":"1.0.0"})");
        (void)ProcessRunner::run({"git", "-C", temp.path.string(), "add", "."});
        (void)ProcessRunner::run({"git", "-C", temp.path.string(), "commit", "-m", "Initial commit"});
        (void)ProcessRunner::run({"git", "-C", temp.path.string(), "tag", "v1.0.0"});
        repoUrl = "file://" + temp.path.generic_string();
    }
};

} // namespace

TEST_CASE("Installer end-to-end installation and lockfile maintenance", "[kattery][installer]") {
    TempDirectory projectDir("kattery_install_proj");
    TempDirectory cacheDir("kattery_install_cache");
    LocalGitRepo gitDep("kattery_git_dep");

    // Create local path dependency
    projectDir.createFile("local_utils/index.keet", "export let utils: string = \"ready\";");
    projectDir.createFile("local_utils/pedigree.json", R"({"name":"utils","version":"0.5.0"})");

    // Create root pedigree.json
    std::string rootManifestText =
        "{\n"
        "  \"name\": \"my-app\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"dependencies\": {\n"
        "    \"utils\": \"path:./local_utils\",\n"
        "    \"gitpkg\": \"git+" + gitDep.repoUrl + "#v1.0.0\"\n"
        "  }\n"
        "}";
    projectDir.createFile("pedigree.json", rootManifestText);

    GitClient gitClient(cacheDir.path);
    Installer installer(&gitClient);

    auto ws = Workspace::discover(projectDir.path);
    InstallResult res = installer.install(ws);

    REQUIRE(res.installedPackages.size() == 2);
    REQUIRE(fs::exists(projectDir.path / "keet_modules" / "utils" / "index.keet"));
    REQUIRE(fs::exists(projectDir.path / "keet_modules" / "gitpkg" / "index.keet"));
    REQUIRE(fs::exists(projectDir.path / "keet_modules" / ".kattery.json"));
    REQUIRE(fs::exists(projectDir.path / "pedigree.lock"));

    // Verify lockfile contains exact commit and tree
    Lockfile lock = Lockfile::load(projectDir.path / "pedigree.lock");
    REQUIRE(lock.packages.size() == 2);
    REQUIRE(lock.packages.at("gitpkg").isGit());
    REQUIRE(lock.packages.at("gitpkg").commit.size() == 40);
    REQUIRE(lock.packages.at("gitpkg").tree.size() == 40);
    REQUIRE(lock.packages.at("utils").isPath());

    SECTION("--frozen succeeds when lockfile matches") {
        InstallOptions frozenOpt;
        frozenOpt.frozen = true;
        REQUIRE_NOTHROW(installer.install(ws, frozenOpt));
    }

    SECTION("--frozen throws KAT3003 when lockfile is out of date") {
        // Add a new dependency to pedigree.json without updating pedigree.lock
        std::string staleManifestText =
            "{\n"
            "  \"name\": \"my-app\",\n"
            "  \"version\": \"1.0.0\",\n"
            "  \"dependencies\": {\n"
            "    \"utils\": \"path:./local_utils\",\n"
            "    \"gitpkg\": \"git+" + gitDep.repoUrl + "#v1.0.0\",\n"
            "    \"extra\": \"path:./local_utils\"\n"
            "  }\n"
            "}";
        projectDir.createFile("pedigree.json", staleManifestText);
        auto updatedWs = Workspace::discover(projectDir.path);
        InstallOptions frozenOpt;
        frozenOpt.frozen = true;

        try {
            (void)installer.install(updatedWs, frozenOpt);
            FAIL("expected throw on stale lockfile under --frozen");
        } catch (const PkgError& err) {
            REQUIRE(err.code() == ErrorCode::LockfileMismatch);
        }
    }

    SECTION("Pruning removed dependencies") {
        // Remove gitpkg dependency from manifest
        projectDir.createFile("pedigree.json", R"({
  "name": "my-app",
  "version": "1.0.0",
  "dependencies": {
    "utils": "path:./local_utils"
  }
})");
        auto prunedWs = Workspace::discover(projectDir.path);
        InstallResult pruneRes = installer.install(prunedWs);

        REQUIRE(pruneRes.prunedPackages.size() == 1);
        REQUIRE(pruneRes.prunedPackages[0] == "gitpkg");
        REQUIRE_FALSE(fs::exists(projectDir.path / "keet_modules" / "gitpkg"));
        REQUIRE(fs::exists(projectDir.path / "keet_modules" / "utils"));
    }
}
