#pragma once

#include <keety/fmt/comment.hpp>
#include <keety/parser/parser.hpp>

#include <vector>

namespace keety::fmt {

/// Binds lexical comments to AST nodes with leading, trailing, and dangling placement.
class CommentAttacher {
public:
    /// Traverses the parsed module and maps each comment to:
    /// - An AST node (leading or trailing)
    /// - A container node (dangling inside an empty block/container or at the end of a block)
    /// - File-level header or footer
    [[nodiscard]] static CommentMap attach(
        const parser::ModuleAST& moduleAst,
        const std::vector<lexer::Comment>& comments
    );
};

} // namespace keety::fmt
