#include <catch2/catch_test_macros.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/resolver/module_graph.hpp>
#include <keety/resolver/module_path_resolver.hpp>
#include <keety/resolver/topological_sorter.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

using namespace keety::resolver;
using namespace keety::diag;

namespace fs = std::filesystem;

/// RAII helper for temporary test directories
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

    void createFile(const fs::path& relPath, const std::string& content = "") {
        fs::path fullPath = path / relPath;
        fs::create_directories(fullPath.parent_path());
        std::ofstream ofs(fullPath);
        ofs << content;
    }
};

TEST_CASE("TopologicalSorter linear chain leaf-first order", "[resolver]") {
    // main -> service -> db
    ModuleGraph graph;
    graph.addEdge("main", "service");
    graph.addEdge("service", "db");

    TopologicalSorter sorter(graph);
    auto order = sorter.sort("main");

    REQUIRE(order.size() == 3);
    REQUIRE(order[0] == "db");
    REQUIRE(order[1] == "service");
    REQUIRE(order[2] == "main");
}

TEST_CASE("TopologicalSorter diamond dependency deduplication (single-instantiation)", "[resolver]") {
    // Diamond:
    //       main
    //      /    \
    //     A      B
    //      \    /
    //      common
    ModuleGraph graph;
    graph.addEdge("main", "A");
    graph.addEdge("main", "B");
    graph.addEdge("A", "common");
    graph.addEdge("B", "common");

    TopologicalSorter sorter(graph);
    auto order = sorter.sort("main");

    REQUIRE(order.size() == 4);

    // common must appear before both A and B
    auto itCommon = std::find(order.begin(), order.end(), "common");
    auto itA = std::find(order.begin(), order.end(), "A");
    auto itB = std::find(order.begin(), order.end(), "B");
    auto itMain = std::find(order.begin(), order.end(), "main");

    REQUIRE(itCommon != order.end());
    REQUIRE(itA != order.end());
    REQUIRE(itB != order.end());
    REQUIRE(itMain != order.end());

    REQUIRE(itCommon < itA);
    REQUIRE(itCommon < itB);
    REQUIRE(itA < itMain);
    REQUIRE(itB < itMain);

    // common appears exactly once!
    REQUIRE(std::count(order.begin(), order.end(), "common") == 1);
}

TEST_CASE("TopologicalSorter multi-level DAG with stdlib modules", "[resolver]") {
    // main -> (service, client)
    // service -> (db, config, io)
    // client -> (config, io)
    // db -> config
    ModuleGraph graph;
    graph.addEdge("main", "service");
    graph.addEdge("main", "client");
    graph.addEdge("service", "db");
    graph.addEdge("service", "config");
    graph.addEdge("service", "<stdlib>/io");
    graph.addEdge("client", "config");
    graph.addEdge("client", "<stdlib>/io");
    graph.addEdge("db", "config");

    TopologicalSorter sorter(graph);

    SECTION("Including standard library modules") {
        auto order = sorter.sort("main", true);
        REQUIRE(order.size() == 6);

        auto itConfig = std::find(order.begin(), order.end(), "config");
        auto itDb = std::find(order.begin(), order.end(), "db");
        auto itService = std::find(order.begin(), order.end(), "service");
        auto itClient = std::find(order.begin(), order.end(), "client");
        auto itMain = std::find(order.begin(), order.end(), "main");

        // config must precede db, service, and client
        REQUIRE(itConfig < itDb);
        REQUIRE(itConfig < itService);
        REQUIRE(itConfig < itClient);

        // db must precede service
        REQUIRE(itDb < itService);

        // service and client must precede main
        REQUIRE(itService < itMain);
        REQUIRE(itClient < itMain);
    }
}

TEST_CASE("TopologicalSorter end-to-end on filesystem diamond modules", "[resolver]") {
    TempDirectory temp("keety_topo_fs_test");
    temp.createFile("src/main.keet", "import \"./auth\";\nimport \"./payment\";\n");
    temp.createFile("src/auth.keet", "import \"./database\";\nexport const AUTH_OK: int = 1;\n");
    temp.createFile("src/payment.keet", "import \"./database\";\nexport const PAY_OK: int = 1;\n");
    temp.createFile("src/database.keet", "export const DB_CONN: string = \"postgres\";\n");

    SourceManager sm;
    DiagnosticEmitter emitter(sm, ColorMode::Never);
    ModulePathResolver pathResolver(temp.path, &emitter);

    ModuleGraph graph;
    fs::path mainPath = temp.path / "src" / "main.keet";
    bool success = graph.buildGraph(mainPath, sm, pathResolver, &emitter);

    REQUIRE(success);
    REQUIRE(graph.vertexCount() == 4);

    TopologicalSorter sorter(graph);
    std::string mainCanonical = fs::canonical(mainPath).string();
    auto nodes = sorter.sortNodes(mainCanonical);

    REQUIRE(nodes.size() == 4);

    // Each node has its parsed AST (single-instantiation)
    for (const auto* node : nodes) {
        REQUIRE(node != nullptr);
        REQUIRE(node->ast != nullptr);
    }

    // database.keet must be first
    std::string dbCanonical = fs::canonical(temp.path / "src" / "database.keet").string();
    std::string authCanonical = fs::canonical(temp.path / "src" / "auth.keet").string();
    std::string paymentCanonical = fs::canonical(temp.path / "src" / "payment.keet").string();

    REQUIRE(nodes[0]->canonicalPath == dbCanonical);
    REQUIRE(nodes[3]->canonicalPath == mainCanonical);

    // Either auth then payment or payment then auth
    bool middleOk = (nodes[1]->canonicalPath == authCanonical && nodes[2]->canonicalPath == paymentCanonical) ||
                    (nodes[1]->canonicalPath == paymentCanonical && nodes[2]->canonicalPath == authCanonical);
    REQUIRE(middleOk);
}
