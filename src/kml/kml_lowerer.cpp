#include <keety/kml/kml_lowerer.hpp>
#include <keety/ast/decl.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace keety::kml {

namespace {

std::string formatImportStatement(const ast::ImportDecl* imp) {
    if (!imp) return "";
    std::string out;
    if (imp->isReExported()) {
        out += "export ";
    }
    auto formatPath = [](std::string_view path) -> std::string {
        if (path.empty()) return "\"\"";
        if (path.front() == '"') return std::string(path);
        if (path.rfind("./", 0) == 0 || path.rfind("../", 0) == 0 ||
            path.rfind(".\\", 0) == 0 || path.rfind("..\\", 0) == 0 ||
            path.find('/') != std::string_view::npos ||
            path.ends_with(".kml") || path.ends_with(".keet")) {
            return "\"" + std::string(path) + "\"";
        }
        return std::string(path);
    };

    if (imp->isSelective()) {
        out += "from ";
        out += formatPath(imp->getModulePath());
        out += " import ";
        const auto& syms = imp->getSymbols();
        for (size_t i = 0; i < syms.size(); ++i) {
            if (i > 0) out += ", ";
            out += syms[i].originalName;
            if (!syms[i].alias.empty()) {
                out += " as " + syms[i].alias;
            }
        }
        out += ";\n";
    } else {
        out += "import ";
        out += formatPath(imp->getModulePath());
        if (!imp->getModuleAlias().empty()) {
            out += " as " + imp->getModuleAlias();
        }
        out += ";\n";
    }
    return out;
}

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
    if (trimmed.starts_with("[") && trimmed.ends_with("]")) return "std::vector<std::string>";
    if (trimmed.find('.') != std::string_view::npos) return "double";
    return "int64_t";
}

void collectChildComponents(const KmlTemplateNode* node, std::set<std::string>& comps) {
    if (!node) return;
    if (node->getKind() == KmlNodeKind::Element) {
        const auto* elem = static_cast<const KmlElementNode*>(node);
        if (elem->isComponent()) {
            comps.insert(elem->getTagName());
        }
        for (const auto& child : elem->getChildren()) {
            collectChildComponents(child.get(), comps);
        }
    } else if (node->getKind() == KmlNodeKind::IfBlock) {
        const auto* ifBlock = static_cast<const KmlIfBlock*>(node);
        for (const auto& child : ifBlock->getThenBranch()) {
            collectChildComponents(child.get(), comps);
        }
        for (const auto& elseIf : ifBlock->getElseIfBranches()) {
            for (const auto& child : elseIf.body) {
                collectChildComponents(child.get(), comps);
            }
        }
        for (const auto& child : ifBlock->getElseBranch()) {
            collectChildComponents(child.get(), comps);
        }
    } else if (node->getKind() == KmlNodeKind::ForBlock) {
        const auto* forBlock = static_cast<const KmlForBlock*>(node);
        for (const auto& child : forBlock->getBody()) {
            collectChildComponents(child.get(), comps);
        }
    }
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
        } else if (child->getKind() == KmlNodeKind::IfBlock) {
            const auto* ifBlock = static_cast<const KmlIfBlock*>(child.get());
            std::string thenStr = "std::string(\"\")";
            for (const auto& ch : ifBlock->getThenBranch()) {
                if (ch->getKind() == KmlNodeKind::Element) {
                    thenStr = buildElementTextCppExpr(static_cast<const KmlElementNode*>(ch.get()));
                } else if (ch->getKind() == KmlNodeKind::Text) {
                    const auto* t = static_cast<const KmlTextNode*>(ch.get());
                    thenStr = t->isInterpolated() ? ("::keet::to_string(" + t->getText() + ")") : ("std::string(\"" + escapeCppString(t->getText()) + "\")");
                }
            }
            std::string elseStr = "std::string(\"\")";
            for (const auto& ch : ifBlock->getElseBranch()) {
                if (ch->getKind() == KmlNodeKind::Element) {
                    elseStr = buildElementTextCppExpr(static_cast<const KmlElementNode*>(ch.get()));
                } else if (ch->getKind() == KmlNodeKind::Text) {
                    const auto* t = static_cast<const KmlTextNode*>(ch.get());
                    elseStr = t->isInterpolated() ? ("::keet::to_string(" + t->getText() + ")") : ("std::string(\"" + escapeCppString(t->getText()) + "\")");
                }
            }
            parts.push_back("((" + ifBlock->getCondition() + ") ? (" + thenStr + ") : (" + elseStr + "))");
        }
    }
    if (parts.empty()) return "std::string(\"\")";
    std::string res = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        res += " + " + parts[i];
    }
    return res;
}

inline void emitDimensionAttr(const std::string& attrName,
                              const std::string& val,
                              const std::string& var,
                              std::stringstream& ss,
                              const std::string& ind) {
    std::string trimmed = val;
    if (trimmed.ends_with("px")) {
        trimmed = trimmed.substr(0, trimmed.size() - 2);
    }
    if (trimmed.ends_with("%")) {
        int pct = 0;
        try { pct = std::stoi(trimmed.substr(0, trimmed.size() - 1)); } catch (...) {}
        ss << ind << var << "->getStyle()." << attrName << " = ::keet::ui::Dimension::Percent(" << pct << ");\n";
    } else {
        int px = 0;
        try { px = std::stoi(trimmed); } catch (...) {}
        ss << ind << var << "->getStyle()." << attrName << " = ::keet::ui::Dimension::Pixels(" << px << ");\n";
    }
}

inline void emitStyleAttributes(const KmlElementNode* elem,
                                const std::string& var,
                                std::stringstream& ss,
                                const std::string& ind) {
    for (const auto& attr : elem->getAttributes()) {
        const std::string& name = attr.name;
        const std::string& val = attr.value;

        if (name == "width") {
            emitDimensionAttr("width", val, var, ss, ind);
        } else if (name == "height") {
            emitDimensionAttr("height", val, var, ss, ind);
        } else if (name == "padding") {
            int p = 0;
            try { p = std::stoi(val); } catch (...) {}
            ss << ind << var << "->getStyle().padding = ::keet::ui::Edges::All(" << p << ");\n";
        } else if (name == "margin") {
            int m = 0;
            try { m = std::stoi(val); } catch (...) {}
            ss << ind << var << "->getStyle().margin = ::keet::ui::Edges::All(" << m << ");\n";
        } else if (name == "gap") {
            int g = 0;
            try { g = std::stoi(val); } catch (...) {}
            ss << ind << var << "->getStyle().gap = " << g << ";\n";
        } else if (name == "grow" || name == "flex-grow") {
            int gr = 0;
            try { gr = std::stoi(val); } catch (...) {}
            ss << ind << var << "->getStyle().flexGrow = " << gr << ";\n";
        } else if (name == "shrink" || name == "flex-shrink") {
            int sh = 1;
            try { sh = std::stoi(val); } catch (...) {}
            ss << ind << var << "->getStyle().flexShrink = " << sh << ";\n";
        } else if (name == "radius" || name == "border-radius") {
            int r = 0;
            try { r = std::stoi(val); } catch (...) {}
            ss << ind << var << "->getStyle().borderRadius = " << r << ";\n";
        } else if (name == "border-color") {
            if (val.starts_with("#")) {
                ss << ind << var << "->getStyle().borderColorRgba = ::keet::ui::RgbaColor::parseHex(\"" << val << "\");\n";
            }
        } else if (name == "border-width" || name == "border") {
            int b = 0;
            try { b = std::stoi(val); } catch (...) {}
            ss << ind << var << "->getStyle().border = ::keet::ui::Edges::All(" << b << ");\n";
        } else if (name == "font-size") {
            int fs = 14;
            try { fs = std::stoi(val); } catch (...) {}
            ss << ind << var << "->getStyle().fontSize = " << fs << ";\n";
        } else if (name == "font-family" || name == "font") {
            ss << ind << var << "->getStyle().fontFamily = \"" << escapeCppString(val) << "\";\n";
        } else if (name == "bg" || name == "background") {
            if (val.starts_with("#")) {
                ss << ind << var << "->getStyle().backgroundColorRgba = ::keet::ui::RgbaColor::parseHex(\"" << val << "\");\n";
            }
        } else if (name == "color") {
            if (val.starts_with("#")) {
                ss << ind << var << "->getStyle().textColorRgba = ::keet::ui::RgbaColor::parseHex(\"" << val << "\");\n";
            }
        } else if (name == "direction" || name == "flex-direction") {
            if (val == "row") {
                ss << ind << var << "->getStyle().flexDirection = ::keet::ui::FlexDirection::Row;\n";
            } else if (val == "column") {
                ss << ind << var << "->getStyle().flexDirection = ::keet::ui::FlexDirection::Column;\n";
            }
        } else if (name == "justify" || name == "justify-content") {
            if (val == "center") {
                ss << ind << var << "->getStyle().justifyContent = ::keet::ui::JustifyContent::Center;\n";
            } else if (val == "flex-end" || val == "end") {
                ss << ind << var << "->getStyle().justifyContent = ::keet::ui::JustifyContent::FlexEnd;\n";
            } else if (val == "space-between") {
                ss << ind << var << "->getStyle().justifyContent = ::keet::ui::JustifyContent::SpaceBetween;\n";
            } else if (val == "space-around") {
                ss << ind << var << "->getStyle().justifyContent = ::keet::ui::JustifyContent::SpaceAround;\n";
            } else if (val == "space-evenly") {
                ss << ind << var << "->getStyle().justifyContent = ::keet::ui::JustifyContent::SpaceEvenly;\n";
            } else if (val == "flex-start" || val == "start") {
                ss << ind << var << "->getStyle().justifyContent = ::keet::ui::FlexStart;\n";
            }
        } else if (name == "align" || name == "align-items") {
            if (val == "center") {
                ss << ind << var << "->getStyle().alignItems = ::keet::ui::AlignItems::Center;\n";
            } else if (val == "stretch") {
                ss << ind << var << "->getStyle().alignItems = ::keet::ui::AlignItems::Stretch;\n";
            } else if (val == "flex-end" || val == "end") {
                ss << ind << var << "->getStyle().alignItems = ::keet::ui::AlignItems::FlexEnd;\n";
            } else if (val == "flex-start" || val == "start") {
                ss << ind << var << "->getStyle().alignItems = ::keet::ui::AlignItems::FlexStart;\n";
            }
        }
    }
}

inline void emitDirectivesAndEvents(const KmlElementNode* elem,
                                    const std::string& var,
                                    std::stringstream& ss,
                                    const std::string& ind) {
    for (const auto& dir : elem->getDirectives()) {
        if (dir.name == "bind" && (dir.targetProperty == "value" || dir.targetProperty.empty())) {
            std::string stateVar = dir.expression;
            size_t start = stateVar.find_first_not_of(" \t\r\n{");
            size_t end = stateVar.find_last_not_of(" \t\r\n}");
            if (start != std::string::npos && end != std::string::npos) {
                stateVar = stateVar.substr(start, end - start + 1);
            }
            if (!stateVar.empty()) {
                ss << ind << var << "->setValueSilent(::keet::to_string(this->" << stateVar << "));\n";
                ss << ind << var << "->on(\"input\", [this](const std::string& val) {\n"
                   << ind << "    if (this->" << stateVar << " != val) {\n"
                   << ind << "        this->" << stateVar << " = val;\n"
                   << ind << "        this->recomputeDerived();\n"
                   << ind << "        this->invalidate(0xFFFFFFFF);\n"
                   << ind << "    }\n"
                   << ind << "});\n";
            }
            continue;
        }

        bool hasEnterMod = false;
        for (const auto& m : dir.modifiers) {
            if (m == "enter") {
                hasEnterMod = true;
                break;
            }
        }

        std::string expr = dir.expression;
        size_t ef = expr.find_first_not_of(" \t\r\n");
        size_t el = expr.find_last_not_of(" \t\r\n");
        if (ef != std::string::npos) expr = expr.substr(ef, el - ef + 1);
        if (expr.empty()) continue;

        size_t arrowBrace = expr.find('{');
        size_t closeBrace = expr.rfind('}');
        std::string handlerBody;
        if (arrowBrace != std::string::npos && closeBrace != std::string::npos && closeBrace > arrowBrace) {
            handlerBody = expr.substr(arrowBrace + 1, closeBrace - arrowBrace - 1);
        } else {
            handlerBody = "this->" + expr + "();";
        }

        if (dir.name == "keydown" && hasEnterMod) {
            ss << ind << var << "->on(\"keydown\", [this](const std::string& key) {\n"
               << ind << "    if (key == \"Enter\") {\n"
               << ind << "        " << handlerBody << "\n"
               << ind << "        this->recomputeDerived();\n"
               << ind << "        this->invalidate(0xFFFFFFFF);\n"
               << ind << "    }\n"
               << ind << "});\n";
        } else {
            ss << ind << var << "->on(\"" << escapeCppString(dir.name) << "\", [this](const std::string&) {\n"
               << ind << "    " << handlerBody << "\n"
               << ind << "    this->recomputeDerived();\n"
               << ind << "    this->invalidate(0xFFFFFFFF);\n"
               << ind << "});\n";
        }
    }
}

void generateCppTemplateNode(const KmlTemplateNode* node,
                             std::stringstream& ss,
                             int& nodeIndex,
                             const std::string& parentVar,
                             int indentSpaces = 8,
                             const std::string& customCompIdExpr = "") {
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
            for (const auto& attr : elem->getAttributes()) {
                if (!attr.isExpression) {
                    ss << ind << var << "->setAttribute(\"" << escapeCppString(attr.name) << "\", \"" << escapeCppString(attr.value) << "\");\n";
                } else {
                    ss << ind << var << "->setAttribute(\"" << escapeCppString(attr.name) << "\", ::keet::to_string(" << attr.value << "));\n";
                }
            }
            emitStyleAttributes(elem, var, ss, ind);
            ss << ind << var << "->setContent(" << buildElementTextCppExpr(elem) << ");\n";
            emitDirectivesAndEvents(elem, var, ss, ind);
            if (!parentVar.empty()) {
                ss << ind << parentVar << "->appendChild(" << var << ");\n";
            } else {
                ss << ind << "root = " << var << ";\n";
            }
            return;
        }

        if (tag == "button") {
            ss << ind << "auto " << var << " = std::make_shared<::keet::ui::ButtonNode>();\n";
            for (const auto& attr : elem->getAttributes()) {
                if (!attr.isExpression) {
                    ss << ind << var << "->setAttribute(\"" << escapeCppString(attr.name) << "\", \"" << escapeCppString(attr.value) << "\");\n";
                } else {
                    ss << ind << var << "->setAttribute(\"" << escapeCppString(attr.name) << "\", ::keet::to_string(" << attr.value << "));\n";
                }
            }
            emitStyleAttributes(elem, var, ss, ind);
            bool hasElemColor = false;
            for (const auto& a : elem->getAttributes()) {
                if (a.name == "color") {
                    hasElemColor = true;
                    break;
                }
            }
            if (!hasElemColor) {
                for (const auto& ch : elem->getChildren()) {
                    if (ch && ch->getKind() == KmlNodeKind::Element) {
                        const auto* chElem = static_cast<const KmlElementNode*>(ch.get());
                        if (chElem->getTagName() == "text") {
                            for (const auto& a : chElem->getAttributes()) {
                                if (a.name == "color" && a.value.starts_with("#")) {
                                    ss << ind << var << "->getStyle().textColorRgba = ::keet::ui::RgbaColor::parseHex(\"" << a.value << "\");\n";
                                }
                            }
                        }
                    }
                }
            }
            ss << ind << var << "->setLabel(" << buildElementTextCppExpr(elem) << ");\n";
            emitDirectivesAndEvents(elem, var, ss, ind);
            if (!parentVar.empty()) {
                ss << ind << parentVar << "->appendChild(" << var << ");\n";
            } else {
                ss << ind << "root = " << var << ";\n";
            }
            return;
        }

        if (elem->isComponent()) {
            std::string compName = elem->getTagName();
            std::string compIdExpr = customCompIdExpr.empty() ? std::to_string(id) : customCompIdExpr;
            ss << ind << "std::shared_ptr<::keet_module_" << compName << "::" << compName << "> " << var << "_comp;\n";
            ss << ind << "bool " << var << "_isNew = false;\n";
            ss << ind << "if (auto existing = findChildById(" << compIdExpr << ")) {\n";
            ss << ind << "    " << var << "_comp = std::static_pointer_cast<::keet_module_" << compName << "::" << compName << ">(existing);\n";
            ss << ind << "} else {\n";
            ss << ind << "    " << var << "_comp = std::make_shared<::keet_module_" << compName << "::" << compName << ">();\n";
            ss << ind << "    registerChild(" << compIdExpr << ", " << var << "_comp);\n";
            ss << ind << "    " << var << "_isNew = true;\n";
            ss << ind << "}\n";

            for (const auto& attr : elem->getAttributes()) {
                const std::string& name = attr.name;
                const std::string& val = attr.value;
                if (name == "width" || name == "height" || name == "margin" || name == "padding" ||
                    name == "gap" || name == "grow" || name == "shrink" || name == "radius" ||
                    name == "bg" || name == "class") {
                    continue;
                }
                if (attr.isExpression) {
                    ss << ind << var << "_comp->" << name << " = (" << val << ");\n";
                } else {
                    ss << ind << "if (" << var << "_isNew) {\n";
                    if (val == "true" || val == "false") {
                        ss << ind << "    " << var << "_comp->" << name << " = " << val << ";\n";
                    } else if (!val.empty() && std::all_of(val.begin(), val.end(), [](char c){ return std::isdigit(static_cast<unsigned char>(c)); })) {
                        ss << ind << "    " << var << "_comp->" << name << " = " << val << ";\n";
                    } else {
                        ss << ind << "    " << var << "_comp->" << name << " = \"" << escapeCppString(val) << "\";\n";
                    }
                    ss << ind << "}\n";
                }
            }

            ss << ind << "auto " << var << " = " << var << "_comp->getRootNode();\n";
            ss << ind << "if (!" << var << ") {\n";
            ss << ind << "    " << var << " = " << var << "_comp->mount();\n";
            ss << ind << "} else {\n";
            ss << ind << "    " << var << "_comp->update(0xFFFFFFFF);\n";
            ss << ind << "}\n";
            ss << ind << "if (" << var << ") {\n";
            emitStyleAttributes(elem, var, ss, ind + "    ");
            if (!parentVar.empty()) {
                ss << ind << "    " << parentVar << "->appendChild(" << var << ");\n";
            } else {
                ss << ind << "    root = " << var << ";\n";
            }
            ss << ind << "}\n";
            return;
        }

        if (tag == "vstack") {
            ss << ind << "auto " << var << " = std::make_shared<::keet::ui::StackNode>(::keet::ui::FlexDirection::Column);\n";
        } else if (tag == "hstack") {
            ss << ind << "auto " << var << " = std::make_shared<::keet::ui::StackNode>(::keet::ui::FlexDirection::Row);\n";
        } else if (tag == "scroll") {
            ss << ind << "auto " << var << " = std::make_shared<::keet::ui::ScrollNode>();\n";
            ss << ind << var << "->getStyle().overflowScrollY = true;\n";
        } else if (tag == "input") {
            ss << ind << "auto " << var << " = std::make_shared<::keet::ui::InputNode>();\n";
        } else {
            ss << ind << "auto " << var << " = std::make_shared<::keet::ui::BoxNode>();\n";
        }

        for (const auto& attr : elem->getAttributes()) {
            if (!attr.isExpression) {
                ss << ind << var << "->setAttribute(\"" << escapeCppString(attr.name) << "\", \"" << escapeCppString(attr.value) << "\");\n";
            } else {
                ss << ind << var << "->setAttribute(\"" << escapeCppString(attr.name) << "\", ::keet::to_string(" << attr.value << "));\n";
            }
        }
        emitStyleAttributes(elem, var, ss, ind);
        emitDirectivesAndEvents(elem, var, ss, ind);

        if (!parentVar.empty()) {
            ss << ind << parentVar << "->appendChild(" << var << ");\n";
        } else {
            ss << ind << "root = " << var << ";\n";
        }
        for (const auto& child : elem->getChildren()) {
            generateCppTemplateNode(child.get(), ss, nodeIndex, var, indentSpaces, customCompIdExpr);
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
                generateCppTemplateNode(child.get(), ss, bIdx, parentVar, indentSpaces + 4, customCompIdExpr);
            }
            if (bIdx > maxBranchIndex) maxBranchIndex = bIdx;
        }
        ss << ind << "}";

        for (const auto& elseIf : ifBlock->getElseIfBranches()) {
            ss << " else if (" << elseIf.condition << ") {\n";
            int bIdx = branchBaseIndex;
            for (const auto& child : elseIf.body) {
                generateCppTemplateNode(child.get(), ss, bIdx, parentVar, indentSpaces + 4, customCompIdExpr);
            }
            if (bIdx > maxBranchIndex) maxBranchIndex = bIdx;
            ss << ind << "}";
        }

        if (!ifBlock->getElseBranch().empty()) {
            ss << " else {\n";
            int bIdx = branchBaseIndex;
            for (const auto& child : ifBlock->getElseBranch()) {
                generateCppTemplateNode(child.get(), ss, bIdx, parentVar, indentSpaces + 4, customCompIdExpr);
            }
            if (bIdx > maxBranchIndex) maxBranchIndex = bIdx;
            ss << ind << "}";
        }
        ss << "\n";
        nodeIndex = maxBranchIndex;
        return;
    }

    if (node->getKind() == KmlNodeKind::ForBlock) {
        const auto* forBlock = static_cast<const KmlForBlock*>(node);
        std::string itemVar = forBlock->getItemVar();
        std::string collExpr = forBlock->getIterableExpr();
        int baseLoopId = ++nodeIndex;
        ss << ind << "size_t for_idx_" << baseLoopId << " = 0;\n";
        ss << ind << "for (const auto& " << itemVar << " : this->" << collExpr << ") {\n";
        ss << ind << "    size_t for_comp_id_" << baseLoopId << " = (" << baseLoopId << " * 10000) + (for_idx_" << baseLoopId << "++);\n";
        std::string compIdExpr = "for_comp_id_" + std::to_string(baseLoopId);
        for (const auto& child : forBlock->getBody()) {
            generateCppTemplateNode(child.get(), ss, nodeIndex, parentVar, indentSpaces + 4, compIdExpr);
        }
        ss << ind << "}\n";
        return;
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
            else if (tag == "vstack") nodeType = "StackNode";
            else if (tag == "hstack") nodeType = "StackNode";
            else if (elem->isComponent()) nodeType = tag;

            ss << "        let node_" << currentIndex << " = new ui." << nodeType << "();\n";
            ss << "        node_" << currentIndex << ".id = " << currentIndex << ";\n";
            if (tag == "vstack") {
                ss << "        node_" << currentIndex << ".style.flexDirection = ui.FlexDirection.Column;\n";
            } else if (tag == "hstack") {
                ss << "        node_" << currentIndex << ".style.flexDirection = ui.FlexDirection.Row;\n";
            } else if (tag == "scroll") {
                ss << "        node_" << currentIndex << ".style.overflowScrollY = true;\n";
            }

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

    // 3. Process script imports and map explicit child component paths
    std::map<std::string, std::string> scriptComponentImports;
    std::vector<std::string> scriptImportStatements;
    std::set<std::string> cppIncludes;

    if (doc_.script) {
        for (const auto& decl : doc_.script->declarations) {
            if (const auto* imp = dynamic_cast<const ast::ImportDecl*>(decl.get())) {
                std::string stmt = formatImportStatement(imp);
                if (!stmt.empty()) {
                    scriptImportStatements.push_back(stmt);
                }

                std::string modPath = imp->getModulePath();
                if (modPath.ends_with(".kml")) {
                    std::string compName;
                    if (!imp->getSymbols().empty()) {
                        compName = imp->getSymbols().front().getEffectiveName();
                    } else if (!imp->getModuleAlias().empty()) {
                        compName = imp->getModuleAlias();
                    } else {
                        compName = std::filesystem::path(modPath).stem().string();
                    }
                    if (!compName.empty()) {
                        scriptComponentImports[compName] = modPath;
                    }
                    std::string hppPath = modPath;
                    if (hppPath.rfind("./", 0) == 0) hppPath = hppPath.substr(2);
                    else if (hppPath.rfind(".\\", 0) == 0) hppPath = hppPath.substr(2);
                    if (hppPath.ends_with(".kml")) {
                        hppPath = hppPath.substr(0, hppPath.size() - 4) + ".hpp";
                    }
                    cppIncludes.insert("#include \"" + hppPath + "\"\n");
                } else if (modPath.ends_with(".keet")) {
                    std::string hppPath = modPath;
                    if (hppPath.rfind("./", 0) == 0) hppPath = hppPath.substr(2);
                    else if (hppPath.rfind(".\\", 0) == 0) hppPath = hppPath.substr(2);
                    if (hppPath.ends_with(".keet")) {
                        hppPath = hppPath.substr(0, hppPath.size() - 5) + ".hpp";
                    }
                    cppIncludes.insert("#include \"" + hppPath + "\"\n");
                } else if (!modPath.empty() && modPath.find('/') == std::string::npos && modPath.find('\\') == std::string::npos) {
                    cppIncludes.insert("#include <keet/std/" + modPath + ".hpp>\n");
                }
            }
        }
    }

    std::set<std::string> childComponents;
    for (const auto& rootNode : doc_.templateNodes) {
        collectChildComponents(rootNode.get(), childComponents);
    }

    std::vector<std::string> fallbackComponentImports;
    for (const auto& compName : childComponents) {
        if (scriptComponentImports.count(compName) == 0) {
            fallbackComponentImports.push_back("import \"./" + compName + ".kml\";\n");
            cppIncludes.insert("#include \"" + compName + ".hpp\"\n");
        }
    }

    std::stringstream headerImports;
    headerImports << "import ui;\n";
    for (const auto& stmt : scriptImportStatements) {
        headerImports << stmt;
    }
    for (const auto& stmt : fallbackComponentImports) {
        headerImports << stmt;
    }
    headerImports << "\n";

    // 4. Synthesize Component Class
    std::stringstream ss;
    ss << headerImports.str();

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

    // 5. Synthesize clean KeetScript module interface for TypeChecker & ModuleGraph
    {
        std::stringstream iface;
        iface << headerImports.str();
        iface << "export class " << doc_.componentName << " inherits ui.Component {\n";
        iface << "}\n";
        res.generatedInterfaceKeet = iface.str();
    }

    // 6. Synthesize C++ Header (<Component>.hpp) and Source (<Component>.cpp)
    {
        std::stringstream hpp;
        hpp << "// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.\n"
            << "#pragma once\n\n"
            << "#include <algorithm>\n"
            << "#include <cstdint>\n"
            << "#include <memory>\n"
            << "#include <set>\n"
            << "#include <string>\n"
            << "#include <vector>\n\n"
            << "#include <keet/runtime.hpp>\n"
            << "#include <keet/std/ui.hpp>\n";

        for (const auto& inc : cppIncludes) {
            hpp << inc;
        }

        hpp << "\nnamespace keet_module_" << doc_.componentName << " {\n"
            << "    struct " << doc_.componentName << " : public ::keet::ui::Component {\n";

        if (doc_.script) {
            for (const auto& rune : doc_.script->runes) {
                if (rune.kind == KmlStateRune::Kind::State && !rune.variableName.empty()) {
                    std::string cppType = inferCppTypeFromInit(rune.initExpr);
                    std::string initExpr = rune.initExpr;
                    if (initExpr.starts_with("[") && initExpr.ends_with("]")) {
                        initExpr[0] = '{';
                        initExpr.back() = '}';
                    }
                    std::string initVal = initExpr.empty() ? "{}" : (" = " + initExpr);
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
            << "            dst->getStyle() = src->getStyle();\n"
            << "            dst->setDirty(true);\n"
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
            << "                auto dInp = std::static_pointer_cast<::keet::ui::InputNode>(dst);\n"
            << "                auto sInp = std::static_pointer_cast<::keet::ui::InputNode>(src);\n"
            << "                if (dInp->getValue() != sInp->getValue()) {\n"
            << "                    dInp->setValueSilent(sInp->getValue());\n"
            << "                }\n"
            << "            } else if (dst->getKind() == ::keet::ui::NodeKind::Scroll && src->getKind() == ::keet::ui::NodeKind::Scroll) {\n"
            << "                auto dScroll = std::static_pointer_cast<::keet::ui::ScrollNode>(dst);\n"
            << "                auto sScroll = std::static_pointer_cast<::keet::ui::ScrollNode>(src);\n"
            << "                dScroll->setContentHeight(sScroll->getContentHeight());\n"
            << "                dScroll->setContentWidth(sScroll->getContentWidth());\n"
            << "            }\n"
            << "            const auto& dChildren = dst->getChildren();\n"
            << "            const auto& sChildren = src->getChildren();\n"
            << "            if (dChildren.size() == sChildren.size()) {\n"
            << "                for (size_t i = 0; i < dChildren.size(); ++i) {\n"
            << "                    dChildren[i]->setParent(dst);\n"
            << "                    if (dChildren[i]->getKind() == sChildren[i]->getKind()) {\n"
            << "                        syncNodeValues(dChildren[i], sChildren[i]);\n"
            << "                    } else {\n"
            << "                        dst->clearChildren();\n"
            << "                        for (const auto& ch : sChildren) dst->appendChild(ch);\n"
            << "                        break;\n"
            << "                    }\n"
            << "                }\n"
            << "            } else {\n"
            << "                dst->clearChildren();\n"
            << "                for (const auto& ch : sChildren) dst->appendChild(ch);\n"
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
            << "        if (!rootNode_) {\n"
            << "            rootNode_ = buildTemplateTree();\n"
            << "        }\n"
            << "        dirtyMask_ = 0;\n"
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
