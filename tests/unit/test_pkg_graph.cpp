#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/graph.hpp>
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

TEST_CASE("DependencyGraph resolution and hoisting", "[kattery][graph]") {
    TempDirectory temp("kattery_graph_test");

    // Create local packages:
    // root -> pkga -> pkgb
    temp.createFile("pkga/pedigree.json", R"({
  "name": "pkga",
  "version": "1.0.0",
  "dependencies": {
    "pkgb": "path:../pkgb"
  }
})");
    temp.createFile("pkga/index.keet", "export let a = 1;");

    temp.createFile("pkgb/pedigree.json", R"({
  "name": "pkgb",
  "version": "1.0.0"
})");
    temp.createFile("pkgb/index.keet", "export let b = 2;");

    Manifest root;
    root.name = "root-app";
    root.version = "0.1.0";
    root.dependencies["pkga"] = SourceSpec::parse("path:./pkga");

    DependencyGraph graph(nullptr);
    graph.resolve(root, temp.path);

    const auto& pkgs = graph.getPackages();
    REQUIRE(pkgs.size() == 2);
    REQUIRE(pkgs.find("pkga") != pkgs.end());
    REQUIRE(pkgs.find("pkgb") != pkgs.end());

    // pkgb is hoisted flatly into graph
    REQUIRE(pkgs.at("pkga").dependencies.size() == 1);
    REQUIRE(pkgs.at("pkga").dependencies[0] == "pkgb");
    REQUIRE(pkgs.at("pkgb").dependencies.empty());

    auto lock = graph.toLockfile();
    REQUIRE(lock.packages.size() == 2);
    REQUIRE(lock.packages.at("pkga").isPath());
    REQUIRE(lock.packages.at("pkgb").isPath());
}

TEST_CASE("DependencyGraph cycle detection", "[kattery][graph]") {
    TempDirectory temp("kattery_graph_cycle");

    // pkga -> pkgb -> pkga
    temp.createFile("pkga/pedigree.json", R"({
  "name": "pkga",
  "version": "1.0.0",
  "dependencies": {
    "pkgb": "path:../pkgb"
  }
})");

    temp.createFile("pkgb/pedigree.json", R"({
  "name": "pkgb",
  "version": "1.0.0",
  "dependencies": {
    "pkga": "path:../pkga"
  }
})");

    Manifest root;
    root.name = "root-app";
    root.version = "0.1.0";
    root.dependencies["pkga"] = SourceSpec::parse("path:./pkga");

    DependencyGraph graph(nullptr);
    try {
        graph.resolve(root, temp.path);
        FAIL("expected throw on dependency cycle");
    } catch (const PkgError& err) {
        REQUIRE(err.code() == ErrorCode::DependencyCycle);
        REQUIRE(err.diagnostic().message.find("pkga -> pkgb -> pkga") != std::string::npos);
    }
}

TEST_CASE("DependencyGraph conflict detection and root override", "[kattery][graph]") {
    TempDirectory temp("kattery_graph_conflict");

    // dep1 -> common (path:../common1)
    // dep2 -> common (path:../common2)
    temp.createFile("dep1/pedigree.json", R"({
  "name": "dep1",
  "version": "1.0.0",
  "dependencies": {
    "common": "path:../common1"
  }
})");
    temp.createFile("dep2/pedigree.json", R"({
  "name": "dep2",
  "version": "1.0.0",
  "dependencies": {
    "common": "path:../common2"
  }
})");
    temp.createFile("common1/pedigree.json", R"({"name":"common","version":"1.0.0"})");
    temp.createFile("common2/pedigree.json", R"({"name":"common","version":"2.0.0"})");

    SECTION("Conflict throws KAT3002 when both are transitive") {
        Manifest root;
        root.name = "root-app";
        root.version = "0.1.0";
        root.dependencies["dep1"] = SourceSpec::parse("path:./dep1");
        root.dependencies["dep2"] = SourceSpec::parse("path:./dep2");

        DependencyGraph graph(nullptr);
        try {
            graph.resolve(root, temp.path);
            FAIL("expected throw on conflicting dependencies");
        } catch (const PkgError& err) {
            REQUIRE(err.code() == ErrorCode::DependencyConflict);
        }
    }

    SECTION("Root declaration wins and overrides transitive requirement") {
        Manifest root;
        root.name = "root-app";
        root.version = "0.1.0";
        root.dependencies["dep1"] = SourceSpec::parse("path:./dep1");
        root.dependencies["dep2"] = SourceSpec::parse("path:./dep2");
        root.dependencies["common"] = SourceSpec::parse("path:./common1"); // Root declaration pins common1

        DependencyGraph graph(nullptr);
        REQUIRE_NOTHROW(graph.resolve(root, temp.path));
        const auto& pkgs = graph.getPackages();
        REQUIRE(pkgs.size() == 3);
        REQUIRE(pkgs.at("common").spec.path == "./common1");
    }
}
