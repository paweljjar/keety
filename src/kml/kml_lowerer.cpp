#include <keety/kml/kml_lowerer.hpp>
#include <iomanip>
#include <sstream>

namespace keety::kml {

namespace {

std::string escapeCppString(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out.push_back(c);
    }
    return out;
}

std::string inferCppTypeFromInit(std::string_view initExpr) {
    size_t f = initExpr.find_first_not_of(" \t\r\n");
    size_t l = initExpr.find_last_not_of(" \t\r\n");
    std::string_view trimmed = (f != std::string_view::npos) ? initExpr.substr(f, l - f + 1) : "";
    if (trimmed == "true" || trimmed == "false") return "bool";
    if (!trimmed.empty() && (trimmed.front() == '"' || trimmed.front() == '\'')) return "std::string";
    if (trimmed.find('.') != std::string_view::npos) return "double";
    return "int64_t";
}

struct ExtractedScriptFn {
    std::string name;
    std::string returnType;
    std::string body;
};

std::vector<ExtractedScriptFn> extractScriptFunctions(std::string_view rawCode) {
    std::vector<ExtractedScriptFn> fns;
    size_t pos = 0;
    while ((pos = rawCode.find("fn ", pos)) != std::string_view::npos) {
        if (pos > 0 && (std::isalnum(static_cast<unsigned char>(rawCode[pos - 1])) || rawCode[pos - 1] == '_')) {
            pos += 3;
            continue;
        }
        size_t nameStart = pos + 3;
        while (nameStart < rawCode.size() && (rawCode[nameStart] == ' ' || rawCode[nameStart] == '\t')) nameStart++;
        size_t nameEnd = nameStart;
        while (nameEnd < rawCode.size() && (std::isalnum(static_cast<unsigned char>(rawCode[nameEnd])) || rawCode[nameEnd] == '_')) nameEnd++;
        if (nameEnd == nameStart) {
            pos += 3;
            continue;
        }
        std::string fnName(rawCode.substr(nameStart, nameEnd - nameStart));
        size_t openParen = rawCode.find('(', nameEnd);
        size_t closeParen = (openParen != std::string_view::npos) ? rawCode.find(')', openParen) : std::string_view::npos;
        size_t openBrace = (closeParen != std::string_view::npos) ? rawCode.find('{', closeParen) : std::string_view::npos;
        if (openBrace == std::string_view::npos) {
            pos = nameEnd;
            continue;
        }

        std::string retType = "void";
        size_t colon = rawCode.find(':', closeParen);
        if (colon != std::string_view::npos && colon < openBrace) {
            std::string_view rt = rawCode.substr(colon + 1, openBrace - colon - 1);
            size_t rf = rt.find_first_not_of(" \t\r\n");
            size_t rl = rt.find_last_not_of(" \t\r\n");
            if (rf != std::string_view::npos) {
                std::string_view rts = rt.substr(rf, rl - rf + 1);
                if (rts == "int") retType = "int64_t";
                else if (rts == "float") retType = "double";
                else if (rts == "bool") retType = "bool";
                else if (rts == "string") retType = "std::string";
                else retType = std::string(rts);
            }
        }

        int depth = 1;
        size_t curr = openBrace + 1;
        while (curr < rawCode.size() && depth > 0) {
            if (rawCode[curr] == '{') depth++;
            else if (rawCode[curr] == '}') depth--;
            curr++;
        }
        std::string body(rawCode.substr(openBrace + 1, (curr > openBrace + 1) ? (curr - openBrace - 2) : 0));
        fns.push_back(ExtractedScriptFn{std::move(fnName), std::move(retType), std::move(body)});
        pos = curr;
    }
    return fns;
}

std::string buildElementTextCppExpr(const KmlElementNode* elem) {
    bool hasElementChild = false;
    for (const auto& child : elem->getChildren()) {
        if (child && child->getKind() == KmlNodeKind::Element) {
            hasElementChild = true;
            break;
        }
    }
    std::vector<std::string> parts;
    for (const auto& child : elem->getChildren()) {
        if (!child) continue;
        if (child->getKind() == KmlNodeKind::Text) {
            const auto* t = static_cast<const KmlTextNode*>(child.get());
            if (!t->isInterpolated() && hasElementChild &&
                t->getText().find_first_not_of(" \t\r\n") == std::string::npos) {
                continue;
            }
            if (t->isInterpolated()) {
                parts.push_back("::keet::to_string(" + t->getText() + ")");
            } else {
                parts.push_back("std::string(\"" + escapeCppString(t->getText()) + "\")");
            }
        } else if (child->getKind() == KmlNodeKind::Element) {
            const auto* childElem = static_cast<const KmlElementNode*>(child.get());
            std::string nested = buildElementTextCppExpr(childElem);
            if (nested != "std::string(\"\")") {
                parts.push_back(nested);
            }
        }
    }
    if (parts.empty()) return "std::string(\"\")";
    std::string res = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        res += " + " + parts[i];
    }
    return res;
}

void generateCppTemplateNode(const KmlTemplateNode* node,
                             std::stringstream& ss,
                             int& nodeIndex,
                             const std::string& parentVar,
                             int indentSpaces = 8) {
    if (!node) return;
    std::string ind(static_cast<size_t>(indentSpaces), ' ');

    if (node->getKind() == KmlNodeKind::Text) {
        const auto* t = static_cast<const KmlTextNode*>(node);
        if (!t->isInterpolated() && t->getText().find_first_not_of(" \t\r\n") == std::string::npos) {
            return; // Ignore structural whitespace between element tags
        }
        int id = ++nodeIndex;
        std::string var = "node_" + std::to_string(id);
        ss << ind << "auto " << var << " = std::make_shared<::keet::ui::TextNode>();\n";
        ss << ind << var << "->setId(" << id << ");\n";
        if (t->isInterpolated()) {
            ss << ind << var << "->setContent(::keet::to_string(" << t->getText() << "));\n";
        } else {
            ss << ind << var << "->setContent(\"" << escapeCppString(t->getText()) << "\");\n";
        }
        if (!parentVar.empty()) {
            ss << ind << parentVar << "->appendChild(" << var << ");\n";
        } else {
            ss << ind << "root = " << var << ";\n";
        }
        return;
    }

    if (node->getKind() == KmlNodeKind::Element) {
        const auto* elem = static_cast<const KmlElementNode*>(node);
        const std::string& tag = elem->getTagName();
        int id = ++nodeIndex;
        std::string var = "node_" + std::to_string(id);

        if (tag == "text" || tag == "span" || tag == "p") {
            ss << ind << "auto " << var << " = std::make_shared<::keet::ui::TextNode>();\n";
            ss << ind << var << "->setId(" << id << ");\n";
            for (const auto& attr : elem->getAttributes()) {
                if (!attr.isExpression) {
                    ss << ind << var << "->setAttribute(\"" << escapeCppString(attr.name) << "\", \"" << escapeCppString(attr.value) << "\");\n";
                } else {
                    ss << ind << var << "->setAttribute(\"" << escapeCppString(attr.name) << "\", ::keet::to_string(" << attr.value << "));\n";
                }
            }
            ss << ind << var << "->setContent(" << buildElementTextCppExpr(elem) << ");\n";
            if (!parentVar.empty()) {
                ss << ind << parentVar << "->appendChild(" << var << ");\n";
            } else {
                ss << ind << "root = " << var << ";\n";
            }
            return;
        }

        if (tag == "button") {
            ss << ind << "auto " << var << " = std::make_shared<::keet::ui::ButtonNode>();\n";
            ss << ind << var << "->setId(" << id << ");\n";
            for (const auto& attr : elem->getAttributes()) {
                if (!attr.isExpression) {
                    ss << ind << var << "->setAttribute(\"" << escapeCppString(attr.name) << "\", \"" << escapeCppString(attr.value) << "\");\n";
                } else {
                    ss << ind << var << "->setAttribute(\"" << escapeCppString(attr.name) << "\", ::keet::to_string(" << attr.value << "));\n";
                }
            }
            ss << ind << var << "->setLabel(" << buildElementTextCppExpr(elem) << ");\n";
            for (const auto& dir : elem->getDirectives()) {
                std::string expr = dir.expression;
                size_t ef = expr.find_first_not_of(" \t\r\n");
                size_t el = expr.find_last_not_of(" \t\r\n");
                if (ef != std::string::npos) expr = expr.substr(ef, el - ef + 1);
                if (!expr.empty()) {
                    size_t arrowBrace = expr.find('{');
                    size_t closeBrace = expr.rfind('}');
                    if (arrowBrace != std::string::npos && closeBrace != std::string::npos && closeBrace > arrowBrace) {
                        std::string lambdaBody = expr.substr(arrowBrace + 1, closeBrace - arrowBrace - 1);
                        ss << ind << var << "->on(\"" << escapeCppString(dir.name) << "\", [this](const std::string&) {\n"
                           << ind << "    " << lambdaBody << "\n"
                           << ind << "    this->recomputeDerived();\n"
                           << ind << "    this->invalidate(0xFFFFFFFF);\n"
                           << ind << "});\n";
                    } else {
                        ss << ind << var << "->on(\"" << escapeCppString(dir.name) << "\", [this](const std::string&) {\n"
                           << ind << "    this->" << expr << "();\n"
                           << ind << "});\n";
                    }
                }
            }
            if (!parentVar.empty()) {
                ss << ind << parentVar << "->appendChild(" << var << ");\n";
            } else {
                ss << ind << "root = " << var << ";\n";
            }
            return;
        }

        std::string cppNodeClass = "BoxNode";
        if (tag == "input") cppNodeClass = "InputNode";
        else if (tag == "scroll") cppNodeClass = "ScrollNode";

        ss << ind << "auto " << var << " = std::make_shared<::keet::ui::" << cppNodeClass << ">();\n";
        ss << ind << var << "->setId(" << id << ");\n";
        for (const auto& attr : elem->getAttributes()) {
            if (!attr.isExpression) {
                ss << ind << var << "->setAttribute(\"" << escapeCppString(attr.name) << "\", \"" << escapeCppString(attr.value) << "\");\n";
            } else {
                ss << ind << var << "->setAttribute(\"" << escapeCppString(attr.name) << "\", ::keet::to_string(" << attr.value << "));\n";
            }
        }
        if (!parentVar.empty()) {
            ss << ind << parentVar << "->appendChild(" << var << ");\n";
        } else {
            ss << ind << "root = " << var << ";\n";
        }
        for (const auto& child : elem->getChildren()) {
            generateCppTemplateNode(child.get(), ss, nodeIndex, var, indentSpaces);
        }
        return;
    }

    if (node->getKind() == KmlNodeKind::IfBlock) {
        const auto* ifBlock = static_cast<const KmlIfBlock*>(node);
        int branchBaseIndex = nodeIndex;
        int maxBranchIndex = nodeIndex;

        ss << ind << "if (" << ifBlock->getCondition() << ") {\n";
        {
            int bIdx = branchBaseIndex;
            for (const auto& child : ifBlock->getThenBranch()) {
                generateCppTemplateNode(child.get(), ss, bIdx, parentVar, indentSpaces + 4);
            }
            if (bIdx > maxBranchIndex) maxBranchIndex = bIdx;
        }
        ss << ind << "}";

        for (const auto& elseIf : ifBlock->getElseIfBranches()) {
            ss << " else if (" << elseIf.condition << ") {\n";
            int bIdx = branchBaseIndex;
            for (const auto& child : elseIf.body) {
                generateCppTemplateNode(child.get(), ss, bIdx, parentVar, indentSpaces + 4);
            }
            if (bIdx > maxBranchIndex) maxBranchIndex = bIdx;
            ss << ind << "}";
        }

        if (!ifBlock->getElseBranch().empty()) {
            ss << " else {\n";
            int bIdx = branchBaseIndex;
            for (const auto& child : ifBlock->getElseBranch()) {
                generateCppTemplateNode(child.get(), ss, bIdx, parentVar, indentSpaces + 4);
            }
            if (bIdx > maxBranchIndex) maxBranchIndex = bIdx;
            ss << ind << "}";
        }
        ss << "\n";
        nodeIndex = maxBranchIndex;
    }
}

} // namespace

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
            wrapped = "ui.debounce(" + wrapped + ", " + ms + ")";
        } else if (mod == "prevent") {
            wrapped = "ui.withPreventDefault(" + wrapped + ")";
        } else if (mod == "enter") {
            wrapped = "ui.withKeyFilter(" + wrapped + ", Key.Enter)";
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

            ss << "        let node_" << currentIndex << " = new ui." << nodeType << "();\n";
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
            ss << "        let node_" << currentIndex << " = new ui.TextNode();\n";
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
    ss << "    pub fn mount(): ui.UINode {\n";
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
    ss << "    pub fn renderVdom(): ui.VNode {\n";
    ss << "        let root = new ui.VNode(\"div\");\n";
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
    ss << "export class " << doc_.componentName << " inherits ui.Component {\n";

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

    // 4. Synthesize clean KeetScript module interface for TypeChecker & ModuleGraph
    {
        std::stringstream iface;
        iface << "import ui;\n\n";
        iface << "export class " << doc_.componentName << " inherits ui.Component {\n";
        iface << "}\n";
        res.generatedInterfaceKeet = iface.str();
    }

    // 5. Synthesize C++ Header (<Component>.hpp) and Source (<Component>.cpp)
    {
        std::stringstream hpp;
        hpp << "// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.\n"
            << "#pragma once\n\n"
            << "#include <algorithm>\n"
            << "#include <cstdint>\n"
            << "#include <memory>\n"
            << "#include <string>\n"
            << "#include <vector>\n\n"
            << "#include <keet/runtime.hpp>\n"
            << "#include <keet/std/ui.hpp>\n\n"
            << "namespace keet_module_" << doc_.componentName << " {\n"
            << "    struct " << doc_.componentName << " : public ::keet::ui::Component {\n";

        if (doc_.script) {
            for (const auto& rune : doc_.script->runes) {
                if (rune.kind == KmlStateRune::Kind::State && !rune.variableName.empty()) {
                    std::string cppType = inferCppTypeFromInit(rune.initExpr);
                    std::string initVal = rune.initExpr.empty() ? "{}" : (" = " + rune.initExpr);
                    hpp << "        " << cppType << " " << rune.variableName << initVal << ";\n";
                }
            }
            for (const auto& rune : doc_.script->runes) {
                if (rune.kind == KmlStateRune::Kind::Derived && !rune.variableName.empty()) {
                    std::string initVal = rune.initExpr.empty() ? "0" : rune.initExpr;
                    hpp << "        int64_t " << rune.variableName << " = " << initVal << ";\n";
                }
            }
        }

        hpp << "\n        " << doc_.componentName << "() = default;\n\n"
            << "        void recomputeDerived() {\n";
        if (doc_.script) {
            for (const auto& rune : doc_.script->runes) {
                if (rune.kind == KmlStateRune::Kind::Derived && !rune.variableName.empty() && !rune.initExpr.empty()) {
                    hpp << "            " << rune.variableName << " = " << rune.initExpr << ";\n";
                }
            }
        }
        hpp << "        }\n\n";

        if (doc_.script) {
            auto scriptFns = extractScriptFunctions(doc_.script->rawCode);
            for (const auto& fn : scriptFns) {
                hpp << "        " << fn.returnType << " " << fn.name << "() {\n"
                    << "            " << fn.body << "\n";
                if (fn.returnType == "void") {
                    hpp << "            recomputeDerived();\n"
                        << "            invalidate(0xFFFFFFFF);\n";
                }
                hpp << "        }\n\n";
            }
        }

        hpp << "        std::shared_ptr<::keet::ui::UINode> buildTemplateTree();\n"
            << "        std::shared_ptr<::keet::ui::UINode> mount() override;\n"
            << "        void update(uint32_t dirtyMask) override;\n"
            << "    };\n"
            << "} // namespace keet_module_" << doc_.componentName << "\n"
            << "using keet_module_" << doc_.componentName << "::" << doc_.componentName << ";\n";
        res.generatedHppCode = hpp.str();
    }

    {
        std::stringstream cpp;
        cpp << "// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.\n"
            << "#include \"" << doc_.componentName << ".hpp\"\n\n"
            << "namespace keet_module_" << doc_.componentName << " {\n"
            << "    namespace {\n"
            << "        void syncNodeValues(const std::shared_ptr<::keet::ui::UINode>& dst,\n"
            << "                            const std::shared_ptr<::keet::ui::UINode>& src) {\n"
            << "            if (!dst || !src) return;\n"
            << "            if (src->hasAttribute(\"class\")) {\n"
            << "                dst->setAttribute(\"class\", src->getAttribute(\"class\"));\n"
            << "            }\n"
            << "            if (dst->getKind() == ::keet::ui::NodeKind::Text && src->getKind() == ::keet::ui::NodeKind::Text) {\n"
            << "                std::static_pointer_cast<::keet::ui::TextNode>(dst)->setContent(\n"
            << "                    std::static_pointer_cast<::keet::ui::TextNode>(src)->getContent());\n"
            << "            } else if (dst->getKind() == ::keet::ui::NodeKind::Button && src->getKind() == ::keet::ui::NodeKind::Button) {\n"
            << "                std::static_pointer_cast<::keet::ui::ButtonNode>(dst)->setLabel(\n"
            << "                    std::static_pointer_cast<::keet::ui::ButtonNode>(src)->getLabel());\n"
            << "            } else if (dst->getKind() == ::keet::ui::NodeKind::Input && src->getKind() == ::keet::ui::NodeKind::Input) {\n"
            << "                std::static_pointer_cast<::keet::ui::InputNode>(dst)->setValue(\n"
            << "                    std::static_pointer_cast<::keet::ui::InputNode>(src)->getValue());\n"
            << "            }\n"
            << "            const auto& dChildren = dst->getChildren();\n"
            << "            const auto& sChildren = src->getChildren();\n"
            << "            size_t n = std::min(dChildren.size(), sChildren.size());\n"
            << "            for (size_t i = 0; i < n; ++i) {\n"
            << "                syncNodeValues(dChildren[i], sChildren[i]);\n"
            << "            }\n"
            << "        }\n"
            << "    } // namespace\n\n"
            << "    std::shared_ptr<::keet::ui::UINode> " << doc_.componentName << "::buildTemplateTree() {\n"
            << "        std::shared_ptr<::keet::ui::UINode> root;\n";

        int nodeIdx = 0;
        for (const auto& rootNode : doc_.templateNodes) {
            generateCppTemplateNode(rootNode.get(), cpp, nodeIdx, "", 8);
        }

        cpp << "        return root;\n"
            << "    }\n\n"
            << "    std::shared_ptr<::keet::ui::UINode> " << doc_.componentName << "::mount() {\n"
            << "        recomputeDerived();\n"
            << "        rootNode_ = buildTemplateTree();\n"
            << "        return rootNode_;\n"
            << "    }\n\n"
            << "    void " << doc_.componentName << "::update([[maybe_unused]] uint32_t dirtyMask) {\n"
            << "        recomputeDerived();\n"
            << "        auto updated = buildTemplateTree();\n"
            << "        syncNodeValues(rootNode_, updated);\n"
            << "    }\n"
            << "} // namespace keet_module_" << doc_.componentName << "\n";
        res.generatedCppCode = cpp.str();
    }

    return res;
}

} // namespace keety::kml
