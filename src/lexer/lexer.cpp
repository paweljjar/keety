#include <keety/lexer/lexer.hpp>

#include <cctype>
#include <string_view>
#include <unordered_map>

namespace keety::lexer {

namespace {

inline bool isAsciiLetter(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline bool isAsciiDigit(char c) noexcept {
    return c >= '0' && c <= '9';
}

inline bool isHexDigit(char c) noexcept {
    return isAsciiDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

inline bool isOctalDigit(char c) noexcept {
    return c >= '0' && c <= '7';
}

inline bool isBinaryDigit(char c) noexcept {
    return c == '0' || c == '1';
}

inline bool isIdentifierStart(char c) noexcept {
    return isAsciiLetter(c) || c == '_';
}

inline bool isIdentifierPart(char c) noexcept {
    return isAsciiLetter(c) || isAsciiDigit(c) || c == '_';
}

const std::unordered_map<std::string_view, TokenKind> KEYWORDS = {
    // Declarations & Bindings
    {"define",    TokenKind::KwDefine},
    {"const",     TokenKind::KwConst},
    {"let",       TokenKind::KwLet},
    {"mut",       TokenKind::KwMut},
    {"fn",        TokenKind::KwFn},
    {"class",     TokenKind::KwClass},
    {"inherits",  TokenKind::KwInherits},
    {"inherited", TokenKind::KwInherited},
    {"this",      TokenKind::KwThis},
    {"import",    TokenKind::KwImport},
    {"export",    TokenKind::KwExport},
    {"extern",    TokenKind::KwExtern},
    {"type",      TokenKind::KwType},
    {"pub",       TokenKind::KwPub},
    {"priv",      TokenKind::KwPriv},
    {"enum",      TokenKind::KwEnum},

    // Control Flow & Type Inspection
    {"if",        TokenKind::KwIf},
    {"else",      TokenKind::KwElse},
    {"elif",      TokenKind::KwElif},
    {"for",       TokenKind::KwFor},
    {"in",        TokenKind::KwIn},
    {"return",    TokenKind::KwReturn},
    {"yield",     TokenKind::KwYield},
    {"while",     TokenKind::KwWhile},
    {"break",     TokenKind::KwBreak},
    {"continue",  TokenKind::KwContinue},
    {"switch",    TokenKind::KwSwitch},
    {"case",      TokenKind::KwCase},
    {"default",   TokenKind::KwDefault},
    {"typeof",    TokenKind::KwTypeof},
    {"as",        TokenKind::KwAs},
    {"from",      TokenKind::KwFrom},

    // Error Handling & Recovery
    {"errtype",   TokenKind::KwErrtype},
    {"zonk",      TokenKind::KwZonk},

    // Built-in Primitive Types
    {"bool",      TokenKind::KwBool},
    {"int",       TokenKind::KwInt},
    {"float",     TokenKind::KwFloat},
    {"char",      TokenKind::KwChar},
    {"string",    TokenKind::KwString},
    {"void",      TokenKind::KwVoid},
    {"error",     TokenKind::KwError},
    {"array",     TokenKind::KwArray},
    {"map",       TokenKind::KwMap},

    // Literals & Constants
    {"true",      TokenKind::KwTrue},
    {"false",     TokenKind::KwFalse},
    {"null",      TokenKind::KwNull},

    // Concurrency
    {"await",     TokenKind::KwAwait},
    {"async",     TokenKind::KwAsync}
};

} // namespace

Lexer::Lexer(diag::FileId fileId, std::string_view source, diag::DiagnosticEmitter* emitter)
    : fileId_(fileId), source_(source), emitter_(emitter) {}

char Lexer::peek() const noexcept {
    if (isAtEnd()) return '\0';
    return source_[cursor_];
}

char Lexer::peekNext() const noexcept {
    if (cursor_ + 1 >= source_.size()) return '\0';
    return source_[cursor_ + 1];
}

char Lexer::advance() noexcept {
    if (isAtEnd()) return '\0';
    char c = source_[cursor_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        // Count non-continuation UTF-8 bytes towards column count
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++column_;
        }
    }
    return c;
}

bool Lexer::match(char expected) noexcept {
    if (isAtEnd() || source_[cursor_] != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        switch (c) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            advance();
            break;
        case '/':
            if (peekNext() == '/') {
                // Line comment: skip until end of line or EOF
                advance(); // '/'
                advance(); // '/'
                while (!isAtEnd() && peek() != '\n') {
                    advance();
                }
            } else if (peekNext() == '*') {
                // Block comment: skip until '*/'
                size_t commentStart = cursor_;
                advance(); // '/'
                advance(); // '*'
                bool closed = false;
                while (!isAtEnd()) {
                    if (peek() == '*' && peekNext() == '/') {
                        advance(); // '*'
                        advance(); // '/'
                        closed = true;
                        break;
                    }
                    advance();
                }
                if (!closed && emitter_) {
                    diag::SourceSpan span = makeSpan(commentStart, cursor_);
                    emitter_->report(diag::Diagnostic::makeError(
                        diag::ErrorCode::UnterminatedComment,
                        "unterminated block comment",
                        span.start
                    ).withLabel(span, "unclosed block comment begins here"));
                }
            } else {
                return;
            }
            break;
        default:
            return;
        }
    }
}

diag::SourceSpan Lexer::makeSpan(size_t startOffset, size_t endOffset) const {
    // In a fully loaded source file, line/column can be precisely computed,
    // or approximated from current lexer tracking:
    diag::SourceLocation start{fileId_, static_cast<uint32_t>(startOffset), line_, column_};
    diag::SourceLocation end{fileId_, static_cast<uint32_t>(endOffset), line_, column_};
    return diag::SourceSpan{start, end};
}

Token Lexer::makeToken(TokenKind kind, size_t startOffset, size_t endOffset) const {
    if (endOffset > source_.size()) {
        endOffset = source_.size();
    }
    std::string_view lexeme = source_.substr(startOffset, endOffset - startOffset);
    return Token{kind, makeSpan(startOffset, endOffset), lexeme};
}

Token Lexer::nextToken() {
    skipWhitespaceAndComments();

    if (isAtEnd()) {
        return makeToken(TokenKind::Eof, cursor_, cursor_);
    }

    return scanToken();
}

Token Lexer::scanToken() {
    size_t start = cursor_;
    char c = advance();

    // Blank identifier (`_`) vs regular Identifier starting with `_`
    // Section 'Letters and digits' & 'Blank identifier':
    // Identifier = ascii_letter { letter | ascii_digit } | "_" ( letter | ascii_digit ) { letter | ascii_digit } .
    // BlankIdent = "_" .
    if (c == '_') {
        if (!isAtEnd() && (isAsciiLetter(peek()) || isAsciiDigit(peek()) || peek() == '_')) {
            // Continues as regular identifier (e.g. `_count`, `_foo`, `__init`)
            while (!isAtEnd() && isIdentifierPart(peek())) {
                advance();
            }
            return makeToken(TokenKind::Identifier, start, cursor_);
        }
        // Standalone blank identifier `_`
        return makeToken(TokenKind::BlankIdent, start, cursor_);
    }

    // Identifiers & Keywords
    if (isIdentifierStart(c)) {
        while (!isAtEnd() && isIdentifierPart(peek())) {
            advance();
        }
        std::string_view text = source_.substr(start, cursor_ - start);
        auto it = KEYWORDS.find(text);
        if (it != KEYWORDS.end()) {
            return makeToken(it->second, start, cursor_);
        }
        return makeToken(TokenKind::Identifier, start, cursor_);
    }

    // Numbers (Integer and Float)
    if (isAsciiDigit(c)) {
        cursor_ = start; // rewind
        return scanNumber();
    }

    // Strings
    if (c == '"' || c == '\'') {
        return scanString(c);
    }

    // Template strings
    if (c == '`') {
        cursor_ = start;
        return scanTemplate();
    }

    // Single-character and Multi-character Operators & Punctuators (Maximal Munch)
    switch (c) {
    case '+':
        if (match('+')) return makeToken(TokenKind::PlusPlus, start, cursor_);
        if (match('=')) return makeToken(TokenKind::PlusEqual, start, cursor_);
        return makeToken(TokenKind::Plus, start, cursor_);
    case '-':
        if (match('-')) return makeToken(TokenKind::MinusMinus, start, cursor_);
        if (match('=')) return makeToken(TokenKind::MinusEqual, start, cursor_);
        return makeToken(TokenKind::Minus, start, cursor_);
    case '*':
        if (match('=')) return makeToken(TokenKind::StarEqual, start, cursor_);
        return makeToken(TokenKind::Star, start, cursor_);
    case '/':
        if (match('=')) return makeToken(TokenKind::SlashEqual, start, cursor_);
        return makeToken(TokenKind::Slash, start, cursor_);
    case '%':
        if (match('=')) return makeToken(TokenKind::PercentEqual, start, cursor_);
        return makeToken(TokenKind::Percent, start, cursor_);
    case '=':
        if (match('=')) return makeToken(TokenKind::EqualEqual, start, cursor_);
        if (match('>')) return makeToken(TokenKind::Arrow, start, cursor_);
        return makeToken(TokenKind::Equal, start, cursor_);
    case '!':
        if (match('=')) return makeToken(TokenKind::ExclaimEqual, start, cursor_);
        return makeToken(TokenKind::Exclaim, start, cursor_);
    case '<':
        if (match('=')) return makeToken(TokenKind::LessEqual, start, cursor_);
        return makeToken(TokenKind::Less, start, cursor_);
    case '>':
        if (mode_ == LexerMode::TypeContext) {
            // In TypeContext, '>' closes type bracket immediately without combining into '>=' or '>>'
            return makeToken(TokenKind::Greater, start, cursor_);
        }
        if (match('=')) return makeToken(TokenKind::GreaterEqual, start, cursor_);
        return makeToken(TokenKind::Greater, start, cursor_);
    case '&':
        if (match('&')) return makeToken(TokenKind::AmpAmp, start, cursor_);
        break;
    case '|':
        if (match('|')) return makeToken(TokenKind::PipePipe, start, cursor_);
        return makeToken(TokenKind::Pipe, start, cursor_);
    case '?':
        return makeToken(TokenKind::Question, start, cursor_);
    case '.':
        return makeToken(TokenKind::Dot, start, cursor_);
    case ':':
        return makeToken(TokenKind::Colon, start, cursor_);
    case ',':
        return makeToken(TokenKind::Comma, start, cursor_);
    case ';':
        return makeToken(TokenKind::Semicolon, start, cursor_);
    case '(':
        return makeToken(TokenKind::LParen, start, cursor_);
    case ')':
        return makeToken(TokenKind::RParen, start, cursor_);
    case '[':
        return makeToken(TokenKind::LBracket, start, cursor_);
    case ']':
        return makeToken(TokenKind::RBracket, start, cursor_);
    case '{':
        if (!templateBraceStack_.empty()) {
            templateBraceStack_.back()++;
        }
        return makeToken(TokenKind::LBrace, start, cursor_);
    case '}':
        if (!templateBraceStack_.empty()) {
            if (--templateBraceStack_.back() == 0) {
                // Resume template string scanning
                templateBraceStack_.pop_back();
                return scanTemplate();
            }
        }
        return makeToken(TokenKind::RBrace, start, cursor_);
    default:
        break;
    }

    if (emitter_) {
        diag::SourceSpan span = makeSpan(start, cursor_);
        emitter_->report(diag::Diagnostic::makeError(
            diag::ErrorCode::UnexpectedToken,
            "unexpected character in input",
            span.start
        ).withLabel(span, "unrecognized character"));
    }
    return makeToken(TokenKind::Invalid, start, cursor_);
}

Token Lexer::scanNumber() {
    size_t start = cursor_;
    char first = advance();

    // Non-decimal prefixes: 0x, 0b, 0o
    if (first == '0' && !isAtEnd()) {
        char prefix = peek();
        if (prefix == 'x' || prefix == 'X') {
            advance();
            size_t digitsStart = cursor_;
            while (!isAtEnd() && isHexDigit(peek())) {
                advance();
            }
            if (cursor_ == digitsStart) {
                if (emitter_) {
                    emitter_->report(diag::Diagnostic::makeError(
                        diag::ErrorCode::InvalidNumericLiteral,
                        "hexadecimal literal requires at least one hex digit",
                        makeSpan(start, cursor_).start
                    ));
                }
                return makeToken(TokenKind::Invalid, start, cursor_);
            }
            return makeToken(TokenKind::IntLiteral, start, cursor_);
        } else if (prefix == 'b' || prefix == 'B') {
            advance();
            size_t digitsStart = cursor_;
            while (!isAtEnd() && isBinaryDigit(peek())) {
                advance();
            }
            if (cursor_ == digitsStart) {
                if (emitter_) {
                    emitter_->report(diag::Diagnostic::makeError(
                        diag::ErrorCode::InvalidNumericLiteral,
                        "binary literal requires at least one binary digit",
                        makeSpan(start, cursor_).start
                    ));
                }
                return makeToken(TokenKind::Invalid, start, cursor_);
            }
            return makeToken(TokenKind::IntLiteral, start, cursor_);
        } else if (prefix == 'o' || prefix == 'O') {
            advance();
            size_t digitsStart = cursor_;
            while (!isAtEnd() && isOctalDigit(peek())) {
                advance();
            }
            if (cursor_ == digitsStart) {
                if (emitter_) {
                    emitter_->report(diag::Diagnostic::makeError(
                        diag::ErrorCode::InvalidNumericLiteral,
                        "octal literal requires at least one octal digit",
                        makeSpan(start, cursor_).start
                    ));
                }
                return makeToken(TokenKind::Invalid, start, cursor_);
            }
            return makeToken(TokenKind::IntLiteral, start, cursor_);
        }
    }

    // Decimal digits
    while (!isAtEnd() && isAsciiDigit(peek())) {
        advance();
    }

    // Check for fractional part
    bool isFloat = false;
    if (!isAtEnd() && peek() == '.' && isAsciiDigit(peekNext())) {
        isFloat = true;
        advance(); // '.'
        while (!isAtEnd() && isAsciiDigit(peek())) {
            advance();
        }
    }

    // Check for exponent part [eE][+-]?[0-9]+
    if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
        isFloat = true;
        advance(); // 'e' or 'E'
        if (!isAtEnd() && (peek() == '+' || peek() == '-')) {
            advance();
        }
        while (!isAtEnd() && isAsciiDigit(peek())) {
            advance();
        }
    }

    return makeToken(isFloat ? TokenKind::FloatLiteral : TokenKind::IntLiteral, start, cursor_);
}

Token Lexer::scanString(char quote) {
    size_t start = cursor_ - 1; // quote already advanced
    bool escaped = false;

    while (!isAtEnd()) {
        char c = advance();
        if (escaped) {
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == quote) {
            return makeToken(TokenKind::StringLiteral, start, cursor_);
        } else if (c == '\n') {
            break;
        }
    }

    if (emitter_) {
        diag::SourceSpan span = makeSpan(start, cursor_);
        emitter_->report(diag::Diagnostic::makeError(
            diag::ErrorCode::UnterminatedStringLiteral,
            "unterminated string literal",
            span.start
        ).withLabel(span, "missing closing quote"));
    }
    return makeToken(TokenKind::Invalid, start, cursor_);
}

Token Lexer::scanTemplate() {
    size_t start = cursor_;
    bool isResume = (!source_.empty() && source_[start] == '}');
    if (isResume) {
        advance(); // consume '}'
    } else {
        advance(); // consume '`'
    }

    bool escaped = false;
    while (!isAtEnd()) {
        char c = advance();
        if (escaped) {
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '$' && !isAtEnd() && peek() == '{') {
            advance(); // consume '{'
            templateBraceStack_.push_back(1);
            TokenKind kind = isResume ? TokenKind::TemplateMiddle : TokenKind::TemplateHead;
            return makeToken(kind, start, cursor_);
        } else if (c == '`') {
            TokenKind kind = isResume ? TokenKind::TemplateTail : TokenKind::TemplateNoSub;
            return makeToken(kind, start, cursor_);
        }
    }

    if (emitter_) {
        diag::SourceSpan span = makeSpan(start, cursor_);
        emitter_->report(diag::Diagnostic::makeError(
            diag::ErrorCode::UnterminatedStringLiteral,
            "unterminated template literal",
            span.start
        ).withLabel(span, "missing closing backtick delimiter"));
    }
    return makeToken(TokenKind::Invalid, start, cursor_);
}

std::vector<Token> Lexer::tokenizeAll() {
    std::vector<Token> tokens;
    while (true) {
        Token tok = nextToken();
        tokens.push_back(tok);
        if (tok.is(TokenKind::Eof)) {
            break;
        }
    }
    return tokens;
}

} // namespace keety::lexer
