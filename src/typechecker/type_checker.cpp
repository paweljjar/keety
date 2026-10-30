#include <keety/typechecker/type_checker.hpp>

namespace keety::typechecker {

TypeChecker::TypeChecker(diag::DiagnosticEmitter* emitter)
    : emitter_(emitter), hasErrors_(false) {}

diag::Diagnostic TypeChecker::createMissingTypeDiagnostic(
    std::string_view entityKind,
    std::string_view entityName,
    diag::SourceSpan span) {
    std::string msg = "missing explicit type annotation for " + std::string(entityKind);
    if (!entityName.empty()) {
        msg += " '" + std::string(entityName) + "'";
    }

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::MissingTypeAnnotation,
        std::move(msg),
        span.isValid() ? span.start : diag::SourceLocation{});

    if (span.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            span,
            "explicit type annotation required here",
            true
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Note,
        "KeetScript strictly forbids unannotated declarations to maintain zero-coercion type safety."
    });

    std::string help = "specify an explicit type, e.g. '";
    if (entityKind.find("variable") != std::string::npos) {
        help += "let " + std::string(entityName) + ": <Type>'";
    } else if (entityKind.find("constant") != std::string::npos) {
        help += "const " + std::string(entityName) + ": <Type>'";
    } else if (!entityName.empty()) {
        help += std::string(entityName) + ": <Type>'";
    } else {
        help += ": <Type>'";
    }
    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        std::move(help)
    });

    return d;
}

void TypeChecker::reportMissingType(
    std::string_view entityKind,
    std::string_view entityName,
    diag::SourceSpan span) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createMissingTypeDiagnostic(entityKind, entityName, span));
    }
}

bool TypeChecker::checkExplicitTypeAnnotations(const parser::ModuleAST& module) {
    for (const auto& decl : module.declarations) {
        if (decl) {
            checkExplicitTypeAnnotations(*decl);
        }
    }
    return !hasErrors_;
}

bool TypeChecker::checkExplicitTypeAnnotations(const ast::Decl& decl) {
    if (const auto* varDecl = dynamic_cast<const ast::VarDecl*>(&decl)) {
        if (!varDecl->getType()) {
            reportMissingType("variable", varDecl->getName(), varDecl->getSpan());
        }
        if (varDecl->hasInitializer()) {
            checkExplicitTypeAnnotations(*varDecl->getInitializer());
        }
    } else if (const auto* constDecl = dynamic_cast<const ast::ConstDecl*>(&decl)) {
        if (!constDecl->getType()) {
            reportMissingType("constant", constDecl->getName(), constDecl->getSpan());
        }
        if (constDecl->getInitializer()) {
            checkExplicitTypeAnnotations(*constDecl->getInitializer());
        }
    } else if (const auto* defineDecl = dynamic_cast<const ast::DefineDecl*>(&decl)) {
        if (!defineDecl->getType()) {
            reportMissingType("compile-time constant", defineDecl->getName(), defineDecl->getSpan());
        }
        if (defineDecl->getValue()) {
            checkExplicitTypeAnnotations(*defineDecl->getValue());
        }
    } else if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(&decl)) {
        for (const auto& param : fnDecl->getParams()) {
            if (!param.type) {
                reportMissingType("parameter", param.name, param.span);
            }
            if (param.defaultValue) {
                checkExplicitTypeAnnotations(*param.defaultValue);
            }
        }
        if (!fnDecl->getReturnType()) {
            reportMissingType("function return", fnDecl->getName(), fnDecl->getSpan());
        }
        if (fnDecl->getBody()) {
            checkExplicitTypeAnnotations(*fnDecl->getBody());
        }
    } else if (const auto* classDecl = dynamic_cast<const ast::ClassDecl*>(&decl)) {
        for (const auto& field : classDecl->getFields()) {
            if (!field.type) {
                reportMissingType("class field", field.name, field.span);
            }
            if (field.initializer) {
                checkExplicitTypeAnnotations(*field.initializer);
            }
        }
        for (const auto& method : classDecl->getMethods()) {
            if (method) {
                checkExplicitTypeAnnotations(*method);
            }
        }
    } else if (const auto* errTypeDecl = dynamic_cast<const ast::ErrTypeDecl*>(&decl)) {
        for (const auto& field : errTypeDecl->getFields()) {
            if (!field.type) {
                reportMissingType("error field", field.name, field.span);
            }
            if (field.initializer) {
                checkExplicitTypeAnnotations(*field.initializer);
            }
        }
    }

    return !hasErrors_;
}

bool TypeChecker::checkExplicitTypeAnnotations(const ast::Stmt& stmt) {
    if (const auto* blockStmt = dynamic_cast<const ast::BlockStmt*>(&stmt)) {
        for (const auto& s : blockStmt->getStatements()) {
            if (s) {
                checkExplicitTypeAnnotations(*s);
            }
        }
    } else if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(&stmt)) {
        if (declStmt->getDeclaration()) {
            checkExplicitTypeAnnotations(*declStmt->getDeclaration());
        }
    } else if (const auto* exprStmt = dynamic_cast<const ast::ExprStmt*>(&stmt)) {
        if (exprStmt->getExpression()) {
            checkExplicitTypeAnnotations(*exprStmt->getExpression());
        }
    } else if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(&stmt)) {
        if (ifStmt->hasInit()) {
            checkExplicitTypeAnnotations(*ifStmt->getInit());
        }
        if (ifStmt->getCondition()) {
            checkExplicitTypeAnnotations(*ifStmt->getCondition());
        }
        if (ifStmt->getThenBranch()) {
            checkExplicitTypeAnnotations(*ifStmt->getThenBranch());
        }
        if (ifStmt->getElseBranch()) {
            checkExplicitTypeAnnotations(*ifStmt->getElseBranch());
        }
    } else if (const auto* whileStmt = dynamic_cast<const ast::WhileStmt*>(&stmt)) {
        if (whileStmt->hasInit()) {
            checkExplicitTypeAnnotations(*whileStmt->getInit());
        }
        if (whileStmt->getCondition()) {
            checkExplicitTypeAnnotations(*whileStmt->getCondition());
        }
        if (whileStmt->getBody()) {
            checkExplicitTypeAnnotations(*whileStmt->getBody());
        }
    } else if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(&stmt)) {
        if (forStmt->getInit()) {
            checkExplicitTypeAnnotations(*forStmt->getInit());
        }
        if (forStmt->getCondition()) {
            checkExplicitTypeAnnotations(*forStmt->getCondition());
        }
        if (forStmt->getPost()) {
            checkExplicitTypeAnnotations(*forStmt->getPost());
        }
        if (forStmt->getBody()) {
            checkExplicitTypeAnnotations(*forStmt->getBody());
        }
    } else if (const auto* forInStmt = dynamic_cast<const ast::ForInStmt*>(&stmt)) {
        if (forInStmt->isKeyValue()) {
            if (!forInStmt->getKeyType()) {
                reportMissingType("loop key variable", forInStmt->getKeyName(), forInStmt->getSpan());
            }
            if (!forInStmt->getValueType()) {
                reportMissingType("loop value variable", forInStmt->getValueName(), forInStmt->getSpan());
            }
        } else {
            if (!forInStmt->getValueType()) {
                reportMissingType("loop variable", forInStmt->getValueName(), forInStmt->getSpan());
            }
        }
        if (forInStmt->getCollection()) {
            checkExplicitTypeAnnotations(*forInStmt->getCollection());
        }
        if (forInStmt->getBody()) {
            checkExplicitTypeAnnotations(*forInStmt->getBody());
        }
    } else if (const auto* returnStmt = dynamic_cast<const ast::ReturnStmt*>(&stmt)) {
        if (returnStmt->hasValue()) {
            checkExplicitTypeAnnotations(*returnStmt->getValue());
        }
    } else if (const auto* switchStmt = dynamic_cast<const ast::SwitchStmt*>(&stmt)) {
        if (switchStmt->hasInit()) {
            checkExplicitTypeAnnotations(*switchStmt->getInit());
        }
        if (switchStmt->getDiscriminant()) {
            checkExplicitTypeAnnotations(*switchStmt->getDiscriminant());
        }
        for (const auto& c : switchStmt->getCases()) {
            for (const auto& expr : c.values) {
                if (expr) {
                    checkExplicitTypeAnnotations(*expr);
                }
            }
            if (c.body) {
                checkExplicitTypeAnnotations(*c.body);
            }
        }
    }

    return !hasErrors_;
}

bool TypeChecker::checkExplicitTypeAnnotations(const ast::Expr& expr) {
    if (const auto* closure = dynamic_cast<const ast::ClosureExpr*>(&expr)) {
        for (const auto& param : closure->getParams()) {
            if (!param.type) {
                reportMissingType("parameter", param.name, param.span);
            }
            if (param.defaultValue) {
                checkExplicitTypeAnnotations(*param.defaultValue);
            }
        }
        if (!closure->getReturnType()) {
            reportMissingType("function literal return", "", closure->getSpan());
        }
        if (closure->getBody()) {
            checkExplicitTypeAnnotations(*closure->getBody());
        }
    } else if (const auto* binary = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
        if (binary->getLeft()) checkExplicitTypeAnnotations(*binary->getLeft());
        if (binary->getRight()) checkExplicitTypeAnnotations(*binary->getRight());
    } else if (const auto* unary = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
        if (unary->getOperand()) checkExplicitTypeAnnotations(*unary->getOperand());
    } else if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expr)) {
        if (call->getCallee()) checkExplicitTypeAnnotations(*call->getCallee());
        for (const auto& arg : call->getArgs()) {
            if (arg.value) checkExplicitTypeAnnotations(*arg.value);
        }
    } else if (const auto* index = dynamic_cast<const ast::IndexExpr*>(&expr)) {
        if (index->getContainer()) checkExplicitTypeAnnotations(*index->getContainer());
        if (index->getIndex()) checkExplicitTypeAnnotations(*index->getIndex());
    } else if (const auto* member = dynamic_cast<const ast::MemberExpr*>(&expr)) {
        if (member->getObject()) checkExplicitTypeAnnotations(*member->getObject());
    } else if (const auto* ternary = dynamic_cast<const ast::TernaryExpr*>(&expr)) {
        if (ternary->getCondition()) checkExplicitTypeAnnotations(*ternary->getCondition());
        if (ternary->getThenExpr()) checkExplicitTypeAnnotations(*ternary->getThenExpr());
        if (ternary->getElseExpr()) checkExplicitTypeAnnotations(*ternary->getElseExpr());
    } else if (const auto* arrayLit = dynamic_cast<const ast::ArrayLitExpr*>(&expr)) {
        for (const auto& el : arrayLit->getElements()) {
            if (el) checkExplicitTypeAnnotations(*el);
        }
    } else if (const auto* mapLit = dynamic_cast<const ast::MapLitExpr*>(&expr)) {
        for (const auto& entry : mapLit->getEntries()) {
            if (entry.key) checkExplicitTypeAnnotations(*entry.key);
            if (entry.value) checkExplicitTypeAnnotations(*entry.value);
        }
    }

    return !hasErrors_;
}

} // namespace keety::typechecker
