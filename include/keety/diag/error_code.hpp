#pragma once

#include <cstdint>
#include <string_view>

namespace keety::diag {

/// Diagnostic error codes categorized according to the Keety Compiler Architecture Specification
enum class ErrorCode : uint16_t {
    // -------------------------------------------------------------------------
    // E0001 - E0999: Lexical & Syntactic Grammar Errors
    // -------------------------------------------------------------------------
    UnterminatedStringLiteral     = 1,
    UnterminatedComment           = 2,
    InvalidCharacterLiteral       = 3,
    InvalidNumericLiteral         = 4,
    UnexpectedToken               = 5,
    ExpectedIdentifier            = 6,
    ExpectedSemicolon             = 7,
    ExpectedTypeAnnotation        = 8,
    ExpectedClosingParen          = 9,
    ExpectedClosingBrace          = 10,
    ExpectedClosingBracket        = 11,
    ExpectedColon                 = 12,
    InvalidEscapeSequence         = 13,
    UnbalancedTemplateBrace       = 14,
    ExpectedOpeningParen          = 15,

    // -------------------------------------------------------------------------
    // E1000 - E1999: Module Resolution & Cyclic Dependency Errors
    // -------------------------------------------------------------------------
    CircularModuleDependency      = 1001,
    ModuleNotFound                = 1002,
    CannotImportSelf              = 1003,
    DuplicateModuleImport         = 1004,
    SymbolNotExported             = 1005,
    AmbiguousImportSymbol         = 1006,

    // -------------------------------------------------------------------------
    // E2000 - E2999: Semantic, Scoping, and Shadowing Errors
    // -------------------------------------------------------------------------
    IdentifierShadowed            = 2001,
    ExportConstWrite              = 2002,
    UndeclaredIdentifier          = 2003,
    RedeclarationInSameScope      = 2004,
    CannotAssignToConstant        = 2005,
    UseBeforeInitialization       = 2006,
    NonLValueAssignmentTarget     = 2007,

    // -------------------------------------------------------------------------
    // E3000 - E3999: Type Checking, Union Narrowing & Assignment Soundness
    // -------------------------------------------------------------------------
    MissingTypeAnnotation         = 3001,
    ImplicitTypeWidening          = 3002,
    TypeMismatch                  = 3003,
    NonBooleanCondition           = 3004,
    InvalidBinaryOperands         = 3005,
    InvalidUnaryOperand           = 3006,
    UnknownType                   = 3007,
    InvalidUnionNarrowing         = 3008,
    CannotInferCollectionType     = 3009,
    InvalidTypeCast               = 3010,
    InvalidMemberAccess           = 3011,
    InvalidIndexOperation         = 3012,

    // -------------------------------------------------------------------------
    // E4000 - E4999: Memory Safety, Aliasing & Mutable Borrow Checker
    // -------------------------------------------------------------------------
    DuplicateMutableBorrow        = 4001, // swap(mut x, mut x)
    ContainerAndElementBorrow     = 4002, // f(mut arr[i], mut arr)
    MapElementMutableBorrow       = 4003, // f(mut m[k])
    AsyncMutableParameter         = 4004, // async fn foo(mut x: int)
    NonLValueMutableBorrow        = 4005, // f(mut 10)
    DefaultArgMutableBorrow       = 4006, // fn foo(mut x: int = 5)

    // -------------------------------------------------------------------------
    // E5000 - E5999: Control Flow, Termination & Reachability Errors
    // -------------------------------------------------------------------------
    MissingReturnStatement        = 5001,
    UnreachableCode               = 5002,
    BreakOutsideLoopOrSwitch      = 5003,
    ContinueOutsideLoop           = 5004,
    InvalidLoopLabel              = 5005,
    YieldOutsideZonkBlock         = 5006,
    NonExhaustiveSwitch           = 5007,

    // -------------------------------------------------------------------------
    // E6000 - E6999: Foreign Function Interface (FFI) & Linkage Errors
    // -------------------------------------------------------------------------
    ExternSymbolNotFound          = 6001,
    IncompatibleFFIType           = 6002,
    InvalidLinkageSpecification   = 6003,

    // -------------------------------------------------------------------------
    // E7000 - E7999: Backend & Toolchain Diagnostics
    // -------------------------------------------------------------------------
    BackendCompilationError       = 7001,
    BackendTemplateInstantiation  = 7002,
    BackendCodegenConstraint      = 7003
};

/// Diagnostic code info helper
struct ErrorCodeInfo {
    std::string_view code;        // e.g. "E4001"
    std::string_view title;       // e.g. "duplicate mutable borrow"
    std::string_view description; // Standard explanation
};

/// Returns code string formatted as "EXXXX"
[[nodiscard]] std::string_view getErrorCodeString(ErrorCode code) noexcept;

/// Returns full metadata about the error code
[[nodiscard]] ErrorCodeInfo getErrorCodeInfo(ErrorCode code) noexcept;

} // namespace keety::diag
