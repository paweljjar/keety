#include <keety/parser/parser.hpp>

namespace keety::parser {

Parser::Parser(lexer::Lexer& lexer, diag::DiagnosticEmitter* emitter)
    : lexer_(lexer), emitter_(emitter) {
    // Prime the lookahead buffer
    lookahead_.push_back(lexer_.nextToken());
}

const lexer::Token& Parser::peek(size_t k) {
    while (k >= lookahead_.size()) {
        lookahead_.push_back(lexer_.nextToken());
    }
    return lookahead_[k];
}

const lexer::Token& Parser::advance() {
    if (lookahead_.empty()) {
        lookahead_.push_back(lexer_.nextToken());
    }
    lastConsumed_ = lookahead_.front();
    lookahead_.erase(lookahead_.begin());
    if (lookahead_.empty()) {
        lookahead_.push_back(lexer_.nextToken());
    }
    return lastConsumed_;
}

bool Parser::check(lexer::TokenKind kind, size_t k) {
    return peek(k).is(kind);
}

bool Parser::match(lexer::TokenKind kind) {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

const lexer::Token& Parser::expect(lexer::TokenKind kind, diag::ErrorCode errCode, std::string_view msg) {
    if (check(kind)) {
        return advance();
    }

    if (emitter_ && !panicMode_) {
        diag::SourceSpan span = peek().span;
        emitter_->report(diag::Diagnostic::makeError(errCode, std::string(msg), span.start)
                             .withLabel(span, std::string(msg)));
    }
    panicMode_ = true;
    return peek();
}

void Parser::synchronize() {
    panicMode_ = false;
    advance();

    while (!check(lexer::TokenKind::Eof)) {
        if (lastConsumed_.is(lexer::TokenKind::Semicolon)) {
            return;
        }

        switch (peek().kind) {
        case lexer::TokenKind::KwClass:
        case lexer::TokenKind::KwFn:
        case lexer::TokenKind::KwLet:
        case lexer::TokenKind::KwConst:
        case lexer::TokenKind::KwDefine:
        case lexer::TokenKind::KwFor:
        case lexer::TokenKind::KwIf:
        case lexer::TokenKind::KwWhile:
        case lexer::TokenKind::KwReturn:
        case lexer::TokenKind::KwImport:
        case lexer::TokenKind::KwExport:
            return;
        default:
            advance();
            break;
        }
    }
}

ModuleAST Parser::parseModule(std::string moduleName) {
    ModuleAST mod;
    mod.fileId = lexer_.getFileId();
    mod.moduleName = std::move(moduleName);

    while (!check(lexer::TokenKind::Eof)) {
        try {
            auto decl = parseDeclaration();
            if (decl) {
                mod.declarations.push_back(std::move(decl));
            }
        } catch (...) {
            synchronize();
        }
    }

    return mod;
}

std::unique_ptr<ast::Decl> Parser::parseDeclaration() {
    bool isExported = match(lexer::TokenKind::KwExport);

    if (check(lexer::TokenKind::KwLet)) {
        advance();
        return parseVarDecl(isExported);
    }
    if (check(lexer::TokenKind::KwConst)) {
        advance();
        return parseConstDecl(isExported);
    }
    if (check(lexer::TokenKind::KwDefine)) {
        advance();
        return parseDefineDecl(isExported);
    }
    if (check(lexer::TokenKind::KwAsync)) {
        advance();
        expect(lexer::TokenKind::KwFn, diag::ErrorCode::UnexpectedToken, "expected 'fn' after 'async'");
        return parseFnDecl(true, isExported);
    }
    if (check(lexer::TokenKind::KwFn)) {
        advance();
        return parseFnDecl(false, isExported);
    }
    if (check(lexer::TokenKind::KwClass)) {
        advance();
        return parseClassDecl(isExported);
    }
    if (check(lexer::TokenKind::KwEnum)) {
        advance();
        return parseEnumDecl(isExported);
    }
    if (check(lexer::TokenKind::KwErrtype)) {
        advance();
        return parseErrTypeDecl(isExported);
    }
    if (check(lexer::TokenKind::KwImport)) {
        advance();
        return parseImportDecl(isExported);
    }

    // Top-level statement or error
    auto stmt = parseStatement();
    if (stmt) {
        // If wrapped in DeclStmt, unwrap or return as a wrapper
        // In KeetScript top-level statements can execute in root module
    }

    if (check(lexer::TokenKind::Invalid)) {
        advance();
    }
    return nullptr;
}

std::unique_ptr<ast::VarDecl> Parser::parseVarDecl(bool isExported) {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    const auto& nameTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected variable name");
    std::string name = std::string(nameTok.lexeme);

    expect(lexer::TokenKind::Colon, diag::ErrorCode::ExpectedColon, "expected ':' type annotation");
    auto type = parseTypeAnnotation();

    std::unique_ptr<ast::Expr> init = nullptr;
    if (match(lexer::TokenKind::Equal)) {
        init = parseExpression();
    }

    const auto& semiTok = expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after variable declaration");
    diag::SourceSpan span{startLoc, semiTok.span.end};

    return std::make_unique<ast::VarDecl>(span, std::move(name), std::move(type), std::move(init), isExported);
}

std::unique_ptr<ast::ConstDecl> Parser::parseConstDecl(bool isExported) {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    const auto& nameTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected constant name");
    std::string name = std::string(nameTok.lexeme);

    expect(lexer::TokenKind::Colon, diag::ErrorCode::ExpectedColon, "expected ':' type annotation");
    auto type = parseTypeAnnotation();

    expect(lexer::TokenKind::Equal, diag::ErrorCode::UnexpectedToken, "constants require an explicit initializer '='");
    auto init = parseExpression();

    const auto& semiTok = expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after constant declaration");
    diag::SourceSpan span{startLoc, semiTok.span.end};

    return std::make_unique<ast::ConstDecl>(span, std::move(name), std::move(type), std::move(init), isExported);
}

std::unique_ptr<ast::DefineDecl> Parser::parseDefineDecl(bool isExported) {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    const auto& nameTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected define constant name");
    std::string name = std::string(nameTok.lexeme);

    expect(lexer::TokenKind::Colon, diag::ErrorCode::ExpectedColon, "expected ':' type annotation");
    auto type = parseTypeAnnotation();

    expect(lexer::TokenKind::Equal, diag::ErrorCode::UnexpectedToken, "define requires an initializer '='");
    auto value = parseExpression();

    const auto& semiTok = expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after define declaration");
    diag::SourceSpan span{startLoc, semiTok.span.end};

    return std::make_unique<ast::DefineDecl>(span, std::move(name), std::move(type), std::move(value), isExported);
}

std::unique_ptr<ast::FnDecl> Parser::parseFnDecl(bool isAsync, bool isExported) {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    const auto& nameTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected function name");
    std::string name = std::string(nameTok.lexeme);

    expect(lexer::TokenKind::LParen, diag::ErrorCode::UnexpectedToken, "expected '(' after function name");
    auto params = parseParamList();
    expect(lexer::TokenKind::RParen, diag::ErrorCode::ExpectedClosingParen, "expected ')' after parameters");

    expect(lexer::TokenKind::Colon, diag::ErrorCode::ExpectedColon, "expected ':' return type annotation");
    auto returnType = parseTypeAnnotation();

    auto body = parseBlockStatement();
    diag::SourceSpan span{startLoc, body ? body->getSpan().end : lastConsumed_.span.end};

    return std::make_unique<ast::FnDecl>(span, std::move(name), std::move(params), std::move(returnType),
                                         std::move(body), isAsync, isExported);
}

std::vector<ast::Param> Parser::parseParamList() {
    std::vector<ast::Param> params;
    if (check(lexer::TokenKind::RParen)) {
        return params;
    }

    do {
        params.push_back(parseParam());
    } while (match(lexer::TokenKind::Comma));

    return params;
}

ast::Param Parser::parseParam() {
    bool isMut = match(lexer::TokenKind::KwMut);
    diag::SourceLocation startLoc = isMut ? lastConsumed_.span.start : peek().span.start;

    const auto& nameTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected parameter name");
    std::string name = std::string(nameTok.lexeme);

    expect(lexer::TokenKind::Colon, diag::ErrorCode::ExpectedColon, "expected ':' type annotation for parameter");
    auto type = parseTypeAnnotation();

    std::unique_ptr<ast::Expr> defVal = nullptr;
    if (match(lexer::TokenKind::Equal)) {
        defVal = parseExpression();
    }

    diag::SourceSpan span{startLoc, defVal ? defVal->getSpan().end : (type ? type->getSpan().end : nameTok.span.end)};
    return ast::Param{span, std::move(name), std::move(type), isMut, std::move(defVal)};
}

std::unique_ptr<ast::ClassDecl> Parser::parseClassDecl(bool isExported) {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    const auto& nameTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected class name");
    std::string name = std::string(nameTok.lexeme);

    std::string baseClass;
    if (match(lexer::TokenKind::KwInherits)) {
        const auto& baseTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected base class name");
        baseClass = std::string(baseTok.lexeme);
    }

    expect(lexer::TokenKind::LBrace, diag::ErrorCode::UnexpectedToken, "expected '{' before class body");

    std::vector<ast::ClassField> fields;
    std::vector<std::unique_ptr<ast::FnDecl>> methods;

    while (!check(lexer::TokenKind::RBrace) && !check(lexer::TokenKind::Eof)) {
        ast::Visibility vis = ast::Visibility::Default;
        if (match(lexer::TokenKind::KwPub)) {
            vis = ast::Visibility::Pub;
        } else if (match(lexer::TokenKind::KwPriv)) {
            vis = ast::Visibility::Priv;
        }

        if (check(lexer::TokenKind::KwLet) || check(lexer::TokenKind::KwConst)) {
            bool isConst = match(lexer::TokenKind::KwConst);
            if (!isConst) {
                match(lexer::TokenKind::KwLet);
            }

            const auto& fName = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected field name");
            std::string fieldName = std::string(fName.lexeme);
            diag::SourceSpan fieldSpan = fName.span;

            expect(lexer::TokenKind::Colon, diag::ErrorCode::ExpectedColon, "expected ':' type annotation");
            auto fType = parseTypeAnnotation();

            std::unique_ptr<ast::Expr> fInit = nullptr;
            if (match(lexer::TokenKind::Equal)) {
                fInit = parseExpression();
            }
            expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after field declaration");

            fields.push_back(ast::ClassField{fieldSpan, vis, isConst, std::move(fieldName), std::move(fType), std::move(fInit)});
        } else if (check(lexer::TokenKind::KwFn)) {
            advance();
            methods.push_back(parseFnDecl(false, false));
        } else {
            advance();
        }
    }

    const auto& endBrace = expect(lexer::TokenKind::RBrace, diag::ErrorCode::ExpectedClosingBrace, "expected '}' after class body");
    diag::SourceSpan span{startLoc, endBrace.span.end};

    return std::make_unique<ast::ClassDecl>(span, std::move(name), std::move(baseClass),
                                           std::move(fields), std::move(methods), isExported);
}

std::unique_ptr<ast::EnumDecl> Parser::parseEnumDecl(bool isExported) {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    const auto& nameTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected enum name");
    std::string name = std::string(nameTok.lexeme);

    std::unique_ptr<ast::TypeAnnotation> underlyingType = nullptr;
    if (match(lexer::TokenKind::Colon)) {
        underlyingType = parseTypeAnnotation();
    }

    expect(lexer::TokenKind::LBrace, diag::ErrorCode::UnexpectedToken, "expected '{' before enum body");
    std::vector<ast::EnumMember> members;

    while (!check(lexer::TokenKind::RBrace) && !check(lexer::TokenKind::Eof)) {
        const auto& mName = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected enum member name");
        std::unique_ptr<ast::Expr> mVal = nullptr;
        if (match(lexer::TokenKind::Equal)) {
            mVal = parseExpression();
        }
        members.push_back(ast::EnumMember{mName.span, std::string(mName.lexeme), std::move(mVal)});
        if (!match(lexer::TokenKind::Comma)) {
            break;
        }
    }

    const auto& endBrace = expect(lexer::TokenKind::RBrace, diag::ErrorCode::ExpectedClosingBrace, "expected '}' after enum body");
    diag::SourceSpan span{startLoc, endBrace.span.end};

    return std::make_unique<ast::EnumDecl>(span, std::move(name), std::move(underlyingType),
                                          std::move(members), isExported);
}

std::unique_ptr<ast::ErrTypeDecl> Parser::parseErrTypeDecl(bool isExported) {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    const auto& nameTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected error type name");
    std::string name = std::string(nameTok.lexeme);

    std::string baseErr = "error";
    if (match(lexer::TokenKind::KwInherits)) {
        const auto& baseTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected base error name");
        baseErr = std::string(baseTok.lexeme);
    }

    expect(lexer::TokenKind::LBrace, diag::ErrorCode::UnexpectedToken, "expected '{' before errtype body");
    std::vector<ast::ClassField> fields;

    while (!check(lexer::TokenKind::RBrace) && !check(lexer::TokenKind::Eof)) {
        ast::Visibility vis = ast::Visibility::Default;
        if (match(lexer::TokenKind::KwPub)) {
            vis = ast::Visibility::Pub;
        }
        if (match(lexer::TokenKind::KwLet) || match(lexer::TokenKind::KwConst)) {
            const auto& fName = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected field name");
            expect(lexer::TokenKind::Colon, diag::ErrorCode::ExpectedColon, "expected ':' type annotation");
            auto fType = parseTypeAnnotation();
            expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after field");
            fields.push_back(ast::ClassField{fName.span, vis, false, std::string(fName.lexeme), std::move(fType), nullptr});
        } else {
            advance();
        }
    }

    const auto& endBrace = expect(lexer::TokenKind::RBrace, diag::ErrorCode::ExpectedClosingBrace, "expected '}' after errtype body");
    diag::SourceSpan span{startLoc, endBrace.span.end};

    return std::make_unique<ast::ErrTypeDecl>(span, std::move(name), std::move(baseErr), std::move(fields), isExported);
}

std::unique_ptr<ast::ImportDecl> Parser::parseImportDecl(bool isReExported) {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    // import "path" [as alias]; or import io;
    std::string path;
    if (check(lexer::TokenKind::StringLiteral) || check(lexer::TokenKind::Identifier)) {
        path = std::string(advance().lexeme);
        // Strip quotes if string literal
        if (path.size() >= 2 && (path.front() == '"' || path.front() == '\'')) {
            path = path.substr(1, path.size() - 2);
        }
    }

    std::string alias;
    if (match(lexer::TokenKind::KwAs)) {
        const auto& aliasTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected import alias");
        alias = std::string(aliasTok.lexeme);
    }

    const auto& semiTok = expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after import");
    diag::SourceSpan span{startLoc, semiTok.span.end};

    return std::make_unique<ast::ImportDecl>(span, std::move(path), std::move(alias), std::vector<ast::ImportedSymbol>{}, isReExported);
}

std::unique_ptr<ast::BlockStmt> Parser::parseBlockStatement() {
    diag::SourceLocation startLoc = peek().span.start;
    expect(lexer::TokenKind::LBrace, diag::ErrorCode::UnexpectedToken, "expected '{' to begin block");

    std::vector<std::unique_ptr<ast::Stmt>> stmts;
    while (!check(lexer::TokenKind::RBrace) && !check(lexer::TokenKind::Eof)) {
        auto stmt = parseStatement();
        if (stmt) {
            stmts.push_back(std::move(stmt));
        }
    }

    const auto& endBrace = expect(lexer::TokenKind::RBrace, diag::ErrorCode::ExpectedClosingBrace, "expected '}' to close block");
    diag::SourceSpan span{startLoc, endBrace.span.end};

    return std::make_unique<ast::BlockStmt>(span, std::move(stmts));
}

std::unique_ptr<ast::Stmt> Parser::parseStatement() {
    if (check(lexer::TokenKind::LBrace)) {
        return parseBlockStatement();
    }
    if (check(lexer::TokenKind::KwIf)) {
        advance();
        return parseIfStatement();
    }
    if (check(lexer::TokenKind::KwWhile)) {
        advance();
        return parseWhileStatement();
    }
    if (check(lexer::TokenKind::KwFor)) {
        advance();
        return parseForStatement();
    }
    if (check(lexer::TokenKind::KwSwitch)) {
        advance();
        return parseSwitchStatement();
    }
    if (check(lexer::TokenKind::KwReturn)) {
        advance();
        return parseReturnStatement();
    }
    if (check(lexer::TokenKind::KwYield)) {
        advance();
        return parseYieldStatement();
    }
    if (check(lexer::TokenKind::KwBreak)) {
        advance();
        return parseBreakStatement();
    }
    if (check(lexer::TokenKind::KwContinue)) {
        advance();
        return parseContinueStatement();
    }
    if (check(lexer::TokenKind::KwZonk)) {
        advance();
        return parseZonkStatement();
    }
    if (check(lexer::TokenKind::KwLet) || check(lexer::TokenKind::KwConst) || check(lexer::TokenKind::KwDefine)) {
        auto decl = parseDeclaration();
        if (decl) {
            diag::SourceSpan span = decl->getSpan();
            return std::make_unique<ast::DeclStmt>(span, std::move(decl));
        }
        return nullptr;
    }

    // Expression statement
    diag::SourceLocation startLoc = peek().span.start;
    auto expr = parseExpression();
    const auto& semiTok = expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after expression statement");
    diag::SourceSpan span{startLoc, semiTok.span.end};

    return std::make_unique<ast::ExprStmt>(span, std::move(expr));
}

std::unique_ptr<ast::Stmt> Parser::parseIfStatement() {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    expect(lexer::TokenKind::LParen, diag::ErrorCode::UnexpectedToken, "expected '(' after 'if'");

    // Simple condition expression (Task 2.3.3 will add full IfHeaderInit disambiguation)
    auto condition = parseExpression();
    expect(lexer::TokenKind::RParen, diag::ErrorCode::ExpectedClosingParen, "expected ')' after if condition");

    auto thenBranch = parseBlockStatement();
    std::unique_ptr<ast::Stmt> elseBranch = nullptr;

    if (match(lexer::TokenKind::KwElif)) {
        elseBranch = parseIfStatement();
    } else if (match(lexer::TokenKind::KwElse)) {
        elseBranch = parseBlockStatement();
    }

    diag::SourceSpan span{startLoc, elseBranch ? elseBranch->getSpan().end : thenBranch->getSpan().end};
    return std::make_unique<ast::IfStmt>(span, nullptr, std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<ast::Stmt> Parser::parseWhileStatement() {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    expect(lexer::TokenKind::LParen, diag::ErrorCode::UnexpectedToken, "expected '(' after 'while'");

    auto condition = parseExpression();
    expect(lexer::TokenKind::RParen, diag::ErrorCode::ExpectedClosingParen, "expected ')' after while condition");

    std::string label;
    if (match(lexer::TokenKind::KwAs)) {
        const auto& lTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected while loop label");
        label = std::string(lTok.lexeme);
    }

    auto body = parseBlockStatement();
    diag::SourceSpan span{startLoc, body ? body->getSpan().end : lastConsumed_.span.end};

    return std::make_unique<ast::WhileStmt>(span, nullptr, std::move(condition), std::move(body), std::move(label));
}

std::unique_ptr<ast::Stmt> Parser::parseForStatement() {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    expect(lexer::TokenKind::LParen, diag::ErrorCode::UnexpectedToken, "expected '(' after 'for'");

    // Simple for loop: for (init; cond; post)
    std::unique_ptr<ast::Stmt> init = nullptr;
    if (!check(lexer::TokenKind::Semicolon)) {
        if (match(lexer::TokenKind::KwLet) || match(lexer::TokenKind::KwConst)) {
            const auto& nameTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected loop variable");
            expect(lexer::TokenKind::Colon, diag::ErrorCode::ExpectedColon, "expected ':' type annotation");
            auto type = parseTypeAnnotation();
            expect(lexer::TokenKind::Equal, diag::ErrorCode::UnexpectedToken, "expected '='");
            auto val = parseExpression();
            diag::SourceSpan s{nameTok.span.start, val->getSpan().end};
            auto decl = std::make_unique<ast::VarDecl>(s, std::string(nameTok.lexeme), std::move(type), std::move(val), false);
            init = std::make_unique<ast::DeclStmt>(s, std::move(decl));
        } else {
            auto expr = parseExpression();
            init = std::make_unique<ast::ExprStmt>(expr->getSpan(), std::move(expr));
        }
    }
    expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after for-init");

    auto cond = parseExpression();
    expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after for-condition");

    auto postExpr = parseExpression();
    auto post = std::make_unique<ast::ExprStmt>(postExpr->getSpan(), std::move(postExpr));

    expect(lexer::TokenKind::RParen, diag::ErrorCode::ExpectedClosingParen, "expected ')' after for-header");

    std::string label;
    if (match(lexer::TokenKind::KwAs)) {
        const auto& lTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected for loop label");
        label = std::string(lTok.lexeme);
    }

    auto body = parseBlockStatement();
    diag::SourceSpan span{startLoc, body ? body->getSpan().end : lastConsumed_.span.end};

    return std::make_unique<ast::ForStmt>(span, std::move(init), std::move(cond), std::move(post), std::move(body), std::move(label));
}

std::unique_ptr<ast::Stmt> Parser::parseSwitchStatement() {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    expect(lexer::TokenKind::LParen, diag::ErrorCode::UnexpectedToken, "expected '(' after 'switch'");

    auto discriminant = parseExpression();
    expect(lexer::TokenKind::RParen, diag::ErrorCode::ExpectedClosingParen, "expected ')' after switch expression");

    std::string label;
    if (match(lexer::TokenKind::KwAs)) {
        const auto& lTok = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected switch label");
        label = std::string(lTok.lexeme);
    }

    expect(lexer::TokenKind::LBrace, diag::ErrorCode::UnexpectedToken, "expected '{' before switch cases");
    std::vector<ast::SwitchCase> cases;

    while (!check(lexer::TokenKind::RBrace) && !check(lexer::TokenKind::Eof)) {
        diag::SourceLocation caseStart = peek().span.start;
        if (match(lexer::TokenKind::KwCase)) {
            expect(lexer::TokenKind::LParen, diag::ErrorCode::UnexpectedToken, "expected '(' after 'case'");
            std::vector<std::unique_ptr<ast::Expr>> vals;
            vals.push_back(parseExpression());
            expect(lexer::TokenKind::RParen, diag::ErrorCode::ExpectedClosingParen, "expected ')' after case value");
            auto caseBody = parseBlockStatement();
            diag::SourceSpan caseSpan{caseStart, caseBody->getSpan().end};
            cases.push_back(ast::SwitchCase{caseSpan, std::move(vals), std::move(caseBody)});
        } else if (match(lexer::TokenKind::KwDefault)) {
            auto defaultBody = parseBlockStatement();
            diag::SourceSpan defaultSpan{caseStart, defaultBody->getSpan().end};
            cases.push_back(ast::SwitchCase{defaultSpan, {}, std::move(defaultBody)});
        } else {
            advance();
        }
    }

    const auto& endBrace = expect(lexer::TokenKind::RBrace, diag::ErrorCode::ExpectedClosingBrace, "expected '}' after switch cases");
    diag::SourceSpan span{startLoc, endBrace.span.end};

    return std::make_unique<ast::SwitchStmt>(span, nullptr, std::move(discriminant), std::move(cases), std::move(label));
}

std::unique_ptr<ast::Stmt> Parser::parseReturnStatement() {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    std::unique_ptr<ast::Expr> val = nullptr;

    if (!check(lexer::TokenKind::Semicolon)) {
        val = parseExpression();
    }

    const auto& semiTok = expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after return statement");
    diag::SourceSpan span{startLoc, semiTok.span.end};

    return std::make_unique<ast::ReturnStmt>(span, std::move(val));
}

std::unique_ptr<ast::Stmt> Parser::parseYieldStatement() {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    auto val = parseExpression();
    const auto& semiTok = expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after yield");
    diag::SourceSpan span{startLoc, semiTok.span.end};

    return std::make_unique<ast::YieldStmt>(span, std::move(val));
}

std::unique_ptr<ast::Stmt> Parser::parseBreakStatement() {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    std::string label;
    if (check(lexer::TokenKind::Identifier)) {
        label = std::string(advance().lexeme);
    }
    const auto& semiTok = expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after break");
    diag::SourceSpan span{startLoc, semiTok.span.end};

    return std::make_unique<ast::BreakStmt>(span, std::move(label));
}

std::unique_ptr<ast::Stmt> Parser::parseContinueStatement() {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    std::string label;
    if (check(lexer::TokenKind::Identifier)) {
        label = std::string(advance().lexeme);
    }
    const auto& semiTok = expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after continue");
    diag::SourceSpan span{startLoc, semiTok.span.end};

    return std::make_unique<ast::ContinueStmt>(span, std::move(label));
}

std::unique_ptr<ast::Stmt> Parser::parseZonkStatement() {
    diag::SourceLocation startLoc = lastConsumed_.span.start;
    auto errExpr = parseExpression();
    const auto& semiTok = expect(lexer::TokenKind::Semicolon, diag::ErrorCode::ExpectedSemicolon, "expected ';' after zonk statement");
    diag::SourceSpan span{startLoc, semiTok.span.end};

    return std::make_unique<ast::ZonkStmt>(span, std::move(errExpr));
}

std::unique_ptr<ast::TypeAnnotation> Parser::parseTypeAnnotation() {
    lexer_.setMode(lexer::LexerMode::TypeContext);
    diag::SourceLocation startLoc = peek().span.start;

    std::unique_ptr<ast::TypeAnnotation> baseType = nullptr;

    if (match(lexer::TokenKind::KwInt)) {
        baseType = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Int);
    } else if (match(lexer::TokenKind::KwFloat)) {
        baseType = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Float);
    } else if (match(lexer::TokenKind::KwBool)) {
        baseType = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Bool);
    } else if (match(lexer::TokenKind::KwString)) {
        baseType = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::String);
    } else if (match(lexer::TokenKind::KwChar)) {
        baseType = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Char);
    } else if (match(lexer::TokenKind::KwVoid)) {
        baseType = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Void);
    } else if (match(lexer::TokenKind::KwNull)) {
        baseType = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Null);
    } else if (match(lexer::TokenKind::KwError)) {
        baseType = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Error);
    } else if (match(lexer::TokenKind::KwArray)) {
        expect(lexer::TokenKind::Less, diag::ErrorCode::UnexpectedToken, "expected '<' after array");
        auto elemT = parseTypeAnnotation();
        expect(lexer::TokenKind::Greater, diag::ErrorCode::UnexpectedToken, "expected '>' after array element type");
        diag::SourceSpan span{startLoc, lastConsumed_.span.end};
        baseType = std::make_unique<ast::ArrayType>(span, std::move(elemT));
    } else if (match(lexer::TokenKind::KwMap)) {
        expect(lexer::TokenKind::Less, diag::ErrorCode::UnexpectedToken, "expected '<' after map");
        auto keyT = parseTypeAnnotation();
        expect(lexer::TokenKind::Comma, diag::ErrorCode::UnexpectedToken, "expected ',' between map key and value types");
        auto valT = parseTypeAnnotation();
        expect(lexer::TokenKind::Greater, diag::ErrorCode::UnexpectedToken, "expected '>' after map value type");
        diag::SourceSpan span{startLoc, lastConsumed_.span.end};
        baseType = std::make_unique<ast::MapType>(span, std::move(keyT), std::move(valT));
    } else if (check(lexer::TokenKind::Identifier)) {
        const auto& nameTok = advance();
        diag::SourceSpan span = nameTok.span;
        baseType = std::make_unique<ast::CustomType>(span, std::string(nameTok.lexeme));
    } else {
        expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedTypeAnnotation, "expected type name");
        lexer_.setMode(lexer::LexerMode::Normal);
        return nullptr;
    }

    // Check for union type: `T1 | T2 | T3`
    if (check(lexer::TokenKind::Pipe)) {
        std::vector<std::unique_ptr<ast::TypeAnnotation>> variants;
        variants.push_back(std::move(baseType));
        while (match(lexer::TokenKind::Pipe)) {
            // Parse next non-union type
            std::unique_ptr<ast::TypeAnnotation> nextVariant = nullptr;

            if (match(lexer::TokenKind::KwInt)) {
                nextVariant = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Int);
            } else if (match(lexer::TokenKind::KwFloat)) {
                nextVariant = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Float);
            } else if (match(lexer::TokenKind::KwBool)) {
                nextVariant = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Bool);
            } else if (match(lexer::TokenKind::KwString)) {
                nextVariant = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::String);
            } else if (match(lexer::TokenKind::KwChar)) {
                nextVariant = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Char);
            } else if (match(lexer::TokenKind::KwVoid)) {
                nextVariant = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Void);
            } else if (match(lexer::TokenKind::KwNull)) {
                nextVariant = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Null);
            } else if (match(lexer::TokenKind::KwError)) {
                nextVariant = std::make_unique<ast::PrimitiveType>(lastConsumed_.span, ast::PrimitiveTypeKind::Error);
            } else if (check(lexer::TokenKind::Identifier)) {
                const auto& nTok = advance();
                nextVariant = std::make_unique<ast::CustomType>(nTok.span, std::string(nTok.lexeme));
            }

            if (nextVariant) {
                variants.push_back(std::move(nextVariant));
            } else {
                break;
            }
        }

        diag::SourceSpan unionSpan{startLoc, variants.back()->getSpan().end};
        lexer_.setMode(lexer::LexerMode::Normal);
        return std::make_unique<ast::UnionType>(unionSpan, std::move(variants));
    }

    lexer_.setMode(lexer::LexerMode::Normal);
    return baseType;
}

std::unique_ptr<ast::Expr> Parser::parseExpression(int minPrecedence) {
    (void)minPrecedence;
    diag::SourceLocation startLoc = peek().span.start;

    // Basic primary expression parsing (Pratt precedence climbing in Task 2.3.2)
    if (match(lexer::TokenKind::IntLiteral)) {
        return std::make_unique<ast::LiteralExpr>(lastConsumed_.span, lexer::TokenKind::IntLiteral, std::string(lastConsumed_.lexeme));
    }
    if (match(lexer::TokenKind::FloatLiteral)) {
        return std::make_unique<ast::LiteralExpr>(lastConsumed_.span, lexer::TokenKind::FloatLiteral, std::string(lastConsumed_.lexeme));
    }
    if (match(lexer::TokenKind::StringLiteral)) {
        return std::make_unique<ast::LiteralExpr>(lastConsumed_.span, lexer::TokenKind::StringLiteral, std::string(lastConsumed_.lexeme));
    }
    if (match(lexer::TokenKind::KwTrue) || match(lexer::TokenKind::KwFalse)) {
        return std::make_unique<ast::LiteralExpr>(lastConsumed_.span, lastConsumed_.kind, std::string(lastConsumed_.lexeme));
    }
    if (match(lexer::TokenKind::KwNull)) {
        return std::make_unique<ast::LiteralExpr>(lastConsumed_.span, lexer::TokenKind::KwNull, "null");
    }
    if (match(lexer::TokenKind::Identifier)) {
        auto id = std::make_unique<ast::IdentExpr>(lastConsumed_.span, std::string(lastConsumed_.lexeme));
        if (match(lexer::TokenKind::PlusPlus)) {
            return std::make_unique<ast::UnaryExpr>(diag::SourceSpan{startLoc, lastConsumed_.span.end},
                                                   lexer::TokenKind::PlusPlus, std::move(id), false);
        }
        return id;
    }

    if (match(lexer::TokenKind::PlusPlus)) {
        const auto& target = expect(lexer::TokenKind::Identifier, diag::ErrorCode::ExpectedIdentifier, "expected identifier");
        auto id = std::make_unique<ast::IdentExpr>(target.span, std::string(target.lexeme));
        return std::make_unique<ast::UnaryExpr>(diag::SourceSpan{startLoc, target.span.end},
                                               lexer::TokenKind::PlusPlus, std::move(id), true);
    }

    advance();
    return nullptr;
}

} // namespace keety::parser
