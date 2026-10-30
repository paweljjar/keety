#pragma once

#include <keety/diag/source_location.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace keety::lexer {

/// Enumeration of all lexical token kinds recognized in KeetScript.
/// Conforms directly to The KeetScript Language Specification (Sections: Keywords, Operators & Punctuation, Literals).
enum class TokenKind : uint16_t {
    // -------------------------------------------------------------------------
    // Special Tokens
    // -------------------------------------------------------------------------
    Eof,            // End of input stream
    Invalid,        // Unrecognized or malformed token

    // -------------------------------------------------------------------------
    // Identifiers & Literals
    // -------------------------------------------------------------------------
    Identifier,     // Regular identifier (e.g. `count`, `_foo`, `User`)
    BlankIdent,     // Standalone blank identifier `_`
    IntLiteral,     // Decimal, binary (0b), octal (0o), or hex (0x) integer
    FloatLiteral,   // Decimal floating point literal
    StringLiteral,  // Single/double quoted string or completed string literal
    CharLiteral,    // Character literal (e.g. `'a'`)

    // Template String Tokens (Backtick interpolations)
    TemplateHead,   // `text${
    TemplateMiddle, // }text${
    TemplateTail,   // }text`
    TemplateNoSub,  // `text without interpolation`

    // -------------------------------------------------------------------------
    // Keywords: Declarations & Bindings
    // -------------------------------------------------------------------------
    KwDefine,       // `define`
    KwConst,        // `const`
    KwLet,          // `let`
    KwMut,          // `mut`
    KwFn,           // `fn`
    KwClass,        // `class`
    KwInherits,     // `inherits`
    KwInherited,    // `inherited`
    KwThis,         // `this`
    KwImport,       // `import`
    KwExport,       // `export`
    KwExtern,       // `extern`
    KwType,         // `type`
    KwPub,          // `pub`
    KwPriv,         // `priv`
    KwEnum,         // `enum`

    // -------------------------------------------------------------------------
    // Keywords: Control Flow & Type Inspection
    // -------------------------------------------------------------------------
    KwIf,           // `if`
    KwElse,         // `else`
    KwElif,         // `elif`
    KwFor,          // `for`
    KwIn,           // `in`
    KwReturn,       // `return`
    KwYield,        // `yield`
    KwWhile,        // `while`
    KwBreak,        // `break`
    KwContinue,     // `continue`
    KwSwitch,       // `switch`
    KwCase,         // `case`
    KwDefault,      // `default`
    KwTypeof,       // `typeof`
    KwAs,           // `as`
    KwFrom,         // `from`

    // -------------------------------------------------------------------------
    // Keywords: Error Handling & Recovery
    // -------------------------------------------------------------------------
    KwErrtype,      // `errtype`
    KwZonk,         // `zonk`

    // -------------------------------------------------------------------------
    // Keywords: Built-in Primitive Types
    // -------------------------------------------------------------------------
    KwBool,         // `bool`
    KwInt,          // `int`
    KwFloat,        // `float`
    KwChar,         // `char`
    KwString,       // `string`
    KwVoid,         // `void`
    KwError,        // `error`
    KwArray,        // `array`
    KwMap,          // `map`

    // -------------------------------------------------------------------------
    // Keywords: Literals & Constants
    // -------------------------------------------------------------------------
    KwTrue,         // `true`
    KwFalse,        // `false`
    KwNull,         // `null`

    // -------------------------------------------------------------------------
    // Keywords: Concurrency
    // -------------------------------------------------------------------------
    KwAwait,        // `await`
    KwAsync,        // `async`

    // -------------------------------------------------------------------------
    // Operators and Punctuators
    // -------------------------------------------------------------------------
    Plus,           // `+`
    Minus,          // `-`
    Star,           // `*`
    Slash,          // `/`
    Percent,        // `%`
    PlusPlus,       // `++`
    MinusMinus,     // `--`

    Question,       // `?`
    EqualEqual,     // `==`
    ExclaimEqual,   // `!=`
    Less,           // `<`
    LessEqual,      // `<=`
    Greater,        // `>`
    GreaterEqual,   // `>=`

    AmpAmp,         // `&&`
    PipePipe,       // `||`
    Exclaim,        // `!`
    Pipe,           // `|`

    Equal,          // `=`
    PlusEqual,      // `+=`
    MinusEqual,     // `-=`
    StarEqual,      // `*=`
    SlashEqual,     // `/=`
    PercentEqual,   // `%=`

    Arrow,          // `=>`
    Dot,            // `.`
    Colon,          // `:`
    Comma,          // `,`
    Semicolon,      // `;`

    LParen,         // `(`
    RParen,         // `)`
    LBracket,       // `[`
    RBracket,       // `]`
    LBrace,         // `{`
    RBrace          // `}`
};

/// Returns human-readable representation or symbol string of a TokenKind
[[nodiscard]] std::string_view getTokenKindName(TokenKind kind) noexcept;

/// Checks if a TokenKind is a keyword
[[nodiscard]] bool isKeyword(TokenKind kind) noexcept;

/// Checks if a TokenKind is a literal or constant keyword (true, false, null)
[[nodiscard]] bool isLiteral(TokenKind kind) noexcept;

/// Checks if a TokenKind is an assignment operator (=, +=, -=, *=, /=, %=)
[[nodiscard]] bool isAssignmentOperator(TokenKind kind) noexcept;

/// Represents a single token produced by the lexer
struct Token {
    TokenKind kind{TokenKind::Invalid};
    diag::SourceSpan span{};
    std::string_view lexeme{};

    [[nodiscard]] constexpr bool is(TokenKind expected) const noexcept {
        return kind == expected;
    }

    [[nodiscard]] constexpr bool isNot(TokenKind expected) const noexcept {
        return kind != expected;
    }

    template <typename... Kinds>
    [[nodiscard]] constexpr bool isOneOf(Kinds... kinds) const noexcept {
        return ((kind == kinds) || ...);
    }
};

} // namespace keety::lexer
