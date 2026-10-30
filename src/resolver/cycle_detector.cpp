#include <keety/resolver/cycle_detector.hpp>

#include <algorithm>
#include <queue>

namespace keety::resolver {

std::string CyclePath::formatCycleString() const {
    if (nodes.empty()) {
        return "";
    }
    std::string result;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) {
            result += " -> ";
        }
        result += nodes[i];
    }
    return result;
}

CycleDetector::CycleDetector(const ModuleGraph& graph, diag::DiagnosticEmitter* emitter)
    : graph_(graph), emitter_(emitter) {}

bool CycleDetector::detectCycles() {
    cycles_.clear();
    sccs_.clear();

    int currentIndex = 0;
    std::vector<std::string> stack;
    std::unordered_map<std::string, int> indices;
    std::unordered_map<std::string, int> lowlinks;
    std::unordered_set<std::string> onStack;

    // Evaluate nodes in deterministic insertion order
    for (const auto& nodePath : graph_.getInsertionOrder()) {
        if (indices.find(nodePath) == indices.end()) {
            strongConnect(nodePath, currentIndex, stack, indices, lowlinks, onStack);
        }
    }

    // Process any remaining nodes not in insertionOrder
    for (const auto& [nodePath, _] : graph_.getNodes()) {
        if (indices.find(nodePath) == indices.end()) {
            strongConnect(nodePath, currentIndex, stack, indices, lowlinks, onStack);
        }
    }

    return cycles_.empty();
}

void CycleDetector::strongConnect(const std::string& u,
                                  int& currentIndex,
                                  std::vector<std::string>& stack,
                                  std::unordered_map<std::string, int>& indices,
                                  std::unordered_map<std::string, int>& lowlinks,
                                  std::unordered_set<std::string>& onStack) {
    indices[u] = currentIndex;
    lowlinks[u] = currentIndex;
    currentIndex++;

    stack.push_back(u);
    onStack.insert(u);

    const auto* node = graph_.getNode(u);
    if (node) {
        for (const auto& edge : node->outgoingEdges) {
            const std::string& v = edge.toCanonicalPath;

            if (indices.find(v) == indices.end()) {
                // Successor v has not yet been visited
                strongConnect(v, currentIndex, stack, indices, lowlinks, onStack);
                lowlinks[u] = std::min(lowlinks[u], lowlinks[v]);
            } else if (onStack.find(v) != onStack.end()) {
                // Successor v is on stack and hence in the current SCC
                lowlinks[u] = std::min(lowlinks[u], indices[v]);
            }
        }
    }

    // If u is a root node, pop the stack and generate an SCC
    if (lowlinks[u] == indices[u]) {
        std::vector<std::string> scc;
        while (true) {
            std::string w = stack.back();
            stack.pop_back();
            onStack.erase(w);
            scc.push_back(w);
            if (w == u) {
                break;
            }
        }

        std::reverse(scc.begin(), scc.end());
        sccs_.push_back(scc);

        if (scc.size() > 1) {
            CyclePath cycle = extractCycle(scc);
            cycles_.push_back(cycle);
            reportCycle(cycle);
        } else if (scc.size() == 1) {
            // Check for self-loop edge (u -> u)
            if (node) {
                for (const auto& edge : node->outgoingEdges) {
                    if (edge.toCanonicalPath == u) {
                        CyclePath cycle{{u, u}, {edge}};
                        cycles_.push_back(cycle);
                        reportCycle(cycle);
                        break;
                    }
                }
            }
        }
    }
}

CyclePath CycleDetector::extractCycle(const std::vector<std::string>& sccNodes) {
    std::unordered_set<std::string> sccSet(sccNodes.begin(), sccNodes.end());
    const std::string& start = sccNodes.front();

    // Use BFS to find the shortest cycle from start back to start within sccSet
    struct QueueEntry {
        std::string current;
        std::vector<std::string> path;
        std::vector<DependencyEdge> edges;
    };

    std::queue<QueueEntry> q;

    const auto* startNode = graph_.getNode(start);
    if (startNode) {
        for (const auto& edge : startNode->outgoingEdges) {
            if (sccSet.find(edge.toCanonicalPath) != sccSet.end()) {
                q.push(QueueEntry{
                    edge.toCanonicalPath,
                    {start, edge.toCanonicalPath},
                    {edge}
                });
            }
        }
    }

    std::unordered_set<std::string> visited;

    while (!q.empty()) {
        auto entry = std::move(q.front());
        q.pop();

        if (entry.current == start) {
            return CyclePath{std::move(entry.path), std::move(entry.edges)};
        }

        if (visited.find(entry.current) != visited.end()) {
            continue;
        }
        visited.insert(entry.current);

        const auto* currNode = graph_.getNode(entry.current);
        if (!currNode) {
            continue;
        }

        for (const auto& edge : currNode->outgoingEdges) {
            if (sccSet.find(edge.toCanonicalPath) != sccSet.end()) {
                auto newPath = entry.path;
                newPath.push_back(edge.toCanonicalPath);
                auto newEdges = entry.edges;
                newEdges.push_back(edge);

                q.push(QueueEntry{
                    edge.toCanonicalPath,
                    std::move(newPath),
                    std::move(newEdges)
                });
            }
        }
    }

    // Fallback if full cycle extraction couldn't close loop
    std::vector<std::string> fallbackNodes = sccNodes;
    fallbackNodes.push_back(start);
    return CyclePath{fallbackNodes, {}};
}

void CycleDetector::reportCycle(const CyclePath& cycle) {
    if (!emitter_) {
        return;
    }

    diag::SourceSpan primarySpan;
    if (!cycle.edges.empty()) {
        primarySpan = cycle.edges[0].importSpan;
    }

    std::string cycleStr = "cycle: " + cycle.formatCycleString();

    auto diag = diag::Diagnostic::makeError(
        diag::ErrorCode::CircularModuleDependency,
        "Circular module dependency detected:",
        primarySpan.start
    );

    if (primarySpan.start.isValid()) {
        diag.withLabel(primarySpan, cycleStr);
    }

    diag.withNote("KeetScript requires the module dependency graph to be a strict Directed Acyclic Graph (DAG) for deterministic initialization.")
        .withHelp("break the dependency cycle by refactoring shared declarations into a separate common module");

    emitter_->report(diag);
}

} // namespace keety::resolver
