#pragma once

#include <keety/diag/source_location.hpp>

namespace keety::ast {

/// Base class for all Abstract Syntax Tree nodes.
class AstNode {
public:
    explicit AstNode(diag::SourceSpan span) : span_(span) {}
    virtual ~AstNode() = default;

    AstNode(const AstNode&) = default;
    AstNode& operator=(const AstNode&) = default;
    AstNode(AstNode&&) noexcept = default;
    AstNode& operator=(AstNode&&) noexcept = default;

    [[nodiscard]] diag::SourceSpan getSpan() const noexcept { return span_; }
    void setSpan(diag::SourceSpan span) noexcept { span_ = span; }

private:
    diag::SourceSpan span_;
};

} // namespace keety::ast
