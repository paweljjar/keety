#pragma once

#include <keety/lint/lint_rule.hpp>
#include <keety/lint/reference_collector.hpp>

#include <unordered_set>

namespace keety::lint {

/// L003: Detects function and closure parameters that are never referenced in the body.
class UnusedParameterRule : public LintRule {
public:
    [[nodiscard]] std::string_view getCode() const noexcept override { return "L003"; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "unused-parameter"; }
    [[nodiscard]] std::string_view getDescription() const noexcept override {
        return "Flags function or closure parameters that are never referenced in the body";
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
            checkSignature(fn->getParams(), fn->getBody(), diags);
            checkStmt(fn->getBody(), diags);
        } else if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl)) {
            for (const auto& m : cls->getMethods()) {
                checkDecl(m.get(), diags);
            }
        }
    }

    void checkSignature(const std::vector<ast::Param>& params, const ast::Stmt* body, std::vector<LintDiagnostic>& diags) {
        if (!body) return;

        std::unordered_set<std::string> referenced;
        ReferenceCollector::collect(body, referenced);

        for (const auto& p : params) {
            // Ignore blank or underscore-prefixed parameters
            if (p.name.empty() || p.name.starts_with("_")) {
                continue;
            }

            if (!referenced.contains(p.name)) {
                std::string msg = "Parameter '" + p.name + "' is never used in the function body. Rename to '_' or prefix with '_' if intentional.";
                diag::SourceSpan targetSpan = p.nameSpan.isValid() ? p.nameSpan : p.span;
                diags.emplace_back(
                    std::string(getCode()),
                    std::string(getName()),
                    getDefaultSeverity(),
                    std::move(msg),
                    targetSpan,
                    FixEdit{targetSpan, "_" + p.name}
                );
            }
        }
    }

    void checkStmt(const ast::Stmt* stmt, std::vector<LintDiagnostic>& diags) {
        if (!stmt) return;

        if (const auto* block = dynamic_cast<const ast::BlockStmt*>(stmt)) {
            for (const auto& s : block->getStatements()) {
                checkStmt(s.get(), diags);
            }
        } else if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(stmt)) {
            const auto* d = declStmt->getDeclaration();
            if (const auto* v = dynamic_cast<const ast::VarDecl*>(d)) {
                checkExpr(v->getInitializer(), diags);
            } else if (const auto* c = dynamic_cast<const ast::ConstDecl*>(d)) {
                checkExpr(c->getInitializer(), diags);
            }
        } else if (const auto* exprStmt = dynamic_cast<const ast::ExprStmt*>(stmt)) {
            checkExpr(exprStmt->getExpression(), diags);
        } else if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(stmt)) {
            checkStmt(ifStmt->getInit(), diags);
            checkExpr(ifStmt->getCondition(), diags);
            checkStmt(ifStmt->getThenBranch(), diags);
            checkStmt(ifStmt->getElseBranch(), diags);
        } else if (const auto* wh = dynamic_cast<const ast::WhileStmt*>(stmt)) {
            checkStmt(wh->getInit(), diags);
            checkExpr(wh->getCondition(), diags);
            checkStmt(wh->getBody(), diags);
        } else if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(stmt)) {
            checkStmt(forStmt->getInit(), diags);
            checkExpr(forStmt->getCondition(), diags);
            checkStmt(forStmt->getPost(), diags);
            checkStmt(forStmt->getBody(), diags);
        } else if (const auto* forIn = dynamic_cast<const ast::ForInStmt*>(stmt)) {
            checkExpr(forIn->getCollection(), diags);
            checkStmt(forIn->getBody(), diags);
        }
    }

    void checkExpr(const ast::Expr* expr, std::vector<LintDiagnostic>& diags) {
        if (!expr) return;

        if (const auto* closure = dynamic_cast<const ast::ClosureExpr*>(expr)) {
            checkSignature(closure->getParams(), closure->getBody(), diags);
            checkStmt(closure->getBody(), diags);
        } else if (const auto* bin = dynamic_cast<const ast::BinaryExpr*>(expr)) {
            checkExpr(bin->getLeft(), diags);
            checkExpr(bin->getRight(), diags);
        } else if (const auto* call = dynamic_cast<const ast::CallExpr*>(expr)) {
            checkExpr(call->getCallee(), diags);
            for (const auto& a : call->getArgs()) {
                checkExpr(a.value.get(), diags);
            }
        }
    }
};

} // namespace keety::lint
