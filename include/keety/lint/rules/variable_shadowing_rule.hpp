#pragma once

#include <keety/lint/lint_rule.hpp>

#include <string>
#include <vector>

namespace keety::lint {

/// L011: Detects local variables that shadow an outer enclosing scope variable or declaration.
class VariableShadowingRule : public LintRule {
public:
    [[nodiscard]] std::string_view getCode() const noexcept override { return "L011"; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "variable-shadowing"; }
    [[nodiscard]] std::string_view getDescription() const noexcept override {
        return "Flags local variables that shadow an outer enclosing scope variable or parameter";
    }
    [[nodiscard]] diag::DiagnosticSeverity getDefaultSeverity() const noexcept override {
        return diag::DiagnosticSeverity::Note;
    }

    [[nodiscard]] std::vector<LintDiagnostic> run(const LintContext& context) override {
        std::vector<LintDiagnostic> diagnostics;

        std::vector<std::string> outerScopeNames;

        // Add top-level declaration names
        for (const auto& decl : context.moduleAst.declarations) {
            if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl.get())) {
                outerScopeNames.push_back(fn->getName());
            } else if (const auto* var = dynamic_cast<const ast::VarDecl*>(decl.get())) {
                outerScopeNames.push_back(var->getName());
            } else if (const auto* c = dynamic_cast<const ast::ConstDecl*>(decl.get())) {
                outerScopeNames.push_back(c->getName());
            }
        }

        for (const auto& decl : context.moduleAst.declarations) {
            checkDecl(decl.get(), outerScopeNames, diagnostics);
        }

        return diagnostics;
    }

private:
    void checkDecl(const ast::Decl* decl, std::vector<std::string>& scopeStack, std::vector<LintDiagnostic>& diags) {
        if (!decl) return;

        if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl)) {
            size_t checkpoint = scopeStack.size();
            for (const auto& p : fn->getParams()) {
                if (!p.name.empty() && p.name != "_") {
                    scopeStack.push_back(p.name);
                }
            }
            checkStmt(fn->getBody(), scopeStack, diags);
            scopeStack.resize(checkpoint);
        } else if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl)) {
            for (const auto& m : cls->getMethods()) {
                checkDecl(m.get(), scopeStack, diags);
            }
        }
    }

    void checkStmt(const ast::Stmt* stmt, std::vector<std::string>& scopeStack, std::vector<LintDiagnostic>& diags) {
        if (!stmt) return;

        if (const auto* block = dynamic_cast<const ast::BlockStmt*>(stmt)) {
            size_t checkpoint = scopeStack.size();
            for (const auto& s : block->getStatements()) {
                if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(s.get())) {
                    const auto* d = declStmt->getDeclaration();
                    std::string varName;
                    diag::SourceSpan varSpan;
                    if (const auto* v = dynamic_cast<const ast::VarDecl*>(d)) {
                        varName = v->getName();
                        varSpan = v->getSpan();
                    } else if (const auto* c = dynamic_cast<const ast::ConstDecl*>(d)) {
                        varName = c->getName();
                        varSpan = c->getSpan();
                    }

                    if (!varName.empty() && varName != "_") {
                        // Check if varName exists in outer scopes
                        for (const auto& outer : scopeStack) {
                            if (outer == varName) {
                                std::string msg = "Variable '" + varName + "' shadows a declaration in an outer enclosing scope";
                                diags.emplace_back(
                                    std::string(getCode()),
                                    std::string(getName()),
                                    getDefaultSeverity(),
                                    std::move(msg),
                                    varSpan
                                );
                                break;
                            }
                        }
                        scopeStack.push_back(varName);
                    }
                } else {
                    checkStmt(s.get(), scopeStack, diags);
                }
            }
            scopeStack.resize(checkpoint);
        } else if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(stmt)) {
            checkStmt(ifStmt->getThenBranch(), scopeStack, diags);
            checkStmt(ifStmt->getElseBranch(), scopeStack, diags);
        } else if (const auto* wh = dynamic_cast<const ast::WhileStmt*>(stmt)) {
            checkStmt(wh->getBody(), scopeStack, diags);
        } else if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(stmt)) {
            checkStmt(forStmt->getBody(), scopeStack, diags);
        } else if (const auto* forIn = dynamic_cast<const ast::ForInStmt*>(stmt)) {
            size_t checkpoint = scopeStack.size();
            if (!forIn->getKeyName().empty()) scopeStack.push_back(forIn->getKeyName());
            if (!forIn->getValueName().empty()) scopeStack.push_back(forIn->getValueName());
            checkStmt(forIn->getBody(), scopeStack, diags);
            scopeStack.resize(checkpoint);
        }
    }
};

} // namespace keety::lint
