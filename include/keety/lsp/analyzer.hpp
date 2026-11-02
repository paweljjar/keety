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
};

} // namespace keety::lsp
