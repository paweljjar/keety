#pragma once

#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/error_code.hpp>
#include <keety/resolver/module_graph.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace keety::resolver {

/// Represents a circular dependency cycle path: A -> B -> ... -> A
struct CyclePath {
    std::vector<std::string> nodes;    // Canonical paths forming the cycle (starts and ends with same node)
    std::vector<DependencyEdge> edges; // Directed dependency edges along the cycle path

    [[nodiscard]] std::string formatCycleString() const;
};

/// Evaluates a ModuleGraph using Tarjan's Strongly Connected Components (SCC) algorithm.
/// Emits E1001 (CircularModuleDependency) for any component with |V_scc| > 1 or self-loops.
class CycleDetector {
public:
    explicit CycleDetector(const ModuleGraph& graph, diag::DiagnosticEmitter* emitter = nullptr);

    /// Runs Tarjan's SCC algorithm on the module graph.
    /// Returns true if the graph is a strict Directed Acyclic Graph (DAG) with no cycles.
    /// Returns false if at least one circular dependency cycle is detected.
    bool detectCycles();

    /// Returns all detected cycle paths
    [[nodiscard]] const std::vector<CyclePath>& getCycles() const noexcept {
        return cycles_;
    }

    /// Returns all strongly connected components found by Tarjan's algorithm
    [[nodiscard]] const std::vector<std::vector<std::string>>& getSCCs() const noexcept {
        return sccs_;
    }

private:
    const ModuleGraph& graph_;
    diag::DiagnosticEmitter* emitter_{nullptr};
    std::vector<CyclePath> cycles_;
    std::vector<std::vector<std::string>> sccs_;

    void strongConnect(const std::string& u,
                       int& currentIndex,
                       std::vector<std::string>& stack,
                       std::unordered_map<std::string, int>& indices,
                       std::unordered_map<std::string, int>& lowlinks,
                       std::unordered_set<std::string>& onStack);

    CyclePath extractCycle(const std::vector<std::string>& sccNodes);
    void reportCycle(const CyclePath& cycle);
};

} // namespace keety::resolver
