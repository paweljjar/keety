#include <keety/typechecker/type_checker.hpp>
#include <keety/sema/universe.hpp>

namespace keety::typechecker {

TypeChecker::TypeChecker(diag::DiagnosticEmitter* emitter)
    : emitter_(emitter), hasErrors_(false) {}

// =============================================================================
// Task 3.3.1: Explicit Type Annotation Validation
// =============================================================================

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

// =============================================================================
// Task 3.3.2: Strict Zero-Coercion Policy (E3002)
// =============================================================================

diag::Diagnostic TypeChecker::createImplicitWideningDiagnostic(
    TypePtr fromType,
    TypePtr toType,
    diag::SourceSpan span,
    std::string_view contextDesc) {
    std::string fromStr = fromType ? (fromType->isLiteral() ? fromType->getBaseType()->toString() : fromType->toString()) : "<unknown>";
    std::string toStr = toType ? (toType->isLiteral() ? toType->getBaseType()->toString() : toType->toString()) : "<unknown>";
    std::string msg = "implicit numeric conversion from '" + fromStr + "' to '" + toStr + "' is disallowed";
    if (!contextDesc.empty()) {
        msg += " in " + std::string(contextDesc);
    }

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::ImplicitTypeWidening,
        std::move(msg),
        span.isValid() ? span.start : diag::SourceLocation{});

    if (span.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            span,
            "expected '" + toStr + "', found '" + fromStr + "'",
            true
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Note,
        "KeetScript enforces a strict zero-coercion policy; numeric types cannot be widened or narrowed implicitly."
    });

    std::string help;
    if (fromType && fromType->isInt() && toType && toType->isFloat()) {
        help = "use an explicit conversion, e.g. 'float(...)', '... .toFloat()', or a float literal like '42.0'";
    } else {
        help = "use an explicit conversion, e.g. 'int(...)', '... .toInt()', or an integer literal like '42'";
    }
    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        std::move(help)
    });

    return d;
}

diag::Diagnostic TypeChecker::createBinaryCoercionDiagnostic(
    TypePtr leftType,
    TypePtr rightType,
    lexer::TokenKind op,
    diag::SourceSpan span) {
    std::string opStr = std::string(lexer::getTokenKindName(op));
    std::string leftStr = leftType ? (leftType->isLiteral() ? leftType->getBaseType()->toString() : leftType->toString()) : "<unknown>";
    std::string rightStr = rightType ? (rightType->isLiteral() ? rightType->getBaseType()->toString() : rightType->toString()) : "<unknown>";

    std::string msg = "implicit numeric conversion between '" + leftStr +
                      "' and '" + rightStr + "' in binary expression is disallowed";

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::ImplicitTypeWidening,
        std::move(msg),
        span.isValid() ? span.start : diag::SourceLocation{});

    if (span.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            span,
            "cannot apply binary operator '" + opStr + "' between '" + leftStr + "' and '" + rightStr + "'",
            true
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Note,
        "KeetScript forbids implicit numeric conversions; both operands of a numeric operator must have the same type."
    });

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        "explicitly convert one operand, e.g. 'float(a) " + opStr + " b' or 'a.toFloat() " + opStr + " b'"
    });

    return d;
}

void TypeChecker::reportImplicitWidening(
    TypePtr fromType,
    TypePtr toType,
    diag::SourceSpan span,
    std::string_view contextDesc) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createImplicitWideningDiagnostic(fromType, toType, span, contextDesc));
    }
}

void TypeChecker::reportBinaryCoercion(
    TypePtr leftType,
    TypePtr rightType,
    lexer::TokenKind op,
    diag::SourceSpan span) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createBinaryCoercionDiagnostic(leftType, rightType, op, span));
    }
}

// =============================================================================
// Task 3.3.3: Contextual Typing of Empty Collection Literals (E3009 / E3003)
// =============================================================================

diag::Diagnostic TypeChecker::createCannotInferCollectionTypeDiagnostic(
    std::string_view literalKind,
    diag::SourceSpan span) {
    std::string msg = "cannot infer type of empty " + std::string(literalKind) + " without explicit contextual type";

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::CannotInferCollectionType,
        std::move(msg),
        span.isValid() ? span.start : diag::SourceLocation{});

    if (span.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            span,
            "empty collection literal requires an explicit contextual type",
            true
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Note,
        "Empty collection literals do not possess an inherent type and derive their concrete type contextually from an explicit type annotation."
    });

    std::string help = "assign to an explicitly typed binding or provide an explicit type annotation, e.g. 'let x: ";
    if (literalKind.find("map") != std::string::npos) {
        help += "map<K, V> = {}'";
    } else {
        help += "array<T> = []'";
    }
    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        std::move(help)
    });

    return d;
}

diag::Diagnostic TypeChecker::createTypeMismatchDiagnostic(
    TypePtr expectedType,
    TypePtr actualType,
    diag::SourceSpan span,
    std::string_view contextDesc) {
    std::string expStr = expectedType ? expectedType->toString() : "<unknown>";
    std::string actStr = actualType ? actualType->toString() : "<unknown>";

    std::string msg = "type mismatch: expected '" + expStr + "', found '" + actStr + "'";
    if (!contextDesc.empty()) {
        msg += " in " + std::string(contextDesc);
    }

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::TypeMismatch,
        std::move(msg),
        span.isValid() ? span.start : diag::SourceLocation{});

    if (span.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            span,
            "expected '" + expStr + "', found '" + actStr + "'",
            true
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Note,
        "KeetScript requires expression types to match expected static types exactly."
    });

    return d;
}

void TypeChecker::reportCannotInferCollectionType(std::string_view literalKind, diag::SourceSpan span) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createCannotInferCollectionTypeDiagnostic(literalKind, span));
    }
}

void TypeChecker::reportTypeMismatch(TypePtr expectedType, TypePtr actualType, diag::SourceSpan span, std::string_view contextDesc) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createTypeMismatchDiagnostic(expectedType, actualType, span, contextDesc));
    }
}

bool TypeChecker::checkAssignment(
    TypePtr expectedType,
    TypePtr actualType,
    diag::SourceSpan span,
    std::string_view contextDesc) {
    if (!expectedType || !actualType) return true;
    if (expectedType->isUnknown() || actualType->isUnknown()) return true;

    // Strict zero-coercion check: int <-> float
    if ((actualType->isInt() && expectedType->isFloat()) ||
        (actualType->isFloat() && expectedType->isInt())) {
        reportImplicitWidening(actualType, expectedType, span, contextDesc);
        return false;
    }

    // Check assignability using union subtyping & canonical algebra
    if (!actualType->isAssignableTo(*expectedType)) {
        reportTypeMismatch(expectedType, actualType, span, contextDesc);
        return false;
    }

    return true;
}

bool TypeChecker::checkZeroCoercion(const parser::ModuleAST& module, sema::Scope* rootScope) {
    return checkTypes(module, rootScope);
}

bool TypeChecker::checkTypes(const parser::ModuleAST& module, sema::Scope* rootScope) {
    std::shared_ptr<sema::Scope> ownedUniverse;
    std::shared_ptr<sema::Scope> ownedModule;
    sema::Scope* targetScope = rootScope;

    if (!targetScope) {
        ownedUniverse = sema::Scope::createUniverse(true);
        ownedModule = ownedUniverse->createChild(sema::ScopeKind::Module);
        targetScope = ownedModule.get();
    }

    // Pass 1: Register top-level module declarations in scope so forward calls can resolve
    for (const auto& decl : module.declarations) {
        if (!decl) continue;
        if (const auto* varDecl = dynamic_cast<const ast::VarDecl*>(decl.get())) {
            auto sym = std::make_shared<sema::Symbol>(varDecl->getName(), sema::SymbolKind::Variable, varDecl->getSpan());
            sym->setTypeAnnotation(varDecl->getType());
            sym->setAstNode(varDecl);
            targetScope->define(sym);
        } else if (const auto* constDecl = dynamic_cast<const ast::ConstDecl*>(decl.get())) {
            auto sym = std::make_shared<sema::Symbol>(constDecl->getName(), sema::SymbolKind::Constant, constDecl->getSpan());
            sym->setTypeAnnotation(constDecl->getType());
            sym->setAstNode(constDecl);
            targetScope->define(sym);
        } else if (const auto* defDecl = dynamic_cast<const ast::DefineDecl*>(decl.get())) {
            auto sym = std::make_shared<sema::Symbol>(defDecl->getName(), sema::SymbolKind::Define, defDecl->getSpan());
            sym->setTypeAnnotation(defDecl->getType());
            sym->setAstNode(defDecl);
            targetScope->define(sym);
        } else if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            auto sym = std::make_shared<sema::Symbol>(fnDecl->getName(), sema::SymbolKind::Function, fnDecl->getSpan());
            sym->setTypeAnnotation(fnDecl->getReturnType());
            sym->setAstNode(fnDecl);
            targetScope->define(sym);
        } else if (const auto* clsDecl = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            auto sym = std::make_shared<sema::Symbol>(clsDecl->getName(), sema::SymbolKind::Class, clsDecl->getSpan());
            sym->setAstNode(clsDecl);
            targetScope->define(sym);
        }
    }

    // Pass 2: Type check each declaration body, statements, and expressions
    for (const auto& decl : module.declarations) {
        if (decl) {
            checkZeroCoercion(*decl, *targetScope);
        }
    }

    return !hasErrors_;
}

bool TypeChecker::checkZeroCoercion(const ast::Decl& decl, sema::Scope& scope) {
    if (const auto* varDecl = dynamic_cast<const ast::VarDecl*>(&decl)) {
        auto declType = Type::fromAnnotation(varDecl->getType());
        auto sym = std::make_shared<sema::Symbol>(varDecl->getName(), sema::SymbolKind::Variable, varDecl->getSpan());
        sym->setTypeAnnotation(varDecl->getType());
        sym->setAstNode(varDecl);
        scope.define(sym);

        if (varDecl->hasInitializer()) {
            auto initType = inferType(*varDecl->getInitializer(), scope, declType);
            checkAssignment(declType, initType, varDecl->getInitializer()->getSpan(), "variable initialization");
        }
    } else if (const auto* constDecl = dynamic_cast<const ast::ConstDecl*>(&decl)) {
        auto declType = Type::fromAnnotation(constDecl->getType());
        auto sym = std::make_shared<sema::Symbol>(constDecl->getName(), sema::SymbolKind::Constant, constDecl->getSpan());
        sym->setTypeAnnotation(constDecl->getType());
        sym->setAstNode(constDecl);
        scope.define(sym);

        if (constDecl->getInitializer()) {
            auto initType = inferType(*constDecl->getInitializer(), scope, declType);
            checkAssignment(declType, initType, constDecl->getInitializer()->getSpan(), "constant initialization");
        }
    } else if (const auto* defDecl = dynamic_cast<const ast::DefineDecl*>(&decl)) {
        auto declType = Type::fromAnnotation(defDecl->getType());
        auto sym = std::make_shared<sema::Symbol>(defDecl->getName(), sema::SymbolKind::Define, defDecl->getSpan());
        sym->setTypeAnnotation(defDecl->getType());
        sym->setAstNode(defDecl);
        scope.define(sym);

        if (defDecl->getValue()) {
            auto initType = inferType(*defDecl->getValue(), scope, declType);
            checkAssignment(declType, initType, defDecl->getValue()->getSpan(), "define initialization");
        }
    } else if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(&decl)) {
        auto retType = Type::fromAnnotation(fnDecl->getReturnType());
        auto fnScope = scope.createChild(sema::ScopeKind::Function);

        for (const auto& param : fnDecl->getParams()) {
            auto pType = Type::fromAnnotation(param.type.get());
            auto pSym = std::make_shared<sema::Symbol>(param.name, sema::SymbolKind::Parameter, param.span);
            pSym->setTypeAnnotation(param.type.get());
            fnScope->define(pSym);

            if (param.defaultValue) {
                auto defType = inferType(*param.defaultValue, *fnScope, pType);
                checkAssignment(pType, defType, param.defaultValue->getSpan(), "parameter default value");
            }
        }

        auto prevRet = currentFunctionReturnType_;
        currentFunctionReturnType_ = retType;
        if (fnDecl->getBody()) {
            checkZeroCoercion(*fnDecl->getBody(), *fnScope);
        }
        currentFunctionReturnType_ = prevRet;
    } else if (const auto* classDecl = dynamic_cast<const ast::ClassDecl*>(&decl)) {
        auto classScope = scope.createChild(sema::ScopeKind::Class);

        for (const auto& field : classDecl->getFields()) {
            auto fType = Type::fromAnnotation(field.type.get());
            auto fSym = std::make_shared<sema::Symbol>(field.name, sema::SymbolKind::Field, field.span);
            fSym->setTypeAnnotation(field.type.get());
            classScope->define(fSym);

            if (field.initializer) {
                auto initType = inferType(*field.initializer, *classScope, fType);
                checkAssignment(fType, initType, field.initializer->getSpan(), "class field initialization");
            }
        }

        for (const auto& method : classDecl->getMethods()) {
            if (method) {
                checkZeroCoercion(*method, *classScope);
            }
        }
    }

    return !hasErrors_;
}

bool TypeChecker::checkZeroCoercion(const ast::Stmt& stmt, sema::Scope& scope) {
    if (const auto* blockStmt = dynamic_cast<const ast::BlockStmt*>(&stmt)) {
        auto blockScope = scope.createChild(sema::ScopeKind::Block);
        for (const auto& s : blockStmt->getStatements()) {
            if (s) {
                checkZeroCoercion(*s, *blockScope);
            }
        }
    } else if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(&stmt)) {
        if (declStmt->getDeclaration()) {
            checkZeroCoercion(*declStmt->getDeclaration(), scope);
        }
    } else if (const auto* exprStmt = dynamic_cast<const ast::ExprStmt*>(&stmt)) {
        if (exprStmt->getExpression()) {
            inferType(*exprStmt->getExpression(), scope);
        }
    } else if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(&stmt)) {
        if (ifStmt->hasInit()) {
            checkZeroCoercion(*ifStmt->getInit(), scope);
        }
        if (ifStmt->getCondition()) {
            inferType(*ifStmt->getCondition(), scope);
        }
        if (ifStmt->getThenBranch()) {
            checkZeroCoercion(*ifStmt->getThenBranch(), scope);
        }
        if (ifStmt->getElseBranch()) {
            checkZeroCoercion(*ifStmt->getElseBranch(), scope);
        }
    } else if (const auto* whileStmt = dynamic_cast<const ast::WhileStmt*>(&stmt)) {
        if (whileStmt->hasInit()) {
            checkZeroCoercion(*whileStmt->getInit(), scope);
        }
        if (whileStmt->getCondition()) {
            inferType(*whileStmt->getCondition(), scope);
        }
        if (whileStmt->getBody()) {
            checkZeroCoercion(*whileStmt->getBody(), scope);
        }
    } else if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(&stmt)) {
        auto forScope = scope.createChild(sema::ScopeKind::Block);
        if (forStmt->getInit()) {
            checkZeroCoercion(*forStmt->getInit(), *forScope);
        }
        if (forStmt->getCondition()) {
            inferType(*forStmt->getCondition(), *forScope);
        }
        if (forStmt->getPost()) {
            checkZeroCoercion(*forStmt->getPost(), *forScope);
        }
        if (forStmt->getBody()) {
            checkZeroCoercion(*forStmt->getBody(), *forScope);
        }
    } else if (const auto* forInStmt = dynamic_cast<const ast::ForInStmt*>(&stmt)) {
        auto forScope = scope.createChild(sema::ScopeKind::Block);
        if (forInStmt->getCollection()) {
            inferType(*forInStmt->getCollection(), scope);
        }
        if (forInStmt->getBody()) {
            checkZeroCoercion(*forInStmt->getBody(), *forScope);
        }
    } else if (const auto* returnStmt = dynamic_cast<const ast::ReturnStmt*>(&stmt)) {
        if (returnStmt->hasValue()) {
            auto valType = inferType(*returnStmt->getValue(), scope, currentFunctionReturnType_);
            if (currentFunctionReturnType_) {
                checkAssignment(currentFunctionReturnType_, valType, returnStmt->getValue()->getSpan(), "return statement");
            }
        }
    } else if (const auto* switchStmt = dynamic_cast<const ast::SwitchStmt*>(&stmt)) {
        if (switchStmt->hasInit()) {
            checkZeroCoercion(*switchStmt->getInit(), scope);
        }
        if (switchStmt->getDiscriminant()) {
            inferType(*switchStmt->getDiscriminant(), scope);
        }
        for (const auto& c : switchStmt->getCases()) {
            for (const auto& expr : c.values) {
                if (expr) {
                    inferType(*expr, scope);
                }
            }
            if (c.body) {
                checkZeroCoercion(*c.body, scope);
            }
        }
    }

    return !hasErrors_;
}

TypePtr TypeChecker::inferType(const ast::Expr& expr, sema::Scope& scope, TypePtr contextualType) {
    if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(&expr)) {
        switch (lit->getLiteralKind()) {
        case lexer::TokenKind::IntLiteral: {
            int64_t val = 0;
            try {
                val = std::stoll(lit->getRawText(), nullptr, 0);
            } catch (...) {}
            return Type::literalInt(val);
        }
        case lexer::TokenKind::FloatLiteral: {
            double val = 0.0;
            try {
                val = std::stod(lit->getRawText());
            } catch (...) {}
            return Type::literalFloat(val);
        }
        case lexer::TokenKind::CharLiteral: {
            char c = lit->getRawText().empty() ? '\0' : lit->getRawText()[0];
            return Type::literalChar(c);
        }
        case lexer::TokenKind::StringLiteral: {
            std::string raw = lit->getRawText();
            // Quote delimiter rule: 1-char in single quotes is char
            if (raw.size() >= 2 && raw.front() == '\'' && raw.back() == '\'') {
                std::string inner = raw.substr(1, raw.size() - 2);
                if (inner.size() == 1) {
                    return Type::literalChar(inner[0]);
                }
                if (inner.size() == 2 && inner[0] == '\\') {
                    char c = inner[1];
                    if (c == 'n') c = '\n';
                    else if (c == 't') c = '\t';
                    else if (c == 'r') c = '\r';
                    else if (c == '0') c = '\0';
                    return Type::literalChar(c);
                }
                return Type::literalString(inner);
            }
            if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
                raw = raw.substr(1, raw.size() - 2);
            }
            return Type::literalString(raw);
        }
        case lexer::TokenKind::KwTrue:
            return Type::literalBool(true);
        case lexer::TokenKind::KwFalse:
            return Type::literalBool(false);
        case lexer::TokenKind::KwNull:
            return Type::nullType();
        default:
            return Type::unknownType();
        }
    }

    if (const auto* ident = dynamic_cast<const ast::IdentExpr*>(&expr)) {
        auto sym = scope.lookup(ident->getName());
        if (sym) {
            if (sym->getTypeAnnotation()) {
                return Type::fromAnnotation(sym->getTypeAnnotation());
            }
            if (sym->isBuiltinType()) {
                return Type::custom(ident->getName());
            }
        }
        return Type::unknownType();
    }

    if (const auto* cast = dynamic_cast<const ast::TypeCastExpr*>(&expr)) {
        if (cast->getExpr()) {
            inferType(*cast->getExpr(), scope);
        }
        return Type::fromAnnotation(cast->getTargetType());
    }

    if (const auto* typeofExpr = dynamic_cast<const ast::TypeofExpr*>(&expr)) {
        if (typeofExpr->getExpr()) {
            inferType(*typeofExpr->getExpr(), scope);
        }
        return Type::stringType();
    }

    if (const auto* unary = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
        auto opType = unary->getOperand() ? inferType(*unary->getOperand(), scope) : Type::unknownType();
        if (opType->isLiteral()) {
            const auto& lit = static_cast<const LiteralType&>(*opType);
            if (unary->getOp() == lexer::TokenKind::Minus) {
                if (lit.getLiteralKind() == LiteralKind::Int) return Type::literalInt(-lit.getIntValue());
                if (lit.getLiteralKind() == LiteralKind::Float) return Type::literalFloat(-lit.getFloatValue());
            } else if (unary->getOp() == lexer::TokenKind::Plus) {
                return opType;
            } else if (unary->getOp() == lexer::TokenKind::Exclaim) {
                if (lit.getLiteralKind() == LiteralKind::Bool) return Type::literalBool(!lit.getBoolValue());
            }
        }
        if (unary->getOp() == lexer::TokenKind::Exclaim) {
            return Type::boolType();
        }
        return opType;
    }

    if (const auto* binary = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
        auto op = binary->getOp();

        if (lexer::isAssignmentOperator(op)) {
            auto leftType = binary->getLeft() ? inferType(*binary->getLeft(), scope) : Type::unknownType();
            auto rightType = binary->getRight() ? inferType(*binary->getRight(), scope, leftType) : Type::unknownType();
            if (binary->getRight()) {
                checkAssignment(leftType, rightType, binary->getRight()->getSpan(), "assignment");
            }
            return leftType;
        }

        auto leftType = binary->getLeft() ? inferType(*binary->getLeft(), scope) : Type::unknownType();
        auto rightType = binary->getRight() ? inferType(*binary->getRight(), scope) : Type::unknownType();

        // Arithmetic operators: +, -, *, /
        if (op == lexer::TokenKind::Plus || op == lexer::TokenKind::Minus ||
            op == lexer::TokenKind::Star || op == lexer::TokenKind::Slash) {
            if ((leftType->isInt() && rightType->isFloat()) ||
                (leftType->isFloat() && rightType->isInt())) {
                reportBinaryCoercion(leftType, rightType, op, binary->getSpan());
                return Type::unknownType();
            }
            if (leftType->isInt() && rightType->isInt()) return Type::intType();
            if (leftType->isFloat() && rightType->isFloat()) return Type::floatType();
            if (op == lexer::TokenKind::Plus && (leftType->isString() || rightType->isString())) {
                return Type::stringType();
            }
            return leftType;
        }

        // Remainder operator: %
        if (op == lexer::TokenKind::Percent) {
            if (leftType->isFloat() || rightType->isFloat()) {
                reportBinaryCoercion(leftType, rightType, op, binary->getSpan());
                return Type::unknownType();
            }
            return Type::intType();
        }

        // Relational and equality operators: <, <=, >, >=, ==, !=
        if (op == lexer::TokenKind::Less || op == lexer::TokenKind::LessEqual ||
            op == lexer::TokenKind::Greater || op == lexer::TokenKind::GreaterEqual ||
            op == lexer::TokenKind::EqualEqual || op == lexer::TokenKind::ExclaimEqual) {
            if ((leftType->isInt() && rightType->isFloat()) ||
                (leftType->isFloat() && rightType->isInt())) {
                reportBinaryCoercion(leftType, rightType, op, binary->getSpan());
            }
            return Type::boolType();
        }

        // Logical operators: &&, ||
        if (op == lexer::TokenKind::AmpAmp || op == lexer::TokenKind::PipePipe) {
            return Type::boolType();
        }

        return Type::unknownType();
    }

    if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expr)) {
        // Built-in numeric conversion methods: .toFloat(), .toInt(), etc.
        if (const auto* member = dynamic_cast<const ast::MemberExpr*>(call->getCallee())) {
            auto objType = member->getObject() ? inferType(*member->getObject(), scope) : Type::unknownType();
            for (const auto& arg : call->getArgs()) {
                if (arg.value) inferType(*arg.value, scope);
            }
            if (member->getMemberName() == "toFloat") return Type::floatType();
            if (member->getMemberName() == "toInt") return Type::intType();
            if (member->getMemberName() == "toString") return Type::stringType();
            if (member->getMemberName() == "toChar") return Type::charType();
            return Type::unknownType();
        }

        // Function call: lookup in scope and check arguments
        if (const auto* calleeIdent = dynamic_cast<const ast::IdentExpr*>(call->getCallee())) {
            auto sym = scope.lookup(calleeIdent->getName());
            if (sym && sym->getAstNode()) {
                if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(sym->getAstNode())) {
                    for (size_t i = 0; i < call->getArgs().size(); ++i) {
                        const auto& arg = call->getArgs()[i];
                        if (arg.value) {
                            TypePtr pType = (i < fnDecl->getParams().size())
                                ? Type::fromAnnotation(fnDecl->getParams()[i].type.get())
                                : nullptr;
                            auto argType = inferType(*arg.value, scope, pType);
                            if (pType) {
                                checkAssignment(pType, argType, arg.value->getSpan(), "function argument");
                            }
                        }
                    }
                    return Type::fromAnnotation(fnDecl->getReturnType());
                }
            }
        }

        for (const auto& arg : call->getArgs()) {
            if (arg.value) inferType(*arg.value, scope);
        }
        return Type::unknownType();
    }

    if (const auto* ternary = dynamic_cast<const ast::TernaryExpr*>(&expr)) {
        if (ternary->getCondition()) inferType(*ternary->getCondition(), scope);
        auto thenType = ternary->getThenExpr() ? inferType(*ternary->getThenExpr(), scope, contextualType) : Type::unknownType();
        auto elseType = ternary->getElseExpr() ? inferType(*ternary->getElseExpr(), scope, contextualType) : Type::unknownType();

        if ((thenType->isInt() && elseType->isFloat()) ||
            (thenType->isFloat() && elseType->isInt())) {
            reportImplicitWidening(thenType, elseType, ternary->getSpan(), "ternary branches");
        }
        return contextualType ? contextualType : thenType;
    }

    if (const auto* arrLit = dynamic_cast<const ast::ArrayLitExpr*>(&expr)) {
        if (arrLit->getElements().empty()) {
            if (contextualType && contextualType->isArray()) {
                return contextualType;
            }
            if (contextualType && !contextualType->isArray() && !contextualType->isUnknown()) {
                reportTypeMismatch(contextualType, Type::array(Type::unknownType()), arrLit->getSpan(), "empty array literal");
                return Type::array(Type::unknownType());
            }
            reportCannotInferCollectionType("array literal '[]'", arrLit->getSpan());
            return Type::array(Type::unknownType());
        }

        TypePtr elemTarget = (contextualType && contextualType->isArray())
            ? static_cast<const ArrayType&>(*contextualType).getElementType()
            : nullptr;
        TypePtr inferredElem = elemTarget;

        for (const auto& el : arrLit->getElements()) {
            if (el) {
                auto elType = inferType(*el, scope, elemTarget);
                if (elemTarget) {
                    checkAssignment(elemTarget, elType, el->getSpan(), "array element");
                } else if (!inferredElem || inferredElem->isUnknown()) {
                    inferredElem = elType;
                }
            }
        }
        return contextualType ? contextualType : Type::array(inferredElem ? inferredElem : Type::unknownType());
    }

    if (const auto* mapLit = dynamic_cast<const ast::MapLitExpr*>(&expr)) {
        if (mapLit->getEntries().empty()) {
            if (contextualType && contextualType->isMap()) {
                return contextualType;
            }
            if (contextualType && !contextualType->isMap() && !contextualType->isUnknown()) {
                reportTypeMismatch(contextualType, Type::map(Type::unknownType(), Type::unknownType()), mapLit->getSpan(), "empty map literal");
                return Type::map(Type::unknownType(), Type::unknownType());
            }
            reportCannotInferCollectionType("map literal '{}'", mapLit->getSpan());
            return Type::map(Type::unknownType(), Type::unknownType());
        }

        TypePtr keyTarget = (contextualType && contextualType->isMap())
            ? static_cast<const MapType&>(*contextualType).getKeyType()
            : nullptr;
        TypePtr valTarget = (contextualType && contextualType->isMap())
            ? static_cast<const MapType&>(*contextualType).getValueType()
            : nullptr;
        TypePtr inferredKey = keyTarget;
        TypePtr inferredVal = valTarget;

        for (const auto& entry : mapLit->getEntries()) {
            if (entry.key) {
                auto kType = inferType(*entry.key, scope, keyTarget);
                if (keyTarget) {
                    checkAssignment(keyTarget, kType, entry.key->getSpan(), "map key");
                } else if (!inferredKey || inferredKey->isUnknown()) {
                    inferredKey = kType;
                }
            }
            if (entry.value) {
                auto vType = inferType(*entry.value, scope, valTarget);
                if (valTarget) {
                    checkAssignment(valTarget, vType, entry.value->getSpan(), "map value");
                } else if (!inferredVal || inferredVal->isUnknown()) {
                    inferredVal = vType;
                }
            }
        }
        return contextualType ? contextualType : Type::map(
            inferredKey ? inferredKey : Type::unknownType(),
            inferredVal ? inferredVal : Type::unknownType());
    }

    if (const auto* closure = dynamic_cast<const ast::ClosureExpr*>(&expr)) {
        auto fnScope = scope.createChild(sema::ScopeKind::Function);
        for (const auto& p : closure->getParams()) {
            auto pSym = std::make_shared<sema::Symbol>(p.name, sema::SymbolKind::Parameter, p.span);
            pSym->setTypeAnnotation(p.type.get());
            fnScope->define(pSym);
        }
        if (closure->getBody()) {
            auto prevRet = currentFunctionReturnType_;
            currentFunctionReturnType_ = Type::fromAnnotation(closure->getReturnType());
            checkZeroCoercion(*closure->getBody(), *fnScope);
            currentFunctionReturnType_ = prevRet;
        }
        return Type::fromAnnotation(closure->getReturnType());
    }

    if (const auto* index = dynamic_cast<const ast::IndexExpr*>(&expr)) {
        auto contType = index->getContainer() ? inferType(*index->getContainer(), scope) : Type::unknownType();
        if (index->getIndex()) inferType(*index->getIndex(), scope);

        if (contType->isArray()) {
            const auto& arr = static_cast<const ArrayType&>(*contType);
            return arr.getElementType();
        }
        if (contType->isMap()) {
            const auto& m = static_cast<const MapType&>(*contType);
            return m.getValueType();
        }
        return Type::unknownType();
    }

    if (const auto* member = dynamic_cast<const ast::MemberExpr*>(&expr)) {
        if (member->getObject()) inferType(*member->getObject(), scope);
        return Type::unknownType();
    }

    if (const auto* zonk = dynamic_cast<const ast::ZonkExpr*>(&expr)) {
        auto pType = zonk->getPrimary() ? inferType(*zonk->getPrimary(), scope, contextualType) : Type::unknownType();
        if (zonk->getFallback()) {
            auto fbType = inferType(*zonk->getFallback(), scope, contextualType ? contextualType : pType);
            if ((pType->isInt() && fbType->isFloat()) ||
                (pType->isFloat() && fbType->isInt())) {
                reportImplicitWidening(fbType, pType, zonk->getFallback()->getSpan(), "zonk fallback");
            }
        }
        if (zonk->getRecoveryBlock()) {
            checkZeroCoercion(*zonk->getRecoveryBlock(), scope);
        }
        return contextualType ? contextualType : pType;
    }

    return Type::unknownType();
}

} // namespace keety::typechecker
