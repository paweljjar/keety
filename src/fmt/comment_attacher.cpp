#include <keety/fmt/comment_attacher.hpp>

#include <keety/ast/decl.hpp>
#include <keety/ast/stmt.hpp>

#include <algorithm>

namespace keety::fmt {

namespace {

struct TargetNode {
    const void* key{nullptr};
    diag::SourceSpan span{};
    bool isContainer{false};
    bool isEmptyContainer{false};
    const void* aliasKey{nullptr};
    std::vector<TargetNode> children{};
};

TargetNode collectFnDecl(const ast::FnDecl* fn);
TargetNode collectStmt(const ast::Stmt* stmt);

TargetNode collectFnDecl(const ast::FnDecl* fn) {
    TargetNode node{fn, fn->getSpan(), false, false, nullptr, {}};
    if (const auto* body = fn->getBody()) {
        node.children.push_back(collectStmt(body));
    }
    return node;
}

TargetNode collectStmt(const ast::Stmt* stmt) {
    if (!stmt) return {};

    if (const auto* block = dynamic_cast<const ast::BlockStmt*>(stmt)) {
        bool empty = block->getStatements().empty();
        TargetNode node{block, block->getSpan(), true, empty, nullptr, {}};
        for (const auto& s : block->getStatements()) {
            node.children.push_back(collectStmt(s.get()));
        }
        return node;
    }

    if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(stmt)) {
        TargetNode node{ifStmt, ifStmt->getSpan(), false, false, nullptr, {}};
        if (ifStmt->hasInit()) {
            node.children.push_back(collectStmt(ifStmt->getInit()));
        }
        if (ifStmt->getThenBranch()) {
            node.children.push_back(collectStmt(ifStmt->getThenBranch()));
        }
        if (ifStmt->hasElseBranch()) {
            node.children.push_back(collectStmt(ifStmt->getElseBranch()));
        }
        return node;
    }

    if (const auto* whileStmt = dynamic_cast<const ast::WhileStmt*>(stmt)) {
        TargetNode node{whileStmt, whileStmt->getSpan(), false, false, nullptr, {}};
        if (whileStmt->hasInit()) {
            node.children.push_back(collectStmt(whileStmt->getInit()));
        }
        if (whileStmt->getBody()) {
            node.children.push_back(collectStmt(whileStmt->getBody()));
        }
        return node;
    }

    if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(stmt)) {
        TargetNode node{forStmt, forStmt->getSpan(), false, false, nullptr, {}};
        if (forStmt->getInit()) {
            node.children.push_back(collectStmt(forStmt->getInit()));
        }
        if (forStmt->getPost()) {
            node.children.push_back(collectStmt(forStmt->getPost()));
        }
        if (forStmt->getBody()) {
            node.children.push_back(collectStmt(forStmt->getBody()));
        }
        return node;
    }

    if (const auto* forIn = dynamic_cast<const ast::ForInStmt*>(stmt)) {
        TargetNode node{forIn, forIn->getSpan(), false, false, nullptr, {}};
        if (forIn->getBody()) {
            node.children.push_back(collectStmt(forIn->getBody()));
        }
        return node;
    }

    if (const auto* sw = dynamic_cast<const ast::SwitchStmt*>(stmt)) {
        TargetNode node{sw, sw->getSpan(), true, sw->getCases().empty(), nullptr, {}};
        if (sw->hasInit()) {
            node.children.push_back(collectStmt(sw->getInit()));
        }
        for (const auto& c : sw->getCases()) {
            bool caseEmpty = !c.body || c.body->getStatements().empty();
            TargetNode caseNode{&c, c.span, true, caseEmpty, nullptr, {}};
            if (c.body) {
                for (const auto& s : c.body->getStatements()) {
                    caseNode.children.push_back(collectStmt(s.get()));
                }
            }
            node.children.push_back(std::move(caseNode));
        }
        return node;
    }

    if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(stmt)) {
        return TargetNode{declStmt, declStmt->getSpan(), false, false, declStmt->getDeclaration(), {}};
    }

    // Default leaf statement (ExprStmt, ReturnStmt, BreakStmt, ContinueStmt, YieldStmt)
    return TargetNode{stmt, stmt->getSpan(), false, false, nullptr, {}};
}

TargetNode collectDecl(const ast::Decl* decl) {
    if (!decl) return {};

    if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl)) {
        return collectFnDecl(fn);
    }

    if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl)) {
        bool empty = cls->getFields().empty() && cls->getMethods().empty();
        TargetNode node{cls, cls->getSpan(), true, empty, nullptr, {}};
        for (const auto& field : cls->getFields()) {
            node.children.push_back(TargetNode{&field, field.span, false, false, nullptr, {}});
        }
        for (const auto& method : cls->getMethods()) {
            node.children.push_back(collectFnDecl(method.get()));
        }
        std::sort(node.children.begin(), node.children.end(), [](const TargetNode& a, const TargetNode& b) {
            return a.span.start.offset < b.span.start.offset;
        });
        return node;
    }

    if (const auto* enm = dynamic_cast<const ast::EnumDecl*>(decl)) {
        bool empty = enm->getMembers().empty();
        TargetNode node{enm, enm->getSpan(), true, empty, nullptr, {}};
        for (const auto& member : enm->getMembers()) {
            node.children.push_back(TargetNode{&member, member.span, false, false, nullptr, {}});
        }
        return node;
    }

    if (const auto* err = dynamic_cast<const ast::ErrTypeDecl*>(decl)) {
        bool empty = err->getFields().empty();
        TargetNode node{err, err->getSpan(), true, empty, nullptr, {}};
        for (const auto& field : err->getFields()) {
            node.children.push_back(TargetNode{&field, field.span, false, false, nullptr, {}});
        }
        return node;
    }

    // Leaf declarations (VarDecl, ConstDecl, DefineDecl, ImportDecl, ExportDecl, TypeAliasDecl)
    return TargetNode{decl, decl->getSpan(), false, false, nullptr, {}};
}

const TargetNode* findTrailingCandidate(const TargetNode& node, uint32_t line, uint32_t commentStartOffset) {
    const TargetNode* best = nullptr;

    if (node.span.isValid() && node.span.end.line == line && node.span.end.offset <= commentStartOffset) {
        best = &node;
    }

    for (const auto& child : node.children) {
        const TargetNode* childBest = findTrailingCandidate(child, line, commentStartOffset);
        if (childBest) {
            if (!best || childBest->span.end.offset >= best->span.end.offset) {
                best = childBest;
            }
        }
    }

    return best;
}

void attachRecursive(
    CommentMap& result,
    const TargetNode& container,
    const std::vector<Comment>& comments,
    bool isRoot
) {
    if (comments.empty()) return;

    if (container.children.empty()) {
        for (const auto& c : comments) {
            if (isRoot) {
                result.addFileHeader(c);
            } else {
                result.addDangling(container.key, c);
            }
        }
        return;
    }

    for (const auto& c : comments) {
        // 1. Check if trailing on any child
        const TargetNode* trailingTarget = nullptr;
        for (const auto& child : container.children) {
            const TargetNode* cand = findTrailingCandidate(child, c.span.start.line, c.span.start.offset);
            if (cand) {
                if (!trailingTarget || cand->span.end.offset >= trailingTarget->span.end.offset) {
                    trailingTarget = cand;
                }
            }
        }

        if (trailingTarget) {
            result.addTrailing(trailingTarget->key, c);
            if (trailingTarget->aliasKey) {
                result.addTrailingAlias(trailingTarget->aliasKey, c);
            }
            continue;
        }

        // 2. Check if strictly inside a child's span
        bool attachedInside = false;
        for (const auto& child : container.children) {
            if (child.span.isValid() &&
                child.span.start.offset <= c.span.start.offset &&
                c.span.end.offset <= child.span.end.offset) {
                attachRecursive(result, child, {c}, false);
                attachedInside = true;
                break;
            }
        }

        if (attachedInside) {
            continue;
        }

        // 3. Find first child starting after this comment
        const TargetNode* nextChild = nullptr;
        size_t nextChildIndex = 0;
        for (size_t i = 0; i < container.children.size(); ++i) {
            if (container.children[i].span.isValid() &&
                container.children[i].span.start.offset >= c.span.end.offset) {
                nextChild = &container.children[i];
                nextChildIndex = i;
                break;
            }
        }

        if (nextChild) {
            if (isRoot && nextChildIndex == 0) {
                // Before the first declaration in file:
                // If separated by a blank line before the declaration -> FileHeader
                bool hasBlankLineBeforeDecl = (nextChild->span.start.line > c.span.end.line + 1);
                if (hasBlankLineBeforeDecl) {
                    result.addFileHeader(c);
                } else {
                    result.addLeading(nextChild->key, c);
                    if (nextChild->aliasKey) {
                        result.addLeadingAlias(nextChild->aliasKey, c);
                    }
                }
            } else {
                result.addLeading(nextChild->key, c);
                if (nextChild->aliasKey) {
                    result.addLeadingAlias(nextChild->aliasKey, c);
                }
            }
        } else {
            // After all children
            if (isRoot) {
                result.addFileFooter(c);
            } else {
                result.addDangling(container.key, c);
            }
        }
    }
}

} // namespace

CommentMap CommentAttacher::attach(
    const parser::ModuleAST& moduleAst,
    const std::vector<lexer::Comment>& comments
) {
    CommentMap result;
    if (comments.empty()) {
        return result;
    }

    TargetNode root{nullptr, {}, true, moduleAst.declarations.empty(), nullptr, {}};
    for (const auto& decl : moduleAst.declarations) {
        root.children.push_back(collectDecl(decl.get()));
    }

    std::sort(root.children.begin(), root.children.end(), [](const TargetNode& a, const TargetNode& b) {
        return a.span.start.offset < b.span.start.offset;
    });

    attachRecursive(result, root, comments, true);
    return result;
}

} // namespace keety::fmt
