#pragma once

#include <keety/diag/source_location.hpp>
#include <keety/kml/kml_ast.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace keety::kml {

/// Represents a dynamic dependency target (e.g. node text, attribute, if-condition, for-list)
struct DependencyTarget {
    enum class Kind {
        TextInterpolation,
        AttributeBinding,
        IfCondition,
        ForIterable,
        ChildComponentProp
    };

    Kind kind{Kind::TextInterpolation};
    uint32_t nodeId{0};             // Stable ID assigned to template node
    std::string propertyName;       // For AttributeBinding or ChildComponentProp
    std::string expression;         // The bound dynamic KeetScript expression
    diag::SourceSpan span;
};

/// Dirty bit mask representation:
/// - Bits 0 to N-1: Props dirty bits (DIRTY_PROP_0, ...)
/// - Bits N to M: State dirty bits (DIRTY_STATE_0, ...)
/// - DIRTY_ALL = 0xFFFFFFFF
struct DirtyMask {
    static constexpr uint32_t DIRTY_NONE = 0x00000000;
    static constexpr uint32_t DIRTY_ALL  = 0xFFFFFFFF;

    std::string name;               // e.g. "DIRTY_PROP_title" or "DIRTY_STATE_count"
    uint32_t bitIndex{0};           // Bit shift: 1 << bitIndex
    uint32_t mask{0};               // Value: (1u << bitIndex)
};

/// Result of reactive dependency analysis on a KML document
struct ReactiveAnalysisResult {
    // Variable mappings: name -> dirty mask
    std::unordered_map<std::string, DirtyMask> propMasks;
    std::unordered_map<std::string, DirtyMask> stateMasks;
    std::vector<DirtyMask> allMasks;

    // Directed Dependency Graph: VariableName -> Set of Target IDs or Targets
    std::unordered_map<std::string, std::vector<DependencyTarget>> variableToTargets;

    // Node ID -> Combined dirty bitmask that affects this node
    std::unordered_map<uint32_t, uint32_t> nodeDirtyMasks;

    // List of all dynamic targets in document
    std::vector<DependencyTarget> allTargets;

    [[nodiscard]] uint32_t getDirtyMaskForVariable(const std::string& varName) const noexcept {
        auto itP = propMasks.find(varName);
        if (itP != propMasks.end()) return itP->second.mask;
        auto itS = stateMasks.find(varName);
        if (itS != stateMasks.end()) return itS->second.mask;
        return 0;
    }

    [[nodiscard]] uint32_t getDirtyMaskForExpression(const std::string& expr) const noexcept;
};

/// Analyzes state runes, props, and template expressions to construct the reactive dependency graph
class DependencyAnalyzer {
public:
    explicit DependencyAnalyzer(const KmlDocument& document);

    /// Runs full analysis pass
    ReactiveAnalysisResult analyze();

    /// Extracts referenced identifiers from a KeetScript expression string
    static std::unordered_set<std::string> extractIdentifiers(std::string_view expression);

private:
    void assignDirtyMasks(ReactiveAnalysisResult& result);
    void assignNodeIds(KmlDocument& doc);
    void scanTemplateNodes(const std::vector<std::unique_ptr<KmlTemplateNode>>& nodes,
                           ReactiveAnalysisResult& result,
                           uint32_t& nextNodeId);
    void scanElement(const KmlElementNode& elem,
                     uint32_t nodeId,
                     ReactiveAnalysisResult& result,
                     uint32_t& nextNodeId);

    const KmlDocument& doc_;
};

} // namespace keety::kml

