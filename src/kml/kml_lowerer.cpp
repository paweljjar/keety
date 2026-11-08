#include <keety/kml/kml_lowerer.hpp>
#include <iomanip>
#include <sstream>

namespace keety::kml {

KmlLowerer::KmlLowerer(const KmlDocument& doc, diag::SourceManager* sourceMgr)
    : doc_(doc), sourceMgr_(sourceMgr) {}

std::string KmlLowerer::desugarTwoWayBinding(const KmlDirective& directive, const std::string& stateVar) {
    // $bind.value={stateVar} -> property value={stateVar} + setter @input={(val) => { stateVar = val; }}
    std::string prop = directive.targetProperty.empty() ? "value" : directive.targetProperty;
    std::stringstream ss;
    ss << "prop: " << prop << "={" << stateVar << "}, ";
    ss << "on" << static_cast<char>(std::toupper(static_cast<unsigned char>(prop[0]))) << prop.substr(1)
       << "Change={(val) => { " << stateVar << " = val; }}";
    return ss.str();
}

std::string KmlLowerer::desugarEventModifiers(const std::string& rawHandler, const std::vector<std::string>& modifiers) {
    std::string wrapped = rawHandler;

    for (const auto& mod : modifiers) {
        if (mod.starts_with("debounce")) {
            // e.g. debounce(150)
            std::string ms = "150";
            size_t pOpen = mod.find('(');
            size_t pClose = mod.find(')');
            if (pOpen != std::string::npos && pClose != std::string::npos && pClose > pOpen + 1) {
                ms = mod.substr(pOpen + 1, pClose - pOpen - 1);
            }
            wrapped = "std.ui.debounce(" + wrapped + ", " + ms + ")";
        } else if (mod == "prevent") {
            wrapped = "std.ui.withPreventDefault(" + wrapped + ")";
        } else if (mod == "enter") {
            wrapped = "std.ui.withKeyFilter(" + wrapped + ", Key.Enter)";
        }
    }

    return wrapped;
}

std::string KmlLowerer::synthesizeConstructor([[maybe_unused]] const ReactiveAnalysisResult& analysis) {
    std::stringstream ss;
    ss << "    pub fn constructor(";

    // Props parameters
    if (doc_.script) {
        bool first = true;
        for (const auto& prop : doc_.script->props) {
            if (!first) ss << ", ";
            first = false;
            ss << prop.name << ": " << (prop.type.empty() ? "any" : prop.type);
            if (!prop.defaultValueExpr.empty()) {
                ss << " = " << prop.defaultValueExpr;
            }
        }
    }

    ss << "): void {\n";
    ss << "        super.constructor();\n";

    // Initialize props fields
    if (doc_.script) {
        for (const auto& prop : doc_.script->props) {
            ss << "        this." << prop.name << " = " << prop.name << ";\n";
        }
        // Initialize state runes
        for (const auto& rune : doc_.script->runes) {
            if (rune.kind == KmlStateRune::Kind::State && !rune.variableName.empty()) {
                ss << "        this." << rune.variableName << " = "
                   << (rune.initExpr.empty() ? "null" : rune.initExpr) << ";\n";
            }
        }
    }

    ss << "        this.dirtyMask = DIRTY_ALL;\n";
    ss << "    }\n";
    return ss.str();
}

void KmlLowerer::generateNodeMountCode(const KmlTemplateNode* node, std::stringstream& ss, int& nodeIndex, int parentIndex) {
    if (!node) return;
    int currentIndex = ++nodeIndex;

    switch (node->getKind()) {
        case KmlNodeKind::Element: {
            auto* elem = static_cast<const KmlElementNode*>(node);
            std::string tag = elem->getTagName();
            std::string nodeType = "BoxNode";
            if (tag == "text" || tag == "span" || tag == "p") nodeType = "TextNode";
            else if (tag == "button") nodeType = "ButtonNode";
            else if (tag == "input") nodeType = "InputNode";
            else if (tag == "scroll") nodeType = "ScrollNode";
            else if (elem->isComponent()) nodeType = tag;

            ss << "        let node_" << currentIndex << " = new std.ui." << nodeType << "();\n";
            ss << "        node_" << currentIndex << ".id = " << currentIndex << ";\n";

            // Static or dynamic attributes
            for (const auto& attr : elem->getAttributes()) {
                if (!attr.isExpression) {
                    ss << "        node_" << currentIndex << ".setAttribute(\"" << attr.name << "\", \"" << attr.value << "\");\n";
                } else {
                    ss << "        node_" << currentIndex << ".setAttribute(\"" << attr.name << "\", this." << attr.value << ");\n";
                }
            }

            // Directives & Event listeners
            for (const auto& dir : elem->getDirectives()) {
                std::string handler = dir.expression.empty() ? "() => {}" : dir.expression;
                if (!dir.modifiers.empty()) {
                    handler = desugarEventModifiers(handler, dir.modifiers);
                }
                ss << "        node_" << currentIndex << ".on(\"" << dir.name << "\", " << handler << ");\n";
            }

            if (parentIndex >= 0) {
                ss << "        node_" << parentIndex << ".appendChild(node_" << currentIndex << ");\n";
            } else {
                ss << "        this.rootNode = node_" << currentIndex << ";\n";
            }

            for (const auto& child : elem->getChildren()) {
                generateNodeMountCode(child.get(), ss, nodeIndex, currentIndex);
            }
            break;
        }

        case KmlNodeKind::Text: {
            auto* text = static_cast<const KmlTextNode*>(node);
            ss << "        let node_" << currentIndex << " = new std.ui.TextNode();\n";
            ss << "        node_" << currentIndex << ".id = " << currentIndex << ";\n";
            if (text->isInterpolated()) {
                ss << "        node_" << currentIndex << ".content = String(this." << text->getText() << ");\n";
            } else {
                ss << "        node_" << currentIndex << ".content = \"" << text->getText() << "\";\n";
            }
            if (parentIndex >= 0) {
                ss << "        node_" << parentIndex << ".appendChild(node_" << currentIndex << ");\n";
            }
            break;
        }

        case KmlNodeKind::IfBlock: {
            auto* ifBlock = static_cast<const KmlIfBlock*>(node);
            ss << "        // IfBlock dynamic mount\n";
            ss << "        if (this." << ifBlock->getCondition() << ") {\n";
            for (const auto& child : ifBlock->getThenBranch()) {
                generateNodeMountCode(child.get(), ss, nodeIndex, parentIndex);
            }
            ss << "        }\n";
            break;
        }

        case KmlNodeKind::ForBlock: {
            auto* forBlock = static_cast<const KmlForBlock*>(node);
            ss << "        // ForBlock mount\n";
            ss << "        for (let " << forBlock->getItemVar() << " in this." << forBlock->getIterableExpr() << ") {\n";
            for (const auto& child : forBlock->getBody()) {
                generateNodeMountCode(child.get(), ss, nodeIndex, parentIndex);
            }
            ss << "        }\n";
            break;
        }

        case KmlNodeKind::AwaitBlock:
        case KmlNodeKind::Slot:
            break;
    }
}

std::string KmlLowerer::synthesizeMount([[maybe_unused]] const ReactiveAnalysisResult& analysis) {
    std::stringstream ss;
    ss << "    pub fn mount(): std.ui.UINode {\n";
    int nodeIndex = 0;
    for (const auto& rootNode : doc_.templateNodes) {
        generateNodeMountCode(rootNode.get(), ss, nodeIndex, -1);
    }
    ss << "        return this.rootNode;\n";
    ss << "    }\n";
    return ss.str();
}

std::string KmlLowerer::synthesizeUpdate(const ReactiveAnalysisResult& analysis) {
    std::stringstream ss;
    ss << "    pub fn update(dirtyMask: int): void {\n";

    // Branch on dirty masks
    for (const auto& target : analysis.allTargets) {
        uint32_t mask = analysis.getDirtyMaskForExpression(target.expression);
        if (mask != 0) {
            ss << "        if ((dirtyMask & 0x" << std::hex << mask << ") != 0) {\n";
            if (target.kind == DependencyTarget::Kind::TextInterpolation) {
                ss << "            this.findNodeById(" << std::dec << target.nodeId
                   << ").content = String(this." << target.expression << ");\n";
            } else if (target.kind == DependencyTarget::Kind::AttributeBinding) {
                ss << "            this.findNodeById(" << std::dec << target.nodeId
                   << ").setAttribute(\"" << target.propertyName << "\", this." << target.expression << ");\n";
            } else if (target.kind == DependencyTarget::Kind::ChildComponentProp) {
                ss << "            this.findChildById(" << std::dec << target.nodeId
                   << ").set" << static_cast<char>(std::toupper(static_cast<unsigned char>(target.propertyName[0])))
                   << target.propertyName.substr(1) << "(this." << target.expression << ");\n";
            }
            ss << "        }\n";
        }
    }

    ss << "        this.dirtyMask = 0;\n";
    ss << "    }\n";
    return ss.str();
}

std::string KmlLowerer::synthesizeRenderVdom([[maybe_unused]] const ReactiveAnalysisResult& analysis) {
    std::stringstream ss;
    ss << "    pub fn renderVdom(): std.ui.VNode {\n";
    ss << "        let root = new std.ui.VNode(\"div\");\n";
    ss << "        root.addClass(\"" << doc_.componentName << "\");\n";
    ss << "        return root;\n";
    ss << "    }\n";
    return ss.str();
}

std::string KmlLowerer::synthesizeDestroy() {
    std::stringstream ss;
    ss << "    pub fn destroy(): void {\n";
    ss << "        super.destroy();\n";
    ss << "    }\n";
    return ss.str();
}

LoweredComponentResult KmlLowerer::lower() {
    LoweredComponentResult res;
    res.componentName = doc_.componentName;

    // 1. Dependency Analysis
    DependencyAnalyzer analyzer(doc_);
    res.analysis = analyzer.analyze();

    // 2. Style Scoping
    StyleScoper scoper(doc_);
    auto styleRes = scoper.lower(StyleTarget::Web);
    res.scopeClass = styleRes.scopeClass;
    res.scopedCss = styleRes.scopedCss;

    // 3. Synthesize Component Class
    std::stringstream ss;

    // Dirty bit constants
    ss << "// Dirty Bit Mask Constants\n";
    for (const auto& mask : res.analysis.allMasks) {
        ss << "const " << mask.name << ": int = 0x" << std::hex << mask.mask << ";\n";
    }
    ss << "const DIRTY_ALL: int = 0xFFFFFFFF;\n\n";

    // Class header
    ss << "export class " << doc_.componentName << " inherits std.ui.Component {\n";

    // Props and state fields
    if (doc_.script) {
        for (const auto& prop : doc_.script->props) {
            ss << "    pub let " << prop.name << ": " << (prop.type.empty() ? "any" : prop.type) << ";\n";
        }
        for (const auto& rune : doc_.script->runes) {
            if (rune.kind == KmlStateRune::Kind::State && !rune.variableName.empty()) {
                ss << "    mut let " << rune.variableName << ": any;\n";
            }
        }
    }
    ss << "\n";

    // Lifecycle methods
    ss << synthesizeConstructor(res.analysis) << "\n";
    ss << synthesizeMount(res.analysis) << "\n";
    ss << synthesizeUpdate(res.analysis) << "\n";
    ss << synthesizeRenderVdom(res.analysis) << "\n";
    ss << synthesizeDestroy() << "\n";

    // Subtask 2.2: Reactive Props Propagation child setters
    if (doc_.script) {
        for (const auto& prop : doc_.script->props) {
            std::string capName = prop.name;
            if (!capName.empty()) capName[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(capName[0])));
            ss << "    pub fn set" << capName << "(value: " << (prop.type.empty() ? "any" : prop.type) << "): void {\n";
            ss << "        if (this." << prop.name << " != value) {\n";
            ss << "            this." << prop.name << " = value;\n";
            auto it = res.analysis.propMasks.find(prop.name);
            if (it != res.analysis.propMasks.end()) {
                ss << "            this.invalidate(" << it->second.name << ");\n";
            }
            ss << "        }\n";
            ss << "    }\n\n";
        }
    }

    ss << "}\n";

    res.generatedKeetScript = ss.str();
    res.generatedCppCode = "// C++ transpilable representation\n" + res.generatedKeetScript;

    return res;
}

} // namespace keety::kml
