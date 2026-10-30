#pragma once

#include <keety/ast/base.hpp>
#include <keety/ast/decl.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace keety::ast {

/// Block statement: `{ stmt1; stmt2; }`
class BlockStmt : public Stmt {
public:
    BlockStmt(diag::SourceSpan span, std::vector<std::unique_ptr<Stmt>> statements)
        : Stmt(span), statements_(std::move(statements)) {}

    [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& getStatements() const noexcept {
        return statements_;
    }

private:
    std::vector<std::unique_ptr<Stmt>> statements_;
};

/// Expression statement: `foo();` or `x = y;`
class ExprStmt : public Stmt {
public:
    ExprStmt(diag::SourceSpan span, std::unique_ptr<Expr> expression)
        : Stmt(span), expression_(std::move(expression)) {}

    [[nodiscard]] const Expr* getExpression() const noexcept { return expression_.get(); }

private:
    std::unique_ptr<Expr> expression_;
};

/// Declaration statement wrapping a local declaration: `let x: int = 10;`
class DeclStmt : public Stmt {
public:
    DeclStmt(diag::SourceSpan span, std::unique_ptr<Decl> declaration)
        : Stmt(span), declaration_(std::move(declaration)) {}

    [[nodiscard]] const Decl* getDeclaration() const noexcept { return declaration_.get(); }

private:
    std::unique_ptr<Decl> declaration_;
};

/// Conditional statement: `if ( [init;] cond ) thenBranch elif (cond2) ... else ...`
class IfStmt : public Stmt {
public:
    IfStmt(diag::SourceSpan span, std::unique_ptr<Stmt> init, std::unique_ptr<Expr> condition,
           std::unique_ptr<BlockStmt> thenBranch, std::unique_ptr<Stmt> elseBranch = nullptr)
        : Stmt(span), init_(std::move(init)), condition_(std::move(condition)),
          thenBranch_(std::move(thenBranch)), elseBranch_(std::move(elseBranch)) {}

    [[nodiscard]] const Stmt* getInit() const noexcept { return init_.get(); }
    [[nodiscard]] bool hasInit() const noexcept { return init_ != nullptr; }
    [[nodiscard]] const Expr* getCondition() const noexcept { return condition_.get(); }
    [[nodiscard]] const BlockStmt* getThenBranch() const noexcept { return thenBranch_.get(); }
    [[nodiscard]] const Stmt* getElseBranch() const noexcept { return elseBranch_.get(); }
    [[nodiscard]] bool hasElseBranch() const noexcept { return elseBranch_ != nullptr; }

private:
    std::unique_ptr<Stmt> init_;
    std::unique_ptr<Expr> condition_;
    std::unique_ptr<BlockStmt> thenBranch_;
    std::unique_ptr<Stmt> elseBranch_; // Can be another IfStmt (elif) or BlockStmt (else)
};

/// While loop: `while ( [init;] cond ) [as label] body`
class WhileStmt : public Stmt {
public:
    WhileStmt(diag::SourceSpan span, std::unique_ptr<Stmt> init, std::unique_ptr<Expr> condition,
              std::unique_ptr<BlockStmt> body, std::string label = "")
        : Stmt(span), init_(std::move(init)), condition_(std::move(condition)),
          body_(std::move(body)), label_(std::move(label)) {}

    [[nodiscard]] const Stmt* getInit() const noexcept { return init_.get(); }
    [[nodiscard]] bool hasInit() const noexcept { return init_ != nullptr; }
    [[nodiscard]] const Expr* getCondition() const noexcept { return condition_.get(); }
    [[nodiscard]] const BlockStmt* getBody() const noexcept { return body_.get(); }
    [[nodiscard]] const std::string& getLabel() const noexcept { return label_; }
    [[nodiscard]] bool hasLabel() const noexcept { return !label_.empty(); }

private:
    std::unique_ptr<Stmt> init_;
    std::unique_ptr<Expr> condition_;
    std::unique_ptr<BlockStmt> body_;
    std::string label_;
};

/// Classic 3-part for loop: `for (init; cond; post) [as label] body`
class ForStmt : public Stmt {
public:
    ForStmt(diag::SourceSpan span, std::unique_ptr<Stmt> init, std::unique_ptr<Expr> condition,
            std::unique_ptr<Stmt> post, std::unique_ptr<BlockStmt> body, std::string label = "")
        : Stmt(span), init_(std::move(init)), condition_(std::move(condition)),
          post_(std::move(post)), body_(std::move(body)), label_(std::move(label)) {}

    [[nodiscard]] const Stmt* getInit() const noexcept { return init_.get(); }
    [[nodiscard]] const Expr* getCondition() const noexcept { return condition_.get(); }
    [[nodiscard]] const Stmt* getPost() const noexcept { return post_.get(); }
    [[nodiscard]] const BlockStmt* getBody() const noexcept { return body_.get(); }
    [[nodiscard]] const std::string& getLabel() const noexcept { return label_; }
    [[nodiscard]] bool hasLabel() const noexcept { return !label_.empty(); }

private:
    std::unique_ptr<Stmt> init_;
    std::unique_ptr<Expr> condition_;
    std::unique_ptr<Stmt> post_;
    std::unique_ptr<BlockStmt> body_;
    std::string label_;
};

/// For-in collection loop: `for (const/let item in collection) [as label] body`
/// or key-value iteration: `for (const k, v in map) ...`
class ForInStmt : public Stmt {
public:
    ForInStmt(diag::SourceSpan span, bool isConst, std::string keyName, std::string valueName,
              std::unique_ptr<Expr> collection, std::unique_ptr<BlockStmt> body,
              std::string label = "")
        : Stmt(span), isConst_(isConst), keyName_(std::move(keyName)),
          valueName_(std::move(valueName)), collection_(std::move(collection)),
          body_(std::move(body)), label_(std::move(label)) {}

    [[nodiscard]] bool isConst() const noexcept { return isConst_; }
    [[nodiscard]] const std::string& getKeyName() const noexcept { return keyName_; }
    [[nodiscard]] const std::string& getValueName() const noexcept { return valueName_; }
    [[nodiscard]] bool isKeyValue() const noexcept { return !keyName_.empty(); }
    [[nodiscard]] const Expr* getCollection() const noexcept { return collection_.get(); }
    [[nodiscard]] const BlockStmt* getBody() const noexcept { return body_.get(); }
    [[nodiscard]] const std::string& getLabel() const noexcept { return label_; }
    [[nodiscard]] bool hasLabel() const noexcept { return !label_.empty(); }

private:
    bool isConst_{true};
    std::string keyName_;   // Empty if single item iterator
    std::string valueName_; // Item or value variable name
    std::unique_ptr<Expr> collection_;
    std::unique_ptr<BlockStmt> body_;
    std::string label_;
};

/// Switch case branch: `case (val) { ... }` or `default { ... }`
struct SwitchCase {
    diag::SourceSpan span{};
    std::vector<std::unique_ptr<Expr>> values; // Empty if default branch
    std::unique_ptr<BlockStmt> body;

    [[nodiscard]] bool isDefault() const noexcept { return values.empty(); }
};

/// Switch pattern matching statement: `switch ( [init;] expr ) [as label] { case ... }`
class SwitchStmt : public Stmt {
public:
    SwitchStmt(diag::SourceSpan span, std::unique_ptr<Stmt> init, std::unique_ptr<Expr> discriminant,
               std::vector<SwitchCase> cases, std::string label = "")
        : Stmt(span), init_(std::move(init)), discriminant_(std::move(discriminant)),
          cases_(std::move(cases)), label_(std::move(label)) {}

    [[nodiscard]] const Stmt* getInit() const noexcept { return init_.get(); }
    [[nodiscard]] bool hasInit() const noexcept { return init_ != nullptr; }
    [[nodiscard]] const Expr* getDiscriminant() const noexcept { return discriminant_.get(); }
    [[nodiscard]] const std::vector<SwitchCase>& getCases() const noexcept { return cases_; }
    [[nodiscard]] const std::string& getLabel() const noexcept { return label_; }
    [[nodiscard]] bool hasLabel() const noexcept { return !label_.empty(); }

private:
    std::unique_ptr<Stmt> init_;
    std::unique_ptr<Expr> discriminant_;
    std::vector<SwitchCase> cases_;
    std::string label_;
};

/// Return statement: `return [expr];`
class ReturnStmt : public Stmt {
public:
    ReturnStmt(diag::SourceSpan span, std::unique_ptr<Expr> value = nullptr)
        : Stmt(span), value_(std::move(value)) {}

    [[nodiscard]] const Expr* getValue() const noexcept { return value_.get(); }
    [[nodiscard]] bool hasValue() const noexcept { return value_ != nullptr; }

private:
    std::unique_ptr<Expr> value_;
};

/// Yield statement used in scoped zonk blocks: `yield expr;`
class YieldStmt : public Stmt {
public:
    YieldStmt(diag::SourceSpan span, std::unique_ptr<Expr> value)
        : Stmt(span), value_(std::move(value)) {}

    [[nodiscard]] const Expr* getValue() const noexcept { return value_.get(); }

private:
    std::unique_ptr<Expr> value_;
};

/// Break statement: `break [targetLabel];`
class BreakStmt : public Stmt {
public:
    BreakStmt(diag::SourceSpan span, std::string targetLabel = "")
        : Stmt(span), targetLabel_(std::move(targetLabel)) {}

    [[nodiscard]] const std::string& getTargetLabel() const noexcept { return targetLabel_; }
    [[nodiscard]] bool hasTargetLabel() const noexcept { return !targetLabel_.empty(); }

private:
    std::string targetLabel_;
};

/// Continue statement: `continue [targetLabel];`
class ContinueStmt : public Stmt {
public:
    ContinueStmt(diag::SourceSpan span, std::string targetLabel = "")
        : Stmt(span), targetLabel_(std::move(targetLabel)) {}

    [[nodiscard]] const std::string& getTargetLabel() const noexcept { return targetLabel_; }
    [[nodiscard]] bool hasTargetLabel() const noexcept { return !targetLabel_.empty(); }

private:
    std::string targetLabel_;
};

/// Zonk statement raising an error: `zonk err;` or `zonk "message";`
class ZonkStmt : public Stmt {
public:
    ZonkStmt(diag::SourceSpan span, std::unique_ptr<Expr> errorExpr)
        : Stmt(span), errorExpr_(std::move(errorExpr)) {}

    [[nodiscard]] const Expr* getErrorExpr() const noexcept { return errorExpr_.get(); }

private:
    std::unique_ptr<Expr> errorExpr_;
};

} // namespace keety::ast
