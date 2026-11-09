#include <keety/kml/kml_parser.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>
#include <sstream>

namespace keety::kml {

KmlParser::KmlParser(std::string_view source,
                     diag::FileId fileId,
                     diag::SourceManager* sourceManager,
                     diag::DiagnosticEmitter* emitter)
    : lexer_(source, fileId, emitter),
      fileId_(fileId),
      sourceMgr_(sourceManager),
      emitter_(emitter) {}

const KmlToken& KmlParser::peek(size_t k) {
    while (buffer_.size() <= k) {
        buffer_.push_back(lexer_.nextToken());
    }
    return buffer_[k];
}

const KmlToken& KmlParser::advance() {
    if (buffer_.empty()) {
        buffer_.push_back(lexer_.nextToken());
    }
    // We return a copy/pop from front
    static KmlToken current;
    current = buffer_.front();
    buffer_.erase(buffer_.begin());
    return current;
}

bool KmlParser::match(KmlTokenKind kind) {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

bool KmlParser::check(KmlTokenKind kind, size_t k) {
    return peek(k).kind == kind;
}

KmlDocument KmlParser::parseDocument(std::string componentName) {
    KmlDocument doc;
    doc.fileId = fileId_;
    doc.componentName = std::move(componentName);

    lexer_.setMode(LexerMode::SfcRoot);
    buffer_.clear();

    while (!check(KmlTokenKind::Eof)) {
        if (check(KmlTokenKind::TagScriptOpen)) {
            advance(); // consume <script>
            diag::SourceSpan scriptSpan;
            std::string rawScript = lexer_.lexRawUntilClosingTag("</script>", scriptSpan);
            // Lexer consumed until </script>
            lexer_.match("</script>");
            doc.script = parseScriptBlock(rawScript, scriptSpan);
            buffer_.clear();
            continue;
        }

        if (check(KmlTokenKind::TagTemplateOpen)) {
            advance(); // consume <template>
            lexer_.setMode(LexerMode::TemplateContent);
            buffer_.clear();
            doc.templateNodes = parseTemplateBlock();
            lexer_.setMode(LexerMode::SfcRoot);
            buffer_.clear();
            continue;
        }

        if (check(KmlTokenKind::TagStyleOpen)) {
            advance(); // consume <style>
            diag::SourceSpan styleSpan;
            std::string rawStyle = lexer_.lexRawUntilClosingTag("</style>", styleSpan);
            lexer_.match("</style>");
            doc.style = parseStyleBlock(rawStyle, styleSpan);
            buffer_.clear();
            continue;
        }

        advance(); // consume unexpected token
    }

    return doc;
}

KmlScriptBlock KmlParser::parseScriptBlock(std::string_view rawScript, diag::SourceSpan span) {
    KmlScriptBlock block;
    block.span = span;
    block.rawCode = std::string(rawScript);

    // Delegate KeetScript AST parsing to keety::parser::Parser
    try {
        keety::lexer::Lexer scriptLexer(fileId_, rawScript, nullptr);
        keety::parser::Parser scriptParser(scriptLexer, nullptr);
        auto mod = scriptParser.parseModule("component_script");
        block.declarations = std::move(mod.declarations);
    } catch (...) {
        // If script parsing fails, declarations will be partial or empty
    }

    // Extract props and state runes ($state, $derived, $effect)
    extractScriptRunesAndProps(block);

    return block;
}

void KmlParser::extractScriptRunesAndProps(KmlScriptBlock& scriptBlock) {
    // Scan raw code or declarations for props & runes
    // 1. Props: e.g., "props: { title: string, count: int = 0 }" or "let { title, count = 0 } = $props();"
    // Also scan for $state(...), $derived(...), $effect(...)
    std::string_view code = scriptBlock.rawCode;

    size_t pos = 0;
    while ((pos = code.find("$state", pos)) != std::string_view::npos) {
        // Check if identifier boundary
        size_t endIdent = pos + 6;
        while (endIdent < code.size() && (code[endIdent] == ' ' || code[endIdent] == '\t')) endIdent++;
        if (endIdent < code.size() && code[endIdent] == '(') {
            // Find argument
            size_t argStart = endIdent + 1;
            int depth = 1;
            size_t curr = argStart;
            while (curr < code.size() && depth > 0) {
                if (code[curr] == '(') depth++;
                else if (code[curr] == ')') depth--;
                curr++;
            }
            std::string initExpr = (curr > argStart) ? std::string(code.substr(argStart, curr - argStart - 1)) : "";

            // Find preceding variable name: e.g. "let mut count = $state(...)"
            std::string varName;
            size_t eqPos = code.rfind('=', pos);
            if (eqPos != std::string_view::npos) {
                size_t vEnd = code.find_last_not_of(" \t\r\n", eqPos - 1);
                if (vEnd != std::string_view::npos) {
                    size_t vStart = vEnd;
                    while (vStart > 0 && (std::isalnum(code[vStart - 1]) || code[vStart - 1] == '_')) {
                        vStart--;
                    }
                    varName = std::string(code.substr(vStart, vEnd - vStart + 1));
                }
            }

            KmlStateRune rune;
            rune.kind = KmlStateRune::Kind::State;
            rune.variableName = varName;
            rune.initExpr = initExpr;
            rune.span = scriptBlock.span;
            scriptBlock.runes.push_back(std::move(rune));
        }
        pos += 6;
    }

    pos = 0;
    while ((pos = code.find("$derived", pos)) != std::string_view::npos) {
        size_t endIdent = pos + 8;
        while (endIdent < code.size() && (code[endIdent] == ' ' || code[endIdent] == '\t')) endIdent++;
        if (endIdent < code.size() && code[endIdent] == '(') {
            size_t argStart = endIdent + 1;
            int depth = 1;
            size_t curr = argStart;
            while (curr < code.size() && depth > 0) {
                if (code[curr] == '(') depth++;
                else if (code[curr] == ')') depth--;
                curr++;
            }
            std::string expr = (curr > argStart) ? std::string(code.substr(argStart, curr - argStart - 1)) : "";

            std::string varName;
            size_t eqPos = code.rfind('=', pos);
            if (eqPos != std::string_view::npos) {
                size_t vEnd = code.find_last_not_of(" \t\r\n", eqPos - 1);
                if (vEnd != std::string_view::npos) {
                    size_t vStart = vEnd;
                    while (vStart > 0 && (std::isalnum(code[vStart - 1]) || code[vStart - 1] == '_')) {
                        vStart--;
                    }
                    varName = std::string(code.substr(vStart, vEnd - vStart + 1));
                }
            }

            KmlStateRune rune;
            rune.kind = KmlStateRune::Kind::Derived;
            rune.variableName = varName;
            rune.initExpr = expr;
            rune.span = scriptBlock.span;
            scriptBlock.runes.push_back(std::move(rune));
        }
        pos += 8;
    }

    pos = 0;
    while ((pos = code.find("$effect", pos)) != std::string_view::npos) {
        KmlStateRune rune;
        rune.kind = KmlStateRune::Kind::Effect;
        rune.variableName = "";
        rune.initExpr = "";
        rune.span = scriptBlock.span;
        scriptBlock.runes.push_back(std::move(rune));
        pos += 7;
    }
}

std::vector<std::unique_ptr<KmlTemplateNode>> KmlParser::parseTemplateBlock() {
    std::vector<std::unique_ptr<KmlTemplateNode>> nodes;

    while (!check(KmlTokenKind::TagTemplateClose) && !check(KmlTokenKind::Eof)) {
        auto node = parseTemplateNode();
        if (node) {
            nodes.push_back(std::move(node));
        }
    }

    if (check(KmlTokenKind::TagTemplateClose)) {
        advance(); // consume </template>
    }

    return nodes;
}

std::unique_ptr<KmlTemplateNode> KmlParser::parseTemplateNode() {
    if (check(KmlTokenKind::Eof) || check(KmlTokenKind::TagTemplateClose)) {
        return nullptr;
    }

    // Text Run
    if (check(KmlTokenKind::TextRun)) {
        auto tok = advance();
        // Skip pure whitespace runs between elements if desired, or preserve them
        return std::make_unique<KmlTextNode>(tok.text, false, tok.span);
    }

    // Interpolated expression: {expr}
    if (check(KmlTokenKind::ExprBlock)) {
        auto tok = advance();
        return std::make_unique<KmlTextNode>(tok.text, true, tok.span);
    }

    // Control flow blocks
    if (check(KmlTokenKind::BlockIfOpen)) {
        auto tok = advance();
        return parseIfBlock(tok);
    }

    if (check(KmlTokenKind::BlockForOpen)) {
        auto tok = advance();
        return parseForBlock(tok);
    }

    if (check(KmlTokenKind::BlockAwaitOpen)) {
        auto tok = advance();
        return parseAwaitBlock(tok);
    }

    // Elements & Components
    if (check(KmlTokenKind::TagOpenStart) || check(KmlTokenKind::ComponentIdentifier)) {
        return parseElement();
    }

    // Unexpected token
    advance();
    return nullptr;
}

std::unique_ptr<KmlElementNode> KmlParser::parseElement() {
    auto startTok = advance();
    std::string tagName = startTok.text;
    bool isComponent = (startTok.kind == KmlTokenKind::ComponentIdentifier);

    // Switch lexer mode to TagAttributes
    lexer_.setMode(LexerMode::TagAttributes);
    buffer_.clear();

    auto element = std::make_unique<KmlElementNode>(tagName, isComponent, false, startTok.span);

    // Parse attributes and directives
    parseTagAttributes(*element);

    // If self closing (e.g. <box />), we are done with this element
    if (element->isSelfClosing()) {
        lexer_.setMode(LexerMode::TemplateContent);
        buffer_.clear();
        return element;
    }

    // Switch back to TemplateContent to parse children
    lexer_.setMode(LexerMode::TemplateContent);
    buffer_.clear();

    // Special case for <slot>: can be standard element or slot
    while (!check(KmlTokenKind::Eof) && !check(KmlTokenKind::TagTemplateClose)) {
        if (check(KmlTokenKind::TagClose)) {
            auto closeTok = advance();
            if (closeTok.text == tagName) {
                // Matching closing tag found
                break;
            } else {
                // Diagnostic: Mismatched closing tag
                if (emitter_) {
                    auto d = diag::Diagnostic::makeError(diag::ErrorCode::UnexpectedToken,
                        "Mismatched closing tag '</" + closeTok.text + ">', expected '</" + tagName + ">'",
                        closeTok.span.start);
                    emitter_->report(d);
                }
                break;
            }
        }

        auto child = parseTemplateNode();
        if (child) {
            element->addChild(std::move(child));
        }
    }

    return element;
}

void KmlParser::parseTagAttributes(KmlElementNode& element) {
    while (!check(KmlTokenKind::Eof)) {
        if (check(KmlTokenKind::TagSelfClosingEnd)) {
            advance(); // consume '/>'
            // Element is self closing
            auto elemSpan = element.getSpan();
            auto updated = std::make_unique<KmlElementNode>(element.getTagName(), element.isComponent(), true, elemSpan);
            for (auto& a : element.getAttributes()) updated->addAttribute(std::move(a));
            for (auto& d : element.getDirectives()) updated->addDirective(std::move(d));
            element = std::move(*updated);
            return;
        }

        if (check(KmlTokenKind::TagOpenEnd)) {
            advance(); // consume '>'
            return;
        }

        // Directives: @event, $bind
        if (check(KmlTokenKind::DirectiveEvent)) {
            auto dirTok = advance();
            KmlDirective dir;
            dir.name = dirTok.text;
            dir.span = dirTok.span;

            // Collect any chained modifiers e.g. .prevent, .debounce(150)
            while (check(KmlTokenKind::DirectiveModifier)) {
                auto modTok = advance();
                dir.modifiers.push_back(modTok.text);
            }

            // Expect '=' and value expression
            if (match(KmlTokenKind::Equals)) {
                if (check(KmlTokenKind::ExprBlock)) {
                    auto exprTok = advance();
                    dir.expression = exprTok.text;
                } else if (check(KmlTokenKind::StringLiteral)) {
                    auto strTok = advance();
                    dir.expression = strTok.text;
                }
            }

            element.addDirective(std::move(dir));
            continue;
        }

        if (check(KmlTokenKind::DirectiveBind)) {
            auto dirTok = advance();
            KmlDirective dir;
            dir.name = "bind";
            dir.span = dirTok.span;
            // e.g. $bind.value or $bind
            if (dirTok.text != "bind") {
                dir.targetProperty = dirTok.text; // e.g. $bind.value -> "bind.value"
                if (dir.targetProperty.starts_with("bind.")) {
                    dir.targetProperty = dir.targetProperty.substr(5);
                }
            }

            while (check(KmlTokenKind::DirectiveModifier)) {
                auto modTok = advance();
                dir.modifiers.push_back(modTok.text);
            }

            if (match(KmlTokenKind::Equals)) {
                if (check(KmlTokenKind::ExprBlock)) {
                    auto exprTok = advance();
                    dir.expression = exprTok.text;
                } else if (check(KmlTokenKind::StringLiteral)) {
                    auto strTok = advance();
                    dir.expression = strTok.text;
                }
            }

            element.addDirective(std::move(dir));
            continue;
        }

        // Regular attribute: name="value" or name={expr} or boolean attribute `disabled`
        if (check(KmlTokenKind::Identifier)) {
            auto attrTok = advance();
            KmlAttribute attr;
            attr.name = attrTok.text;
            attr.span = attrTok.span;

            if (match(KmlTokenKind::Equals)) {
                if (check(KmlTokenKind::StringLiteral)) {
                    auto strTok = advance();
                    attr.value = strTok.text;
                    attr.isExpression = false;
                } else if (check(KmlTokenKind::ExprBlock)) {
                    auto exprTok = advance();
                    attr.value = exprTok.text;
                    attr.isExpression = true;
                }
            } else {
                // Boolean attribute
                attr.value = "true";
                attr.isExpression = false;
            }

            element.addAttribute(std::move(attr));
            continue;
        }

        // Shorthand attribute binding: {prop} -> prop={prop}
        if (check(KmlTokenKind::ExprBlock)) {
            auto exprTok = advance();
            KmlAttribute attr;
            attr.name = exprTok.text;
            attr.value = exprTok.text;
            attr.isExpression = true;
            attr.span = exprTok.span;
            element.addAttribute(std::move(attr));
            continue;
        }

        advance(); // consume unknown token
    }
}

std::unique_ptr<KmlIfBlock> KmlParser::parseIfBlock(const KmlToken& ifTok) {
    auto ifBlock = std::make_unique<KmlIfBlock>(ifTok.text, ifTok.span);

    // Parse then branch
    while (!check(KmlTokenKind::BlockElseIf) &&
           !check(KmlTokenKind::BlockElse) &&
           !check(KmlTokenKind::BlockIfClose) &&
           !check(KmlTokenKind::Eof) &&
           !check(KmlTokenKind::TagTemplateClose)) {
        auto node = parseTemplateNode();
        if (node) {
            ifBlock->addThenChild(std::move(node));
        }
    }

    // Parse {:else if ...} chains
    while (check(KmlTokenKind::BlockElseIf)) {
        auto elseIfTok = advance();
        KmlIfBlock::ElseIfBranch branch;
        branch.condition = elseIfTok.text;
        branch.span = elseIfTok.span;

        while (!check(KmlTokenKind::BlockElseIf) &&
               !check(KmlTokenKind::BlockElse) &&
               !check(KmlTokenKind::BlockIfClose) &&
               !check(KmlTokenKind::Eof) &&
               !check(KmlTokenKind::TagTemplateClose)) {
            auto node = parseTemplateNode();
            if (node) {
                branch.body.push_back(std::move(node));
            }
        }
        ifBlock->addElseIfBranch(std::move(branch));
    }

    // Parse optional {:else}
    if (check(KmlTokenKind::BlockElse)) {
        advance(); // consume {:else}
        while (!check(KmlTokenKind::BlockIfClose) &&
               !check(KmlTokenKind::Eof) &&
               !check(KmlTokenKind::TagTemplateClose)) {
            auto node = parseTemplateNode();
            if (node) {
                ifBlock->addElseChild(std::move(node));
            }
        }
    }

    if (check(KmlTokenKind::BlockIfClose)) {
        advance(); // consume {/if}
    }

    return ifBlock;
}

std::unique_ptr<KmlForBlock> KmlParser::parseForBlock(const KmlToken& forTok) {
    // Syntax variants:
    // (item in items; item.id)
    // (item, index in items; key=item.id)
    // item in items
    std::string text = forTok.text;

    // Strip optional enclosing parens
    size_t startPos = 0;
    while (startPos < text.size() && (text[startPos] == ' ' || text[startPos] == '\t' || text[startPos] == '(')) startPos++;
    size_t endPos = text.size();
    while (endPos > startPos && (text[endPos - 1] == ' ' || text[endPos - 1] == '\t' || text[endPos - 1] == ')')) endPos--;
    std::string inner = text.substr(startPos, endPos - startPos);

    // Split on ';' for keyed expression
    std::string loopPart = inner;
    std::optional<std::string> keyExpr;
    size_t semi = inner.find(';');
    if (semi != std::string::npos) {
        loopPart = inner.substr(0, semi);
        std::string keyPart = inner.substr(semi + 1);
        // trim keyPart
        size_t kFirst = keyPart.find_first_not_of(" \t");
        size_t kLast = keyPart.find_last_not_of(" \t");
        if (kFirst != std::string::npos) {
            std::string kSub = keyPart.substr(kFirst, kLast - kFirst + 1);
            if (kSub.starts_with("key=")) {
                kSub = kSub.substr(4);
            }
            keyExpr = kSub;
        }
    }

    // Split loopPart on " in "
    std::string itemVar;
    std::optional<std::string> indexVar;
    std::string iterableExpr;

    size_t inPos = loopPart.find(" in ");
    if (inPos != std::string::npos) {
        std::string varsPart = loopPart.substr(0, inPos);
        iterableExpr = loopPart.substr(inPos + 4);

        // Check if varsPart has comma: item, index
        size_t commaPos = varsPart.find(',');
        if (commaPos != std::string::npos) {
            std::string item = varsPart.substr(0, commaPos);
            std::string idx = varsPart.substr(commaPos + 1);
            size_t iF = item.find_first_not_of(" \t");
            size_t iL = item.find_last_not_of(" \t");
            if (iF != std::string::npos) itemVar = item.substr(iF, iL - iF + 1);

            size_t idF = idx.find_first_not_of(" \t");
            size_t idL = idx.find_last_not_of(" \t");
            if (idF != std::string::npos) indexVar = idx.substr(idF, idL - idF + 1);
        } else {
            size_t iF = varsPart.find_first_not_of(" \t");
            size_t iL = varsPart.find_last_not_of(" \t");
            if (iF != std::string::npos) itemVar = varsPart.substr(iF, iL - iF + 1);
        }

        // Trim iterableExpr
        size_t itF = iterableExpr.find_first_not_of(" \t");
        size_t itL = iterableExpr.find_last_not_of(" \t");
        if (itF != std::string::npos) iterableExpr = iterableExpr.substr(itF, itL - itF + 1);
    } else {
        itemVar = loopPart;
    }

    auto forBlock = std::make_unique<KmlForBlock>(itemVar, indexVar, iterableExpr, keyExpr, forTok.span);

    while (!check(KmlTokenKind::BlockForClose) &&
           !check(KmlTokenKind::Eof) &&
           !check(KmlTokenKind::TagTemplateClose)) {
        auto node = parseTemplateNode();
        if (node) {
            forBlock->addChild(std::move(node));
        }
    }

    if (check(KmlTokenKind::BlockForClose)) {
        advance(); // consume {/for}
    }

    return forBlock;
}

std::unique_ptr<KmlAwaitBlock> KmlParser::parseAwaitBlock(const KmlToken& awaitTok) {
    auto awaitBlock = std::make_unique<KmlAwaitBlock>(awaitTok.text, awaitTok.span);

    // Pending branch
    while (!check(KmlTokenKind::BlockAwaitThen) &&
           !check(KmlTokenKind::BlockAwaitCatch) &&
           !check(KmlTokenKind::BlockAwaitClose) &&
           !check(KmlTokenKind::Eof) &&
           !check(KmlTokenKind::TagTemplateClose)) {
        auto node = parseTemplateNode();
        if (node) {
            awaitBlock->addPendingChild(std::move(node));
        }
    }

    // {:then result}
    if (check(KmlTokenKind::BlockAwaitThen)) {
        auto thenTok = advance();
        awaitBlock->setThenVariable(thenTok.text);
        while (!check(KmlTokenKind::BlockAwaitCatch) &&
               !check(KmlTokenKind::BlockAwaitClose) &&
               !check(KmlTokenKind::Eof) &&
               !check(KmlTokenKind::TagTemplateClose)) {
            auto node = parseTemplateNode();
            if (node) {
                awaitBlock->addThenChild(std::move(node));
            }
        }
    }

    // {:catch error}
    if (check(KmlTokenKind::BlockAwaitCatch)) {
        auto catchTok = advance();
        awaitBlock->setCatchVariable(catchTok.text);
        while (!check(KmlTokenKind::BlockAwaitClose) &&
               !check(KmlTokenKind::Eof) &&
               !check(KmlTokenKind::TagTemplateClose)) {
            auto node = parseTemplateNode();
            if (node) {
                awaitBlock->addCatchChild(std::move(node));
            }
        }
    }

    if (check(KmlTokenKind::BlockAwaitClose)) {
        advance(); // consume {/await}
    }

    return awaitBlock;
}

KmlStyleBlock KmlParser::parseStyleBlock(std::string_view rawStyle, diag::SourceSpan span) {
    KmlStyleBlock styleBlock;
    styleBlock.span = span;
    styleBlock.rawCss = std::string(rawStyle);

    std::string currentSelector;
    std::string currentBody;
    bool inRule = false;

    for (char c : styleBlock.rawCss) {
        if (c == '{') {
            inRule = true;
            currentBody.clear();
        } else if (c == '}') {
            if (inRule) {
                KmlStyleRule rule;
                size_t sFirst = currentSelector.find_first_not_of(" \t\r\n");
                size_t sLast = currentSelector.find_last_not_of(" \t\r\n");
                if (sFirst != std::string::npos) {
                    rule.selector = currentSelector.substr(sFirst, sLast - sFirst + 1);
                }

                // Parse declarations inside currentBody
                std::istringstream bStream(currentBody);
                std::string declStr;
                while (std::getline(bStream, declStr, ';')) {
                    size_t colon = declStr.find(':');
                    if (colon != std::string::npos) {
                        std::string prop = declStr.substr(0, colon);
                        std::string val = declStr.substr(colon + 1);
                        size_t pF = prop.find_first_not_of(" \t\r\n");
                        size_t pL = prop.find_last_not_of(" \t\r\n");
                        size_t vF = val.find_first_not_of(" \t\r\n");
                        size_t vL = val.find_last_not_of(" \t\r\n");
                        if (pF != std::string::npos && vF != std::string::npos) {
                            KmlStyleDeclaration decl;
                            decl.property = prop.substr(pF, pL - pF + 1);
                            decl.value = val.substr(vF, vL - vF + 1);
                            decl.span = span;
                            rule.declarations.push_back(std::move(decl));
                        }
                    }
                }

                rule.span = span;
                styleBlock.rules.push_back(std::move(rule));
            }
            currentSelector.clear();
            currentBody.clear();
            inRule = false;
        } else if (!inRule) {
            currentSelector += c;
        } else {
            currentBody += c;
        }
    }

    return styleBlock;
}

} // namespace keety::kml
