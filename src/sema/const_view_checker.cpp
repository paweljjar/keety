#include <keety/sema/const_view_checker.hpp>

namespace keety::sema {

diag::Diagnostic ConstViewChecker::createConstViewWriteDiagnostic(
    const Symbol& symbol,
    diag::SourceSpan writeSpan,
    MutationKind kind) {
    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::ExportConstWrite,
        "cannot modify imported 'export let' variable '" + symbol.getName() + "'",
        writeSpan.isValid() ? writeSpan.start : diag::SourceLocation{});

    if (writeSpan.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            writeSpan,
            "cannot modify 'export let' variable across module boundaries via " +
                std::string(mutationKindToString(kind)),
            true
        });
    }

    auto orig = symbol.getOriginalExportedSymbol();
    if (orig && orig->getSpan().isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            orig->getSpan(),
            "variable '" + symbol.getName() + "' is declared as 'export let' here",
            false
        });
    } else if (symbol.getSpan().isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            symbol.getSpan(),
            "variable '" + symbol.getName() + "' is imported here as a read-only const-view",
            false
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Note,
        "KeetScript enforces asymmetric const-view semantics on exported mutable variables to prevent cross-module state corruption and data races."
    });

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        "If external modification is required, export an explicit mutator function from the defining module."
    });

    return d;
}

diag::Diagnostic ConstViewChecker::createConstantWriteDiagnostic(
    const Symbol& symbol,
    diag::SourceSpan writeSpan,
    MutationKind kind) {
    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::CannotAssignToConstant,
        "cannot assign to immutable constant '" + symbol.getName() + "'",
        writeSpan.isValid() ? writeSpan.start : diag::SourceLocation{});

    if (writeSpan.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            writeSpan,
            "cannot modify immutable constant via " + std::string(mutationKindToString(kind)),
            true
        });
    }

    if (symbol.getSpan().isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            symbol.getSpan(),
            "constant '" + symbol.getName() + "' is declared here",
            false
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        "Constants declared with 'const' or 'define' cannot be reassigned. Use 'let' if mutability is required."
    });

    return d;
}

MutationValidationResult ConstViewChecker::validateMutation(
    const Symbol& symbol,
    diag::SourceSpan writeSpan,
    MutationKind kind,
    diag::DiagnosticEmitter* emitter) {
    if (symbol.isConstView()) {
        if (emitter != nullptr) {
            emitter->report(createConstViewWriteDiagnostic(symbol, writeSpan, kind));
        }
        return MutationValidationResult::error(
            diag::ErrorCode::ExportConstWrite,
            "cannot modify imported 'export let' variable '" + symbol.getName() + "'");
    }

    if (symbol.isConstant() || !symbol.isMutable()) {
        if (emitter != nullptr) {
            emitter->report(createConstantWriteDiagnostic(symbol, writeSpan, kind));
        }
        return MutationValidationResult::error(
            diag::ErrorCode::CannotAssignToConstant,
            "cannot assign to immutable constant '" + symbol.getName() + "'");
    }

    return MutationValidationResult::ok();
}

std::shared_ptr<Symbol> ConstViewChecker::importExportedSymbol(
    const std::shared_ptr<Symbol>& exportedSymbol,
    diag::SourceSpan importSpan) {
    if (!exportedSymbol) {
        return nullptr;
    }

    if (exportedSymbol->isVariable()) {
        // Enforce asymmetric const-view semantics:
        // exported let variable becomes read-only const-view in importing module
        return Symbol::createImportedConstView(exportedSymbol, importSpan);
    }

    auto imported = std::make_shared<Symbol>(
        exportedSymbol->getName(), SymbolKind::ImportedSymbol, importSpan);
    imported->setConst(exportedSymbol->isConst());
    imported->setMutable(false);
    imported->setExported(false);
    imported->setAstNode(exportedSymbol->getAstNode());
    imported->setTypeAnnotation(exportedSymbol->getTypeAnnotation());
    imported->setOriginalExportedSymbol(exportedSymbol);
    return imported;
}

} // namespace keety::sema
