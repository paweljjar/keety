#pragma once

#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/kml/kml_token.hpp>
#include <string_view>
#include <vector>

namespace keety::kml {

enum class LexerMode {
    SfcRoot,        // Top-level document scanning for <script>, <template>, <style>
    TemplateContent,// Inside <template>, reading tags, text, expressions, control blocks
    TagAttributes   // Inside an opening tag <tag ... >, reading attributes & directives
};

class KmlLexer {
public:
    KmlLexer(std::string_view source, diag::FileId fileId, diag::DiagnosticEmitter* emitter = nullptr);

    /// Get next token according to the current lexer mode
    KmlToken nextToken();

    /// Tokenize the entire stream (helpful for testing or full scans)
    std::vector<KmlToken> tokenizeAll();

    void setMode(LexerMode mode) noexcept { mode_ = mode; }
    [[nodiscard]] LexerMode getMode() const noexcept { return mode_; }

    /// Helper to lex a balanced KeetScript expression inside { ... }
    /// Returns the contents of the expression between the outer braces.
    std::string lexBalancedExpression(diag::SourceLocation& outStart, diag::SourceLocation& outEnd);

    // SFC block contents (raw verbatim slice until </script> or </style>)
    std::string lexRawUntilClosingTag(std::string_view closingTag, diag::SourceSpan& outSpan);

private:
    char peek(size_t offset = 0) const noexcept;
    char advance() noexcept;
    bool match(char expected) noexcept;
    bool match(std::string_view expected) noexcept;
    bool isAtEnd() const noexcept;

    void skipWhitespace();
    void skipWhitespaceAndComments();

    diag::SourceLocation currentLocation() const noexcept;
    diag::SourceSpan makeSpan(diag::SourceLocation start) const noexcept;

    // Mode-specific token handlers
    KmlToken nextSfcRootToken();
    KmlToken nextTemplateContentToken();
    KmlToken nextTagAttributesToken();

    // Specific tokenizers
    KmlToken lexTagOrClose();
    KmlToken lexControlBlockOrExpression();
    KmlToken lexDirectiveOrIdentifier();
    KmlToken lexStringLiteral(char quoteChar);
    KmlToken lexTextRun();

    std::string_view source_;
    diag::FileId fileId_;
    diag::DiagnosticEmitter* emitter_;

    size_t cursor_{0};
    uint32_t line_{1};
    uint32_t column_{1};
    LexerMode mode_{LexerMode::SfcRoot};

    friend class KmlParser;
};

} // namespace keety::kml
