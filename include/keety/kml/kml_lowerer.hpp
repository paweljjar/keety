#pragma once

#include <keety/ast/decl.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/kml/dependency_analyzer.hpp>
#include <keety/kml/kml_ast.hpp>
#include <keety/kml/style_scoper.hpp>

#include <memory>
#include <string>
#include <vector>

namespace keety::kml {

/// Desugared event handler or binding wrapper
struct DesugaredCallback {
    std::string name;           // e.g. "handleClick_cb0"
    std::string rawLambda;       // e.g. "() => { count = count + 1; }"
    std::string modifierWrapper; // e.g. debounce wrapper or prevent wrapper
};

/// Result of lowering a KML Single-File Component
struct LoweredComponentResult {
    std::string componentName;
    std::string scopeClass;
    std::string scopedCss;

    // Generated KeetScript source code extending std.ui.Component
    std::string generatedKeetScript;

    // Generated C++ code embedding lifecycle methods and surgical updates
    std::string generatedCppCode;

    // Desugared callbacks
    std::vector<DesugaredCallback> callbacks;

    // Analysis results (masks, dependency graph)
    ReactiveAnalysisResult analysis;
};

class KmlLowerer {
public:
    explicit KmlLowerer(const KmlDocument& doc, diag::SourceManager* sourceMgr = nullptr);

    /// Lowers the complete KML Document into a Component class definition
    LoweredComponentResult lower();

    /// Desugars a `$bind.prop={state}` directive into a getter + setter callback
    static std::string desugarTwoWayBinding(const KmlDirective& directive, const std::string& stateVar);

    /// Desugars event modifiers (e.g. `@input.debounce(150)`) into runtime wrappers
    static std::string desugarEventModifiers(const std::string& rawHandler, const std::vector<std::string>& modifiers);

private:
    std::string synthesizeConstructor(const ReactiveAnalysisResult& analysis);
    std::string synthesizeMount(const ReactiveAnalysisResult& analysis);
    std::string synthesizeUpdate(const ReactiveAnalysisResult& analysis);
    std::string synthesizeRenderVdom(const ReactiveAnalysisResult& analysis);
    std::string synthesizeDestroy();

    void generateNodeMountCode(const KmlTemplateNode* node, std::stringstream& ss, int& nodeIndex, int parentIndex);
    void generateNodeVdomCode(const KmlTemplateNode* node, std::stringstream& ss);

    const KmlDocument& doc_;
    [[maybe_unused]] diag::SourceManager* sourceMgr_;
};

} // namespace keety::kml

