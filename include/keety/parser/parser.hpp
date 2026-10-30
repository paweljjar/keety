#pragma once

#include <keety/ast/base.hpp>
#include <keety/ast/decl.hpp>
#include <keety/ast/expr.hpp>
#include <keety/ast/stmt.hpp>
#include <keety/ast/type.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/lexer/token.hpp>

#include <memory>
#include <string>
#include <vector>

namespace keety::parser {

/// Represents a parsed compilation unit (file module)
struct ModuleAST {
    diag::FileId fileId{diag::INVALID_FILE_ID};
    std::string moduleName;
    std::vector<std::unique_ptr<ast::Decl>> declarations;
};

/// Deterministic Top-Down Recursive Descent Parser with bounded LL(k) lookahead.
/// Conforms to Keety Compiler Architecture Specification (Stage 2).
class Parser {
public:
    Parser(lexer::Lexer& lexer, diag::DiagnosticEmitter* emitter = nullptr);

    /// Parses the entire compilation unit into a ModuleAST.
    ModuleAST parseModule(std::string moduleName = "main");

    // --- Core parsing entry points ---
    std::unique_ptr<ast::Decl> parseDeclaration();
    std::unique_ptr<ast::Stmt> parseStatement();
    std::unique_ptr<ast::BlockStmt> parseBlockStatement();
    std::unique_ptr<ast::TypeAnnotation> parseTypeAnnotation();
    std::unique_ptr<ast::Expr> parseExpression(int minPrecedence = 0);

private:
    // --- Disambiguation helpers ---
    bool hasTopLevelSemicolonInParens();

    // --- Pratt Expression Parsing ---
    std::unique_ptr<ast::Expr> parsePrimaryExpression();
    std::unique_ptr<ast::Expr> parsePrefixExpression();
    static int getBinaryPrecedence(lexer::TokenKind kind) noexcept;
    static bool isRightAssociative(lexer::TokenKind kind) noexcept;

    // --- LL(k) lookahead buffer management ---
    const lexer::Token& peek(size_t k = 0);
    const lexer::Token& advance();
    bool check(lexer::TokenKind kind, size_t k = 0);
    bool match(lexer::TokenKind kind);
    const lexer::Token& expect(lexer::TokenKind kind, diag::ErrorCode errCode, std::string_view msg);
    void synchronize();

    // --- Declarations ---
    std::unique_ptr<ast::VarDecl> parseVarDecl(bool isExported);
    std::unique_ptr<ast::ConstDecl> parseConstDecl(bool isExported);
    std::unique_ptr<ast::DefineDecl> parseDefineDecl(bool isExported);
    std::unique_ptr<ast::FnDecl> parseFnDecl(bool isAsync, bool isExported);
    std::unique_ptr<ast::ClassDecl> parseClassDecl(bool isExported);
    std::unique_ptr<ast::EnumDecl> parseEnumDecl(bool isExported);
    std::unique_ptr<ast::ErrTypeDecl> parseErrTypeDecl(bool isExported);
    std::unique_ptr<ast::ImportDecl> parseImportDecl(bool isReExported);
    std::unique_ptr<ast::ExportDecl> parseExportDecl();

    // --- Statements ---
    std::unique_ptr<ast::Stmt> parseIfStatement();
    std::unique_ptr<ast::Stmt> parseWhileStatement();
    std::unique_ptr<ast::Stmt> parseForStatement();
    std::unique_ptr<ast::Stmt> parseSwitchStatement();
    std::unique_ptr<ast::Stmt> parseReturnStatement();
    std::unique_ptr<ast::Stmt> parseYieldStatement();
    std::unique_ptr<ast::Stmt> parseBreakStatement();
    std::unique_ptr<ast::Stmt> parseContinueStatement();
    std::unique_ptr<ast::Stmt> parseZonkStatement();

    // --- Helpers ---
    std::vector<ast::Param> parseParamList();
    ast::Param parseParam();

    lexer::Lexer& lexer_;
    diag::DiagnosticEmitter* emitter_{nullptr};

    std::vector<lexer::Token> lookahead_; // Ring/bounded buffer for LL(k)
    lexer::Token lastConsumed_{};
    bool panicMode_{false};
};

} // namespace keety::parser
