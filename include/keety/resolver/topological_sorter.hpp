#pragma once

#include <keety/resolver/module_graph.hpp>

#include <string>
#include <unordered_set>
#include <vector>

namespace keety::resolver {

/// Computes the deterministic leaf-first topological initialization order of modules
/// using post-order Depth-First Search (DFS) according to the
/// Keety Compiler Architecture Specification (Stage 3).
class TopologicalSorter {
public:
    explicit TopologicalSorter(const ModuleGraph& graph);

    /// Computes the leaf-first topological initialization order.
    /// If entryModule is specified, orders only the transitive dependencies reachable from entryModule,
    /// with entryModule being the final initialized module.
    /// If entryModule is empty, orders all modules in the graph.
    /// If includeStdlib is false, built-in virtual stdlib modules are excluded.
    [[nodiscard]] std::vector<std::string> sort(const std::string& entryModule = "",
                                                bool includeStdlib = true);

    /// Returns the leaf-first topological initialization order as a list of const ModuleNode* pointers.
    /// Guaranteed to contain each reachable module exactly once (single-instantiation deduplication).
    [[nodiscard]] std::vector<const ModuleNode*> sortNodes(const std::string& entryModule = "",
                                                           bool includeStdlib = true);

private:
    const ModuleGraph& graph_;

    void postOrderDFS(const std::string& u,
                      std::unordered_set<std::string>& visited,
                      std::vector<std::string>& order,
                      bool includeStdlib);
};

} // namespace keety::resolver
