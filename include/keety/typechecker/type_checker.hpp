#pragma once

#include <keety/ast/decl.hpp>
#include <keety/ast/expr.hpp>
#include <keety/ast/stmt.hpp>
#include <keety/diag/diagnostic.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/parser/parser.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace keety::typechecker {

/// Static semantic type checker for KeetScript programs.
class TypeChecker {
public:
    explicit TypeChecker(diag::DiagnosticEmitter* emitter = nullptr);

    void setEmitter(diag::DiagnosticEmitter* emitter) noexcept { emitter_ = emitter; }
    [[nodiscard]] diag::DiagnosticEmitter* getEmitter() const noexcept { return emitter_; }
    [[nodiscard]] bool hasErrors() const noexcept { return hasErrors_; }

    // --- Task 3.3.1: Explicit Type Annotation Validation ---

    /// Validates that all declarations in the parsed module AST have explicit type annotations.
    /// Emits E3001 (MissingTypeAnnotation) for any declaration with an omitted type.
    /// Returns true if all checked declarations have explicit types (no E3001 errors).
    bool checkExplicitTypeAnnotations(const parser::ModuleAST& module);

    /// Checks a single declaration for explicit type annotations.
    bool checkExplicitTypeAnnotations(const ast::Decl& decl);

    /// Checks a statement (including nested blocks and for-in loops) for explicit type annotations.
    bool checkExplicitTypeAnnotations(const ast::Stmt& stmt);

    /// Checks an expression (including closure literals) for explicit type annotations.
    bool checkExplicitTypeAnnotations(const ast::Expr& expr);

    // Factory method for creating standardized E3001 diagnostics
    [[nodiscard]] static diag::Diagnostic createMissingTypeDiagnostic(
        std::string_view entityKind,
        std::string_view entityName,
        diag::SourceSpan span);

private:
    diag::DiagnosticEmitter* emitter_{nullptr};
    bool hasErrors_{false};

    void reportMissingType(std::string_view entityKind, std::string_view entityName, diag::SourceSpan span);
};

} // namespace keety::typechecker
