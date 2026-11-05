#pragma once

#include <keety/ast/base.hpp>
#include <keety/ast/decl.hpp>
#include <keety/ast/expr.hpp>
#include <keety/ast/stmt.hpp>
#include <keety/ast/type.hpp>

#include <unordered_set>

namespace keety::lint {

/// Utility visitor that traverses an AST node and collects all identifier and type references.
class ReferenceCollector {
public:
    /// Collects all referenced identifier names and module qualifiers within an AST subtree.
    static void collect(const ast::AstNode* node, std::unordered_set<std::string>& outIdentifiers) {
        if (!node) return;
        ReferenceCollector collector(outIdentifiers);
        collector.visit(node);
    }

    /// Collects all referenced identifiers within a statement.
    static void collect(const ast::Stmt* stmt, std::unordered_set<std::string>& outIdentifiers) {
        if (!stmt) return;
        ReferenceCollector collector(outIdentifiers);
        collector.visitStmt(stmt);
    }

    /// Collects all referenced identifiers within an expression.
    static void collect(const ast::Expr* expr, std::unordered_set<std::string>& outIdentifiers) {
        if (!expr) return;
        ReferenceCollector collector(outIdentifiers);
        collector.visitExpr(expr);
    }

    /// Collects all referenced identifiers within a type annotation.
    static void collect(const ast::TypeAnnotation* type, std::unordered_set<std::string>& outIdentifiers) {
        if (!type) return;
        ReferenceCollector collector(outIdentifiers);
        collector.visitType(type);
    }

private:
    explicit ReferenceCollector(std::unordered_set<std::string>& out) : out_(out) {}

    void visit(const ast::AstNode* node) {
        if (const auto* d = dynamic_cast<const ast::Decl*>(node)) visitDecl(d);
        else if (const auto* s = dynamic_cast<const ast::Stmt*>(node)) visitStmt(s);
        else if (const auto* e = dynamic_cast<const ast::Expr*>(node)) visitExpr(e);
        else if (const auto* t = dynamic_cast<const ast::TypeAnnotation*>(node)) visitType(t);
    }

    void visitDecl(const ast::Decl* decl) {
        if (!decl) return;
        if (const auto* var = dynamic_cast<const ast::VarDecl*>(decl)) {
            visitType(var->getType());
            visitExpr(var->getInitializer());
        } else if (const auto* c = dynamic_cast<const ast::ConstDecl*>(decl)) {
            visitType(c->getType());
            visitExpr(c->getInitializer());
        } else if (const auto* def = dynamic_cast<const ast::DefineDecl*>(decl)) {
            visitType(def->getType());
            visitExpr(def->getValue());
        } else if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl)) {
            for (const auto& p : fn->getParams()) {
                visitType(p.type.get());
                visitExpr(p.defaultValue.get());
            }
            visitType(fn->getReturnType());
            visitStmt(fn->getBody());
        } else if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl)) {
            if (!cls->getBaseClass().empty()) {
                out_.insert(cls->getBaseClass());
            }
            for (const auto& f : cls->getFields()) {
                visitType(f.type.get());
                visitExpr(f.initializer.get());
            }
            for (const auto& m : cls->getMethods()) {
                visitDecl(m.get());
            }
        } else if (const auto* en = dynamic_cast<const ast::EnumDecl*>(decl)) {
            for (const auto& member : en->getMembers()) {
                visitExpr(member.value.get());
            }
        } else if (const auto* err = dynamic_cast<const ast::ErrTypeDecl*>(decl)) {
            if (!err->getBaseError().empty()) {
                out_.insert(err->getBaseError());
            }
            for (const auto& f : err->getFields()) {
                visitType(f.type.get());
                visitExpr(f.initializer.get());
            }
        }
    }

    void visitStmt(const ast::Stmt* stmt) {
        if (!stmt) return;
        if (const auto* block = dynamic_cast<const ast::BlockStmt*>(stmt)) {
            for (const auto& s : block->getStatements()) {
                visitStmt(s.get());
            }
        } else if (const auto* exprStmt = dynamic_cast<const ast::ExprStmt*>(stmt)) {
            visitExpr(exprStmt->getExpression());
        } else if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(stmt)) {
            visitDecl(declStmt->getDeclaration());
        } else if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(stmt)) {
            visitStmt(ifStmt->getInit());
            visitExpr(ifStmt->getCondition());
            visitStmt(ifStmt->getThenBranch());
            visitStmt(ifStmt->getElseBranch());
        } else if (const auto* wh = dynamic_cast<const ast::WhileStmt*>(stmt)) {
            visitStmt(wh->getInit());
            visitExpr(wh->getCondition());
            visitStmt(wh->getBody());
        } else if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(stmt)) {
            visitStmt(forStmt->getInit());
            visitExpr(forStmt->getCondition());
            visitStmt(forStmt->getPost());
            visitStmt(forStmt->getBody());
        } else if (const auto* forIn = dynamic_cast<const ast::ForInStmt*>(stmt)) {
            visitExpr(forIn->getCollection());
            visitType(forIn->getKeyType());
            visitType(forIn->getValueType());
            visitStmt(forIn->getBody());
        } else if (const auto* sw = dynamic_cast<const ast::SwitchStmt*>(stmt)) {
            visitStmt(sw->getInit());
            visitExpr(sw->getDiscriminant());
            for (const auto& c : sw->getCases()) {
                for (const auto& val : c.values) {
                    visitExpr(val.get());
                }
                visitStmt(c.body.get());
            }
        } else if (const auto* ret = dynamic_cast<const ast::ReturnStmt*>(stmt)) {
            visitExpr(ret->getValue());
        } else if (const auto* yld = dynamic_cast<const ast::YieldStmt*>(stmt)) {
            visitExpr(yld->getValue());
        } else if (const auto* znk = dynamic_cast<const ast::ZonkStmt*>(stmt)) {
            visitExpr(znk->getErrorExpr());
        }
    }

    void visitExpr(const ast::Expr* expr) {
        if (!expr) return;
        if (const auto* id = dynamic_cast<const ast::IdentExpr*>(expr)) {
            out_.insert(id->getName());
        } else if (const auto* bin = dynamic_cast<const ast::BinaryExpr*>(expr)) {
            visitExpr(bin->getLeft());
            visitExpr(bin->getRight());
        } else if (const auto* un = dynamic_cast<const ast::UnaryExpr*>(expr)) {
            visitExpr(un->getOperand());
        } else if (const auto* call = dynamic_cast<const ast::CallExpr*>(expr)) {
            visitExpr(call->getCallee());
            for (const auto& tArg : call->getTypeArgs()) {
                visitType(tArg.get());
            }
            for (const auto& arg : call->getArgs()) {
                visitExpr(arg.value.get());
            }
        } else if (const auto* idx = dynamic_cast<const ast::IndexExpr*>(expr)) {
            visitExpr(idx->getContainer());
            visitExpr(idx->getIndex());
        } else if (const auto* mem = dynamic_cast<const ast::MemberExpr*>(expr)) {
            visitExpr(mem->getObject());
            // If the object is an identifier (e.g. `module.func`), note that object is already visited.
        } else if (const auto* znk = dynamic_cast<const ast::ZonkExpr*>(expr)) {
            visitExpr(znk->getPrimary());
            visitExpr(znk->getFallback());
            visitStmt(znk->getRecoveryBlock());
        } else if (const auto* tern = dynamic_cast<const ast::TernaryExpr*>(expr)) {
            visitExpr(tern->getCondition());
            visitExpr(tern->getThenExpr());
            visitExpr(tern->getElseExpr());
        } else if (const auto* cast = dynamic_cast<const ast::TypeCastExpr*>(expr)) {
            visitExpr(cast->getExpr());
            visitType(cast->getTargetType());
        } else if (const auto* arr = dynamic_cast<const ast::ArrayLitExpr*>(expr)) {
            for (const auto& elem : arr->getElements()) {
                visitExpr(elem.get());
            }
        } else if (const auto* map = dynamic_cast<const ast::MapLitExpr*>(expr)) {
            for (const auto& entry : map->getEntries()) {
                visitExpr(entry.key.get());
                visitExpr(entry.value.get());
            }
        } else if (const auto* closure = dynamic_cast<const ast::ClosureExpr*>(expr)) {
            for (const auto& p : closure->getParams()) {
                visitType(p.type.get());
                visitExpr(p.defaultValue.get());
            }
            visitType(closure->getReturnType());
            visitStmt(closure->getBody());
        } else if (const auto* tmpl = dynamic_cast<const ast::TemplateLitExpr*>(expr)) {
            for (const auto& part : tmpl->getParts()) {
                if (part.isExpr) {
                    visitExpr(part.expr.get());
                }
            }
        }
    }

    void visitType(const ast::TypeAnnotation* type) {
        if (!type) return;
        if (const auto* cust = dynamic_cast<const ast::CustomType*>(type)) {
            if (cust->isQualified()) {
                out_.insert(cust->getModuleQualifier());
            } else {
                out_.insert(cust->getName());
            }
            for (const auto& arg : cust->getTypeArgs()) {
                visitType(arg.get());
            }
        } else if (const auto* arr = dynamic_cast<const ast::ArrayType*>(type)) {
            visitType(arr->getElementType());
        } else if (const auto* map = dynamic_cast<const ast::MapType*>(type)) {
            visitType(map->getKeyType());
            visitType(map->getValueType());
        } else if (const auto* u = dynamic_cast<const ast::UnionType*>(type)) {
            for (const auto& v : u->getVariants()) {
                visitType(v.get());
            }
        } else if (const auto* fn = dynamic_cast<const ast::FunctionType*>(type)) {
            for (const auto& p : fn->getParams()) {
                visitType(p.type.get());
            }
            visitType(fn->getReturnType());
        }
    }

    std::unordered_set<std::string>& out_;
};

} // namespace keety::lint
