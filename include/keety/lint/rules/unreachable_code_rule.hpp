#pragma once

#include <keety/lint/lint_rule.hpp>

namespace keety::lint {

/// L004: Detects statements situated after a terminal control-flow statement
/// (return, break, continue, yield, or zonk statement).
class UnreachableCodeRule : public LintRule {
public:
    [[nodiscard]] std::string_view getCode() const noexcept override { return "L004"; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "unreachable-code"; }
    [[nodiscard]] std::string_view getDescription() const noexcept override {
        return "Flags unreachable statements positioned after return, break, continue, yield, or zonk";
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
            checkBlock(block, diags);
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

    void checkBlock(const ast::BlockStmt* block, std::vector<LintDiagnostic>& diags) {
        if (!block) return;

        const auto& stmts = block->getStatements();
        bool terminated = false;

        for (size_t i = 0; i < stmts.size(); ++i) {
            const auto* s = stmts[i].get();
            if (!s) continue;

            if (terminated) {
                // Any statement after a terminator is unreachable!
                diags.emplace_back(
                    std::string(getCode()),
                    std::string(getName()),
                    getDefaultSeverity(),
                    "Unreachable statement detected after terminal control flow",
                    s->getSpan()
                );
            } else if (isTerminator(s)) {
                terminated = true;
            } else {
                // Check nested blocks
                checkStmt(s, diags);
            }
        }
    }

    static bool isTerminator(const ast::Stmt* stmt) {
        if (!stmt) return false;
        if (dynamic_cast<const ast::ReturnStmt*>(stmt)) return true;
        if (dynamic_cast<const ast::BreakStmt*>(stmt)) return true;
        if (dynamic_cast<const ast::ContinueStmt*>(stmt)) return true;
        if (dynamic_cast<const ast::YieldStmt*>(stmt)) return true;
        if (dynamic_cast<const ast::ZonkStmt*>(stmt)) return true;
        return false;
    }
};

} // namespace keety::lint
