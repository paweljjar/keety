#pragma once

#include <keety/lint/lint_rule.hpp>

namespace keety::lint {

/// L020: Detects redundant explicit type annotations when the type is trivially evident
/// from a primitive literal initializer (e.g. `let x: int = 10;`).
class RedundantTypeAnnotationRule : public LintRule {
public:
    [[nodiscard]] std::string_view getCode() const noexcept override { return "L020"; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "redundant-type-annotation"; }
    [[nodiscard]] std::string_view getDescription() const noexcept override {
        return "Flags redundant type annotations on variables initialized with primitive literals";
    }
    [[nodiscard]] diag::DiagnosticSeverity getDefaultSeverity() const noexcept override {
        return diag::DiagnosticSeverity::Note;
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

        if (const auto* var = dynamic_cast<const ast::VarDecl*>(decl)) {
            checkVarOrConst(var->getType(), var->getInitializer(), diags);
        } else if (const auto* c = dynamic_cast<const ast::ConstDecl*>(decl)) {
            checkVarOrConst(c->getType(), c->getInitializer(), diags);
        } else if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl)) {
            checkStmt(fn->getBody(), diags);
        } else if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl)) {
            for (const auto& m : cls->getMethods()) {
                checkDecl(m.get(), diags);
            }
        }
    }

    void checkVarOrConst(const ast::TypeAnnotation* type, const ast::Expr* init, std::vector<LintDiagnostic>& diags) {
        if (!type || !init) return;

        const auto* prim = dynamic_cast<const ast::PrimitiveType*>(type);
        const auto* lit = dynamic_cast<const ast::LiteralExpr*>(init);

        if (!prim || !lit) return;

        bool isRedundant = false;
        switch (prim->getKind()) {
        case ast::PrimitiveTypeKind::Int:
            isRedundant = (lit->getLiteralKind() == lexer::TokenKind::IntLiteral);
            break;
        case ast::PrimitiveTypeKind::Float:
            isRedundant = (lit->getLiteralKind() == lexer::TokenKind::FloatLiteral);
            break;
        case ast::PrimitiveTypeKind::String:
            isRedundant = (lit->getLiteralKind() == lexer::TokenKind::StringLiteral);
            break;
        case ast::PrimitiveTypeKind::Bool:
            isRedundant = (lit->getLiteralKind() == lexer::TokenKind::KwTrue ||
                           lit->getLiteralKind() == lexer::TokenKind::KwFalse);
            break;
        case ast::PrimitiveTypeKind::Char:
            isRedundant = (lit->getLiteralKind() == lexer::TokenKind::CharLiteral);
            break;
        default:
            break;
        }

        if (isRedundant) {
            std::string msg = "Explicit type annotation '" + std::string(prim->getTypeName()) +
                              "' is redundant because the initializer is a literal of the same type";
            diags.emplace_back(
                std::string(getCode()),
                std::string(getName()),
                getDefaultSeverity(),
                std::move(msg),
                prim->getSpan()
            );
        }
    }

    void checkStmt(const ast::Stmt* stmt, std::vector<LintDiagnostic>& diags) {
        if (!stmt) return;

        if (const auto* block = dynamic_cast<const ast::BlockStmt*>(stmt)) {
            for (const auto& s : block->getStatements()) {
                checkStmt(s.get(), diags);
            }
        } else if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(stmt)) {
            checkDecl(declStmt->getDeclaration(), diags);
        } else if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(stmt)) {
            checkStmt(ifStmt->getThenBranch(), diags);
            checkStmt(ifStmt->getElseBranch(), diags);
        } else if (const auto* wh = dynamic_cast<const ast::WhileStmt*>(stmt)) {
            checkStmt(wh->getBody(), diags);
        } else if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(stmt)) {
            checkStmt(forStmt->getBody(), diags);
        }
    }
};

} // namespace keety::lint
