#include <keety/resolver/topological_sorter.hpp>

namespace keety::resolver {

TopologicalSorter::TopologicalSorter(const ModuleGraph& graph)
    : graph_(graph) {}

std::vector<std::string> TopologicalSorter::sort(const std::string& entryModule, bool includeStdlib) {
    std::unordered_set<std::string> visited;
    std::vector<std::string> order;

    if (!entryModule.empty()) {
        if (graph_.hasNode(entryModule)) {
            postOrderDFS(entryModule, visited, order, includeStdlib);
        }
    } else {
        // Process in deterministic insertion order
        for (const auto& nodePath : graph_.getInsertionOrder()) {
            postOrderDFS(nodePath, visited, order, includeStdlib);
        }
        // Fallback for any nodes added outside insertion order
        for (const auto& [nodePath, _] : graph_.getNodes()) {
            postOrderDFS(nodePath, visited, order, includeStdlib);
        }
    }

    return order;
}

std::vector<const ModuleNode*> TopologicalSorter::sortNodes(const std::string& entryModule, bool includeStdlib) {
    auto pathOrder = sort(entryModule, includeStdlib);
    std::vector<const ModuleNode*> result;
    result.reserve(pathOrder.size());

    for (const auto& path : pathOrder) {
        const auto* node = graph_.getNode(path);
        if (node) {
            result.push_back(node);
        }
    }

    return result;
}

void TopologicalSorter::postOrderDFS(const std::string& u,
                                     std::unordered_set<std::string>& visited,
                                     std::vector<std::string>& order,
                                     bool includeStdlib) {
    if (visited.find(u) != visited.end()) {
        return; // Already visited and deduplicated (single instantiation)
    }
    visited.insert(u);

    const auto* node = graph_.getNode(u);
    if (node) {
        // Visit all downstream dependencies first (leaf-first)
        for (const auto& edge : node->outgoingEdges) {
            postOrderDFS(edge.toCanonicalPath, visited, order, includeStdlib);
        }

        if (!includeStdlib && node->kind == ModuleKind::StandardLibrary && node->fileId == diag::INVALID_FILE_ID) {
            return;
        }
    }

    order.push_back(u);
}

} // namespace keety::resolver
