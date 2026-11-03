#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/manifest.hpp>
#include <keety/pkg/diagnostics.hpp>

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

} // namespace

TEST_CASE("Manifest parsing: valid pedigree.json", "[kattery][manifest]") {
    const std::string json = R"({
  "name": "my-app",
  "version": "1.2.3",
  "description": "My test app",
  "entry": "src/main.keet",
  "buildDir": "dist",
  "keety": ">=0.1.0",
  "dependencies": {
    "mathx": "git+https://github.com/user/mathx.git#v1.0.0",
    "utils": "path:../utils"
  },
  "devDependencies": {
    "testkit": "git+https://github.com/user/testkit.git#main"
  },
  "scripts": {
    "start": "keety run src/main.keet",
    "test": "keety run tests/all.keet"
  },
  "workspaces": [
    "packages/*",
    "apps/cli"
  ]
})";

    auto m = Manifest::parse(json);
    REQUIRE(m.name == "my-app");
    REQUIRE(m.version == "1.2.3");
    REQUIRE(m.description == "My test app");
    REQUIRE(m.entry == "src/main.keet");
    REQUIRE(m.buildDir == "dist");
    REQUIRE(m.keety == ">=0.1.0");

    REQUIRE(m.dependencies.size() == 2);
    REQUIRE(m.dependencies.at("mathx").isGit());
    REQUIRE(m.dependencies.at("mathx").ref == "v1.0.0");
    REQUIRE(m.dependencies.at("utils").isPath());
    REQUIRE(m.dependencies.at("utils").path == "../utils");

    REQUIRE(m.devDependencies.size() == 1);
    REQUIRE(m.devDependencies.at("testkit").ref == "main");

    REQUIRE(m.hasDependency("mathx"));
    REQUIRE(m.hasDependency("utils"));
    REQUIRE(m.hasDependency("testkit"));
    REQUIRE_FALSE(m.hasDependency("unknown"));

    REQUIRE(m.scripts.size() == 2);
    REQUIRE(m.scripts.at("start") == "keety run src/main.keet");
    REQUIRE(m.scripts.at("test") == "keety run tests/all.keet");

    REQUIRE(m.workspaces.size() == 2);
    REQUIRE(m.workspaces[0] == "packages/*");
    REQUIRE(m.workspaces[1] == "apps/cli");
}

TEST_CASE("Manifest parsing: invalid inputs", "[kattery][manifest]") {
    SECTION("Malformed JSON") {
        try {
            (void)Manifest::parse("{ not valid json }");
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::InvalidJson);
        }
    }

    SECTION("Missing name or version") {
        try {
            (void)Manifest::parse(R"({"version":"1.0.0"})");
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::MissingField);
        }

        try {
            (void)Manifest::parse(R"({"name":"my-app"})");
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::MissingField);
        }
    }

    SECTION("Invalid field types") {
        try {
            (void)Manifest::parse(R"({"name":"my-app","version":123})");
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::InvalidFieldType);
        }

        try {
            (void)Manifest::parse(R"({"name":"my-app","version":"1.0.0","dependencies":"invalid"})");
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::InvalidFieldType);
        }
    }

    SECTION("Self dependency is rejected") {
        try {
            (void)Manifest::parse(R"({"name":"my-app","version":"1.0.0","dependencies":{"my-app":"path:."}})");
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::SelfDependency);
        }

        try {
            (void)Manifest::parse(R"({"name":"my-app","version":"1.0.0","devDependencies":{"my-app":"path:."}})");
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::SelfDependency);
        }
    }
}

TEST_CASE("Manifest file load, save, and round-trip", "[kattery][manifest]") {
    TempDirectory temp("kattery_manifest_test");

    Manifest m;
    m.name = "roundtrip-pkg";
    m.version = "0.5.1";
    m.description = "Manifest disk test";
    m.entry = "src/index.keet";
    m.buildDir = "target/bin";
    m.keety = ">=0.1.0";
    m.dependencies["dep-a"] = SourceSpec::parse("git+https://github.com/u/dep-a.git#v1.0.0");
    m.scripts["build"] = "keety build src/index.keet";

    fs::path targetFile = temp.path / "pedigree.json";
    m.save(targetFile);
    REQUIRE(fs::exists(targetFile));

    Manifest loaded = Manifest::load(targetFile);
    REQUIRE(loaded.name == m.name);
    REQUIRE(loaded.version == m.version);
    REQUIRE(loaded.description == m.description);
    REQUIRE(loaded.entry == m.entry);
    REQUIRE(loaded.buildDir == m.buildDir);
    REQUIRE(loaded.keety == m.keety);
    REQUIRE(loaded.dependencies.size() == 1);
    REQUIRE(loaded.dependencies.at("dep-a") == m.dependencies.at("dep-a"));
    REQUIRE(loaded.scripts.at("build") == "keety build src/index.keet");
}
