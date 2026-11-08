#pragma once

#include <keety/kml/kml_ast.hpp>
#include <string>
#include <vector>

namespace keety::kml {

/// Scoped style lowering target
enum class StyleTarget {
    Web,    // Emits scoped CSS with generated hash class (.btn.kml-xxxx)
    Tui     // Emits pre-resolved C++ struct initializers directly bound to node styles
};

struct ScopedStyleResult {
    std::string scopeClass;              // e.g. "kml-7a8f12"
    std::string scopedCss;               // Generated scoped stylesheet for Web
    std::string tuiStyleInitializers;    // C++ struct initializers for TUI
};

class StyleScoper {
public:
    explicit StyleScoper(const KmlDocument& doc);

    /// Generates deterministic scope class name (e.g. "kml-xxxxxx") based on component name
    [[nodiscard]] std::string generateScopeHash() const;

    /// Lower style block for Web or TUI targets
    ScopedStyleResult lower(StyleTarget target = StyleTarget::Web);

private:
    const KmlDocument& doc_;
};

} // namespace keety::kml

