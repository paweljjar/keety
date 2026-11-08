#include <keety/kml/style_scoper.hpp>
#include <iomanip>
#include <sstream>

namespace keety::kml {

StyleScoper::StyleScoper(const KmlDocument& doc)
    : doc_(doc) {}

std::string StyleScoper::generateScopeHash() const {
    // Deterministic 32-bit FNV-1a hash over componentName
    uint32_t hash = 2166136261u;
    for (char c : doc_.componentName) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }

    std::stringstream ss;
    ss << "kml-" << std::hex << std::setw(6) << std::setfill('0') << (hash & 0xFFFFFF);
    return ss.str();
}

ScopedStyleResult StyleScoper::lower(StyleTarget target) {
    ScopedStyleResult result;
    result.scopeClass = generateScopeHash();

    if (!doc_.style) {
        return result;
    }

    if (target == StyleTarget::Web) {
        // Lower style rules into scoped CSS: append .kml-hash to each selector
        std::stringstream ss;
        for (const auto& rule : doc_.style->rules) {
            std::string sel = rule.selector;
            if (!sel.empty()) {
                ss << sel << "." << result.scopeClass << " {\n";
                for (const auto& decl : rule.declarations) {
                    ss << "    " << decl.property << ": " << decl.value << ";\n";
                }
                ss << "}\n";
            }
        }
        std::string generated = ss.str();
        if (generated.empty() && !doc_.style->rawCss.empty()) {
            result.scopedCss = "/* " + result.scopeClass + " */\n" + doc_.style->rawCss;
        } else {
            result.scopedCss = generated;
        }
    } else if (target == StyleTarget::Tui) {
        // Pre-resolve typed style rules at compile time into static C++ struct initializers
        std::stringstream ss;
        ss << "// Pre-resolved TUI style initializers for " << doc_.componentName << "\n";
        for (const auto& rule : doc_.style->rules) {
            ss << "// Selector: " << rule.selector << "\n";
            for (const auto& decl : rule.declarations) {
                ss << "// " << decl.property << " = " << decl.value << "\n";
            }
        }
        result.tuiStyleInitializers = ss.str();
    }

    return result;
}

} // namespace keety::kml
