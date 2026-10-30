#pragma once

#include <keety/diag/diagnostic.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/error_code.hpp>
#include <keety/sema/symbol.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace keety::sema {

/// Identifies the kind of mutation attempted on a symbol.
enum class MutationKind {
    Assignment,          ///< Direct assignment (`x = expr;`)
    CompoundAssignment,  ///< Compound assignment (`x += expr;`, etc.)
    IncrementDecrement,  ///< Increment or decrement (`x++`, `--x`)
    MutableBorrow,       ///< Passing as mutable borrow (`f(mut x)`)
    MutatingMethodCall   ///< Calling a mutating method (`arr.push(...)`)
};

/// Returns a human-readable description of a mutation kind.
[[nodiscard]] constexpr std::string_view mutationKindToString(MutationKind kind) noexcept {
    switch (kind) {
    case MutationKind::Assignment:
        return "assignment";
    case MutationKind::CompoundAssignment:
        return "compound assignment";
    case MutationKind::IncrementDecrement:
        return "increment/decrement operation";
    case MutationKind::MutableBorrow:
        return "mutable reference borrow ('mut')";
    case MutationKind::MutatingMethodCall:
        return "mutating method call";
    }
    return "mutation";
}

/// Result of validating a mutation target.
struct MutationValidationResult {
    bool valid{true};
    std::optional<diag::ErrorCode> errorCode{std::nullopt};
    std::string errorMessage{};

    [[nodiscard]] static MutationValidationResult ok() {
        return {true, std::nullopt, ""};
    }

    [[nodiscard]] static MutationValidationResult error(diag::ErrorCode code, std::string msg) {
        return {false, code, std::move(msg)};
    }
};

/// Enforces asymmetric const-view semantics for imported `export let` variables (E2002)
/// and immutable constant re-assignment checks (E2005).
class ConstViewChecker {
public:
    /// Constructs a standardized E2002 diagnostic for attempting to modify an imported const-view.
    [[nodiscard]] static diag::Diagnostic createConstViewWriteDiagnostic(
        const Symbol& symbol,
        diag::SourceSpan writeSpan,
        MutationKind kind);

    /// Constructs a standardized E2005 diagnostic for attempting to assign to an immutable constant.
    [[nodiscard]] static diag::Diagnostic createConstantWriteDiagnostic(
        const Symbol& symbol,
        diag::SourceSpan writeSpan,
        MutationKind kind);

    /// Validates whether `symbol` can be mutated by the specified `kind` of operation.
    /// Emits E2002 or E2005 to `emitter` if provided upon any violation.
    static MutationValidationResult validateMutation(
        const Symbol& symbol,
        diag::SourceSpan writeSpan,
        MutationKind kind = MutationKind::Assignment,
        diag::DiagnosticEmitter* emitter = nullptr);

    /// Imports an exported symbol into an importing module's scope.
    ///
    /// If the symbol is an exported mutable variable (`export let`), it is transformed
    /// into an immutable `const`-view symbol with `isConstView = true` and `isMutable = false`.
    [[nodiscard]] static std::shared_ptr<Symbol> importExportedSymbol(
        const std::shared_ptr<Symbol>& exportedSymbol,
        diag::SourceSpan importSpan);
};

} // namespace keety::sema
