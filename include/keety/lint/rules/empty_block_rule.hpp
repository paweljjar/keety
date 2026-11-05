#pragma once

#include <keety/lint/lint_rule.hpp>

namespace keety::lint {

/// L022: Detects empty statement blocks `{}` in control flow or function bodies.
class EmptyBlockRule : public LintRule {
public:
    [[nodiscard]] std::string_view getCode() const noexcept override { return "L022"; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "empty-block"; }
    [[nodiscard]] std::string_view getDescription() const noexcept override {
        return "Flags empty statement blocks '{}' in control flow or function definitions";
    }
    [[nodiscard]] diag::DiagnosticSeverity getDefaultSeverity() const noexcept override {
        return diag::DiagnosticSeverity::Warning;
    }

    [[nodiscard]] std::vector<LintDiagnostic> run(const LintContext& context) override {
        std::vector<LintDiagnostic> diagnostics;

        for (const auto& decl : context.moduleAst.declarations) {
            checkDecl(decl.get(), diagnostics);
        }

        return diagnostics;
    }

private:
    void checkDecl(const ast::Decl* decl, std::vector<LintDiagnostic>& diags) {
        if (!decl) return;

        if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl)) {
            checkStmt(fn->getBody(), diags);
        } else if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl)) {
            for (const auto& m : cls->getMethods()) {
                checkDecl(m.get(), diags);
            }
        }
    }

    void checkStmt(const ast::Stmt* stmt, std::vector<LintDiagnostic>& diags) {
        if (!stmt) return;

        if (const auto* block = dynamic_cast<const ast::BlockStmt*>(stmt)) {
            if (block->getStatements().empty()) {
                diags.emplace_back(
                    std::string(getCode()),
                    std::string(getName()),
                    getDefaultSeverity(),
                    "Empty statement block '{}'",
                    block->getSpan()
                );
            } else {
                for (const auto& s : block->getStatements()) {
                    checkStmt(s.get(), diags);
                }
            }
        } else if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(stmt)) {
            checkStmt(ifStmt->getThenBranch(), diags);
            checkStmt(ifStmt->getElseBranch(), diags);
        } else if (const auto* wh = dynamic_cast<const ast::WhileStmt*>(stmt)) {
            checkStmt(wh->getBody(), diags);
        } else if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(stmt)) {
            checkStmt(forStmt->getBody(), diags);
        } else if (const auto* forIn = dynamic_cast<const ast::ForInStmt*>(stmt)) {
            checkStmt(forIn->getBody(), diags);
        } else if (const auto* sw = dynamic_cast<const ast::SwitchStmt*>(stmt)) {
            for (const auto& c : sw->getCases()) {
                checkStmt(c.body.get(), diags);
            }
        }
    }
};

} // namespace keety::lint
