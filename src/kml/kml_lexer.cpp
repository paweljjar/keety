#include <keety/kml/kml_lexer.hpp>
#include <cctype>

namespace keety::kml {

KmlLexer::KmlLexer(std::string_view source, diag::FileId fileId, diag::DiagnosticEmitter* emitter)
    : source_(source), fileId_(fileId), emitter_(emitter) {}

char KmlLexer::peek(size_t offset) const noexcept {
    if (cursor_ + offset >= source_.size()) return '\0';
    return source_[cursor_ + offset];
}

char KmlLexer::advance() noexcept {
    if (isAtEnd()) return '\0';
    char c = source_[cursor_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool KmlLexer::match(char expected) noexcept {
    if (isAtEnd() || source_[cursor_] != expected) return false;
    advance();
    return true;
}

bool KmlLexer::match(std::string_view expected) noexcept {
    if (cursor_ + expected.size() > source_.size()) return false;
    if (source_.substr(cursor_, expected.size()) == expected) {
        for (size_t i = 0; i < expected.size(); ++i) {
            advance();
        }
        return true;
    }
    return false;
}

bool KmlLexer::isAtEnd() const noexcept {
    return cursor_ >= source_.size();
}

diag::SourceLocation KmlLexer::currentLocation() const noexcept {
    return diag::SourceLocation{fileId_, static_cast<uint32_t>(cursor_), line_, column_};
}

diag::SourceSpan KmlLexer::makeSpan(diag::SourceLocation start) const noexcept {
    return diag::SourceSpan{start, currentLocation()};
}

void KmlLexer::skipWhitespace() {
    while (!isAtEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n')) {
        advance();
    }
}

void KmlLexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        if (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n') {
            advance();
            continue;
        }
        // HTML comment: <!-- ... -->
        if (peek() == '<' && peek(1) == '!' && peek(2) == '-' && peek(3) == '-') {
            advance(); advance(); advance(); advance();
            while (!isAtEnd()) {
                if (peek() == '-' && peek(1) == '-' && peek(2) == '>') {
                    advance(); advance(); advance();
                    break;
                }
                advance();
            }
            continue;
        }
        break;
    }
}

KmlToken KmlLexer::nextToken() {
    if (isAtEnd()) {
        auto loc = currentLocation();
        return KmlToken{KmlTokenKind::Eof, "", diag::SourceSpan{loc, loc}};
    }

    switch (mode_) {
        case LexerMode::SfcRoot:
            return nextSfcRootToken();
        case LexerMode::TemplateContent:
            return nextTemplateContentToken();
        case LexerMode::TagAttributes:
            return nextTagAttributesToken();
    }

    auto loc = currentLocation();
    return KmlToken{KmlTokenKind::Invalid, "", diag::SourceSpan{loc, loc}};
}

std::vector<KmlToken> KmlLexer::tokenizeAll() {
    std::vector<KmlToken> tokens;
    while (true) {
        auto tok = nextToken();
        tokens.push_back(tok);
        if (tok.kind == KmlTokenKind::Eof) break;
    }
    return tokens;
}

KmlToken KmlLexer::nextSfcRootToken() {
    skipWhitespaceAndComments();
    if (isAtEnd()) {
        auto loc = currentLocation();
        return KmlToken{KmlTokenKind::Eof, "", diag::SourceSpan{loc, loc}};
    }

    auto start = currentLocation();

    if (peek() == '<') {
        if (match("<script>")) {
            return KmlToken{KmlTokenKind::TagScriptOpen, "<script>", makeSpan(start)};
        }
        if (match("</script>")) {
            return KmlToken{KmlTokenKind::TagScriptClose, "</script>", makeSpan(start)};
        }
        if (match("<template>")) {
            return KmlToken{KmlTokenKind::TagTemplateOpen, "<template>", makeSpan(start)};
        }
        if (match("</template>")) {
            return KmlToken{KmlTokenKind::TagTemplateClose, "</template>", makeSpan(start)};
        }
        if (match("<style>")) {
            return KmlToken{KmlTokenKind::TagStyleOpen, "<style>", makeSpan(start)};
        }
        if (match("</style>")) {
            return KmlToken{KmlTokenKind::TagStyleClose, "</style>", makeSpan(start)};
        }

        // Check for tags with attributes e.g. <script lang="ts"> or <style scoped>
        if (source_.substr(cursor_).starts_with("<script")) {
            // Read until '>'
            while (!isAtEnd() && peek() != '>') advance();
            if (!isAtEnd()) advance(); // consume '>'
            return KmlToken{KmlTokenKind::TagScriptOpen, "<script>", makeSpan(start)};
        }
        if (source_.substr(cursor_).starts_with("<template")) {
            while (!isAtEnd() && peek() != '>') advance();
            if (!isAtEnd()) advance();
            return KmlToken{KmlTokenKind::TagTemplateOpen, "<template>", makeSpan(start)};
        }
        if (source_.substr(cursor_).starts_with("<style")) {
            while (!isAtEnd() && peek() != '>') advance();
            if (!isAtEnd()) advance();
            return KmlToken{KmlTokenKind::TagStyleOpen, "<style>", makeSpan(start)};
        }
    }

    // Unexpected character at SFC root
    char c = advance();
    return KmlToken{KmlTokenKind::Invalid, std::string(1, c), makeSpan(start)};
}

std::string KmlLexer::lexRawUntilClosingTag(std::string_view closingTag, diag::SourceSpan& outSpan) {
    auto start = currentLocation();
    size_t startPos = cursor_;

    while (!isAtEnd()) {
        if (source_.substr(cursor_).starts_with(closingTag)) {
            break;
        }
        advance();
    }

    size_t endPos = cursor_;
    outSpan = makeSpan(start);
    return std::string(source_.substr(startPos, endPos - startPos));
}

KmlToken KmlLexer::nextTemplateContentToken() {
    if (isAtEnd()) {
        auto loc = currentLocation();
        return KmlToken{KmlTokenKind::Eof, "", diag::SourceSpan{loc, loc}};
    }

    // Check for closing </template>
    if (source_.substr(cursor_).starts_with("</template>")) {
        auto start = currentLocation();
        match("</template>");
        return KmlToken{KmlTokenKind::TagTemplateClose, "</template>", makeSpan(start)};
    }

    // Tag opening or closing
    if (peek() == '<') {
        // Skip comment
        if (peek(1) == '!' && peek(2) == '-' && peek(3) == '-') {
            skipWhitespaceAndComments();
            return nextTemplateContentToken();
        }
        return lexTagOrClose();
    }

    // Control flow block or expression block
    if (peek() == '{') {
        return lexControlBlockOrExpression();
    }

    // Otherwise text content run
    return lexTextRun();
}

KmlToken KmlLexer::lexTagOrClose() {
    auto start = currentLocation();
    advance(); // consume '<'

    // Closing tag: </tag>
    if (match('/')) {
        skipWhitespace();
        size_t idStart = cursor_;
        while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '-' || peek() == '_' || peek() == ':')) {
            advance();
        }
        std::string tagName = std::string(source_.substr(idStart, cursor_ - idStart));
        skipWhitespace();
        if (match('>')) {
            return KmlToken{KmlTokenKind::TagClose, tagName, makeSpan(start)};
        }
        return KmlToken{KmlTokenKind::Invalid, tagName, makeSpan(start)};
    }

    // Opening tag: <tag
    skipWhitespace();
    size_t idStart = cursor_;
    while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '-' || peek() == '_' || peek() == ':')) {
        advance();
    }
    std::string tagName = std::string(source_.substr(idStart, cursor_ - idStart));
    bool isComponent = !tagName.empty() && std::isupper(static_cast<unsigned char>(tagName[0]));

    KmlTokenKind kind = isComponent ? KmlTokenKind::ComponentIdentifier : KmlTokenKind::TagOpenStart;
    return KmlToken{kind, tagName, makeSpan(start)};
}

std::string KmlLexer::lexBalancedExpression(diag::SourceLocation& outStart, diag::SourceLocation& outEnd) {
    // Assumes cursor is positioned right after the opening '{'
    outStart = currentLocation();
    size_t startPos = cursor_;
    int braceDepth = 1;
    int parenDepth = 0;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool inBacktick = false;

    while (!isAtEnd() && braceDepth > 0) {
        char c = peek();

        if (c == '\\' && (inSingleQuote || inDoubleQuote || inBacktick)) {
            advance(); // consume '\'
            if (!isAtEnd()) advance(); // consume escaped char
            continue;
        }

        if (inSingleQuote) {
            if (c == '\'') inSingleQuote = false;
            advance();
            continue;
        }
        if (inDoubleQuote) {
            if (c == '"') inDoubleQuote = false;
            advance();
            continue;
        }
        if (inBacktick) {
            if (c == '`') inBacktick = false;
            advance();
            continue;
        }

        if (c == '\'') { inSingleQuote = true; advance(); continue; }
        if (c == '"') { inDoubleQuote = true; advance(); continue; }
        if (c == '`') { inBacktick = true; advance(); continue; }

        if (c == '(') { parenDepth++; advance(); continue; }
        if (c == ')') {
            if (parenDepth > 0) parenDepth--;
            advance();
            continue;
        }

        if (c == '{') {
            braceDepth++;
            advance();
            continue;
        }
        if (c == '}') {
            braceDepth--;
            if (braceDepth == 0) {
                // Done with outer expression
                size_t endPos = cursor_;
                advance(); // consume final '}'
                outEnd = currentLocation();
                return std::string(source_.substr(startPos, endPos - startPos));
            }
            advance();
            continue;
        }

        advance();
    }

    size_t endPos = cursor_;
    outEnd = currentLocation();
    return std::string(source_.substr(startPos, endPos - startPos));
}

KmlToken KmlLexer::lexControlBlockOrExpression() {
    auto start = currentLocation();
    advance(); // consume '{'

    char nextChar = peek();

    // Control flow blocks: {# ... }, {: ... }, {/ ... }
    if (nextChar == '#' || nextChar == ':' || nextChar == '/') {
        advance(); // consume '#', ':', or '/'
        // Read block keyword (e.g. "if", "for", "await", "else", "else if", "then", "catch")
        skipWhitespace();
        size_t kwStart = cursor_;
        while (!isAtEnd() && peek() != '}' && peek() != ' ' && peek() != '\t' && peek() != '\r' && peek() != '\n' && peek() != '(') {
            advance();
        }
        std::string keyword = std::string(source_.substr(kwStart, cursor_ - kwStart));

        // Read remaining content of the block tag until '}'
        // The rest inside the block until matching '}'
        size_t restStart = cursor_;
        int depth = 1; // we already consumed '{'
        while (!isAtEnd() && depth > 0) {
            char c = peek();
            if (c == '{') depth++;
            else if (c == '}') {
                depth--;
                if (depth == 0) break;
            }
            advance();
        }
        size_t restEnd = cursor_;
        if (peek() == '}') advance(); // consume '}'

        std::string rest = std::string(source_.substr(restStart, restEnd - restStart));

        // Trim leading and trailing whitespace of rest
        size_t first = rest.find_first_not_of(" \t\r\n");
        size_t last = rest.find_last_not_of(" \t\r\n");
        std::string trimmedRest = (first != std::string::npos) ? rest.substr(first, (last - first + 1)) : "";

        if (nextChar == '#') {
            if (keyword == "if") return KmlToken{KmlTokenKind::BlockIfOpen, trimmedRest, makeSpan(start)};
            if (keyword == "for") return KmlToken{KmlTokenKind::BlockForOpen, trimmedRest, makeSpan(start)};
            if (keyword == "await") return KmlToken{KmlTokenKind::BlockAwaitOpen, trimmedRest, makeSpan(start)};
        } else if (nextChar == ':') {
            if (keyword == "else") {
                if (trimmedRest.starts_with("if")) {
                    std::string condition = trimmedRest.substr(2);
                    size_t cFirst = condition.find_first_not_of(" \t\r\n");
                    if (cFirst != std::string::npos) condition = condition.substr(cFirst);
                    return KmlToken{KmlTokenKind::BlockElseIf, condition, makeSpan(start)};
                }
                return KmlToken{KmlTokenKind::BlockElse, trimmedRest, makeSpan(start)};
            }
            if (keyword == "then") return KmlToken{KmlTokenKind::BlockAwaitThen, trimmedRest, makeSpan(start)};
            if (keyword == "catch") return KmlToken{KmlTokenKind::BlockAwaitCatch, trimmedRest, makeSpan(start)};
        } else if (nextChar == '/') {
            if (keyword == "if") return KmlToken{KmlTokenKind::BlockIfClose, "", makeSpan(start)};
            if (keyword == "for") return KmlToken{KmlTokenKind::BlockForClose, "", makeSpan(start)};
            if (keyword == "await") return KmlToken{KmlTokenKind::BlockAwaitClose, "", makeSpan(start)};
        }

        return KmlToken{KmlTokenKind::Invalid, keyword, makeSpan(start)};
    }

    // It's a standard expression block: {expr}
    diag::SourceLocation exprStart, exprEnd;
    std::string expr = lexBalancedExpression(exprStart, exprEnd);
    return KmlToken{KmlTokenKind::ExprBlock, expr, makeSpan(start)};
}

KmlToken KmlLexer::lexTextRun() {
    auto start = currentLocation();
    size_t startPos = cursor_;

    while (!isAtEnd() && peek() != '<' && peek() != '{') {
        advance();
    }

    size_t endPos = cursor_;
    std::string text = std::string(source_.substr(startPos, endPos - startPos));
    return KmlToken{KmlTokenKind::TextRun, text, makeSpan(start)};
}

KmlToken KmlLexer::nextTagAttributesToken() {
    skipWhitespace();
    if (isAtEnd()) {
        auto loc = currentLocation();
        return KmlToken{KmlTokenKind::Eof, "", diag::SourceSpan{loc, loc}};
    }

    auto start = currentLocation();

    if (peek() == '/' && peek(1) == '>') {
        advance(); advance();
        return KmlToken{KmlTokenKind::TagSelfClosingEnd, "/>", makeSpan(start)};
    }

    if (peek() == '>') {
        advance();
        return KmlToken{KmlTokenKind::TagOpenEnd, ">", makeSpan(start)};
    }

    if (peek() == '=') {
        advance();
        return KmlToken{KmlTokenKind::Equals, "=", makeSpan(start)};
    }

    if (peek() == '"' || peek() == '\'') {
        char quote = advance();
        return lexStringLiteral(quote);
    }

    if (peek() == '{') {
        advance(); // consume '{'
        diag::SourceLocation exprStart, exprEnd;
        std::string expr = lexBalancedExpression(exprStart, exprEnd);
        return KmlToken{KmlTokenKind::ExprBlock, expr, makeSpan(start)};
    }

    return lexDirectiveOrIdentifier();
}

KmlToken KmlLexer::lexStringLiteral(char quoteChar) {
    auto start = currentLocation();
    size_t startPos = cursor_;

    while (!isAtEnd() && peek() != quoteChar) {
        if (peek() == '\\') {
            advance();
            if (!isAtEnd()) advance();
            continue;
        }
        advance();
    }

    size_t endPos = cursor_;
    if (peek() == quoteChar) {
        advance(); // consume closing quote
    }

    std::string content = std::string(source_.substr(startPos, endPos - startPos));
    return KmlToken{KmlTokenKind::StringLiteral, content, makeSpan(start)};
}

KmlToken KmlLexer::lexDirectiveOrIdentifier() {
    auto start = currentLocation();

    // Event listener: @click, @keydown.enter, @input.debounce(150)
    if (peek() == '@') {
        advance(); // consume '@'
        size_t idStart = cursor_;
        while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '-' || peek() == '_')) {
            advance();
        }
        std::string eventName = std::string(source_.substr(idStart, cursor_ - idStart));
        return KmlToken{KmlTokenKind::DirectiveEvent, eventName, makeSpan(start)};
    }

    // Two-way binding: $bind, $bind.value, $bind.checked
    if (peek() == '$') {
        advance(); // consume '$'
        size_t idStart = cursor_;
        while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '-' || peek() == '_' || peek() == '.')) {
            advance();
        }
        std::string bindName = std::string(source_.substr(idStart, cursor_ - idStart));
        return KmlToken{KmlTokenKind::DirectiveBind, bindName, makeSpan(start)};
    }

    // Modifier dot e.g. .debounce(150), .prevent
    if (peek() == '.') {
        advance(); // consume '.'
        size_t idStart = cursor_;
        while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '-' || peek() == '_')) {
            advance();
        }
        std::string mod = std::string(source_.substr(idStart, cursor_ - idStart));

        // Modifier args e.g. (150)
        if (peek() == '(') {
            advance(); // consume '('
            size_t argStart = cursor_;
            int pDepth = 1;
            while (!isAtEnd() && pDepth > 0) {
                if (peek() == '(') pDepth++;
                else if (peek() == ')') {
                    pDepth--;
                    if (pDepth == 0) break;
                }
                advance();
            }
            size_t argEnd = cursor_;
            if (peek() == ')') advance();
            mod += "(" + std::string(source_.substr(argStart, argEnd - argStart)) + ")";
        }

        return KmlToken{KmlTokenKind::DirectiveModifier, mod, makeSpan(start)};
    }

    // Regular attribute or element identifier
    size_t idStart = cursor_;
    while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '-' || peek() == '_' || peek() == ':')) {
        advance();
    }

    std::string ident = std::string(source_.substr(idStart, cursor_ - idStart));
    if (ident.empty()) {
        char c = advance();
        return KmlToken{KmlTokenKind::Invalid, std::string(1, c), makeSpan(start)};
    }

    bool isComponent = std::isupper(static_cast<unsigned char>(ident[0]));
    return KmlToken{isComponent ? KmlTokenKind::ComponentIdentifier : KmlTokenKind::Identifier, ident, makeSpan(start)};
}

} // namespace keety::kml
