#pragma once

#include <keety/ast/ast_node.hpp>

namespace keety::ast {

/// Base class for all Statement AST nodes.
class Stmt : public AstNode {
public:
    using AstNode::AstNode;
};

/// Base class for all Expression AST nodes.
class Expr : public AstNode {
public:
    using AstNode::AstNode;
};

/// Base class for all Type Annotation AST nodes.
class TypeAnnotation : public AstNode {
public:
    using AstNode::AstNode;
};

} // namespace keety::ast
