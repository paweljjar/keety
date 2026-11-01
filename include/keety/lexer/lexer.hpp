#pragma once

#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/lexer/token.hpp>

#include <string_view>
#include <vector>

namespace keety::lexer {

/// Contextual flags influencing tokenization rules
enum class LexerMode {
    Normal,
    TypeContext // In generic type arguments or annotations, '>' does not combine into '>>' or '>='
};

/// High-performance UTF-8 Lexical Analyzer conforming to The KeetScript Language Specification.
class Lexer {
public:
    Lexer(diag::FileId fileId, std::string_view source, diag::DiagnosticEmitter* emitter = nullptr);

    /// Scans and returns the next token from the source stream.
    Token nextToken();

    /// Scans all remaining tokens until EOF.
    std::vector<Token> tokenizeAll();

    /// Sets the current lexer mode (e.g. TypeContext).
    void setMode(LexerMode mode) noexcept { mode_ = mode; }
    [[nodiscard]] LexerMode getMode() const noexcept { return mode_; }

    /// Explicitly enter or exit generic type argument / parameter context
    void enterGenericContext() noexcept { ++genericDepth_; mode_ = LexerMode::TypeContext; }
    void exitGenericContext() noexcept {
        if (genericDepth_ > 0) {
            if (--genericDepth_ == 0) {
                mode_ = LexerMode::Normal;
            }
        }
    }
    [[nodiscard]] bool isInGenericContext() const noexcept { return genericDepth_ > 0; }

    [[nodiscard]] diag::FileId getFileId() const noexcept { return fileId_; }
    [[nodiscard]] size_t getOffset() const noexcept { return cursor_; }

private:
    [[nodiscard]] bool isAtEnd() const noexcept { return cursor_ >= source_.size(); }
    [[nodiscard]] char peek() const noexcept;
    [[nodiscard]] char peekNext() const noexcept;
    char advance() noexcept;
    bool match(char expected) noexcept;

    void skipWhitespaceAndComments();
    Token scanToken();

    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanString(char quote);
    Token scanChar();
    Token scanTemplate();

    Token makeToken(TokenKind kind, size_t startOffset, size_t endOffset) const;
    diag::SourceSpan makeSpan(size_t startOffset, size_t endOffset) const;

    diag::FileId fileId_{diag::INVALID_FILE_ID};
    std::string_view source_;
    diag::DiagnosticEmitter* emitter_{nullptr};

    size_t cursor_{0};
    uint32_t line_{1};
    uint32_t column_{1};
    uint32_t tokenStartLine_{1};
    uint32_t tokenStartColumn_{1};
    LexerMode mode_{LexerMode::Normal};
    int genericDepth_{0};
    bool hadPrecedingWhitespace_{false};

    // Tracking state for template interpolation depth
    std::vector<int> templateBraceStack_;
};

} // namespace keety::lexer
