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

    // --- Explicit Type Annotation Validation ---

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

    // --- Strict Zero-Coercion Policy (E3002) ---

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

    // --- Contextual Typing of Empty Collection Literals (E3009 / E3003) ---

    /// Evaluates/infers the static semantic type of an expression within a given scope,
    /// taking an optional expected contextual type into account (e.g. for [] and {}).
    TypePtr inferType(const ast::Expr& expr, sema::Scope& scope, TypePtr contextualType = nullptr);

    /// Checks whether actualType can be assigned/passed to expectedType without coercion or mismatch.
    /// Emits E3002 if numeric coercion occurs, or E3003 if types are incompatible.
    bool checkAssignment(TypePtr expectedType, TypePtr actualType, diag::SourceSpan span, std::string_view contextDesc = "");

    /// Complete type check validating explicit annotations, zero-coercion, and contextual collections.
    bool checkTypes(const parser::ModuleAST& module, sema::Scope* rootScope = nullptr);

    // Factory methods for Contextual Typing diagnostics
    [[nodiscard]] static diag::Diagnostic createCannotInferCollectionTypeDiagnostic(
        std::string_view literalKind,
        diag::SourceSpan span);

    [[nodiscard]] static diag::Diagnostic createTypeMismatchDiagnostic(
        TypePtr expectedType,
        TypePtr actualType,
        diag::SourceSpan span,
        std::string_view contextDesc = "");

    // --- Control-Flow Analysis (CFA) & Type Narrowing ---

    struct NarrowingFact {
        std::string varName;
        TypePtr narrowedType;
    };

    struct BranchNarrowing {
        std::vector<NarrowingFact> thenFacts;
        std::vector<NarrowingFact> elseFacts;
    };

    /// Analyzes a boolean condition expression (such as `typeof(x) == "T"` or `x != null`)
    /// and extracts flow-based type narrowing facts for the 'then' and 'else' branches.
    BranchNarrowing analyzeCondition(const ast::Expr* condition, sema::Scope& scope);

    // --- Exhaustiveness & Reachability Analysis ---

    struct ReachabilityResult {
        bool locallyTerminates{false}; // Cannot reach statement textually following in same block
        bool functionExits{false};     // Guarantees return/divergence from function
    };

    /// Checks statement reachability, detecting dead code following terminating statements (E5002)
    ReachabilityResult checkReachability(const ast::Stmt* stmt);

    /// Checks whether all paths through a function/method/closure body return or diverge (E5001)
    bool checkExhaustiveness(const ast::Stmt* body, std::string_view name, TypePtr returnType, diag::SourceSpan span);

    /// Checks nested closures within expressions for return exhaustiveness and dead code
    void checkClosuresInExpr(const ast::Expr* expr);

    // Factory methods for Exhaustiveness & Reachability diagnostics
    [[nodiscard]] static diag::Diagnostic createMissingReturnDiagnostic(
        std::string_view functionName,
        TypePtr returnType,
        diag::SourceSpan span);

    [[nodiscard]] static diag::Diagnostic createUnreachableCodeDiagnostic(
        diag::SourceSpan span);

    // --- Mutable Reference Borrow Checker (L-Value Validation - E4005) ---

    /// Validates whether an expression is an addressable mutable l-value.
    /// Returns false if expr is an r-value, literal, const binding, or temporary.
    bool isMutableLValue(const ast::Expr* expr, sema::Scope& scope, std::string* outReason = nullptr);

    [[nodiscard]] static diag::Diagnostic createNonLValueMutableBorrowDiagnostic(
        std::string_view exprDesc,
        diag::SourceSpan span);

    // --- Mutable Reference Borrow Checker (Exclusive Borrowing - E4001) ---

    /// Computes the normalized storage path of an l-value expression (e.g. "x", "p.hp", "arr[0]").
    /// Returns empty string if the expression does not have a recognizable storage path.
    [[nodiscard]] static std::string computeStoragePath(const ast::Expr* expr);

    /// Checks if two storage paths alias or overlap (identical path or parent/child field relationship).
    [[nodiscard]] static bool storagePathsAlias(const std::string& p1, const std::string& p2);

    [[nodiscard]] static diag::Diagnostic createDuplicateMutableBorrowDiagnostic(
        std::string_view path,
        diag::SourceSpan firstSpan,
        diag::SourceSpan duplicateSpan);

    // --- Mutable Reference Borrow Checker (Container & Element Borrowing - E4002) ---

    /// Checks if one storage path is a container and the other is an indexed element of it.
    [[nodiscard]] static bool isContainerAndElementPair(
        const std::string& p1,
        const std::string& p2,
        std::string* outContainer = nullptr,
        std::string* outElement = nullptr);

    [[nodiscard]] static diag::Diagnostic createContainerAndElementBorrowDiagnostic(
        std::string_view containerPath,
        std::string_view elementPath,
        diag::SourceSpan firstSpan,
        diag::SourceSpan secondSpan);

    // --- Mutable Reference Borrow Checker (Map Element Borrowing - E4003) ---

    /// Checks if an expression is a map element access (e.g. m[k]).
    [[nodiscard]] bool isMapIndexExpr(const ast::Expr* expr, sema::Scope& scope);

    [[nodiscard]] static diag::Diagnostic createMapElementMutableBorrowDiagnostic(
        std::string_view path,
        diag::SourceSpan span);

    // --- Mutable Reference Borrow Checker (Async & Default Args - E4004, E4006) ---

    [[nodiscard]] static diag::Diagnostic createAsyncMutableParameterDiagnostic(
        std::string_view paramName,
        diag::SourceSpan span);

    [[nodiscard]] static diag::Diagnostic createDefaultArgMutableBorrowDiagnostic(
        std::string_view paramName,
        diag::SourceSpan span);

private:
    diag::DiagnosticEmitter* emitter_{nullptr};
    bool hasErrors_{false};
    TypePtr currentFunctionReturnType_{nullptr};
    sema::Scope* currentScope_{nullptr};

    void reportMissingType(std::string_view entityKind, std::string_view entityName, diag::SourceSpan span);
    void reportImplicitWidening(TypePtr fromType, TypePtr toType, diag::SourceSpan span, std::string_view contextDesc);
    void reportBinaryCoercion(TypePtr leftType, TypePtr rightType, lexer::TokenKind op, diag::SourceSpan span);
    void reportCannotInferCollectionType(std::string_view literalKind, diag::SourceSpan span);
    void reportTypeMismatch(TypePtr expectedType, TypePtr actualType, diag::SourceSpan span, std::string_view contextDesc);
    void reportMissingReturn(std::string_view functionName, TypePtr returnType, diag::SourceSpan span);
    void reportUnreachableCode(diag::SourceSpan span);
    void reportNonLValueMutableBorrow(std::string_view exprDesc, diag::SourceSpan span);
    void reportDuplicateMutableBorrow(std::string_view path, diag::SourceSpan firstSpan, diag::SourceSpan duplicateSpan);
    void reportContainerAndElementBorrow(std::string_view containerPath, std::string_view elementPath, diag::SourceSpan firstSpan, diag::SourceSpan secondSpan);
    void reportMapElementMutableBorrow(std::string_view path, diag::SourceSpan span);
    void reportAsyncMutableParameter(std::string_view paramName, diag::SourceSpan span);
    void reportDefaultArgMutableBorrow(std::string_view paramName, diag::SourceSpan span);
    void reportCannotAssignToConstant(std::string_view name, diag::SourceSpan span);
    void checkTypeAnnotationValidity(const ast::TypeAnnotation* annot);

    [[nodiscard]] bool isIoColorType(const TypePtr& t) const noexcept;
    TypePtr checkIoMemberCall(const ast::MemberExpr* member, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkIoDirectCall(const ast::IdentExpr* callee, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkFsMemberCall(const ast::MemberExpr* member, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkFsDirectCall(const ast::IdentExpr* callee, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkSysMemberCall(const ast::MemberExpr* member, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkSysDirectCall(const ast::IdentExpr* callee, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkMathMemberCall(const ast::MemberExpr* member, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkMathDirectCall(const ast::IdentExpr* callee, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkTimeMemberCall(const ast::MemberExpr* member, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkTimeDirectCall(const ast::IdentExpr* callee, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkJsonMemberCall(const ast::MemberExpr* member, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkJsonDirectCall(const ast::IdentExpr* callee, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkNetMemberCall(const ast::MemberExpr* member, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkNetDirectCall(const ast::IdentExpr* callee, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkTestMemberCall(const ast::MemberExpr* member, const ast::CallExpr* call, sema::Scope& scope);
    TypePtr checkTestDirectCall(const ast::IdentExpr* callee, const ast::CallExpr* call, sema::Scope& scope);
};

} // namespace keety::typechecker
