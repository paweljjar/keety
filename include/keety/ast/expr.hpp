#pragma once

#include <keety/ast/base.hpp>
#include <keety/ast/decl.hpp>
#include <keety/ast/stmt.hpp>
#include <keety/lexer/token.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace keety::ast {

/// Literal expression: integer, float, string, char, bool, or null
class LiteralExpr : public Expr {
public:
    LiteralExpr(diag::SourceSpan span, lexer::TokenKind literalKind, std::string rawText)
        : Expr(span), literalKind_(literalKind), rawText_(std::move(rawText)) {}

    [[nodiscard]] lexer::TokenKind getLiteralKind() const noexcept { return literalKind_; }
    [[nodiscard]] const std::string& getRawText() const noexcept { return rawText_; }

private:
    lexer::TokenKind literalKind_;
    std::string rawText_;
};

/// Identifier or variable reference: `count`, `_foo`
class IdentExpr : public Expr {
public:
    IdentExpr(diag::SourceSpan span, std::string name)
        : Expr(span), name_(std::move(name)) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

private:
    std::string name_;
};

/// Binary operator expression: `a + b`, `x == y`, `a && b`, `x = y`
class BinaryExpr : public Expr {
public:
    BinaryExpr(diag::SourceSpan span, lexer::TokenKind op,
               std::unique_ptr<Expr> left, std::unique_ptr<Expr> right)
        : Expr(span), op_(op), left_(std::move(left)), right_(std::move(right)) {}

    [[nodiscard]] lexer::TokenKind getOp() const noexcept { return op_; }
    [[nodiscard]] const Expr* getLeft() const noexcept { return left_.get(); }
    [[nodiscard]] const Expr* getRight() const noexcept { return right_.get(); }

private:
    lexer::TokenKind op_;
    std::unique_ptr<Expr> left_;
    std::unique_ptr<Expr> right_;
};

/// Unary operator expression: `-x`, `!flag`
class UnaryExpr : public Expr {
public:
    UnaryExpr(diag::SourceSpan span, lexer::TokenKind op, std::unique_ptr<Expr> operand, bool isPrefix = true)
        : Expr(span), op_(op), operand_(std::move(operand)), isPrefix_(isPrefix) {}

    [[nodiscard]] lexer::TokenKind getOp() const noexcept { return op_; }
    [[nodiscard]] const Expr* getOperand() const noexcept { return operand_.get(); }
    [[nodiscard]] bool isPrefix() const noexcept { return isPrefix_; }

private:
    lexer::TokenKind op_;
    std::unique_ptr<Expr> operand_;
    bool isPrefix_{true};
};

/// Call expression: `calculate(x, y)` or `obj.method(z)`
/// Supports `mut` arguments: `swap(mut a, mut b)`
struct CallArg {
    diag::SourceSpan span{};
    std::unique_ptr<Expr> value;
    bool isMut{false};
};

class CallExpr : public Expr {
public:
    CallExpr(diag::SourceSpan span, std::unique_ptr<Expr> callee,
             std::vector<CallArg> args, std::vector<std::unique_ptr<TypeAnnotation>> typeArgs = {})
        : Expr(span), callee_(std::move(callee)), args_(std::move(args)), typeArgs_(std::move(typeArgs)) {}

    [[nodiscard]] const Expr* getCallee() const noexcept { return callee_.get(); }
    [[nodiscard]] const std::vector<CallArg>& getArgs() const noexcept { return args_; }
    [[nodiscard]] const std::vector<std::unique_ptr<TypeAnnotation>>& getTypeArgs() const noexcept { return typeArgs_; }

private:
    std::unique_ptr<Expr> callee_;
    std::vector<CallArg> args_;
    std::vector<std::unique_ptr<TypeAnnotation>> typeArgs_;
};

/// Index expression: `arr[0]` or `map["key"]`
class IndexExpr : public Expr {
public:
    IndexExpr(diag::SourceSpan span, std::unique_ptr<Expr> container, std::unique_ptr<Expr> index)
        : Expr(span), container_(std::move(container)), index_(std::move(index)) {}

    [[nodiscard]] const Expr* getContainer() const noexcept { return container_.get(); }
    [[nodiscard]] const Expr* getIndex() const noexcept { return index_.get(); }

private:
    std::unique_ptr<Expr> container_;
    std::unique_ptr<Expr> index_;
};

/// Member / Field / Method selector expression: `user.name` or `this.id`
class MemberExpr : public Expr {
public:
    MemberExpr(diag::SourceSpan span, std::unique_ptr<Expr> object, std::string memberName)
        : Expr(span), object_(std::move(object)), memberName_(std::move(memberName)) {}

    [[nodiscard]] const Expr* getObject() const noexcept { return object_.get(); }
    [[nodiscard]] const std::string& getMemberName() const noexcept { return memberName_; }

private:
    std::unique_ptr<Expr> object_;
    std::string memberName_;
};

/// Error recovery expression:
/// 1. Fallback value form: `expr zonk fallbackExpr`
/// 2. Scoped recovery block form: `expr zonk ( [err] ) { yield fallback; }`
class ZonkExpr : public Expr {
public:
    ZonkExpr(diag::SourceSpan span, std::unique_ptr<Expr> primary,
             std::unique_ptr<Expr> fallback, std::string errorVarName = "",
             std::unique_ptr<BlockStmt> recoveryBlock = nullptr)
        : Expr(span), primary_(std::move(primary)), fallback_(std::move(fallback)),
          errorVarName_(std::move(errorVarName)), recoveryBlock_(std::move(recoveryBlock)) {}

    [[nodiscard]] const Expr* getPrimary() const noexcept { return primary_.get(); }
    [[nodiscard]] const Expr* getFallback() const noexcept { return fallback_.get(); }
    [[nodiscard]] bool hasRecoveryBlock() const noexcept { return recoveryBlock_ != nullptr; }
    [[nodiscard]] const BlockStmt* getRecoveryBlock() const noexcept { return recoveryBlock_.get(); }
    [[nodiscard]] const std::string& getErrorVarName() const noexcept { return errorVarName_; }

private:
    std::unique_ptr<Expr> primary_;
    std::unique_ptr<Expr> fallback_; // Present in `expr zonk fallback`
    std::string errorVarName_;
    std::unique_ptr<BlockStmt> recoveryBlock_; // Present in `expr zonk (err) { ... }`
};

/// Ternary conditional expression: `cond ? thenExpr : elseExpr`
class TernaryExpr : public Expr {
public:
    TernaryExpr(diag::SourceSpan span, std::unique_ptr<Expr> condition,
                std::unique_ptr<Expr> thenExpr, std::unique_ptr<Expr> elseExpr)
        : Expr(span), condition_(std::move(condition)),
          thenExpr_(std::move(thenExpr)), elseExpr_(std::move(elseExpr)) {}

    [[nodiscard]] const Expr* getCondition() const noexcept { return condition_.get(); }
    [[nodiscard]] const Expr* getThenExpr() const noexcept { return thenExpr_.get(); }
    [[nodiscard]] const Expr* getElseExpr() const noexcept { return elseExpr_.get(); }

private:
    std::unique_ptr<Expr> condition_;
    std::unique_ptr<Expr> thenExpr_;
    std::unique_ptr<Expr> elseExpr_;
};

/// Type assertion or conversion expression: `val as string` or `int(42.5)`
class TypeCastExpr : public Expr {
public:
    TypeCastExpr(diag::SourceSpan span, std::unique_ptr<Expr> expr,
                 std::unique_ptr<TypeAnnotation> targetType, bool isAssertion = true)
        : Expr(span), expr_(std::move(expr)), targetType_(std::move(targetType)),
          isAssertion_(isAssertion) {}

    [[nodiscard]] const Expr* getExpr() const noexcept { return expr_.get(); }
    [[nodiscard]] const TypeAnnotation* getTargetType() const noexcept { return targetType_.get(); }
    [[nodiscard]] bool isAssertion() const noexcept { return isAssertion_; } // 'as' vs functional cast T(x)

private:
    std::unique_ptr<Expr> expr_;
    std::unique_ptr<TypeAnnotation> targetType_;
    bool isAssertion_{true};
};

/// Type inspection expression: `typeof(x)`
class TypeofExpr : public Expr {
public:
    TypeofExpr(diag::SourceSpan span, std::unique_ptr<Expr> expr)
        : Expr(span), expr_(std::move(expr)) {}

    [[nodiscard]] const Expr* getExpr() const noexcept { return expr_.get(); }

private:
    std::unique_ptr<Expr> expr_;
};

/// Array literal: `[1, 2, 3]`
class ArrayLitExpr : public Expr {
public:
    ArrayLitExpr(diag::SourceSpan span, std::vector<std::unique_ptr<Expr>> elements)
        : Expr(span), elements_(std::move(elements)) {}

    [[nodiscard]] const std::vector<std::unique_ptr<Expr>>& getElements() const noexcept {
        return elements_;
    }

private:
    std::vector<std::unique_ptr<Expr>> elements_;
};

/// Map entry: `"key": value`
struct MapEntry {
    std::unique_ptr<Expr> key;
    std::unique_ptr<Expr> value;
};

/// Map literal: `{"a": 1, "b": 2}`
class MapLitExpr : public Expr {
public:
    MapLitExpr(diag::SourceSpan span, std::vector<MapEntry> entries)
        : Expr(span), entries_(std::move(entries)) {}

    [[nodiscard]] const std::vector<MapEntry>& getEntries() const noexcept {
        return entries_;
    }

private:
    std::vector<MapEntry> entries_;
};

/// Closure / Lambda function literal: `fn (x: int): int { return x * 2; }`
class ClosureExpr : public Expr {
public:
    ClosureExpr(diag::SourceSpan span, std::vector<Param> params,
                std::unique_ptr<TypeAnnotation> returnType, std::unique_ptr<BlockStmt> body,
                bool isAsync = false)
        : Expr(span), params_(std::move(params)), returnType_(std::move(returnType)),
          body_(std::move(body)), isAsync_(isAsync) {}

    [[nodiscard]] const std::vector<Param>& getParams() const noexcept { return params_; }
    [[nodiscard]] const TypeAnnotation* getReturnType() const noexcept { return returnType_.get(); }
    [[nodiscard]] const BlockStmt* getBody() const noexcept { return body_.get(); }
    [[nodiscard]] bool isAsync() const noexcept { return isAsync_; }

private:
    std::vector<Param> params_;
    std::unique_ptr<TypeAnnotation> returnType_;
    std::unique_ptr<BlockStmt> body_;
    bool isAsync_{false};
};

/// Await asynchronous expression: `await fetchUser(id)`
class AwaitExpr : public Expr {
public:
    AwaitExpr(diag::SourceSpan span, std::unique_ptr<Expr> target)
        : Expr(span), target_(std::move(target)) {}

    [[nodiscard]] const Expr* getTarget() const noexcept { return target_.get(); }

private:
    std::unique_ptr<Expr> target_;
};

/// Template literal part: static text chunk or interpolated expression
struct TemplatePart {
    std::string text{};
    std::unique_ptr<Expr> expr{nullptr};
    bool isExpr{false};
};

/// Template string literal expression: `val_${i}` or `hello world`
class TemplateLitExpr : public Expr {
public:
    TemplateLitExpr(diag::SourceSpan span, std::vector<TemplatePart> parts)
        : Expr(span), parts_(std::move(parts)) {}

    [[nodiscard]] const std::vector<TemplatePart>& getParts() const noexcept { return parts_; }

private:
    std::vector<TemplatePart> parts_;
};

} // namespace keety::ast
