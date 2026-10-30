#pragma once

#include <keety/ast/decl.hpp>
#include <keety/ast/expr.hpp>
#include <keety/ast/stmt.hpp>
#include <keety/diag/diagnostic.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/parser/parser.hpp>
#include <keety/sema/scope.hpp>
#include <keety/typechecker/type.hpp>

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

    // --- Task 3.3.2: Strict Zero-Coercion Policy (E3002) ---

    /// Validates zero-coercion across declarations, statements, expressions, assignments,
    /// returns, and function calls. Emits E3002 (ImplicitTypeWidening) on implicit
    /// conversions between numeric types (such as int -> float or float -> int).
    bool checkZeroCoercion(const parser::ModuleAST& module, sema::Scope* rootScope = nullptr);

    /// Checks an individual declaration for zero-coercion violations.
    bool checkZeroCoercion(const ast::Decl& decl, sema::Scope& scope);

    /// Checks a statement for zero-coercion violations.
    bool checkZeroCoercion(const ast::Stmt& stmt, sema::Scope& scope);

    // Factory methods for creating standardized E3002 diagnostics
    [[nodiscard]] static diag::Diagnostic createImplicitWideningDiagnostic(
        TypePtr fromType,
        TypePtr toType,
        diag::SourceSpan span,
        std::string_view contextDesc = "");

    [[nodiscard]] static diag::Diagnostic createBinaryCoercionDiagnostic(
        TypePtr leftType,
        TypePtr rightType,
        lexer::TokenKind op,
        diag::SourceSpan span);

    // --- Task 3.3.3: Contextual Typing of Empty Collection Literals (E3009 / E3003) ---

    /// Evaluates/infers the static semantic type of an expression within a given scope,
    /// taking an optional expected contextual type into account (e.g. for [] and {}).
    TypePtr inferType(const ast::Expr& expr, sema::Scope& scope, TypePtr contextualType = nullptr);

    /// Checks whether actualType can be assigned/passed to expectedType without coercion or mismatch.
    /// Emits E3002 if numeric coercion occurs, or E3003 if types are incompatible.
    bool checkAssignment(TypePtr expectedType, TypePtr actualType, diag::SourceSpan span, std::string_view contextDesc = "");

    /// Complete type check validating explicit annotations, zero-coercion, and contextual collections.
    bool checkTypes(const parser::ModuleAST& module, sema::Scope* rootScope = nullptr);

    // Factory methods for Task 3.3.3 diagnostics
    [[nodiscard]] static diag::Diagnostic createCannotInferCollectionTypeDiagnostic(
        std::string_view literalKind,
        diag::SourceSpan span);

    [[nodiscard]] static diag::Diagnostic createTypeMismatchDiagnostic(
        TypePtr expectedType,
        TypePtr actualType,
        diag::SourceSpan span,
        std::string_view contextDesc = "");

private:
    diag::DiagnosticEmitter* emitter_{nullptr};
    bool hasErrors_{false};
    TypePtr currentFunctionReturnType_{nullptr};

    void reportMissingType(std::string_view entityKind, std::string_view entityName, diag::SourceSpan span);
    void reportImplicitWidening(TypePtr fromType, TypePtr toType, diag::SourceSpan span, std::string_view contextDesc);
    void reportBinaryCoercion(TypePtr leftType, TypePtr rightType, lexer::TokenKind op, diag::SourceSpan span);
    void reportCannotInferCollectionType(std::string_view literalKind, diag::SourceSpan span);
    void reportTypeMismatch(TypePtr expectedType, TypePtr actualType, diag::SourceSpan span, std::string_view contextDesc);
};

} // namespace keety::typechecker
