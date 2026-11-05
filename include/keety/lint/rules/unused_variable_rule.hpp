#pragma once

#include <keety/lint/lint_rule.hpp>
#include <keety/lint/reference_collector.hpp>

#include <unordered_set>

namespace keety::lint {

/// L002: Detects local variables declared with let or const that are never read.
class UnusedVariableRule : public LintRule {
public:
    [[nodiscard]] std::string_view getCode() const noexcept override { return "L002"; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "unused-variable"; }
    [[nodiscard]] std::string_view getDescription() const noexcept override {
        return "Flags local variables declared with let or const that are never read";
    }
    [[nodiscard]] diag::DiagnosticSeverity getDefaultSeverity() const noexcept override {
        return diag::DiagnosticSeverity::Warning;
    }

    [[nodiscard]] std::vector<LintDiagnostic> run(const LintContext& context) override {
        std::vector<LintDiagnostic> diagnostics;

        // Traverse all declarations, functions, and methods
        for (const auto& decl : context.moduleAst.declarations) {
            checkDecl(decl.get(), diagnostics);
        }

        return diagnostics;
    }

private:
    struct LocalVar {
        std::string name;
        diag::SourceSpan span;
        diag::SourceSpan nameSpan;
    };

    void checkDecl(const ast::Decl* decl, std::vector<LintDiagnostic>& diags) {
        if (!decl) return;

        if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl)) {
            checkStmt(fn->getBody(), diags);
        } else if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl)) {
            for (const auto& m : cls->getMethods()) {
                checkDecl(m.get(), diags);
            }
        }
    }

    void checkStmt(const ast::Stmt* stmt, std::vector<LintDiagnostic>& diags) {
        if (!stmt) return;

        if (const auto* block = dynamic_cast<const ast::BlockStmt*>(stmt)) {
            checkBlock(block, diags);
        } else if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(stmt)) {
            checkStmt(ifStmt->getInit(), diags);
            checkStmt(ifStmt->getThenBranch(), diags);
            checkStmt(ifStmt->getElseBranch(), diags);
        } else if (const auto* wh = dynamic_cast<const ast::WhileStmt*>(stmt)) {
            checkStmt(wh->getInit(), diags);
            checkStmt(wh->getBody(), diags);
        } else if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(stmt)) {
            checkStmt(forStmt->getInit(), diags);
            checkStmt(forStmt->getBody(), diags);
        } else if (const auto* forIn = dynamic_cast<const ast::ForInStmt*>(stmt)) {
            checkStmt(forIn->getBody(), diags);
        } else if (const auto* sw = dynamic_cast<const ast::SwitchStmt*>(stmt)) {
            checkStmt(sw->getInit(), diags);
            for (const auto& c : sw->getCases()) {
                checkStmt(c.body.get(), diags);
            }
        }
    }

    void checkBlock(const ast::BlockStmt* block, std::vector<LintDiagnostic>& diags) {
        if (!block) return;

        const auto& stmts = block->getStatements();
        std::vector<LocalVar> locals;

        // 1. Identify local declarations in this block
        for (const auto& s : stmts) {
            if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(s.get())) {
                const auto* d = declStmt->getDeclaration();
                if (const auto* var = dynamic_cast<const ast::VarDecl*>(d)) {
                    // Ignore variables prefixed with '_'
                    if (!var->getName().empty() && !var->getName().starts_with("_")) {
                        locals.push_back({var->getName(), var->getSpan(), var->getNameSpan()});
                    }
                } else if (const auto* c = dynamic_cast<const ast::ConstDecl*>(d)) {
                    if (!c->getName().empty() && !c->getName().starts_with("_")) {
                        locals.push_back({c->getName(), c->getSpan(), c->getNameSpan()});
                    }
                }
            }
        }

        // 2. Collect all identifier references in the entire block (excluding the declarators themselves)
        std::unordered_set<std::string> references;
        for (const auto& s : stmts) {
            if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(s.get())) {
                const auto* d = declStmt->getDeclaration();
                if (const auto* var = dynamic_cast<const ast::VarDecl*>(d)) {
                    ReferenceCollector::collect(var->getType(), references);
                    ReferenceCollector::collect(var->getInitializer(), references);
                    continue;
                }
                if (const auto* c = dynamic_cast<const ast::ConstDecl*>(d)) {
                    ReferenceCollector::collect(c->getType(), references);
                    ReferenceCollector::collect(c->getInitializer(), references);
                    continue;
                }
            }
            ReferenceCollector::collect(s.get(), references);
        }

        // 3. Flag unused locals
        for (const auto& local : locals) {
            if (!references.contains(local.name)) {
                std::string msg = "Variable '" + local.name + "' is declared but never read. Prefix with '_' if intentional.";
                diag::SourceSpan targetSpan = local.nameSpan.isValid() ? local.nameSpan : local.span;
                diags.emplace_back(
                    std::string(getCode()),
                    std::string(getName()),
                    getDefaultSeverity(),
                    std::move(msg),
                    targetSpan,
                    FixEdit{targetSpan, "_" + local.name}
                );
            }
        }

        // 4. Recurse into nested blocks
        for (const auto& s : stmts) {
            if (dynamic_cast<const ast::DeclStmt*>(s.get())) {
                continue;
            }
            checkStmt(s.get(), diags);
        }
    }
};

} // namespace keety::lint
