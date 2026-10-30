#pragma once

#include <keety/diag/diagnostic.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/sema/scope.hpp>
#include <keety/sema/symbol.hpp>

#include <memory>
#include <string>

namespace keety::sema {

/// Utility for validating strict no-shadowing rules and generating Rustc-style diagnostics.
class ShadowChecker {
public:
    /// Constructs a standardized E2001 diagnostic for a shadowed identifier.
    [[nodiscard]] static diag::Diagnostic createShadowingDiagnostic(
        const Symbol& newSymbol,
        const Symbol& outerSymbol);

    /// Constructs a standardized E2004 diagnostic for duplicate declarations in same scope.
    [[nodiscard]] static diag::Diagnostic createDuplicateDiagnostic(
        const Symbol& newSymbol,
        const Symbol& existingSymbol);

    /// Validates whether `symbol` can be defined in `scope` without shadowing any outer declaration.
    /// If shadowed, returns the conflicting outer symbol; otherwise returns nullptr.
    [[nodiscard]] static std::shared_ptr<Symbol> checkShadowing(
        const Scope& scope,
        const Symbol& symbol);

    /// Defines a symbol in `scope`, automatically emitting E2001 (shadowing) or E2004 (duplicate)
    /// diagnostics to `emitter` if provided upon any violation.
    static DefineResult defineWithDiagnostics(
        Scope& scope,
        std::shared_ptr<Symbol> symbol,
        diag::DiagnosticEmitter* emitter = nullptr);
};

} // namespace keety::sema
