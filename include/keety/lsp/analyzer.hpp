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

    /// Computes rich hover information and Markdown documentation for the symbol or keyword under position.
    std::optional<Hover> getHover(const Document& doc, Position pos, const DocumentManager* docManager = nullptr);

    /// Computes context-aware code completion items (keywords, stdlib, locals, classes, dot completions).
    CompletionList getCompletions(const Document& doc, Position pos, const DocumentManager* docManager = nullptr);

    /// Extracts the hierarchical document outline and symbols (classes, methods, fields, functions, enums).
    std::vector<DocumentSymbol> getDocumentSymbols(const Document& doc);

    /// Formats the document according to canonical formatting rules.
    /// Returns a list of TextEdits (empty if already formatted or on syntax errors).
    std::vector<TextEdit> format(const Document& doc, const FormattingOptions& options = {});
};

} // namespace keety::lsp
