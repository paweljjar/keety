#include <catch2/catch_test_macros.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/resolver/module_graph.hpp>

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

TEST_CASE("ModuleGraph manual vertex and edge construction", "[resolver]") {
    ModuleGraph graph;

    REQUIRE(graph.vertexCount() == 0);
    REQUIRE(graph.edgeCount() == 0);

    // Add nodes
    graph.getOrCreateNode("/path/to/main.keet", "main");
    graph.getOrCreateNode("/path/to/config.keet", "config");
    graph.getOrCreateNode("/path/to/logger.keet", "logger");

    REQUIRE(graph.vertexCount() == 3);
    REQUIRE(graph.hasNode("/path/to/main.keet"));
    REQUIRE(graph.hasNode("/path/to/config.keet"));
    REQUIRE(graph.hasNode("/path/to/logger.keet"));
    REQUIRE_FALSE(graph.hasNode("/path/to/unknown.keet"));

    // Add edges
    graph.addEdge("/path/to/main.keet", "/path/to/config.keet");
    graph.addEdge("/path/to/main.keet", "/path/to/logger.keet");
    graph.addEdge("/path/to/logger.keet", "/path/to/config.keet");

    REQUIRE(graph.edgeCount() == 3);

    const auto* mainNode = graph.getNode("/path/to/main.keet");
    REQUIRE(mainNode != nullptr);
    REQUIRE(mainNode->outgoingEdges.size() == 2);

    const auto* loggerNode = graph.getNode("/path/to/logger.keet");
    REQUIRE(loggerNode != nullptr);
    REQUIRE(loggerNode->outgoingEdges.size() == 1);
    REQUIRE(loggerNode->outgoingEdges[0].toCanonicalPath == "/path/to/config.keet");

    // Edge deduplication
    graph.addEdge("/path/to/main.keet", "/path/to/config.keet");
    REQUIRE(graph.edgeCount() == 3);
    REQUIRE(mainNode->outgoingEdges.size() == 2);
}

TEST_CASE("ModuleGraph builds from entry file with imports and stdlib", "[resolver]") {
    TempDirectory temp("keety_graph_test");
    temp.createFile("src/main.keet",
                    "import io;\n"
                    "import \"./config\";\n"
                    "import \"./service\";\n"
                    "fn main(): void {}\n");
    temp.createFile("src/config.keet",
                    "export const PORT: int = 8080;\n");
    temp.createFile("src/service.keet",
                    "import \"./config\";\n"
                    "export fn start(): void {}\n");

    SourceManager sm;
    DiagnosticEmitter emitter(sm, ColorMode::Never);
    ModulePathResolver pathResolver(temp.path, &emitter);

    ModuleGraph graph;
    fs::path mainPath = temp.path / "src" / "main.keet";
    bool success = graph.buildGraph(mainPath, sm, pathResolver, &emitter);

    REQUIRE(success);
    REQUIRE_FALSE(emitter.hasErrors());

    // Vertices: main.keet, <stdlib>/io, config.keet, service.keet
    REQUIRE(graph.vertexCount() == 4);

    std::string mainCanonical = fs::canonical(mainPath).string();
    std::string configCanonical = fs::canonical(temp.path / "src" / "config.keet").string();
    std::string serviceCanonical = fs::canonical(temp.path / "src" / "service.keet").string();

    REQUIRE(graph.hasNode(mainCanonical));
    REQUIRE(graph.hasNode(configCanonical));
    REQUIRE(graph.hasNode(serviceCanonical));
    REQUIRE(graph.hasNode("<stdlib>/io"));

    // Check main dependencies (io, config, service)
    const auto* mainNode = graph.getNode(mainCanonical);
    REQUIRE(mainNode != nullptr);
    REQUIRE(mainNode->outgoingEdges.size() == 3);
    REQUIRE(mainNode->ast != nullptr);

    // Check service dependencies (config)
    const auto* serviceNode = graph.getNode(serviceCanonical);
    REQUIRE(serviceNode != nullptr);
    REQUIRE(serviceNode->outgoingEdges.size() == 1);
    REQUIRE(serviceNode->outgoingEdges[0].toCanonicalPath == configCanonical);
    REQUIRE(serviceNode->ast != nullptr);

    // Check config dependencies (none)
    const auto* configNode = graph.getNode(configCanonical);
    REQUIRE(configNode != nullptr);
    REQUIRE(configNode->outgoingEdges.empty());
    REQUIRE(configNode->ast != nullptr);
}

TEST_CASE("ModuleGraph diamond dependency deduplication", "[resolver]") {
    // Diamond dependency:
    //      main
    //     /    \
    //    A      B
    //     \    /
    //     common
    TempDirectory temp("keety_diamond_test");
    temp.createFile("src/main.keet", "import \"./a\";\nimport \"./b\";\n");
    temp.createFile("src/a.keet", "import \"./common\";\nexport const A: int = 1;\n");
    temp.createFile("src/b.keet", "import \"./common\";\nexport const B: int = 2;\n");
    temp.createFile("src/common.keet", "export const VER: int = 100;\n");

    SourceManager sm;
    DiagnosticEmitter emitter(sm, ColorMode::Never);
    ModulePathResolver pathResolver(temp.path, &emitter);

    ModuleGraph graph;
    fs::path mainPath = temp.path / "src" / "main.keet";
    bool success = graph.buildGraph(mainPath, sm, pathResolver, &emitter);

    REQUIRE(success);
    REQUIRE(graph.vertexCount() == 4);

    std::string commonCanonical = fs::canonical(temp.path / "src" / "common.keet").string();
    const auto* commonNode = graph.getNode(commonCanonical);
    REQUIRE(commonNode != nullptr);
    REQUIRE(commonNode->ast != nullptr);

    // Total edges: main->a, main->b, a->common, b->common = 4
    REQUIRE(graph.edgeCount() == 4);
}

TEST_CASE("ModuleGraph safely terminates recursion and records cyclic dependencies", "[resolver]") {
    // Cycle: auth -> user -> auth
    TempDirectory temp("keety_cycle_test");
    temp.createFile("src/auth.keet", "import \"./user\";\nexport const AUTH: int = 1;\n");
    temp.createFile("src/user.keet", "import \"./auth\";\nexport const USER: int = 2;\n");

    SourceManager sm;
    DiagnosticEmitter emitter(sm, ColorMode::Never);
    ModulePathResolver pathResolver(temp.path, &emitter);

    ModuleGraph graph;
    fs::path authPath = temp.path / "src" / "auth.keet";

    // Graph construction terminates safely without stack overflow
    bool success = graph.buildGraph(authPath, sm, pathResolver, &emitter);
    REQUIRE(success);

    std::string authCanonical = fs::canonical(authPath).string();
    std::string userCanonical = fs::canonical(temp.path / "src" / "user.keet").string();

    REQUIRE(graph.vertexCount() == 2);
    REQUIRE(graph.edgeCount() == 2);

    const auto* authNode = graph.getNode(authCanonical);
    REQUIRE(authNode != nullptr);
    REQUIRE(authNode->outgoingEdges.size() == 1);
    REQUIRE(authNode->outgoingEdges[0].toCanonicalPath == userCanonical);

    const auto* userNode = graph.getNode(userCanonical);
    REQUIRE(userNode != nullptr);
    REQUIRE(userNode->outgoingEdges.size() == 1);
    REQUIRE(userNode->outgoingEdges[0].toCanonicalPath == authCanonical);
}
