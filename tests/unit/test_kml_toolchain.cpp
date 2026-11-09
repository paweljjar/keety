#include <catch2/catch_test_macros.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/driver/cli_driver.hpp>
#include <keety/resolver/module_graph.hpp>
#include <keety/resolver/module_path_resolver.hpp>

#include <filesystem>
#include <fstream>

using namespace keety;
using namespace keety::resolver;

TEST_CASE("KML Module Resolution & Graph Building", "[kml][resolver][driver]") {
    auto tempDir = std::filesystem::temp_directory_path() / "keety_kml_test_phase3";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    // Create a .kml component
    auto kmlPath = tempDir / "Counter.kml";
    {
        std::ofstream ofs(kmlPath);
        ofs << R"kml(
<script>
    let mut count = $state(0);
</script>
<template>
    <box><text>Count: {count}</text></box>
</template>
        )kml";
    }

    // Create a main.keet importing the component
    auto mainPath = tempDir / "main.keet";
    {
        std::ofstream ofs(mainPath);
        ofs << R"keet(
import Counter from "./Counter.kml";

fn main(): void {
    let c = new Counter();
}
        )keet";
    }

    // Test 1: ModulePathResolver resolves .kml explicitly and implicitly
    ModulePathResolver resolver(tempDir);
    auto resExplicit = resolver.resolve(mainPath, "./Counter.kml");
    REQUIRE(resExplicit.has_value());
    CHECK(resExplicit->moduleName == "Counter");
    CHECK(resExplicit->canonicalPath == std::filesystem::canonical(kmlPath));

    auto resImplicit = resolver.resolve(mainPath, "./Counter");
    REQUIRE(resImplicit.has_value());
    CHECK(resImplicit->moduleName == "Counter");
    CHECK(resImplicit->canonicalPath == std::filesystem::canonical(kmlPath));

    // Test 2: ModuleGraph automatically intercepts .kml, lowers it, and parses the module AST
    diag::SourceManager sm;
    ModuleGraph graph;
    bool ok = graph.buildGraph(mainPath, sm, resolver);
    CHECK(ok);

    std::string kmlCanonical = std::filesystem::canonical(kmlPath).string();
    const auto* kmlNode = graph.getNode(kmlCanonical);
    REQUIRE(kmlNode != nullptr);
    REQUIRE(kmlNode->ast != nullptr);
    CHECK(kmlNode->moduleName == "Counter");
    // Verify lowered declarations inside the AST include Counter class
    bool foundClass = false;
    for (const auto& decl : kmlNode->ast->declarations) {
        if (auto* classDecl = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            if (classDecl->getName() == "Counter") {
                foundClass = true;
                break;
            }
        }
    }
    CHECK(foundClass);

    // Test 3: CLI driver discover_keet_files includes .kml
    auto discovered = driver::CliDriver::discover_keet_files(tempDir);
    bool foundKmlInDiscovery = false;
    for (const auto& p : discovered) {
        if (p.filename() == "Counter.kml") {
            foundKmlInDiscovery = true;
            break;
        }
    }
    CHECK(foundKmlInDiscovery);

    std::filesystem::remove_all(tempDir);
}

