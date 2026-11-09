#include <keety/kml/dependency_analyzer.hpp>
#include <cctype>

namespace keety::kml {

std::unordered_set<std::string> DependencyAnalyzer::extractIdentifiers(std::string_view expression) {
    std::unordered_set<std::string> identifiers;

    size_t i = 0;
    while (i < expression.size()) {
        char c = expression[i];

        // Skip strings
        if (c == '"' || c == '\'' || c == '`') {
            char quote = c;
            i++;
            while (i < expression.size() && expression[i] != quote) {
                if (expression[i] == '\\' && i + 1 < expression.size()) {
                    i += 2;
                } else {
                    i++;
                }
            }
            if (i < expression.size()) i++;
            continue;
        }

        // Identifier start: [a-zA-Z_]
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            while (i < expression.size() && (std::isalnum(static_cast<unsigned char>(expression[i])) || expression[i] == '_')) {
                i++;
            }
            std::string ident = std::string(expression.substr(start, i - start));

            // Filter out language keywords
            if (ident != "true" && ident != "false" && ident != "null" &&
                ident != "let" && ident != "const" && ident != "mut" &&
                ident != "fn" && ident != "return" && ident != "if" &&
                ident != "else" && ident != "for" && ident != "in" &&
                ident != "while" && ident != "import" && ident != "export") {
                identifiers.insert(ident);
            }
            continue;
        }

        i++;
    }

    return identifiers;
}

uint32_t ReactiveAnalysisResult::getDirtyMaskForExpression(const std::string& expr) const noexcept {
    auto idents = DependencyAnalyzer::extractIdentifiers(expr);
    uint32_t mask = 0;
    for (const auto& id : idents) {
        auto itP = propMasks.find(id);
        if (itP != propMasks.end()) {
            mask |= itP->second.mask;
        }
        auto itS = stateMasks.find(id);
        if (itS != stateMasks.end()) {
            mask |= itS->second.mask;
        }
    }
    return mask;
}

DependencyAnalyzer::DependencyAnalyzer(const KmlDocument& document)
    : doc_(document) {}

void DependencyAnalyzer::assignDirtyMasks(ReactiveAnalysisResult& result) {
    uint32_t currentBit = 0;

    // 1. Assign Props dirty bits: 0 to N-1
    if (doc_.script) {
        for (const auto& prop : doc_.script->props) {
            if (currentBit >= 31) break; // Max 32 bits
            DirtyMask m;
            m.name = "DIRTY_PROP_" + prop.name;
            m.bitIndex = currentBit;
            m.mask = (1u << currentBit);
            result.propMasks[prop.name] = m;
            result.allMasks.push_back(m);
            currentBit++;
        }

        // 2. Assign State dirty bits: N to M
        for (const auto& rune : doc_.script->runes) {
            if (rune.kind == KmlStateRune::Kind::State && !rune.variableName.empty()) {
                if (currentBit >= 31) break;
                DirtyMask m;
                m.name = "DIRTY_STATE_" + rune.variableName;
                m.bitIndex = currentBit;
                m.mask = (1u << currentBit);
                result.stateMasks[rune.variableName] = m;
                result.allMasks.push_back(m);
                currentBit++;
            }
        }
    }
}

void DependencyAnalyzer::scanTemplateNodes(const std::vector<std::unique_ptr<KmlTemplateNode>>& nodes,
                                           ReactiveAnalysisResult& result,
                                           uint32_t& nextNodeId) {
    for (const auto& node : nodes) {
        if (!node) continue;
        uint32_t nodeId = ++nextNodeId;

        switch (node->getKind()) {
            case KmlNodeKind::Text: {
                auto* textNode = static_cast<const KmlTextNode*>(node.get());
                if (textNode->isInterpolated()) {
                    DependencyTarget target;
                    target.kind = DependencyTarget::Kind::TextInterpolation;
                    target.nodeId = nodeId;
                    target.expression = textNode->getText();
                    target.span = textNode->getSpan();

                    uint32_t mask = result.getDirtyMaskForExpression(target.expression);
                    result.nodeDirtyMasks[nodeId] |= mask;

                    auto idents = extractIdentifiers(target.expression);
                    for (const auto& id : idents) {
                        result.variableToTargets[id].push_back(target);
                    }
                    result.allTargets.push_back(std::move(target));
                }
                break;
            }

            case KmlNodeKind::Element: {
                auto* elemNode = static_cast<const KmlElementNode*>(node.get());
                scanElement(*elemNode, nodeId, result, nextNodeId);
                break;
            }

            case KmlNodeKind::IfBlock: {
                auto* ifBlock = static_cast<const KmlIfBlock*>(node.get());
                DependencyTarget target;
                target.kind = DependencyTarget::Kind::IfCondition;
                target.nodeId = nodeId;
                target.expression = ifBlock->getCondition();
                target.span = ifBlock->getSpan();

                uint32_t mask = result.getDirtyMaskForExpression(target.expression);
                result.nodeDirtyMasks[nodeId] |= mask;

                auto idents = extractIdentifiers(target.expression);
                for (const auto& id : idents) {
                    result.variableToTargets[id].push_back(target);
                }
                result.allTargets.push_back(std::move(target));

                scanTemplateNodes(ifBlock->getThenBranch(), result, nextNodeId);
                for (const auto& elseIf : ifBlock->getElseIfBranches()) {
                    scanTemplateNodes(elseIf.body, result, nextNodeId);
                }
                scanTemplateNodes(ifBlock->getElseBranch(), result, nextNodeId);
                break;
            }

            case KmlNodeKind::ForBlock: {
                auto* forBlock = static_cast<const KmlForBlock*>(node.get());
                DependencyTarget target;
                target.kind = DependencyTarget::Kind::ForIterable;
                target.nodeId = nodeId;
                target.expression = forBlock->getIterableExpr();
                target.span = forBlock->getSpan();

                uint32_t mask = result.getDirtyMaskForExpression(target.expression);
                result.nodeDirtyMasks[nodeId] |= mask;

                auto idents = extractIdentifiers(target.expression);
                for (const auto& id : idents) {
                    result.variableToTargets[id].push_back(target);
                }
                result.allTargets.push_back(std::move(target));

                scanTemplateNodes(forBlock->getBody(), result, nextNodeId);
                break;
            }

            case KmlNodeKind::AwaitBlock:
            case KmlNodeKind::Slot:
                break;
        }
    }
}

void DependencyAnalyzer::scanElement(const KmlElementNode& elem,
                                    uint32_t nodeId,
                                    ReactiveAnalysisResult& result,
                                    uint32_t& nextNodeId) {
    for (const auto& attr : elem.getAttributes()) {
        if (attr.isExpression) {
            DependencyTarget target;
            target.kind = elem.isComponent() ? DependencyTarget::Kind::ChildComponentProp
                                             : DependencyTarget::Kind::AttributeBinding;
            target.nodeId = nodeId;
            target.propertyName = attr.name;
            target.expression = attr.value;
            target.span = attr.span;

            uint32_t mask = result.getDirtyMaskForExpression(target.expression);
            result.nodeDirtyMasks[nodeId] |= mask;

            auto idents = extractIdentifiers(target.expression);
            for (const auto& id : idents) {
                result.variableToTargets[id].push_back(target);
            }
            result.allTargets.push_back(std::move(target));
        }
    }

    scanTemplateNodes(elem.getChildren(), result, nextNodeId);
}

ReactiveAnalysisResult DependencyAnalyzer::analyze() {
    ReactiveAnalysisResult result;
    assignDirtyMasks(result);

    uint32_t nextNodeId = 0;
    scanTemplateNodes(doc_.templateNodes, result, nextNodeId);

    return result;
}

} // namespace keety::kml

