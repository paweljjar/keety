#include <keety/lexer/token.hpp>

namespace keety::lexer {

std::string_view getTokenKindName(TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::Eof:            return "EOF";
    case TokenKind::Invalid:        return "<invalid>";

    case TokenKind::Identifier:     return "identifier";
    case TokenKind::BlankIdent:     return "_";
    case TokenKind::IntLiteral:     return "integer literal";
    case TokenKind::FloatLiteral:   return "float literal";
    case TokenKind::StringLiteral:  return "string literal";
    case TokenKind::CharLiteral:    return "character literal";

    case TokenKind::TemplateHead:   return "template head";
    case TokenKind::TemplateMiddle: return "template middle";
    case TokenKind::TemplateTail:   return "template tail";
    case TokenKind::TemplateNoSub:  return "template string";

    case TokenKind::KwDefine:       return "define";
    case TokenKind::KwConst:        return "const";
    case TokenKind::KwLet:          return "let";
    case TokenKind::KwMut:          return "mut";
    case TokenKind::KwFn:           return "fn";
    case TokenKind::KwClass:        return "class";
    case TokenKind::KwInherits:     return "inherits";
    case TokenKind::KwInherited:    return "inherited";
    case TokenKind::KwThis:         return "this";
    case TokenKind::KwImport:       return "import";
    case TokenKind::KwExport:       return "export";
    case TokenKind::KwExtern:       return "extern";
    case TokenKind::KwType:         return "type";
    case TokenKind::KwPub:          return "pub";
    case TokenKind::KwPriv:         return "priv";
    case TokenKind::KwEnum:         return "enum";

    case TokenKind::KwIf:           return "if";
    case TokenKind::KwElse:         return "else";
    case TokenKind::KwElif:         return "elif";
    case TokenKind::KwFor:          return "for";
    case TokenKind::KwIn:           return "in";
    case TokenKind::KwReturn:       return "return";
    case TokenKind::KwYield:        return "yield";
    case TokenKind::KwWhile:        return "while";
    case TokenKind::KwBreak:        return "break";
    case TokenKind::KwContinue:     return "continue";
    case TokenKind::KwSwitch:       return "switch";
    case TokenKind::KwCase:         return "case";
    case TokenKind::KwDefault:      return "default";
    case TokenKind::KwTypeof:       return "typeof";
    case TokenKind::KwAs:           return "as";
    case TokenKind::KwFrom:         return "from";

    case TokenKind::KwErrtype:      return "errtype";
    case TokenKind::KwZonk:         return "zonk";

    case TokenKind::KwBool:         return "bool";
    case TokenKind::KwInt:          return "int";
    case TokenKind::KwFloat:        return "float";
    case TokenKind::KwChar:         return "char";
    case TokenKind::KwString:       return "string";
    case TokenKind::KwVoid:         return "void";
    case TokenKind::KwError:        return "error";
    case TokenKind::KwArray:        return "array";
    case TokenKind::KwMap:          return "map";

    case TokenKind::KwTrue:         return "true";
    case TokenKind::KwFalse:        return "false";
    case TokenKind::KwNull:         return "null";

    case TokenKind::KwAwait:        return "await";
    case TokenKind::KwAsync:        return "async";

    case TokenKind::Plus:           return "+";
    case TokenKind::Minus:          return "-";
    case TokenKind::Star:           return "*";
    case TokenKind::Slash:          return "/";
    case TokenKind::Percent:        return "%";
    case TokenKind::PlusPlus:       return "++";
    case TokenKind::MinusMinus:     return "--";

    case TokenKind::Question:       return "?";
    case TokenKind::EqualEqual:     return "==";
    case TokenKind::ExclaimEqual:   return "!=";
    case TokenKind::Less:           return "<";
    case TokenKind::LessEqual:      return "<=";
    case TokenKind::Greater:        return ">";
    case TokenKind::GreaterEqual:   return ">=";

    case TokenKind::AmpAmp:         return "&&";
    case TokenKind::PipePipe:       return "||";
    case TokenKind::Exclaim:        return "!";
    case TokenKind::Pipe:           return "|";

    case TokenKind::Equal:          return "=";
    case TokenKind::PlusEqual:      return "+=";
    case TokenKind::MinusEqual:     return "-=";
    case TokenKind::StarEqual:      return "*=";
    case TokenKind::SlashEqual:     return "/=";
    case TokenKind::PercentEqual:   return "%=";

    case TokenKind::Arrow:          return "=>";
    case TokenKind::Dot:            return ".";
    case TokenKind::Colon:          return ":";
    case TokenKind::Comma:          return ",";
    case TokenKind::Semicolon:      return ";";

    case TokenKind::LParen:         return "(";
    case TokenKind::RParen:         return ")";
    case TokenKind::LBracket:       return "[";
    case TokenKind::RBracket:       return "]";
    case TokenKind::LBrace:         return "{";
    case TokenKind::RBrace:         return "}";
    }

    return "<unknown>";
}

bool isKeyword(TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::KwDefine:
    case TokenKind::KwConst:
    case TokenKind::KwLet:
    case TokenKind::KwMut:
    case TokenKind::KwFn:
    case TokenKind::KwClass:
    case TokenKind::KwInherits:
    case TokenKind::KwInherited:
    case TokenKind::KwThis:
    case TokenKind::KwImport:
    case TokenKind::KwExport:
    case TokenKind::KwExtern:
    case TokenKind::KwType:
    case TokenKind::KwPub:
    case TokenKind::KwPriv:
    case TokenKind::KwEnum:
    case TokenKind::KwIf:
    case TokenKind::KwElse:
    case TokenKind::KwElif:
    case TokenKind::KwFor:
    case TokenKind::KwIn:
    case TokenKind::KwReturn:
    case TokenKind::KwYield:
    case TokenKind::KwWhile:
    case TokenKind::KwBreak:
    case TokenKind::KwContinue:
    case TokenKind::KwSwitch:
    case TokenKind::KwCase:
    case TokenKind::KwDefault:
    case TokenKind::KwTypeof:
    case TokenKind::KwAs:
    case TokenKind::KwFrom:
    case TokenKind::KwErrtype:
    case TokenKind::KwZonk:
    case TokenKind::KwBool:
    case TokenKind::KwInt:
    case TokenKind::KwFloat:
    case TokenKind::KwChar:
    case TokenKind::KwString:
    case TokenKind::KwVoid:
    case TokenKind::KwError:
    case TokenKind::KwArray:
    case TokenKind::KwMap:
    case TokenKind::KwTrue:
    case TokenKind::KwFalse:
    case TokenKind::KwNull:
    case TokenKind::KwAwait:
    case TokenKind::KwAsync:
        return true;
    default:
        return false;
    }
}

bool isLiteral(TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::IntLiteral:
    case TokenKind::FloatLiteral:
    case TokenKind::StringLiteral:
    case TokenKind::CharLiteral:
    case TokenKind::TemplateNoSub:
    case TokenKind::KwTrue:
    case TokenKind::KwFalse:
    case TokenKind::KwNull:
        return true;
    default:
        return false;
    }
}

bool isAssignmentOperator(TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::Equal:
    case TokenKind::PlusEqual:
    case TokenKind::MinusEqual:
    case TokenKind::StarEqual:
    case TokenKind::SlashEqual:
    case TokenKind::PercentEqual:
        return true;
    default:
        return false;
    }
}

} // namespace keety::lexer
