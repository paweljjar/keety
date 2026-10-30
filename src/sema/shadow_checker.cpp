#include <keety/sema/shadow_checker.hpp>

namespace keety::sema {

diag::Diagnostic ShadowChecker::createShadowingDiagnostic(
    const Symbol& newSymbol,
    const Symbol& outerSymbol) {
    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::IdentifierShadowed,
        "identifier '" + newSymbol.getName() + "' shadows outer declaration",
        newSymbol.getSpan().isValid() ? newSymbol.getSpan().start : diag::SourceLocation{});

    if (newSymbol.getSpan().isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            newSymbol.getSpan(),
            "re-declaration of '" + newSymbol.getName() + "' shadows outer binding here",
            true
        });
    }

    if (outerSymbol.getSpan().isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            outerSymbol.getSpan(),
            "previous declaration of '" + outerSymbol.getName() + "' is here",
            false
        });
    } else {
        d.footers.push_back(diag::DiagnosticFooter{
            diag::DiagnosticFooter::Kind::Note,
            "previous declaration is a predeclared Universe " + std::string(outerSymbol.getKindName())
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        "KeetScript strictly forbids shadowing identifiers declared in enclosing lexical scopes. Choose a distinct identifier name."
    });

    return d;
}

diag::Diagnostic ShadowChecker::createDuplicateDiagnostic(
    const Symbol& newSymbol,
    const Symbol& existingSymbol) {
    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::RedeclarationInSameScope,
        "redeclaration of identifier '" + newSymbol.getName() + "' in the same scope",
        newSymbol.getSpan().isValid() ? newSymbol.getSpan().start : diag::SourceLocation{});

    if (newSymbol.getSpan().isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            newSymbol.getSpan(),
            "redeclaration of '" + newSymbol.getName() + "' occurs here",
            true
        });
    }

    if (existingSymbol.getSpan().isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            existingSymbol.getSpan(),
            "previous declaration of '" + existingSymbol.getName() + "' is here",
            false
        });
    }

    return d;
}

std::shared_ptr<Symbol> ShadowChecker::checkShadowing(
    const Scope& scope,
    const Symbol& symbol) {
    if (symbol.getName() == "_") {
        return nullptr;
    }

    // Class fields and methods do not shadow outer declarations
    if (scope.isClass() && (symbol.isField() || symbol.isMethod())) {
        return nullptr;
    }

    return scope.lookupParentForShadowing(symbol.getName());
}

DefineResult ShadowChecker::defineWithDiagnostics(
    Scope& scope,
    std::shared_ptr<Symbol> symbol,
    diag::DiagnosticEmitter* emitter) {
    auto res = scope.define(symbol, /*enforceNoShadowing=*/true);
    if (!res.success && emitter != nullptr && symbol) {
        if (res.errorCode == diag::ErrorCode::IdentifierShadowed && res.existingSymbol) {
            emitter->report(createShadowingDiagnostic(*symbol, *res.existingSymbol));
        } else if (res.errorCode == diag::ErrorCode::RedeclarationInSameScope && res.existingSymbol) {
            emitter->report(createDuplicateDiagnostic(*symbol, *res.existingSymbol));
        }
    }
    return res;
}

} // namespace keety::sema
