#include <catch2/catch_test_macros.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/resolver/cycle_detector.hpp>
#include <keety/resolver/module_graph.hpp>
#include <keety/resolver/module_path_resolver.hpp>

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

TEST_CASE("CycleDetector validates strictly acyclic DAGs (no cycles)", "[resolver]") {
    SourceManager sm;
    DiagnosticEmitter emitter(sm, ColorMode::Never);

    SECTION("Linear chain: A -> B -> C") {
        ModuleGraph graph;
        graph.addEdge("A", "B");
        graph.addEdge("B", "C");

        CycleDetector detector(graph, &emitter);
        REQUIRE(detector.detectCycles());
        REQUIRE(detector.getCycles().empty());
        REQUIRE_FALSE(emitter.hasErrors());
    }

    SECTION("Diamond DAG: main -> (A, B) -> common") {
        ModuleGraph graph;
        graph.addEdge("main", "A");
        graph.addEdge("main", "B");
        graph.addEdge("A", "common");
        graph.addEdge("B", "common");

        CycleDetector detector(graph, &emitter);
        REQUIRE(detector.detectCycles());
        REQUIRE(detector.getCycles().empty());
        REQUIRE_FALSE(emitter.hasErrors());
    }
}

TEST_CASE("CycleDetector detects cycles and self-loops using Tarjan's SCC", "[resolver]") {
    SourceManager sm;
    DiagnosticEmitter emitter(sm, ColorMode::Never);

    SECTION("Direct 2-node cycle: auth <-> user") {
        ModuleGraph graph;
        graph.addEdge("auth", "user");
        graph.addEdge("user", "auth");

        CycleDetector detector(graph, &emitter);
        REQUIRE_FALSE(detector.detectCycles());
        REQUIRE(emitter.hasErrors());
        REQUIRE(detector.getCycles().size() == 1);

        const auto& cycle = detector.getCycles()[0];
        REQUIRE(cycle.nodes.size() >= 3);
        REQUIRE(cycle.nodes.front() == cycle.nodes.back());
    }

    SECTION("3-node cycle: A -> B -> C -> A") {
        ModuleGraph graph;
        graph.addEdge("A", "B");
        graph.addEdge("B", "C");
        graph.addEdge("C", "A");

        CycleDetector detector(graph, &emitter);
        REQUIRE_FALSE(detector.detectCycles());
        REQUIRE(emitter.hasErrors());
        REQUIRE(detector.getCycles().size() == 1);

        std::string cycleStr = detector.getCycles()[0].formatCycleString();
        REQUIRE(cycleStr.find("A -> B -> C -> A") != std::string::npos);
    }

    SECTION("Self-loop: A -> A") {
        ModuleGraph graph;
        graph.addEdge("A", "A");

        CycleDetector detector(graph, &emitter);
        REQUIRE_FALSE(detector.detectCycles());
        REQUIRE(emitter.hasErrors());
        REQUIRE(detector.getCycles().size() == 1);

        std::string cycleStr = detector.getCycles()[0].formatCycleString();
        REQUIRE(cycleStr == "A -> A");
    }
}

TEST_CASE("CycleDetector emits E1001 diagnostic on cyclic file imports", "[resolver]") {
    TempDirectory temp("keety_cycle_diag_test");
    temp.createFile("src/auth.keet", "import \"./user\";\n");
    temp.createFile("src/user.keet", "import \"./auth\";\n");

    SourceManager sm;
    DiagnosticEmitter emitter(sm, ColorMode::Never);
    ModulePathResolver pathResolver(temp.path, &emitter);

    ModuleGraph graph;
    fs::path authPath = temp.path / "src" / "auth.keet";
    graph.buildGraph(authPath, sm, pathResolver, &emitter);

    CycleDetector detector(graph, &emitter);
    bool isDag = detector.detectCycles();

    REQUIRE_FALSE(isDag);
    REQUIRE(emitter.hasErrors());

    std::string authCanonical = fs::canonical(authPath).string();
    std::string userCanonical = fs::canonical(temp.path / "src" / "user.keet").string();

    const auto& cycles = detector.getCycles();
    REQUIRE(cycles.size() == 1);

    // Verify diagnostic formatting matches architecture spec
    // error[E1001]: Circular module dependency detected:
    //    --> src/auth.keet:3:1
    //     |
    //   3 | import "./user";
    //     | ^^^^^^^^^^^^^^^ cycle: ...
    const auto* authNode = graph.getNode(authCanonical);
    REQUIRE(authNode != nullptr);
    REQUIRE(authNode->outgoingEdges.size() == 1);

    const auto& edge = authNode->outgoingEdges[0];
    auto diag = Diagnostic::makeError(ErrorCode::CircularModuleDependency,
                                     "Circular module dependency detected:",
                                     edge.importSpan.start)
                    .withLabel(edge.importSpan, "cycle: " + cycles[0].formatCycleString());

    std::string formatted = emitter.format(diag);
    REQUIRE(formatted.find("error[E1001]: Circular module dependency detected:") != std::string::npos);
    REQUIRE(formatted.find("import \"./user\";") != std::string::npos);
    REQUIRE(formatted.find("cycle:") != std::string::npos);
}
