#pragma once

#include <keety/lsp/document.hpp>
#include <keety/lsp/protocol.hpp>

#include <vector>

namespace keety::lsp {

/// Analyzes in-memory KeetScript document contents and produces LSP Diagnostics
/// using the lexer, parser, resolver, sema, and typechecker from keety_core.
class Analyzer {
public:
    Analyzer() = default;

    /// Runs single-file in-memory analysis and returns LSP diagnostics.
    std::vector<Diagnostic> analyze(const Document& doc);

    /// Locates the declaration location for the symbol under position (Go-to-Definition).
    std::optional<Location> findDefinition(const Document& doc, Position pos, const DocumentManager* docManager = nullptr);

    /// Finds all references to the symbol under position (Find All References),
    /// respecting lexical scope and shadow boundaries, across all open documents.
    std::vector<Location> findReferences(const Document& doc, Position pos, bool includeDeclaration = true, const DocumentManager* docManager = nullptr);

    /// Prepares symbol renaming by validating that the cursor is on a valid user-defined identifier.
    /// Returns the symbol range and placeholder string, or nullopt if renaming is not permitted.
    std::optional<PrepareRenameResult> prepareRename(const Document& doc, Position pos, const DocumentManager* docManager = nullptr);

    /// Renames a symbol across declarations and all references respecting lexical scopes.
    /// Produces a WorkspaceEdit with file edits, or nullopt if invalid.
    std::optional<WorkspaceEdit> renameSymbol(const Document& doc, Position pos, const std::string& newName, const DocumentManager* docManager = nullptr);

    /// Computes rich hover information and Markdown documentation for the symbol or keyword under position.
    std::optional<Hover> getHover(const Document& doc, Position pos, const DocumentManager* docManager = nullptr);

    /// Computes context-aware code completion items (keywords, stdlib, locals, classes, dot completions).
    CompletionList getCompletions(const Document& doc, Position pos, const DocumentManager* docManager = nullptr);

    /// Extracts the hierarchical document outline and symbols (classes, methods, fields, functions, enums).
    std::vector<DocumentSymbol> getDocumentSymbols(const Document& doc);

    /// Formats the document according to canonical formatting rules.
    /// Returns a list of TextEdits (empty if already formatted or on syntax errors).
    std::vector<TextEdit> format(const Document& doc, const FormattingOptions& options = {});

    /// Computes Code Actions (Quick Fixes) for the given range in the document.
    std::vector<CodeAction> getCodeActions(const Document& doc, Range range, const std::vector<Diagnostic>& contextDiagnostics = {});
};

} // namespace keety::lsp
