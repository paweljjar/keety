#pragma once

#include <keety/diag/source_location.hpp>
#include <string>
#include <string_view>

namespace keety::kml {

/// Token types for KML lexical analysis
enum class KmlTokenKind {
    // End of file / Invalid
    Eof,
    Invalid,

    // SFC Delimiters
    TagScriptOpen,          // <script> or <script ...>
    TagScriptClose,         // </script>
    TagTemplateOpen,        // <template> or <template ...>
    TagTemplateClose,       // </template>
    TagStyleOpen,           // <style> or <style ...>
    TagStyleClose,          // </style>

    // Tags & Syntax
    TagOpenStart,           // '<' followed by tag identifier
    TagOpenEnd,             // '>'
    TagSelfClosingEnd,      // '/>'
    TagClose,               // '</' tag identifier '>'

    // Identifiers & Values
    Identifier,             // tag name or attribute name
    ComponentIdentifier,    // Capitalized identifier e.g. <UserProfile>
    StringLiteral,          // "value" or 'value'
    TextRun,                // static text content between tags

    // KeetScript Expression Blocks: {expr}
    ExprBlock,              // balanced { ... } expression content (excluding outer braces)

    // Directives
    DirectiveEvent,         // @click, @keydown, etc.
    DirectiveModifier,      // .debounce(150), .prevent, .enter
    DirectiveBind,          // $bind, $bind.value, $bind.checked

    // Control Flow Blocks
    BlockIfOpen,            // {#if expr}
    BlockElseIf,            // {:else if expr}
    BlockElse,              // {:else}
    BlockIfClose,           // {/if}

    BlockForOpen,           // {#for (item in items; item.id)} or {#for ...}
    BlockForClose,          // {/for}

    BlockAwaitOpen,         // {#await promise}
    BlockAwaitThen,         // {:then result}
    BlockAwaitCatch,        // {:catch error}
    BlockAwaitClose,        // {/await}

    // Punctuation
    Equals,                 // '='
    Dot,                    // '.'
    Colon,                  // ':'
    Semicolon,              // ';'
    LParen,                 // '('
    RParen                  // ')'
};

/// High precision token representation preserving exact SourceSpan
struct KmlToken {
    KmlTokenKind kind{KmlTokenKind::Invalid};
    std::string text;
    diag::SourceSpan span;

    [[nodiscard]] std::string_view getKindName() const noexcept;
    [[nodiscard]] bool isDirective() const noexcept {
        return kind == KmlTokenKind::DirectiveEvent ||
               kind == KmlTokenKind::DirectiveModifier ||
               kind == KmlTokenKind::DirectiveBind;
    }
};

std::string_view kmlTokenKindToString(KmlTokenKind kind) noexcept;

} // namespace keety::kml

