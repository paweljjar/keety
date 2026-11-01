#include <keety/diag/error_code.hpp>

namespace keety::diag {

#define KEETY_ERROR_ENTRY(id, num, str, titleStr, descStr) \
    case ErrorCode::id: return ErrorCodeInfo{str, titleStr, descStr};

ErrorCodeInfo getErrorCodeInfo(ErrorCode code) noexcept {
    switch (code) {
    // E0001 - E0999: Lexical & Syntactic Grammar Errors
    KEETY_ERROR_ENTRY(UnterminatedStringLiteral, 1, "E0001", "unterminated string literal", "The string literal is missing a closing quote delimiter.")
    KEETY_ERROR_ENTRY(UnterminatedComment, 2, "E0002", "unterminated block comment", "The block comment is missing a closing '*/' delimiter.")
    KEETY_ERROR_ENTRY(InvalidCharacterLiteral, 3, "E0003", "invalid character literal", "The character literal syntax is malformed.")
    KEETY_ERROR_ENTRY(InvalidNumericLiteral, 4, "E0004", "invalid numeric literal", "The numeric literal has an invalid base prefix, digits, or exponent.")
    KEETY_ERROR_ENTRY(UnexpectedToken, 5, "E0005", "unexpected token", "The parser encountered an unexpected token in the current grammatical context.")
    KEETY_ERROR_ENTRY(ExpectedIdentifier, 6, "E0006", "expected identifier", "Expected an identifier name at this position.")
    KEETY_ERROR_ENTRY(ExpectedSemicolon, 7, "E0007", "expected semicolon", "KeetScript requires an explicit semicolon ';' to terminate statements.")
    KEETY_ERROR_ENTRY(ExpectedTypeAnnotation, 8, "E0008", "expected type annotation", "Expected a type annotation ':' followed by a type.")
    KEETY_ERROR_ENTRY(ExpectedClosingParen, 9, "E0009", "expected ')'", "Missing closing parenthesis.")
    KEETY_ERROR_ENTRY(ExpectedClosingBrace, 10, "E0010", "expected '}'", "Missing closing brace.")
    KEETY_ERROR_ENTRY(ExpectedClosingBracket, 11, "E0011", "expected ']'", "Missing closing square bracket.")
    KEETY_ERROR_ENTRY(ExpectedColon, 12, "E0012", "expected ':'", "Expected a colon ':' at this position.")
    KEETY_ERROR_ENTRY(InvalidEscapeSequence, 13, "E0013", "invalid escape sequence", "The escape sequence inside the string or character literal is invalid.")
    KEETY_ERROR_ENTRY(UnbalancedTemplateBrace, 14, "E0014", "unbalanced template brace", "Unbalanced braces inside template string interpolation.")
    KEETY_ERROR_ENTRY(ExpectedOpeningParen, 15, "E0015", "expected '('", "Missing opening parenthesis.")

    // E1000 - E1999: Module Resolution & Cyclic Dependency Errors
    KEETY_ERROR_ENTRY(CircularModuleDependency, 1001, "E1001", "circular module dependency detected", "KeetScript enforces a strict acyclic dependency graph (DAG) across modules.")
    KEETY_ERROR_ENTRY(ModuleNotFound, 1002, "E1002", "module not found", "The specified module file or package could not be resolved.")
    KEETY_ERROR_ENTRY(CannotImportSelf, 1003, "E1003", "cannot import self", "A module cannot import itself.")
    KEETY_ERROR_ENTRY(DuplicateModuleImport, 1004, "E1004", "duplicate module import", "The module has already been imported in this file.")
    KEETY_ERROR_ENTRY(SymbolNotExported, 1005, "E1005", "symbol not exported", "The imported symbol is not exported by the target module.")
    KEETY_ERROR_ENTRY(AmbiguousImportSymbol, 1006, "E1006", "ambiguous imported symbol", "The symbol is imported from multiple modules without qualification.")

    // E2000 - E2999: Semantic, Scoping, and Shadowing Errors
    KEETY_ERROR_ENTRY(IdentifierShadowed, 2001, "E2001", "identifier shadows outer declaration", "KeetScript strictly forbids shadowing identifiers declared in enclosing lexical scopes.")
    KEETY_ERROR_ENTRY(ExportConstWrite, 2002, "E2002", "cannot modify imported export let variable", "Imported 'export let' variables are exposed as read-only const-views.")
    KEETY_ERROR_ENTRY(UndeclaredIdentifier, 2003, "E2003", "undeclared identifier", "The identifier is not declared in the current or any enclosing scope.")
    KEETY_ERROR_ENTRY(RedeclarationInSameScope, 2004, "E2004", "redeclaration in same scope", "An identifier with this name has already been declared in the current scope.")
    KEETY_ERROR_ENTRY(CannotAssignToConstant, 2005, "E2005", "cannot assign to immutable constant", "Constants declared with 'const' or 'define' cannot be reassigned.")
    KEETY_ERROR_ENTRY(UseBeforeInitialization, 2006, "E2006", "variable used before initialization", "The variable is accessed before being assigned an initial value.")
    KEETY_ERROR_ENTRY(NonLValueAssignmentTarget, 2007, "E2007", "invalid assignment target", "The left-hand side of an assignment must be an assignable l-value.")

    // E3000 - E3999: Type Checking, Union Narrowing & Assignment Soundness
    KEETY_ERROR_ENTRY(MissingTypeAnnotation, 3001, "E3001", "missing explicit type annotation", "KeetScript requires explicit type annotations on all variable and constant declarations.")
    KEETY_ERROR_ENTRY(ImplicitTypeWidening, 3002, "E3002", "implicit numeric widening disallowed", "KeetScript enforces strict typing; numeric conversions require explicit cast or conversion.")
    KEETY_ERROR_ENTRY(TypeMismatch, 3003, "E3003", "type mismatch", "The expression type is incompatible with the expected type.")
    KEETY_ERROR_ENTRY(NonBooleanCondition, 3004, "E3004", "non-boolean condition expression", "Conditions in 'if', 'while', or 'for' must evaluate strictly to 'bool'.")
    KEETY_ERROR_ENTRY(InvalidBinaryOperands, 3005, "E3005", "invalid operands to binary operator", "The operator cannot be applied to operands of these types.")
    KEETY_ERROR_ENTRY(InvalidUnaryOperand, 3006, "E3006", "invalid operand to unary operator", "The unary operator cannot be applied to this operand type.")
    KEETY_ERROR_ENTRY(UnknownType, 3007, "E3007", "unknown type", "The specified type name is not recognized.")
    KEETY_ERROR_ENTRY(InvalidUnionNarrowing, 3008, "E3008", "invalid union narrowing", "The requested type is not a member of the union.")
    KEETY_ERROR_ENTRY(CannotInferCollectionType, 3009, "E3009", "cannot infer collection type", "Empty collection literals require contextual or explicit element typing.")
    KEETY_ERROR_ENTRY(InvalidTypeCast, 3010, "E3010", "invalid type cast", "The value cannot be converted or asserted to the requested target type.")
    KEETY_ERROR_ENTRY(InvalidMemberAccess, 3011, "E3011", "invalid member access", "The type does not contain a member or method with this name.")
    KEETY_ERROR_ENTRY(InvalidIndexOperation, 3012, "E3012", "invalid indexing operation", "The type does not support indexing or the index type is invalid.")

    // E4000 - E4999: Memory Safety, Aliasing & Mutable Borrow Checker
    KEETY_ERROR_ENTRY(DuplicateMutableBorrow, 4001, "E4001", "cannot pass multiple mutable references to the same storage path", "KeetScript enforces exclusive mutable borrowing to prevent reference aliasing bugs.")
    KEETY_ERROR_ENTRY(ContainerAndElementBorrow, 4002, "E4002", "cannot simultaneously borrow container and its element", "Simultaneous mutable borrowing of a container and one of its elements violates disjointness.")
    KEETY_ERROR_ENTRY(MapElementMutableBorrow, 4003, "E4003", "cannot take mutable reference to map element", "Map elements cannot be borrowed mutably because map rehashing may invalidate storage references.")
    KEETY_ERROR_ENTRY(AsyncMutableParameter, 4004, "E4004", "mutable reference parameter in async function", "Async functions cannot accept 'mut' parameters due to asynchronous lifetime extensions.")
    KEETY_ERROR_ENTRY(NonLValueMutableBorrow, 4005, "E4005", "cannot pass non-l-value to mutable reference parameter", "Arguments to 'mut' parameters must be addressable l-values.")
    KEETY_ERROR_ENTRY(DefaultArgMutableBorrow, 4006, "E4006", "mutable reference parameter cannot have default value", "Default arguments cannot be supplied to mutable reference parameters.")

    // E5000 - E5999: Control Flow, Termination & Reachability Errors
    KEETY_ERROR_ENTRY(MissingReturnStatement, 5001, "E5001", "missing return in non-void function", "All code paths in a non-void function must terminate in a return, yield, or zonk statement.")
    KEETY_ERROR_ENTRY(UnreachableCode, 5002, "E5002", "unreachable code detected", "Statements following a terminating statement cannot be reached.")
    KEETY_ERROR_ENTRY(BreakOutsideLoopOrSwitch, 5003, "E5003", "'break' outside loop or switch", "The 'break' statement must appear inside a loop or switch body.")
    KEETY_ERROR_ENTRY(ContinueOutsideLoop, 5004, "E5004", "'continue' outside loop", "The 'continue' statement must appear inside a loop body.")
    KEETY_ERROR_ENTRY(InvalidLoopLabel, 5005, "E5005", "invalid or unknown label target", "No matching loop or switch with this label was found in scope.")
    KEETY_ERROR_ENTRY(YieldOutsideZonkBlock, 5006, "E5006", "'yield' outside zonk recovery block", "The 'yield' statement is only permitted inside a scoped zonk recovery block.")
    KEETY_ERROR_ENTRY(NonExhaustiveSwitch, 5007, "E5007", "non-exhaustive switch statement", "The switch statement does not cover all possible values or union variants and lacks a default branch.")

    // E6000 - E6999: Foreign Function Interface (FFI) & Linkage Errors
    KEETY_ERROR_ENTRY(ExternSymbolNotFound, 6001, "E6001", "extern symbol not found", "Could not locate the declared extern symbol in linked libraries.")
    KEETY_ERROR_ENTRY(IncompatibleFFIType, 6002, "E6002", "incompatible FFI type", "The specified type cannot be safely passed across the native C ABI.")
    KEETY_ERROR_ENTRY(InvalidLinkageSpecification, 6003, "E6003", "invalid linkage specification", "The extern linkage convention is unsupported or malformed.")
    }

    return ErrorCodeInfo{"E9999", "unknown error", "An unrecognized compiler error occurred."};
}

#undef KEETY_ERROR_ENTRY

std::string_view getErrorCodeString(ErrorCode code) noexcept {
    return getErrorCodeInfo(code).code;
}

} // namespace keety::diag
