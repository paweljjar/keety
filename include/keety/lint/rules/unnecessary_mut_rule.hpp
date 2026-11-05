#pragma once

#include <keety/lint/lint_rule.hpp>

#include <unordered_set>

namespace keety::lint {

/// L010: Detects parameters declared with `mut` that are never reassigned or mutably borrowed.
class UnnecessaryMutRule : public LintRule {
public:
    [[nodiscard]] std::string_view getCode() const noexcept override { return "L010"; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "unnecessary-mut"; }
    [[nodiscard]] std::string_view getDescription() const noexcept override {
        return "Flags parameters declared with 'mut' that are never reassigned or mutably borrowed";
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
            checkParams(fn->getParams(), fn->getBody(), diags);
            checkStmt(fn->getBody(), diags);
        } else if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl)) {
            for (const auto& m : cls->getMethods()) {
                checkDecl(m.get(), diags);
            }
        }
    }

    void checkParams(const std::vector<ast::Param>& params, const ast::Stmt* body, std::vector<LintDiagnostic>& diags) {
        if (!body) return;

        // Collect all variable identifiers that are mutated in body
        std::unordered_set<std::string> mutated;
        collectMutations(body, mutated);

        for (const auto& p : params) {
            if (p.isMut && !mutated.contains(p.name)) {
                std::string msg = "Parameter '" + p.name + "' is marked 'mut' but is never reassigned or mutably borrowed";
                diags.emplace_back(
                    std::string(getCode()),
                    std::string(getName()),
                    getDefaultSeverity(),
                    std::move(msg),
                    p.span,
                    FixEdit{p.span, p.name}
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
            checkParams(closure->getParams(), closure->getBody(), diags);
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

    static void collectMutations(const ast::Stmt* stmt, std::unordered_set<std::string>& outMutated) {
        if (!stmt) return;

        if (const auto* block = dynamic_cast<const ast::BlockStmt*>(stmt)) {
            for (const auto& s : block->getStatements()) {
                collectMutations(s.get(), outMutated);
            }
        } else if (const auto* exprStmt = dynamic_cast<const ast::ExprStmt*>(stmt)) {
            collectExprMutations(exprStmt->getExpression(), outMutated);
        } else if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(stmt)) {
            collectMutations(ifStmt->getInit(), outMutated);
            collectExprMutations(ifStmt->getCondition(), outMutated);
            collectMutations(ifStmt->getThenBranch(), outMutated);
            collectMutations(ifStmt->getElseBranch(), outMutated);
        } else if (const auto* wh = dynamic_cast<const ast::WhileStmt*>(stmt)) {
            collectMutations(wh->getInit(), outMutated);
            collectExprMutations(wh->getCondition(), outMutated);
            collectMutations(wh->getBody(), outMutated);
        } else if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(stmt)) {
            collectMutations(forStmt->getInit(), outMutated);
            collectExprMutations(forStmt->getCondition(), outMutated);
            collectMutations(forStmt->getPost(), outMutated);
            collectMutations(forStmt->getBody(), outMutated);
        } else if (const auto* forIn = dynamic_cast<const ast::ForInStmt*>(stmt)) {
            collectExprMutations(forIn->getCollection(), outMutated);
            collectMutations(forIn->getBody(), outMutated);
        }
    }

    static void collectExprMutations(const ast::Expr* expr, std::unordered_set<std::string>& outMutated) {
        if (!expr) return;

        if (const auto* bin = dynamic_cast<const ast::BinaryExpr*>(expr)) {
            switch (bin->getOp()) {
            case lexer::TokenKind::Equal:
            case lexer::TokenKind::PlusEqual:
            case lexer::TokenKind::MinusEqual:
            case lexer::TokenKind::StarEqual:
            case lexer::TokenKind::SlashEqual:
            case lexer::TokenKind::PercentEqual:
                if (const auto* id = dynamic_cast<const ast::IdentExpr*>(bin->getLeft())) {
                    outMutated.insert(id->getName());
                }
                break;
            default:
                break;
            }
            collectExprMutations(bin->getLeft(), outMutated);
            collectExprMutations(bin->getRight(), outMutated);
        } else if (const auto* un = dynamic_cast<const ast::UnaryExpr*>(expr)) {
            if (un->getOp() == lexer::TokenKind::PlusPlus || un->getOp() == lexer::TokenKind::MinusMinus) {
                if (const auto* id = dynamic_cast<const ast::IdentExpr*>(un->getOperand())) {
                    outMutated.insert(id->getName());
                }
            }
            collectExprMutations(un->getOperand(), outMutated);
        } else if (const auto* call = dynamic_cast<const ast::CallExpr*>(expr)) {
            collectExprMutations(call->getCallee(), outMutated);
            for (const auto& a : call->getArgs()) {
                if (a.isMut) {
                    if (const auto* id = dynamic_cast<const ast::IdentExpr*>(a.value.get())) {
                        outMutated.insert(id->getName());
                    }
                }
                collectExprMutations(a.value.get(), outMutated);
            }
        }
    }
};

} // namespace keety::lint
