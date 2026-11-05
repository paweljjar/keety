#pragma once

#include <keety/lint/lint_rule.hpp>
#include <keety/lint/reference_collector.hpp>

namespace keety::lint {

/// L001: Detects imported modules and symbols that are never referenced in the file.
class UnusedImportRule : public LintRule {
public:
    [[nodiscard]] std::string_view getCode() const noexcept override { return "L001"; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "unused-import"; }
    [[nodiscard]] std::string_view getDescription() const noexcept override {
        return "Flags imported modules and symbols that are never referenced in the file";
    }
    [[nodiscard]] diag::DiagnosticSeverity getDefaultSeverity() const noexcept override {
        return diag::DiagnosticSeverity::Warning;
    }

    [[nodiscard]] std::vector<LintDiagnostic> run(const LintContext& context) override {
        std::vector<LintDiagnostic> diagnostics;

        // 1. Collect all referenced identifiers across all non-import declarations
        std::unordered_set<std::string> referencedIdentifiers;
        for (const auto& decl : context.moduleAst.declarations) {
            if (dynamic_cast<const ast::ImportDecl*>(decl.get())) {
                continue; // Skip the import declaration itself
            }
            ReferenceCollector::collect(decl.get(), referencedIdentifiers);
        }

        // 2. Inspect each ImportDecl
        for (const auto& decl : context.moduleAst.declarations) {
            const auto* importDecl = dynamic_cast<const ast::ImportDecl*>(decl.get());
            if (!importDecl || importDecl->isReExported()) {
                continue;
            }

            if (importDecl->isSelective()) {
                // from "..." import sym1, sym2;
                bool allUnused = true;
                for (const auto& sym : importDecl->getSymbols()) {
                    if (referencedIdentifiers.contains(sym.getEffectiveName())) {
                        allUnused = false;
                        break;
                    }
                }

                for (const auto& sym : importDecl->getSymbols()) {
                    const std::string& name = sym.getEffectiveName();
                    if (!referencedIdentifiers.contains(name)) {
                        std::string msg = "Imported symbol '" + name + "' is never used";
                        diag::SourceSpan span = sym.span.isValid() ? sym.span : importDecl->getSpan();
                        std::optional<FixEdit> fix = std::nullopt;
                        if (allUnused) {
                            fix = FixEdit{importDecl->getSpan(), ""};
                        }
                        diagnostics.emplace_back(
                            std::string(getCode()),
                            std::string(getName()),
                            getDefaultSeverity(),
                            std::move(msg),
                            span,
                            std::move(fix)
                        );
                    }
                }
            } else {
                // import module; or import module as alias;
                std::string boundName = importDecl->getModuleAlias();
                if (boundName.empty()) {
                    // Extract basename from module path (e.g. "io", "./foo/bar" -> "bar")
                    std::string path = importDecl->getModulePath();
                    size_t slash = path.find_last_of("/\\");
                    if (slash != std::string::npos) {
                        boundName = path.substr(slash + 1);
                    } else {
                        boundName = path;
                    }
                }

                if (!referencedIdentifiers.contains(boundName)) {
                    std::string msg = "Imported module '" + boundName + "' is never used";
                    diagnostics.emplace_back(
                        std::string(getCode()),
                        std::string(getName()),
                        getDefaultSeverity(),
                        std::move(msg),
                        importDecl->getSpan(),
                        FixEdit{importDecl->getSpan(), ""}
                    );
                }
            }
        }

        return diagnostics;
    }
};

} // namespace keety::lint
