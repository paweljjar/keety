#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/cli.hpp>
#include <keety/pkg/manifest.hpp>
#include <keety/pkg/lockfile.hpp>
#include <keety/pkg/process.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

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
        temp.createFile("index.keet", "export let version: string = \"1.0.0\";");
        temp.createFile("pedigree.json", R"({"name":"mathx","version":"1.0.0"})");
        (void)ProcessRunner::run({"git", "-C", temp.path.string(), "add", "."});
        (void)ProcessRunner::run({"git", "-C", temp.path.string(), "commit", "-m", "Initial commit"});
        (void)ProcessRunner::run({"git", "-C", temp.path.string(), "tag", "v1.0.0"});
        repoUrl = "file://" + temp.path.generic_string();
    }
};

} // namespace

TEST_CASE("Cli: kattery init", "[kattery][cli]") {
    TempDirectory temp("kattery_cli_init");
    std::ostringstream out, err;

    SECTION("Scaffold new pedigree.json with default name") {
        int code = Cli::run({"init", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code == 0);
        REQUIRE(fs::exists(temp.path / "pedigree.json"));

        Manifest m = Manifest::load(temp.path / "pedigree.json");
        REQUIRE(m.version == "0.1.0");
        REQUIRE(m.entry == "src/main.keet");
        REQUIRE(m.scripts.find("start") != m.scripts.end());
        REQUIRE(m.scripts.at("start") == "keety run src/main.keet --output-dir build");
    }

    SECTION("Scaffold with explicit --name") {
        int code = Cli::run({"init", "--name", "custom-app", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code == 0);

        Manifest m = Manifest::load(temp.path / "pedigree.json");
        REQUIRE(m.name == "custom-app");
    }

    SECTION("Refuses to overwrite existing pedigree.json") {
        temp.createFile("pedigree.json", R"({"name":"existing","version":"1.0.0"})");
        int code = Cli::run({"init", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code != 0);
        REQUIRE(err.str().find("KAT4005") != std::string::npos);
    }
}

TEST_CASE("Cli: kattery add and remove", "[kattery][cli]") {
    TempDirectory temp("kattery_cli_add_remove");
    LocalGitRepo gitDep("kattery_git_mathx");

    // Init project
    std::ostringstream out, err;
    REQUIRE(Cli::run({"init", "--name", "my-app", "--cwd", temp.path.string()}, out, err) == 0);

    // Create local path package
    temp.createFile("local_lib/index.keet", "export let ok = true;");
    temp.createFile("local_lib/pedigree.json", R"({"name":"locallib","version":"0.1.0"})");

    SECTION("kattery add path dependency") {
        out.str(""); err.str("");
        int code = Cli::run({"add", "locallib", "path:./local_lib", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code == 0);
        REQUIRE(fs::exists(temp.path / "keet_modules" / "locallib" / "index.keet"));

        Manifest m = Manifest::load(temp.path / "pedigree.json");
        REQUIRE(m.dependencies.find("locallib") != m.dependencies.end());

        Lockfile lock = Lockfile::load(temp.path / "pedigree.lock");
        REQUIRE(lock.packages.find("locallib") != lock.packages.end());
    }

    SECTION("kattery adopt alias with git URL") {
        out.str(""); err.str("");
        std::string gitSpec = gitDep.repoUrl + "#v1.0.0";
        int code = Cli::run({"adopt", "mathx", gitSpec, "--cwd", temp.path.string()}, out, err);
        REQUIRE(code == 0);
        REQUIRE(fs::exists(temp.path / "keet_modules" / "mathx" / "index.keet"));

        Manifest m = Manifest::load(temp.path / "pedigree.json");
        REQUIRE(m.dependencies.find("mathx") != m.dependencies.end());

        Lockfile lock = Lockfile::load(temp.path / "pedigree.lock");
        REQUIRE(lock.packages.find("mathx") != lock.packages.end());
        REQUIRE(lock.packages.at("mathx").isGit());
        REQUIRE(lock.packages.at("mathx").commit.size() == 40);
    }

    SECTION("kattery add -D (dev dependency)") {
        out.str(""); err.str("");
        int code = Cli::run({"add", "locallib", "path:./local_lib", "-D", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code == 0);

        Manifest m = Manifest::load(temp.path / "pedigree.json");
        REQUIRE(m.dependencies.find("locallib") == m.dependencies.end());
        REQUIRE(m.devDependencies.find("locallib") != m.devDependencies.end());
    }

    SECTION("kattery remove dependency") {
        // First add locallib
        REQUIRE(Cli::run({"add", "locallib", "path:./local_lib", "--cwd", temp.path.string()}, out, err) == 0);
        REQUIRE(fs::exists(temp.path / "keet_modules" / "locallib"));

        // Now remove it
        out.str(""); err.str("");
        int code = Cli::run({"remove", "locallib", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code == 0);
        REQUIRE_FALSE(fs::exists(temp.path / "keet_modules" / "locallib"));

        Manifest m = Manifest::load(temp.path / "pedigree.json");
        REQUIRE_FALSE(m.hasDependency("locallib"));
    }

    SECTION("kattery remove nonexistent throws KAT4006") {
        out.str(""); err.str("");
        int code = Cli::run({"remove", "nonexistent_pkg", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code != 0);
        REQUIRE(err.str().find("KAT4006") != std::string::npos);
    }
}
