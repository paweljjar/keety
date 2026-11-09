#include <keety/kml/kml_token.hpp>

namespace keety::kml {

std::string_view kmlTokenKindToString(KmlTokenKind kind) noexcept {
    switch (kind) {
        case KmlTokenKind::Eof: return "Eof";
        case KmlTokenKind::Invalid: return "Invalid";
        case KmlTokenKind::TagScriptOpen: return "TagScriptOpen";
        case KmlTokenKind::TagScriptClose: return "TagScriptClose";
        case KmlTokenKind::TagTemplateOpen: return "TagTemplateOpen";
        case KmlTokenKind::TagTemplateClose: return "TagTemplateClose";
        case KmlTokenKind::TagStyleOpen: return "TagStyleOpen";
        case KmlTokenKind::TagStyleClose: return "TagStyleClose";
        case KmlTokenKind::TagOpenStart: return "TagOpenStart";
        case KmlTokenKind::TagOpenEnd: return "TagOpenEnd";
        case KmlTokenKind::TagSelfClosingEnd: return "TagSelfClosingEnd";
        case KmlTokenKind::TagClose: return "TagClose";
        case KmlTokenKind::Identifier: return "Identifier";
        case KmlTokenKind::ComponentIdentifier: return "ComponentIdentifier";
        case KmlTokenKind::StringLiteral: return "StringLiteral";
        case KmlTokenKind::TextRun: return "TextRun";
        case KmlTokenKind::ExprBlock: return "ExprBlock";
        case KmlTokenKind::DirectiveEvent: return "DirectiveEvent";
        case KmlTokenKind::DirectiveModifier: return "DirectiveModifier";
        case KmlTokenKind::DirectiveBind: return "DirectiveBind";
        case KmlTokenKind::BlockIfOpen: return "BlockIfOpen";
        case KmlTokenKind::BlockElseIf: return "BlockElseIf";
        case KmlTokenKind::BlockElse: return "BlockElse";
        case KmlTokenKind::BlockIfClose: return "BlockIfClose";
        case KmlTokenKind::BlockForOpen: return "BlockForOpen";
        case KmlTokenKind::BlockForClose: return "BlockForClose";
        case KmlTokenKind::BlockAwaitOpen: return "BlockAwaitOpen";
        case KmlTokenKind::BlockAwaitThen: return "BlockAwaitThen";
        case KmlTokenKind::BlockAwaitCatch: return "BlockAwaitCatch";
        case KmlTokenKind::BlockAwaitClose: return "BlockAwaitClose";
        case KmlTokenKind::Equals: return "Equals";
        case KmlTokenKind::Dot: return "Dot";
        case KmlTokenKind::Colon: return "Colon";
        case KmlTokenKind::Semicolon: return "Semicolon";
        case KmlTokenKind::LParen: return "LParen";
        case KmlTokenKind::RParen: return "RParen";
    }
    return "Unknown";
}

std::string_view KmlToken::getKindName() const noexcept {
    return kmlTokenKindToString(kind);
}

} // namespace keety::kml

