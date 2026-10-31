#include <keety/typechecker/type_checker.hpp>
#include <keety/sema/const_view_checker.hpp>
#include <keety/sema/shadow_checker.hpp>
#include <keety/sema/universe.hpp>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace keety::typechecker {

namespace {

TypePtr substituteTypeParams(const TypePtr& type, const std::unordered_map<std::string, TypePtr>& subst) {
    if (!type || subst.empty()) {
        return type;
    }
    if (type->isCustom()) {
        const auto* custom = dynamic_cast<const CustomType*>(type.get());
        if (custom) {
            auto it = subst.find(custom->getName());
            if (it != subst.end()) {
                return it->second;
            }
        }
        return type;
    }
    if (type->isArray()) {
        const auto* arr = dynamic_cast<const ArrayType*>(type.get());
        if (arr) {
            auto newElem = substituteTypeParams(arr->getElementType(), subst);
            if (newElem != arr->getElementType()) {
                return Type::array(newElem);
            }
        }
        return type;
    }
    if (type->isMap()) {
        const auto* map = dynamic_cast<const MapType*>(type.get());
        if (map) {
            auto newKey = substituteTypeParams(map->getKeyType(), subst);
            auto newVal = substituteTypeParams(map->getValueType(), subst);
            if (newKey != map->getKeyType() || newVal != map->getValueType()) {
                return Type::map(newKey, newVal);
            }
        }
        return type;
    }
    if (type->isUnion()) {
        const auto* u = dynamic_cast<const UnionType*>(type.get());
        if (u) {
            std::vector<TypePtr> newVariants;
            newVariants.reserve(u->getVariants().size());
            bool changed = false;
            for (const auto& v : u->getVariants()) {
                auto sub = substituteTypeParams(v, subst);
                if (sub != v) changed = true;
                newVariants.push_back(sub);
            }
            if (changed) {
                return Type::normalizeUnion(newVariants);
            }
        }
        return type;
    }
    if (type->isFunction()) {
        const auto* fn = dynamic_cast<const FunctionType*>(type.get());
        if (fn) {
            std::vector<TypePtr> newParams;
            newParams.reserve(fn->getParamTypes().size());
            bool changed = false;
            for (const auto& p : fn->getParamTypes()) {
                auto sub = substituteTypeParams(p, subst);
                if (sub != p) changed = true;
                newParams.push_back(sub);
            }
            auto newRet = substituteTypeParams(fn->getReturnType(), subst);
            if (newRet != fn->getReturnType()) changed = true;
            if (changed) {
                return Type::function(std::move(newParams), newRet, fn->isAsync());
            }
        }
        return type;
    }
    return type;
}

void deduceTypeParams(
    const TypePtr& paramType,
    const TypePtr& argType,
    const std::unordered_set<std::string>& typeParamNames,
    std::unordered_map<std::string, TypePtr>& subst) {
    if (!paramType || !argType) return;

    if (paramType->isCustom()) {
        const auto* custom = dynamic_cast<const CustomType*>(paramType.get());
        if (custom && typeParamNames.count(custom->getName())) {
            if (subst.find(custom->getName()) == subst.end()) {
                subst[custom->getName()] = argType->getBaseType();
            }
            return;
        }
    }
    if (paramType->isArray() && argType->isArray()) {
        const auto* pArr = dynamic_cast<const ArrayType*>(paramType.get());
        const auto* aArr = dynamic_cast<const ArrayType*>(argType.get());
        if (pArr && aArr) {
            deduceTypeParams(pArr->getElementType(), aArr->getElementType(), typeParamNames, subst);
        }
        return;
    }
    if (paramType->isMap() && argType->isMap()) {
        const auto* pMap = dynamic_cast<const MapType*>(paramType.get());
        const auto* aMap = dynamic_cast<const MapType*>(argType.get());
        if (pMap && aMap) {
            deduceTypeParams(pMap->getKeyType(), aMap->getKeyType(), typeParamNames, subst);
            deduceTypeParams(pMap->getValueType(), aMap->getValueType(), typeParamNames, subst);
        }
        return;
    }
    if (paramType->isFunction() && argType->isFunction()) {
        const auto* pFn = dynamic_cast<const FunctionType*>(paramType.get());
        const auto* aFn = dynamic_cast<const FunctionType*>(argType.get());
        if (pFn && aFn) {
            size_t count = std::min(pFn->getParamTypes().size(), aFn->getParamTypes().size());
            for (size_t i = 0; i < count; ++i) {
                deduceTypeParams(pFn->getParamTypes()[i], aFn->getParamTypes()[i], typeParamNames, subst);
            }
            deduceTypeParams(pFn->getReturnType(), aFn->getReturnType(), typeParamNames, subst);
        }
        return;
    }
}

} // anonymous namespace

TypeChecker::TypeChecker(diag::DiagnosticEmitter* emitter)
    : emitter_(emitter), hasErrors_(false) {}

// =============================================================================
// Explicit Type Annotation Validation
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
    } else if (const auto* yieldStmt = dynamic_cast<const ast::YieldStmt*>(&stmt)) {
        if (yieldStmt->getValue()) {
            checkExplicitTypeAnnotations(*yieldStmt->getValue());
        }
    } else if (const auto* zonkStmt = dynamic_cast<const ast::ZonkStmt*>(&stmt)) {
        if (zonkStmt->getErrorExpr()) {
            checkExplicitTypeAnnotations(*zonkStmt->getErrorExpr());
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
    } else if (const auto* zonk = dynamic_cast<const ast::ZonkExpr*>(&expr)) {
        if (zonk->getPrimary()) checkExplicitTypeAnnotations(*zonk->getPrimary());
        if (zonk->getFallback()) checkExplicitTypeAnnotations(*zonk->getFallback());
        if (zonk->getRecoveryBlock()) checkExplicitTypeAnnotations(*zonk->getRecoveryBlock());
    } else if (const auto* tpl = dynamic_cast<const ast::TemplateLitExpr*>(&expr)) {
        for (const auto& part : tpl->getParts()) {
            if (part.isExpr && part.expr) checkExplicitTypeAnnotations(*part.expr);
        }
    }

    return !hasErrors_;
}

// =============================================================================
// Strict Zero-Coercion Policy (E3002)
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
// Contextual Typing of Empty Collection Literals (E3009 / E3003)
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

// =============================================================================
// Exhaustiveness & Reachability Analysis (E5001 / E5002)
// =============================================================================

diag::Diagnostic TypeChecker::createMissingReturnDiagnostic(
    std::string_view functionName,
    TypePtr returnType,
    diag::SourceSpan span) {
    std::string retStr = returnType ? returnType->toString() : "non-void";
    std::string msg;
    if (!functionName.empty() && functionName != "<anonymous closure>") {
        msg = "missing return statement in non-void function '" + std::string(functionName) + "' returning '" + retStr + "'";
    } else {
        msg = "missing return statement in closure returning '" + retStr + "'";
    }

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::MissingReturnStatement,
        std::move(msg),
        span.isValid() ? span.start : diag::SourceLocation{});

    if (span.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            span,
            "all execution paths must terminate in a return, yield, or zonk statement",
            true
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        "ensure every control-flow branch ends with a 'return <expr>;' or 'zonk <err>;' statement"
    });

    return d;
}

diag::Diagnostic TypeChecker::createUnreachableCodeDiagnostic(diag::SourceSpan span) {
    std::string msg = "unreachable code detected: statement follows a terminating statement";

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::UnreachableCode,
        std::move(msg),
        span.isValid() ? span.start : diag::SourceLocation{});

    if (span.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            span,
            "this statement cannot be executed",
            true
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        "remove the unreachable statement or reorder control flow"
    });

    return d;
}

void TypeChecker::reportMissingReturn(std::string_view functionName, TypePtr returnType, diag::SourceSpan span) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createMissingReturnDiagnostic(functionName, returnType, span));
    }
}

void TypeChecker::reportUnreachableCode(diag::SourceSpan span) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createUnreachableCodeDiagnostic(span));
    }
}

// =============================================================================
// Mutable Reference Borrow Checker (L-Value Validation - E4005)
// =============================================================================

diag::Diagnostic TypeChecker::createNonLValueMutableBorrowDiagnostic(
    std::string_view exprDesc,
    diag::SourceSpan span) {
    std::string msg = "cannot pass non-l-value to mutable reference parameter ('mut')";
    if (!exprDesc.empty()) {
        msg += ": " + std::string(exprDesc);
    }

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::NonLValueMutableBorrow,
        std::move(msg),
        span.isValid() ? span.start : diag::SourceLocation{});

    if (span.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            span,
            "expected mutable addressable l-value (e.g. 'let' variable, array element, or class field)",
            true
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        "only mutable variables ('let'), array indices, and mutable fields can be passed by mutable reference"
    });
    return d;
}

void TypeChecker::reportNonLValueMutableBorrow(std::string_view exprDesc, diag::SourceSpan span) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createNonLValueMutableBorrowDiagnostic(exprDesc, span));
    }
}

bool TypeChecker::isMutableLValue(const ast::Expr* expr, sema::Scope& scope, std::string* outReason) {
    if (!expr) {
        if (outReason) *outReason = "null expression";
        return false;
    }

    // 1. Variable or Parameter identifier
    if (const auto* ident = dynamic_cast<const ast::IdentExpr*>(expr)) {
        if (ident->getName() == "this") {
            return true;
        }

        auto sym = scope.lookup(ident->getName());
        if (!sym) {
            if (outReason) *outReason = "unresolved identifier '" + ident->getName() + "'";
            return false;
        }

        if (sym->isConstant() || sym->isConst()) {
            if (outReason) *outReason = "constant '" + ident->getName() + "' is immutable";
            return false;
        }

        if (sym->isDefine()) {
            if (outReason) *outReason = "compile-time constant '" + ident->getName() + "' is immutable";
            return false;
        }

        if (sym->isConstView()) {
            if (outReason) *outReason = "imported binding '" + ident->getName() + "' is a read-only const-view";
            return false;
        }

        if (sym->isParameter()) {
            if (!sym->isMutable()) {
                if (outReason) *outReason = "parameter '" + ident->getName() + "' is immutable (not declared 'mut')";
                return false;
            }
            return true;
        }

        if (sym->isVariable()) {
            if (sym->isConst() || sym->isConstView()) {
                if (outReason) *outReason = "variable '" + ident->getName() + "' is immutable";
                return false;
            }
            return true;
        }

        if (sym->isField()) {
            if (sym->isConst() || !sym->isMutable()) {
                if (outReason) *outReason = "field '" + ident->getName() + "' is immutable";
                return false;
            }
            return true;
        }

        if (outReason) *outReason = "'" + ident->getName() + "' (" + std::string(sym->getKindName()) + ") is not an addressable mutable variable";
        return false;
    }

    // 2. Index expression: arr[i]
    if (const auto* index = dynamic_cast<const ast::IndexExpr*>(expr)) {
        if (!index->getContainer()) {
            if (outReason) *outReason = "invalid index container";
            return false;
        }

        // Map elements are unstable in storage and cannot be passed as mut
        auto contType = inferType(*index->getContainer(), scope);
        if (contType && contType->isMap()) {
            if (outReason) *outReason = "map elements cannot be passed by mutable reference";
            return false;
        }

        // The container itself must be an addressable mutable l-value.
        // This enforces deep immutability: const arr[0] is rejected.
        std::string contReason;
        if (!isMutableLValue(index->getContainer(), scope, &contReason)) {
            if (outReason) {
                *outReason = "container is not a mutable l-value (" + contReason + ")";
            }
            return false;
        }

        return true;
    }

    // 3. Member expression: obj.field
    if (const auto* member = dynamic_cast<const ast::MemberExpr*>(expr)) {
        if (!member->getObject()) {
            if (outReason) *outReason = "invalid member object";
            return false;
        }

        std::string objReason;
        if (!isMutableLValue(member->getObject(), scope, &objReason)) {
            if (outReason) {
                *outReason = "object is not a mutable l-value (" + objReason + ")";
            }
            return false;
        }

        // Check if member access is on 'this'
        if (const auto* objIdent = dynamic_cast<const ast::IdentExpr*>(member->getObject())) {
            if (objIdent->getName() == "this") {
                auto fieldSym = scope.lookup(member->getMemberName());
                if (fieldSym && fieldSym->isField()) {
                    if (fieldSym->isConst() || !fieldSym->isMutable()) {
                        if (outReason) *outReason = "field '" + member->getMemberName() + "' is immutable";
                        return false;
                    }
                    return true;
                }
            }
        }

        // Check if field itself is const in the object's class declaration
        auto objType = inferType(*member->getObject(), scope);
        if (objType && objType->isCustom()) {
            auto clsName = static_cast<const CustomType*>(objType.get())->getName();
            auto clsSym = scope.lookup(clsName);
            if (clsSym && clsSym->getAstNode()) {
                const auto* clsDecl = dynamic_cast<const ast::ClassDecl*>(clsSym->getAstNode());
                while (clsDecl) {
                    for (const auto& f : clsDecl->getFields()) {
                        if (f.name == member->getMemberName()) {
                            if (f.isConst) {
                                if (outReason) *outReason = "field '" + member->getMemberName() + "' of class '" + clsName + "' is declared const";
                                return false;
                            }
                            return true;
                        }
                    }
                    if (clsDecl->hasBaseClass()) {
                        auto baseSym = scope.lookup(clsDecl->getBaseClass());
                        clsDecl = (baseSym && baseSym->getAstNode()) ? dynamic_cast<const ast::ClassDecl*>(baseSym->getAstNode()) : nullptr;
                    } else {
                        clsDecl = nullptr;
                    }
                }
            }
        }

        return true;
    }

    // 4. Literals
    if (dynamic_cast<const ast::LiteralExpr*>(expr)) {
        if (outReason) *outReason = "literal value is not an l-value";
        return false;
    }
    if (dynamic_cast<const ast::ArrayLitExpr*>(expr)) {
        if (outReason) *outReason = "array literal is not an l-value";
        return false;
    }
    if (dynamic_cast<const ast::MapLitExpr*>(expr)) {
        if (outReason) *outReason = "map literal is not an l-value";
        return false;
    }

    // 5. Binary expressions (a + b, a == b, etc.)
    if (dynamic_cast<const ast::BinaryExpr*>(expr)) {
        if (outReason) *outReason = "binary expression result is a temporary value";
        return false;
    }

    // 6. Unary expressions (-x, !b, etc.)
    if (dynamic_cast<const ast::UnaryExpr*>(expr)) {
        if (outReason) *outReason = "unary expression result is a temporary value";
        return false;
    }

    // 7. Function / method call results: f()
    if (dynamic_cast<const ast::CallExpr*>(expr)) {
        if (outReason) *outReason = "function call return value is a temporary value";
        return false;
    }

    // 8. Conditional expressions: a ? b : c
    if (dynamic_cast<const ast::TernaryExpr*>(expr)) {
        if (outReason) *outReason = "conditional expression result is a temporary value";
        return false;
    }

    // 9. Cast expressions: x as int
    if (dynamic_cast<const ast::TypeCastExpr*>(expr)) {
        if (outReason) *outReason = "type cast result is a temporary value";
        return false;
    }

    // 10. Zonk expressions: a ?? b
    if (dynamic_cast<const ast::ZonkExpr*>(expr)) {
        if (outReason) *outReason = "zonk expression result is a temporary value";
        return false;
    }

    // 11. Typeof expressions: typeof(x)
    if (dynamic_cast<const ast::TypeofExpr*>(expr)) {
        if (outReason) *outReason = "typeof expression result is a temporary string";
        return false;
    }

    // 12. Await expressions: await t
    if (dynamic_cast<const ast::AwaitExpr*>(expr)) {
        if (outReason) *outReason = "await expression result is a temporary value";
        return false;
    }

    // 13. Closure literals: fn(x) { ... }
    if (dynamic_cast<const ast::ClosureExpr*>(expr)) {
        if (outReason) *outReason = "closure literal is not an l-value";
        return false;
    }

    if (outReason) *outReason = "expression is not an addressable mutable l-value";
    return false;
}

// =============================================================================
// Mutable Reference Borrow Checker (Exclusive Borrowing - E4001)
// =============================================================================

namespace {

std::string exprToStorageString(const ast::Expr* expr) {
    if (!expr) return "";
    if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(expr)) {
        return lit->getRawText();
    }
    if (const auto* ident = dynamic_cast<const ast::IdentExpr*>(expr)) {
        return ident->getName();
    }
    if (const auto* member = dynamic_cast<const ast::MemberExpr*>(expr)) {
        auto o = exprToStorageString(member->getObject());
        return o.empty() ? member->getMemberName() : o + "." + member->getMemberName();
    }
    if (const auto* bin = dynamic_cast<const ast::BinaryExpr*>(expr)) {
        auto left = exprToStorageString(bin->getLeft());
        auto right = exprToStorageString(bin->getRight());
        if (!left.empty() && !right.empty()) {
            return left + " " + std::string(lexer::getTokenKindName(bin->getOp())) + " " + right;
        }
    }
    if (const auto* unary = dynamic_cast<const ast::UnaryExpr*>(expr)) {
        auto operand = exprToStorageString(unary->getOperand());
        if (!operand.empty()) {
            return std::string(lexer::getTokenKindName(unary->getOp())) + operand;
        }
    }
    return "";
}

} // namespace

std::string TypeChecker::computeStoragePath(const ast::Expr* expr) {
    if (!expr) return "";

    if (const auto* ident = dynamic_cast<const ast::IdentExpr*>(expr)) {
        return ident->getName();
    }

    if (const auto* member = dynamic_cast<const ast::MemberExpr*>(expr)) {
        auto obj = computeStoragePath(member->getObject());
        if (obj.empty()) return "";
        return obj + "." + member->getMemberName();
    }

    if (const auto* index = dynamic_cast<const ast::IndexExpr*>(expr)) {
        auto cont = computeStoragePath(index->getContainer());
        if (cont.empty()) return "";
        auto idxStr = exprToStorageString(index->getIndex());
        if (idxStr.empty()) {
            idxStr = "?";
        }
        return cont + "[" + idxStr + "]";
    }

    return "";
}

bool TypeChecker::storagePathsAlias(const std::string& p1, const std::string& p2) {
    if (p1.empty() || p2.empty()) return false;
    if (p1 == p2) return true;
    if (p1.size() > p2.size() && p1[p2.size()] == '.' && p1.rfind(p2, 0) == 0) {
        return true;
    }
    if (p2.size() > p1.size() && p2[p1.size()] == '.' && p2.rfind(p1, 0) == 0) {
        return true;
    }
    return false;
}

diag::Diagnostic TypeChecker::createDuplicateMutableBorrowDiagnostic(
    std::string_view path,
    diag::SourceSpan firstSpan,
    diag::SourceSpan duplicateSpan) {
    std::string msg = "cannot pass multiple mutable references to the same storage path";
    if (!path.empty()) {
        msg += ": '" + std::string(path) + "'";
    }

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::DuplicateMutableBorrow,
        std::move(msg),
        duplicateSpan.isValid() ? duplicateSpan.start : diag::SourceLocation{});

    if (duplicateSpan.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            duplicateSpan,
            "duplicate mutable borrow",
            true
        });
    }

    if (firstSpan.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            firstSpan,
            "first mutable borrow occurs here",
            false
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Note,
        "KeetScript enforces exclusive mutable borrowing to prevent reference aliasing bugs"
    });

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        "ensure each 'mut' argument references a distinct, disjoint storage location"
    });

    return d;
}

void TypeChecker::reportDuplicateMutableBorrow(
    std::string_view path,
    diag::SourceSpan firstSpan,
    diag::SourceSpan duplicateSpan) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createDuplicateMutableBorrowDiagnostic(path, firstSpan, duplicateSpan));
    }
}

// =============================================================================
// Mutable Reference Borrow Checker (Container & Element Borrowing - E4002)
// =============================================================================

bool TypeChecker::isContainerAndElementPair(
    const std::string& p1,
    const std::string& p2,
    std::string* outContainer,
    std::string* outElement) {
    if (p1.empty() || p2.empty()) return false;

    // Check if p2 is an indexed element of container p1 (e.g. p1="arr", p2="arr[0]")
    if (p2.size() > p1.size() && p2[p1.size()] == '[' && p2.rfind(p1, 0) == 0) {
        if (outContainer) *outContainer = p1;
        if (outElement) *outElement = p2;
        return true;
    }

    // Check if p1 is an indexed element of container p2 (e.g. p1="arr[0]", p2="arr")
    if (p1.size() > p2.size() && p1[p2.size()] == '[' && p1.rfind(p2, 0) == 0) {
        if (outContainer) *outContainer = p2;
        if (outElement) *outElement = p1;
        return true;
    }

    return false;
}

diag::Diagnostic TypeChecker::createContainerAndElementBorrowDiagnostic(
    std::string_view containerPath,
    std::string_view elementPath,
    diag::SourceSpan firstSpan,
    diag::SourceSpan secondSpan) {
    std::string msg = "cannot simultaneously borrow container '" + std::string(containerPath) +
                      "' and its element '" + std::string(elementPath) + "'";

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::ContainerAndElementBorrow,
        std::move(msg),
        secondSpan.isValid() ? secondSpan.start : diag::SourceLocation{});

    if (secondSpan.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            secondSpan,
            "simultaneous borrow of container and element occurs here",
            true
        });
    }

    if (firstSpan.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            firstSpan,
            "first borrow occurs here",
            false
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Note,
        "Simultaneous mutable borrowing of a container and one of its elements violates disjointness."
    });

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        "container mutations can reallocate storage and invalidate active element references"
    });

    return d;
}

void TypeChecker::reportContainerAndElementBorrow(
    std::string_view containerPath,
    std::string_view elementPath,
    diag::SourceSpan firstSpan,
    diag::SourceSpan secondSpan) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createContainerAndElementBorrowDiagnostic(containerPath, elementPath, firstSpan, secondSpan));
    }
}

// =============================================================================
// Mutable Reference Borrow Checker (Map Element Borrowing - E4003)
// =============================================================================

bool TypeChecker::isMapIndexExpr(const ast::Expr* expr, sema::Scope& scope) {
    if (!expr) return false;
    if (const auto* index = dynamic_cast<const ast::IndexExpr*>(expr)) {
        if (index->getContainer()) {
            auto contType = inferType(*index->getContainer(), scope);
            if (contType && contType->isMap()) {
                return true;
            }
            return isMapIndexExpr(index->getContainer(), scope);
        }
    }
    return false;
}

diag::Diagnostic TypeChecker::createMapElementMutableBorrowDiagnostic(
    std::string_view path,
    diag::SourceSpan span) {
    std::string msg = "cannot take mutable reference to map element";
    if (!path.empty()) {
        msg += ": '" + std::string(path) + "'";
    }

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::MapElementMutableBorrow,
        std::move(msg),
        span.isValid() ? span.start : diag::SourceLocation{});

    if (span.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            span,
            "mutable borrow of map element occurs here",
            true
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Note,
        "Map elements cannot be borrowed mutably because map rehashing may invalidate storage references."
    });

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        "update map elements using direct assignment ('m[k] = val;') or map methods"
    });

    return d;
}

void TypeChecker::reportMapElementMutableBorrow(
    std::string_view path,
    diag::SourceSpan span) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createMapElementMutableBorrowDiagnostic(path, span));
    }
}

// =============================================================================
// Mutable Reference Borrow Checker (Async & Default Args - E4004, E4006)
// =============================================================================

diag::Diagnostic TypeChecker::createAsyncMutableParameterDiagnostic(
    std::string_view paramName,
    diag::SourceSpan span) {
    std::string msg = "mutable reference parameter in async callable";
    if (!paramName.empty()) {
        msg += ": '" + std::string(paramName) + "'";
    }

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::AsyncMutableParameter,
        std::move(msg),
        span.isValid() ? span.start : diag::SourceLocation{});

    if (span.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            span,
            "mutable reference parameter not allowed in async callable",
            true
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Note,
        "Async functions cannot accept 'mut' parameters due to asynchronous lifetime extensions."
    });

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        "parameters of async callables must be passed by value to ensure thread-safety"
    });

    return d;
}

void TypeChecker::reportAsyncMutableParameter(
    std::string_view paramName,
    diag::SourceSpan span) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createAsyncMutableParameterDiagnostic(paramName, span));
    }
}

diag::Diagnostic TypeChecker::createDefaultArgMutableBorrowDiagnostic(
    std::string_view paramName,
    diag::SourceSpan span) {
    std::string msg = "mutable reference parameter cannot have default value";
    if (!paramName.empty()) {
        msg += ": '" + std::string(paramName) + "'";
    }

    auto d = diag::Diagnostic::makeError(
        diag::ErrorCode::DefaultArgMutableBorrow,
        std::move(msg),
        span.isValid() ? span.start : diag::SourceLocation{});

    if (span.isValid()) {
        d.labels.push_back(diag::DiagnosticLabel{
            span,
            "default value not allowed on 'mut' parameter",
            true
        });
    }

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Note,
        "Default arguments cannot be supplied to mutable reference parameters."
    });

    d.footers.push_back(diag::DiagnosticFooter{
        diag::DiagnosticFooter::Kind::Help,
        "mutable reference parameters ('mut') must bind to an explicit mutable l-value argument at the call site"
    });

    return d;
}

void TypeChecker::reportDefaultArgMutableBorrow(
    std::string_view paramName,
    diag::SourceSpan span) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        emitter_->report(createDefaultArgMutableBorrowDiagnostic(paramName, span));
    }
}

void TypeChecker::reportCannotAssignToConstant(std::string_view name, diag::SourceSpan span) {
    hasErrors_ = true;
    if (emitter_ != nullptr) {
        auto d = diag::Diagnostic::makeError(
            diag::ErrorCode::CannotAssignToConstant,
            "cannot assign to immutable constant '" + std::string(name) + "'",
            span.isValid() ? span.start : diag::SourceLocation{});
        if (span.isValid()) {
            d.labels.push_back(diag::DiagnosticLabel{
                span,
                "cannot reassign to constant '" + std::string(name) + "'",
                true
            });
        }
        d.footers.push_back(diag::DiagnosticFooter{
            diag::DiagnosticFooter::Kind::Help,
            "constants declared with 'const' or 'define' are immutable; consider declaring with 'let' instead"
        });
        emitter_->report(d);
    }
}

void TypeChecker::checkTypeAnnotationValidity(const ast::TypeAnnotation* annot) {
    if (!annot) return;
    if (const auto* fnType = dynamic_cast<const ast::FunctionType*>(annot)) {
        if (fnType->isAsync()) {
            for (const auto& p : fnType->getParams()) {
                if (p.isMut) {
                    reportAsyncMutableParameter(p.name, fnType->getSpan());
                }
            }
        }
        for (const auto& p : fnType->getParams()) {
            checkTypeAnnotationValidity(p.type.get());
        }
        checkTypeAnnotationValidity(fnType->getReturnType());
    } else if (const auto* u = dynamic_cast<const ast::UnionType*>(annot)) {
        for (const auto& v : u->getVariants()) {
            checkTypeAnnotationValidity(v.get());
        }
    } else if (const auto* arr = dynamic_cast<const ast::ArrayType*>(annot)) {
        checkTypeAnnotationValidity(arr->getElementType());
    } else if (const auto* map = dynamic_cast<const ast::MapType*>(annot)) {
        checkTypeAnnotationValidity(map->getKeyType());
        checkTypeAnnotationValidity(map->getValueType());
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
        bool isSubclass = false;
        if (actualType->isCustom() && expectedType->isCustom() && currentScope_) {
            auto curName = static_cast<const CustomType*>(actualType.get())->getName();
            auto targetName = static_cast<const CustomType*>(expectedType.get())->getName();
            auto sym = currentScope_->lookup(curName);
            const auto* cls = (sym && sym->getAstNode()) ? dynamic_cast<const ast::ClassDecl*>(sym->getAstNode()) : nullptr;
            while (cls && cls->hasBaseClass()) {
                if (cls->getBaseClass() == targetName) {
                    isSubclass = true;
                    break;
                }
                auto baseSym = currentScope_->lookup(cls->getBaseClass());
                cls = (baseSym && baseSym->getAstNode()) ? dynamic_cast<const ast::ClassDecl*>(baseSym->getAstNode()) : nullptr;
            }
        }
        if (!isSubclass) {
            reportTypeMismatch(expectedType, actualType, span, contextDesc);
            return false;
        }
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
    currentScope_ = targetScope;

    // Pass 1: Register top-level module declarations in scope so forward calls can resolve
    for (const auto& decl : module.declarations) {
        if (!decl) continue;
        if (const auto* varDecl = dynamic_cast<const ast::VarDecl*>(decl.get())) {
            auto sym = std::make_shared<sema::Symbol>(varDecl->getName(), sema::SymbolKind::Variable, varDecl->getSpan());
            sym->setTypeAnnotation(varDecl->getType());
            sym->setAstNode(varDecl);
            sym->setMutable(true);
            sym->setExported(varDecl->isExported());
            auto res = sema::ShadowChecker::defineWithDiagnostics(*targetScope, sym, emitter_);
            if (!res.success) hasErrors_ = true;
        } else if (const auto* constDecl = dynamic_cast<const ast::ConstDecl*>(decl.get())) {
            auto sym = std::make_shared<sema::Symbol>(constDecl->getName(), sema::SymbolKind::Constant, constDecl->getSpan());
            sym->setTypeAnnotation(constDecl->getType());
            sym->setAstNode(constDecl);
            sym->setConst(true);
            sym->setExported(constDecl->isExported());
            auto res = sema::ShadowChecker::defineWithDiagnostics(*targetScope, sym, emitter_);
            if (!res.success) hasErrors_ = true;
        } else if (const auto* defDecl = dynamic_cast<const ast::DefineDecl*>(decl.get())) {
            auto sym = std::make_shared<sema::Symbol>(defDecl->getName(), sema::SymbolKind::Define, defDecl->getSpan());
            sym->setTypeAnnotation(defDecl->getType());
            sym->setAstNode(defDecl);
            sym->setConst(true);
            sym->setExported(defDecl->isExported());
            auto res = sema::ShadowChecker::defineWithDiagnostics(*targetScope, sym, emitter_);
            if (!res.success) hasErrors_ = true;
        } else if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            auto sym = std::make_shared<sema::Symbol>(fnDecl->getName(), sema::SymbolKind::Function, fnDecl->getSpan());
            sym->setTypeAnnotation(fnDecl->getReturnType());
            sym->setAstNode(fnDecl);
            sym->setExported(fnDecl->isExported());
            auto res = sema::ShadowChecker::defineWithDiagnostics(*targetScope, sym, emitter_);
            if (!res.success) hasErrors_ = true;
        } else if (const auto* clsDecl = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            auto sym = std::make_shared<sema::Symbol>(clsDecl->getName(), sema::SymbolKind::Class, clsDecl->getSpan());
            sym->setAstNode(clsDecl);
            sym->setExported(clsDecl->isExported());
            auto res = sema::ShadowChecker::defineWithDiagnostics(*targetScope, sym, emitter_);
            if (!res.success) hasErrors_ = true;
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
        checkTypeAnnotationValidity(varDecl->getType());
        auto declType = Type::fromAnnotation(varDecl->getType());
        if (!scope.isModule()) {
            auto sym = std::make_shared<sema::Symbol>(varDecl->getName(), sema::SymbolKind::Variable, varDecl->getSpan());
            sym->setTypeAnnotation(varDecl->getType());
            sym->setAstNode(varDecl);
            sym->setMutable(true);
            auto res = sema::ShadowChecker::defineWithDiagnostics(scope, sym, emitter_);
            if (!res.success) hasErrors_ = true;
        }

        if (varDecl->hasInitializer()) {
            auto initType = inferType(*varDecl->getInitializer(), scope, declType);
            checkAssignment(declType, initType, varDecl->getInitializer()->getSpan(), "variable initialization");
        }
    } else if (const auto* constDecl = dynamic_cast<const ast::ConstDecl*>(&decl)) {
        checkTypeAnnotationValidity(constDecl->getType());
        auto declType = Type::fromAnnotation(constDecl->getType());
        if (!scope.isModule()) {
            auto sym = std::make_shared<sema::Symbol>(constDecl->getName(), sema::SymbolKind::Constant, constDecl->getSpan());
            sym->setTypeAnnotation(constDecl->getType());
            sym->setAstNode(constDecl);
            sym->setConst(true);
            auto res = sema::ShadowChecker::defineWithDiagnostics(scope, sym, emitter_);
            if (!res.success) hasErrors_ = true;
        }

        if (constDecl->getInitializer()) {
            auto initType = inferType(*constDecl->getInitializer(), scope, declType);
            checkAssignment(declType, initType, constDecl->getInitializer()->getSpan(), "constant initialization");
        }
    } else if (const auto* defDecl = dynamic_cast<const ast::DefineDecl*>(&decl)) {
        checkTypeAnnotationValidity(defDecl->getType());
        auto declType = Type::fromAnnotation(defDecl->getType());
        if (!scope.isModule()) {
            auto sym = std::make_shared<sema::Symbol>(defDecl->getName(), sema::SymbolKind::Define, defDecl->getSpan());
            sym->setTypeAnnotation(defDecl->getType());
            sym->setAstNode(defDecl);
            sym->setConst(true);
            auto res = sema::ShadowChecker::defineWithDiagnostics(scope, sym, emitter_);
            if (!res.success) hasErrors_ = true;
        }

        if (defDecl->getValue()) {
            auto initType = inferType(*defDecl->getValue(), scope, declType);
            checkAssignment(declType, initType, defDecl->getValue()->getSpan(), "define initialization");
        }
    } else if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(&decl)) {
        checkTypeAnnotationValidity(fnDecl->getReturnType());
        auto retType = Type::fromAnnotation(fnDecl->getReturnType());
        auto fnScope = scope.createChild(sema::ScopeKind::Function);

        for (const auto& param : fnDecl->getParams()) {
            checkTypeAnnotationValidity(param.type.get());
            if (param.isMut) {
                if (fnDecl->isAsync()) {
                    reportAsyncMutableParameter(param.name, param.span);
                }
                if (param.defaultValue) {
                    reportDefaultArgMutableBorrow(param.name, param.defaultValue->getSpan().isValid() ? param.defaultValue->getSpan() : param.span);
                }
            }
            auto pType = Type::fromAnnotation(param.type.get());
            auto pSym = std::make_shared<sema::Symbol>(param.name, sema::SymbolKind::Parameter, param.span);
            pSym->setTypeAnnotation(param.type.get());
            pSym->setMutable(param.isMut);
            pSym->setConst(!param.isMut);
            auto res = sema::ShadowChecker::defineWithDiagnostics(*fnScope, pSym, emitter_);
            if (!res.success) hasErrors_ = true;

            if (param.defaultValue) {
                auto defType = inferType(*param.defaultValue, *fnScope, pType);
                checkAssignment(pType, defType, param.defaultValue->getSpan(), "parameter default value");
            }
        }

        auto prevRet = currentFunctionReturnType_;
        currentFunctionReturnType_ = retType;
        if (fnDecl->getBody()) {
            checkZeroCoercion(*fnDecl->getBody(), *fnScope);
            checkExhaustiveness(fnDecl->getBody(), fnDecl->getName(), retType, fnDecl->getSpan());
        }
        currentFunctionReturnType_ = prevRet;
    } else if (const auto* classDecl = dynamic_cast<const ast::ClassDecl*>(&decl)) {
        auto classScope = scope.createChild(sema::ScopeKind::Class);

        if (classDecl->hasBaseClass()) {
            auto baseSym = scope.lookup(classDecl->getBaseClass());
            const auto* baseDecl = (baseSym && baseSym->getAstNode()) ? dynamic_cast<const ast::ClassDecl*>(baseSym->getAstNode()) : nullptr;
            while (baseDecl) {
                for (const auto& bf : baseDecl->getFields()) {
                    auto bfSym = std::make_shared<sema::Symbol>(bf.name, sema::SymbolKind::Field, bf.span);
                    bfSym->setTypeAnnotation(bf.type.get());
                    bfSym->setMutable(!bf.isConst);
                    bfSym->setConst(bf.isConst);
                    classScope->define(bfSym);
                }
                if (baseDecl->hasBaseClass()) {
                    auto nextSym = scope.lookup(baseDecl->getBaseClass());
                    baseDecl = (nextSym && nextSym->getAstNode()) ? dynamic_cast<const ast::ClassDecl*>(nextSym->getAstNode()) : nullptr;
                } else {
                    baseDecl = nullptr;
                }
            }
        }

        for (const auto& field : classDecl->getFields()) {
            checkTypeAnnotationValidity(field.type.get());
            auto fType = Type::fromAnnotation(field.type.get());
            auto fSym = std::make_shared<sema::Symbol>(field.name, sema::SymbolKind::Field, field.span);
            fSym->setTypeAnnotation(field.type.get());
            fSym->setMutable(!field.isConst);
            fSym->setConst(field.isConst);
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

namespace {

bool isTrueCondition(const ast::Expr* cond) {
    if (!cond) return false;
    if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(cond)) {
        return lit->getLiteralKind() == lexer::TokenKind::KwTrue;
    }
    return false;
}

bool hasBreakTargetingLoop(const ast::Stmt* stmt, std::string_view targetLabel, int loopDepth = 0) {
    if (!stmt) return false;

    if (const auto* breakStmt = dynamic_cast<const ast::BreakStmt*>(stmt)) {
        if (breakStmt->hasTargetLabel()) {
            return !targetLabel.empty() && breakStmt->getTargetLabel() == targetLabel;
        }
        return loopDepth == 0;
    }

    if (const auto* blockStmt = dynamic_cast<const ast::BlockStmt*>(stmt)) {
        for (const auto& s : blockStmt->getStatements()) {
            if (s && hasBreakTargetingLoop(s.get(), targetLabel, loopDepth)) {
                return true;
            }
        }
        return false;
    }

    if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(stmt)) {
        if (hasBreakTargetingLoop(ifStmt->getThenBranch(), targetLabel, loopDepth)) return true;
        if (hasBreakTargetingLoop(ifStmt->getElseBranch(), targetLabel, loopDepth)) return true;
        return false;
    }

    if (const auto* switchStmt = dynamic_cast<const ast::SwitchStmt*>(stmt)) {
        for (const auto& c : switchStmt->getCases()) {
            if (hasBreakTargetingLoop(c.body.get(), targetLabel, loopDepth + 1)) return true;
        }
        return false;
    }

    if (const auto* whileStmt = dynamic_cast<const ast::WhileStmt*>(stmt)) {
        return hasBreakTargetingLoop(whileStmt->getBody(), targetLabel, loopDepth + 1);
    }

    if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(stmt)) {
        return hasBreakTargetingLoop(forStmt->getBody(), targetLabel, loopDepth + 1);
    }

    if (const auto* forInStmt = dynamic_cast<const ast::ForInStmt*>(stmt)) {
        return hasBreakTargetingLoop(forInStmt->getBody(), targetLabel, loopDepth + 1);
    }

    return false;
}

bool matchesTypeofTarget(const Type& variant, std::string_view targetStr) {
    if (targetStr == "int") return variant.isInt();
    if (targetStr == "float") return variant.isFloat();
    if (targetStr == "bool") return variant.isBool();
    if (targetStr == "char") return variant.isChar();
    if (targetStr == "string") return variant.isString();
    if (targetStr == "null") return variant.isNull();
    if (targetStr == "void") return variant.isVoid();
    if (targetStr == "error") return variant.isError();
    if (targetStr == "array") return variant.isArray();
    if (targetStr == "map") return variant.isMap();
    if (targetStr == "function") return variant.isFunction();
    if (variant.isCustom()) {
        return static_cast<const CustomType&>(variant).getName() == targetStr;
    }
    return false;
}

TypePtr typeFromTargetStr(std::string_view targetStr) {
    if (targetStr == "int") return Type::intType();
    if (targetStr == "float") return Type::floatType();
    if (targetStr == "bool") return Type::boolType();
    if (targetStr == "char") return Type::charType();
    if (targetStr == "string") return Type::stringType();
    if (targetStr == "null") return Type::nullType();
    if (targetStr == "void") return Type::voidType();
    if (targetStr == "error") return Type::errorType();
    return Type::custom(std::string(targetStr));
}

TypePtr narrowToTypeof(TypePtr currentType, std::string_view targetStr) {
    if (!currentType) return typeFromTargetStr(targetStr);
    if (currentType->isUnion()) {
        const auto& u = static_cast<const UnionType&>(*currentType);
        std::vector<TypePtr> matched;
        for (const auto& v : u.getVariants()) {
            if (v && matchesTypeofTarget(*v, targetStr)) {
                matched.push_back(v);
            }
        }
        if (!matched.empty()) {
            return Type::normalizeUnion(matched);
        }
    }
    if (currentType->isError() && targetStr != "error") {
        return Type::custom(std::string(targetStr));
    }
    return typeFromTargetStr(targetStr);
}

TypePtr narrowExcludeTypeof(TypePtr currentType, std::string_view targetStr) {
    if (!currentType) return Type::unknownType();
    if (currentType->isUnion()) {
        const auto& u = static_cast<const UnionType&>(*currentType);
        std::vector<TypePtr> kept;
        for (const auto& v : u.getVariants()) {
            if (v && !matchesTypeofTarget(*v, targetStr)) {
                kept.push_back(v);
            }
        }
        if (!kept.empty()) {
            return Type::normalizeUnion(kept);
        }
        return Type::unknownType();
    }
    return currentType;
}

TypePtr narrowExcludeNull(TypePtr currentType) {
    if (!currentType) return Type::unknownType();
    if (currentType->isUnion()) {
        const auto& u = static_cast<const UnionType&>(*currentType);
        std::vector<TypePtr> kept;
        for (const auto& v : u.getVariants()) {
            if (v && !v->isNull()) {
                kept.push_back(v);
            }
        }
        if (!kept.empty()) {
            return Type::normalizeUnion(kept);
        }
        return Type::unknownType();
    }
    if (currentType->isNull()) {
        return Type::unknownType();
    }
    return currentType;
}

TypePtr getActiveType(std::string_view varName, sema::Scope& scope) {
    if (auto narrowed = scope.getNarrowedType(varName)) {
        return narrowed;
    }
    if (auto sym = scope.lookup(varName)) {
        if (sym->getTypeAnnotation()) {
            return Type::fromAnnotation(sym->getTypeAnnotation());
        }
        if (sym->isBuiltinType()) {
            return Type::custom(std::string(varName));
        }
    }
    return Type::unknownType();
}

} // namespace

TypeChecker::BranchNarrowing TypeChecker::analyzeCondition(const ast::Expr* condition, sema::Scope& scope) {
    BranchNarrowing result;
    if (!condition) return result;

    // 1. Unary NOT: `!cond`
    if (const auto* unary = dynamic_cast<const ast::UnaryExpr*>(condition)) {
        if (unary->getOp() == lexer::TokenKind::Exclaim && unary->getOperand()) {
            auto inner = analyzeCondition(unary->getOperand(), scope);
            result.thenFacts = std::move(inner.elseFacts);
            result.elseFacts = std::move(inner.thenFacts);
            return result;
        }
    }

    // 2. Binary operations: `==`, `!=`, `&&`, `||`
    if (const auto* binary = dynamic_cast<const ast::BinaryExpr*>(condition)) {
        auto op = binary->getOp();

        // Logical AND: `A && B`
        if (op == lexer::TokenKind::AmpAmp) {
            auto left = analyzeCondition(binary->getLeft(), scope);
            auto right = analyzeCondition(binary->getRight(), scope);

            result.thenFacts = left.thenFacts;
            for (const auto& rf : right.thenFacts) {
                bool updated = false;
                for (auto& lf : result.thenFacts) {
                    if (lf.varName == rf.varName) {
                        lf.narrowedType = rf.narrowedType;
                        updated = true;
                        break;
                    }
                }
                if (!updated) {
                    result.thenFacts.push_back(rf);
                }
            }
            result.elseFacts = std::move(left.elseFacts);
            return result;
        }

        // Logical OR: `A || B`
        if (op == lexer::TokenKind::PipePipe) {
            auto left = analyzeCondition(binary->getLeft(), scope);
            auto right = analyzeCondition(binary->getRight(), scope);

            for (const auto& lf : left.thenFacts) {
                for (const auto& rf : right.thenFacts) {
                    if (lf.varName == rf.varName) {
                        result.thenFacts.push_back({lf.varName, Type::unionOf({lf.narrowedType, rf.narrowedType})});
                    }
                }
            }
            result.elseFacts = left.elseFacts;
            for (const auto& rf : right.elseFacts) {
                bool updated = false;
                for (auto& ef : result.elseFacts) {
                    if (ef.varName == rf.varName) {
                        ef.narrowedType = rf.narrowedType;
                        updated = true;
                        break;
                    }
                }
                if (!updated) {
                    result.elseFacts.push_back(rf);
                }
            }
            return result;
        }

        // Equality / Inequality: `==` and `!=`
        if (op == lexer::TokenKind::EqualEqual || op == lexer::TokenKind::ExclaimEqual) {
            // Pattern A: `typeof(x) == "T"` or `typeof(x) != "T"`
            const ast::TypeofExpr* typeofExpr = nullptr;
            std::string targetStr;
            bool foundTypeof = false;

            auto checkTypeofAndStr = [&](const ast::Expr* a, const ast::Expr* b) {
                if (const auto* te = dynamic_cast<const ast::TypeofExpr*>(a)) {
                    if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(b)) {
                        if (lit->getLiteralKind() == lexer::TokenKind::StringLiteral) {
                            std::string raw = lit->getRawText();
                            if (raw.size() >= 2 && ((raw.front() == '"' && raw.back() == '"') ||
                                                    (raw.front() == '\'' && raw.back() == '\''))) {
                                raw = raw.substr(1, raw.size() - 2);
                            }
                            typeofExpr = te;
                            targetStr = raw;
                            foundTypeof = true;
                        }
                    }
                }
            };
            checkTypeofAndStr(binary->getLeft(), binary->getRight());
            if (!foundTypeof) checkTypeofAndStr(binary->getRight(), binary->getLeft());

            if (foundTypeof && typeofExpr && typeofExpr->getExpr()) {
                if (const auto* ident = dynamic_cast<const ast::IdentExpr*>(typeofExpr->getExpr())) {
                    std::string varName = ident->getName();
                    auto currentType = getActiveType(varName, scope);

                    if (op == lexer::TokenKind::EqualEqual) {
                        result.thenFacts.push_back({varName, narrowToTypeof(currentType, targetStr)});
                        result.elseFacts.push_back({varName, narrowExcludeTypeof(currentType, targetStr)});
                    } else {
                        result.thenFacts.push_back({varName, narrowExcludeTypeof(currentType, targetStr)});
                        result.elseFacts.push_back({varName, narrowToTypeof(currentType, targetStr)});
                    }
                    return result;
                }
            }

            // Pattern B: `x == null` or `x != null`
            const ast::IdentExpr* identExpr = nullptr;
            bool foundNull = false;

            auto checkIdentAndNull = [&](const ast::Expr* a, const ast::Expr* b) {
                if (const auto* ie = dynamic_cast<const ast::IdentExpr*>(a)) {
                    if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(b)) {
                        if (lit->getLiteralKind() == lexer::TokenKind::KwNull) {
                            identExpr = ie;
                            foundNull = true;
                        }
                    }
                }
            };
            checkIdentAndNull(binary->getLeft(), binary->getRight());
            if (!foundNull) checkIdentAndNull(binary->getRight(), binary->getLeft());

            if (foundNull && identExpr) {
                std::string varName = identExpr->getName();
                auto currentType = getActiveType(varName, scope);

                if (op == lexer::TokenKind::ExclaimEqual) { // x != null
                    result.thenFacts.push_back({varName, narrowExcludeNull(currentType)});
                    result.elseFacts.push_back({varName, Type::nullType()});
                } else { // x == null
                    result.thenFacts.push_back({varName, Type::nullType()});
                    result.elseFacts.push_back({varName, narrowExcludeNull(currentType)});
                }
                return result;
            }
        }
    }

    return result;
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

        BranchNarrowing narrowing = analyzeCondition(ifStmt->getCondition(), scope);

        if (ifStmt->getThenBranch()) {
            auto thenScope = scope.createChild(sema::ScopeKind::Block);
            for (const auto& fact : narrowing.thenFacts) {
                thenScope->setNarrowedType(fact.varName, fact.narrowedType);
            }
            for (const auto& s : ifStmt->getThenBranch()->getStatements()) {
                if (s) {
                    checkZeroCoercion(*s, *thenScope);
                }
            }
        }
        if (ifStmt->getElseBranch()) {
            auto elseScope = scope.createChild(sema::ScopeKind::Block);
            for (const auto& fact : narrowing.elseFacts) {
                elseScope->setNarrowedType(fact.varName, fact.narrowedType);
            }
            if (const auto* elseBlock = dynamic_cast<const ast::BlockStmt*>(ifStmt->getElseBranch())) {
                for (const auto& s : elseBlock->getStatements()) {
                    if (s) {
                        checkZeroCoercion(*s, *elseScope);
                    }
                }
            } else {
                checkZeroCoercion(*ifStmt->getElseBranch(), *elseScope);
            }
        }
    } else if (const auto* whileStmt = dynamic_cast<const ast::WhileStmt*>(&stmt)) {
        if (whileStmt->hasInit()) {
            checkZeroCoercion(*whileStmt->getInit(), scope);
        }
        if (whileStmt->getCondition()) {
            inferType(*whileStmt->getCondition(), scope);
        }

        BranchNarrowing narrowing = analyzeCondition(whileStmt->getCondition(), scope);

        if (whileStmt->getBody()) {
            auto bodyScope = scope.createChild(sema::ScopeKind::Block);
            for (const auto& fact : narrowing.thenFacts) {
                bodyScope->setNarrowedType(fact.varName, fact.narrowedType);
            }
            for (const auto& s : whileStmt->getBody()->getStatements()) {
                if (s) {
                    checkZeroCoercion(*s, *bodyScope);
                }
            }
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
        if (forInStmt->isKeyValue()) {
            if (forInStmt->getKeyType()) {
                auto kSym = std::make_shared<sema::Symbol>(forInStmt->getKeyName(), sema::SymbolKind::Variable, forInStmt->getSpan());
                kSym->setTypeAnnotation(forInStmt->getKeyType());
                kSym->setMutable(true);
                auto res = sema::ShadowChecker::defineWithDiagnostics(*forScope, kSym, emitter_);
                if (!res.success) hasErrors_ = true;
            }
            if (forInStmt->getValueType()) {
                auto vSym = std::make_shared<sema::Symbol>(forInStmt->getValueName(), sema::SymbolKind::Variable, forInStmt->getSpan());
                vSym->setTypeAnnotation(forInStmt->getValueType());
                vSym->setMutable(true);
                auto res = sema::ShadowChecker::defineWithDiagnostics(*forScope, vSym, emitter_);
                if (!res.success) hasErrors_ = true;
            }
        } else {
            if (forInStmt->getValueType()) {
                auto vSym = std::make_shared<sema::Symbol>(forInStmt->getValueName(), sema::SymbolKind::Variable, forInStmt->getSpan());
                vSym->setTypeAnnotation(forInStmt->getValueType());
                vSym->setMutable(true);
                auto res = sema::ShadowChecker::defineWithDiagnostics(*forScope, vSym, emitter_);
                if (!res.success) hasErrors_ = true;
            }
        }
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
    } else if (const auto* yieldStmt = dynamic_cast<const ast::YieldStmt*>(&stmt)) {
        if (yieldStmt->getValue()) {
            inferType(*yieldStmt->getValue(), scope);
        }
    } else if (const auto* zonkStmt = dynamic_cast<const ast::ZonkStmt*>(&stmt)) {
        if (zonkStmt->getErrorExpr()) {
            inferType(*zonkStmt->getErrorExpr(), scope);
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
        if (auto narrowed = scope.getNarrowedType(ident->getName())) {
            return narrowed;
        }
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

        // Increment / Decrement mutation: ++x, --x, x++, x--
        if (unary->getOp() == lexer::TokenKind::PlusPlus || unary->getOp() == lexer::TokenKind::MinusMinus) {
            if (opType->isUnion()) {
                reportTypeMismatch(Type::intType(), opType, unary->getSpan(), "unary increment/decrement");
            }
            if (unary->getOperand()) {
                if (const auto* ident = dynamic_cast<const ast::IdentExpr*>(unary->getOperand())) {
                    if (auto sym = scope.lookup(ident->getName())) {
                        if (sym->isConstView()) {
                            if (emitter_) {
                                emitter_->report(sema::ConstViewChecker::createConstViewWriteDiagnostic(
                                    *sym, unary->getSpan(), sema::MutationKind::IncrementDecrement));
                            }
                            hasErrors_ = true;
                        } else if (sym->isConstant() || sym->isConst() || !sym->isMutable()) {
                            reportCannotAssignToConstant(ident->getName(), unary->getSpan());
                        }
                    }
                    scope.resetNarrowedType(ident->getName());
                }
            }
            return opType;
        }

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
            std::string varName;
            if (const auto* ident = dynamic_cast<const ast::IdentExpr*>(binary->getLeft())) {
                varName = ident->getName();
                auto sym = scope.lookup(varName);
                if (sym) {
                    if (sym->isConstView()) {
                        if (emitter_) {
                            emitter_->report(sema::ConstViewChecker::createConstViewWriteDiagnostic(
                                *sym, binary->getLeft()->getSpan(),
                                op == lexer::TokenKind::Equal ? sema::MutationKind::Assignment : sema::MutationKind::CompoundAssignment));
                        }
                        hasErrors_ = true;
                    } else if (sym->isConstant() || sym->isConst() || !sym->isMutable()) {
                        reportCannotAssignToConstant(varName, binary->getLeft()->getSpan());
                    }
                }
            }

            TypePtr leftType = nullptr;
            if (op == lexer::TokenKind::Equal && !varName.empty()) {
                // Direct reassignment: the target type checked for the assignment MUST be
                // the variable's declared type, NOT the ephemeral narrowed type, because
                // reassignment can change the active variant (e.g. from int to string).
                if (auto sym = scope.lookup(varName)) {
                    if (sym->getTypeAnnotation()) {
                        leftType = Type::fromAnnotation(sym->getTypeAnnotation());
                    }
                }
            }
            if (!leftType) {
                leftType = binary->getLeft() ? inferType(*binary->getLeft(), scope) : Type::unknownType();
            }

            if (op != lexer::TokenKind::Equal) {
                if (leftType->isUnion()) {
                    reportTypeMismatch(Type::intType(), leftType, binary->getSpan(), "compound assignment on union type");
                }
            }

            auto rightType = binary->getRight() ? inferType(*binary->getRight(), scope, leftType) : Type::unknownType();
            if (binary->getRight()) {
                checkAssignment(leftType, rightType, binary->getRight()->getSpan(), "assignment");
            }

            // Mutation Watcher: invalidate narrowed type in scope and all enclosing branches
            if (!varName.empty()) {
                scope.resetNarrowedType(varName);
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
        std::vector<std::string> mutatedVars;

        struct CallArgBorrowInfo {
            std::string path;
            diag::SourceSpan span;
            bool isMut{false};
        };
        std::vector<CallArgBorrowInfo> callArgBorrows;

        auto checkCallArg = [&](const ast::CallArg& arg, bool isMut) {
            if (!arg.value) return;
            if (isMut) {
                if (const auto* argIdent = dynamic_cast<const ast::IdentExpr*>(arg.value.get())) {
                    mutatedVars.push_back(argIdent->getName());
                }
            }
            diag::SourceSpan errSpan = arg.span.isValid() ? arg.span : arg.value->getSpan();
            if (isMut) {
                if (isMapIndexExpr(arg.value.get(), scope)) {
                    reportMapElementMutableBorrow(computeStoragePath(arg.value.get()), errSpan);
                    return;
                }
                std::string reason;
                if (!isMutableLValue(arg.value.get(), scope, &reason)) {
                    reportNonLValueMutableBorrow(reason, errSpan);
                    return;
                }
            }

            auto path = computeStoragePath(arg.value.get());
            if (!path.empty()) {
                for (const auto& prev : callArgBorrows) {
                    if (isMut && prev.isMut && storagePathsAlias(prev.path, path)) {
                        reportDuplicateMutableBorrow(path, prev.span, errSpan);
                        break;
                    }
                    if (isMut || prev.isMut) {
                        std::string cont, elem;
                        if (isContainerAndElementPair(prev.path, path, &cont, &elem)) {
                            reportContainerAndElementBorrow(cont, elem, prev.span, errSpan);
                            break;
                        }
                    }
                }
                callArgBorrows.push_back(CallArgBorrowInfo{std::move(path), errSpan, isMut});
            }
        };

        // Member call: built-in numeric conversions (.toFloat(), .toInt(), etc.) or class method call
        if (const auto* member = dynamic_cast<const ast::MemberExpr*>(call->getCallee())) {
            auto objType = member->getObject() ? inferType(*member->getObject(), scope) : Type::unknownType();

            if (member->getMemberName() == "toFloat" ||
                member->getMemberName() == "toInt" ||
                member->getMemberName() == "toString" ||
                member->getMemberName() == "toChar") {
                for (const auto& arg : call->getArgs()) {
                    if (arg.value) inferType(*arg.value, scope);
                    checkCallArg(arg, arg.isMut);
                }
                for (const auto& v : mutatedVars) {
                    scope.resetNarrowedType(v);
                }
                if (member->getMemberName() == "toFloat") return Type::floatType();
                if (member->getMemberName() == "toInt") return Type::intType();
                if (member->getMemberName() == "toString") return Type::stringType();
                return Type::charType();
            }

            // Built-in array member methods: push, pop, len, length, clear, reserve, filter, map, reduce
            if (objType && objType->isArray()) {
                const auto& arrType = static_cast<const ArrayType&>(*objType);
                auto elemType = arrType.getElementType();
                const std::string& mName = member->getMemberName();

                if (mName == "push") {
                    if (!call->getArgs().empty() && call->getArgs()[0].value) {
                        auto argType = inferType(*call->getArgs()[0].value, scope, elemType);
                        checkAssignment(elemType, argType, call->getArgs()[0].value->getSpan(), "push element");
                    }
                    return Type::voidType();
                }
                if (mName == "pop") {
                    return elemType;
                }
                if (mName == "len" || mName == "length") {
                    return Type::intType();
                }
                if (mName == "clear") {
                    return Type::voidType();
                }
                if (mName == "reserve") {
                    if (!call->getArgs().empty() && call->getArgs()[0].value) {
                        inferType(*call->getArgs()[0].value, scope, Type::intType());
                    }
                    return Type::voidType();
                }
                if (mName == "filter") {
                    TypePtr expectedPred = Type::function({elemType}, Type::boolType(), false);
                    if (!call->getArgs().empty() && call->getArgs()[0].value) {
                        inferType(*call->getArgs()[0].value, scope, expectedPred);
                    }
                    return objType;
                }
                if (mName == "map") {
                    TypePtr retElemType = nullptr;
                    if (!call->getTypeArgs().empty() && call->getTypeArgs()[0]) {
                        retElemType = Type::fromAnnotation(call->getTypeArgs()[0].get());
                    }
                    TypePtr expectedFunc = retElemType ? Type::function({elemType}, retElemType, false) : nullptr;
                    if (!call->getArgs().empty() && call->getArgs()[0].value) {
                        auto fnType = inferType(*call->getArgs()[0].value, scope, expectedFunc);
                        if (!retElemType && fnType && fnType->isFunction()) {
                            retElemType = static_cast<const FunctionType&>(*fnType).getReturnType();
                        }
                    }
                    return Type::array(retElemType ? retElemType : elemType);
                }
                if (mName == "reduce") {
                    TypePtr accType = nullptr;
                    if (!call->getTypeArgs().empty() && call->getTypeArgs()[0]) {
                        accType = Type::fromAnnotation(call->getTypeArgs()[0].get());
                    }
                    if (!call->getArgs().empty() && call->getArgs()[0].value) {
                        auto initType = inferType(*call->getArgs()[0].value, scope, accType);
                        if (!accType) accType = initType;
                    }
                    if (call->getArgs().size() > 1 && call->getArgs()[1].value) {
                        TypePtr expectedFunc = accType ? Type::function({accType, elemType}, accType, false) : nullptr;
                        auto fnType = inferType(*call->getArgs()[1].value, scope, expectedFunc);
                        if (!accType && fnType && fnType->isFunction()) {
                            accType = static_cast<const FunctionType&>(*fnType).getReturnType();
                        }
                    }
                    return accType ? accType : elemType;
                }
            }

            // Method call on a class instance
            const ast::FnDecl* methodDecl = nullptr;
            if (objType && objType->isCustom()) {
                auto clsName = static_cast<const CustomType*>(objType.get())->getName();
                auto clsSym = scope.lookup(clsName);
                if (clsSym && clsSym->getAstNode()) {
                    const auto* clsDecl = dynamic_cast<const ast::ClassDecl*>(clsSym->getAstNode());
                    while (clsDecl) {
                        for (const auto& m : clsDecl->getMethods()) {
                            if (m && m->getName() == member->getMemberName()) {
                                methodDecl = m.get();
                                break;
                            }
                        }
                        if (methodDecl) break;
                        if (clsDecl->hasBaseClass()) {
                            auto baseSym = scope.lookup(clsDecl->getBaseClass());
                            clsDecl = (baseSym && baseSym->getAstNode()) ? dynamic_cast<const ast::ClassDecl*>(baseSym->getAstNode()) : nullptr;
                        } else {
                            clsDecl = nullptr;
                        }
                    }
                }
            }

            std::unordered_map<std::string, TypePtr> methodSubst;
            std::unordered_set<std::string> methodParamSet;
            if (methodDecl) {
                methodParamSet.insert(methodDecl->getTypeParams().begin(), methodDecl->getTypeParams().end());
                for (size_t k = 0; k < call->getTypeArgs().size() && k < methodDecl->getTypeParams().size(); ++k) {
                    if (call->getTypeArgs()[k]) {
                        methodSubst[methodDecl->getTypeParams()[k]] = Type::fromAnnotation(call->getTypeArgs()[k].get());
                    }
                }
            }

            for (size_t i = 0; i < call->getArgs().size(); ++i) {
                const auto& arg = call->getArgs()[i];
                if (arg.value) {
                    TypePtr pType = (methodDecl && i < methodDecl->getParams().size())
                        ? Type::fromAnnotation(methodDecl->getParams()[i].type.get())
                        : nullptr;
                    TypePtr expectedPType = substituteTypeParams(pType, methodSubst);
                    auto argType = inferType(*arg.value, scope, expectedPType);
                    if (pType && !methodParamSet.empty()) {
                        deduceTypeParams(pType, argType, methodParamSet, methodSubst);
                        expectedPType = substituteTypeParams(pType, methodSubst);
                    }
                    if (expectedPType) {
                        checkAssignment(expectedPType, argType, arg.value->getSpan(), "method argument");
                    }
                    bool isMut = arg.isMut || (methodDecl && i < methodDecl->getParams().size() && methodDecl->getParams()[i].isMut);
                    checkCallArg(arg, isMut);
                }
            }
            for (const auto& v : mutatedVars) {
                scope.resetNarrowedType(v);
            }
            if (methodDecl) {
                auto retType = Type::fromAnnotation(methodDecl->getReturnType());
                return substituteTypeParams(retType, methodSubst);
            }
            return Type::unknownType();
        }

        // Function call: lookup in scope and check arguments
        if (const auto* calleeIdent = dynamic_cast<const ast::IdentExpr*>(call->getCallee())) {
            auto sym = scope.lookup(calleeIdent->getName());
            const ast::FnDecl* fnDecl = nullptr;
            const ast::ClassDecl* clsDecl = nullptr;
            if (sym && sym->getAstNode()) {
                fnDecl = dynamic_cast<const ast::FnDecl*>(sym->getAstNode());
                clsDecl = dynamic_cast<const ast::ClassDecl*>(sym->getAstNode());
            }

            // Check overloads if sym itself is not a matching FnDecl
            if (!fnDecl && sym && sym->hasOverloads()) {
                for (const auto& ov : sym->getOverloads()) {
                    if (ov && ov->getAstNode()) {
                        if (const auto* fd = dynamic_cast<const ast::FnDecl*>(ov->getAstNode())) {
                            if (fd->getParams().size() == call->getArgs().size()) {
                                fnDecl = fd;
                                break;
                            }
                        }
                    }
                }
            }

            const ast::FunctionType* funcTypeAnnotation = nullptr;
            if (sym && sym->getTypeAnnotation()) {
                funcTypeAnnotation = dynamic_cast<const ast::FunctionType*>(sym->getTypeAnnotation());
            }

            if (fnDecl) {
                std::unordered_map<std::string, TypePtr> typeSubst;
                std::unordered_set<std::string> typeParamSet(
                    fnDecl->getTypeParams().begin(), fnDecl->getTypeParams().end()
                );
                for (size_t k = 0; k < call->getTypeArgs().size() && k < fnDecl->getTypeParams().size(); ++k) {
                    if (call->getTypeArgs()[k]) {
                        typeSubst[fnDecl->getTypeParams()[k]] = Type::fromAnnotation(call->getTypeArgs()[k].get());
                    }
                }

                for (size_t i = 0; i < call->getArgs().size(); ++i) {
                    const auto& arg = call->getArgs()[i];
                    if (arg.value) {
                        TypePtr pType = (i < fnDecl->getParams().size())
                            ? Type::fromAnnotation(fnDecl->getParams()[i].type.get())
                            : nullptr;
                        TypePtr expectedPType = substituteTypeParams(pType, typeSubst);
                        auto argType = inferType(*arg.value, scope, expectedPType);
                        if (pType && !typeParamSet.empty()) {
                            deduceTypeParams(pType, argType, typeParamSet, typeSubst);
                            expectedPType = substituteTypeParams(pType, typeSubst);
                        }
                        if (expectedPType) {
                            checkAssignment(expectedPType, argType, arg.value->getSpan(), "function argument");
                        }
                        bool isMut = arg.isMut || (i < fnDecl->getParams().size() && fnDecl->getParams()[i].isMut);
                        checkCallArg(arg, isMut);
                    }
                }
                for (const auto& v : mutatedVars) {
                    scope.resetNarrowedType(v);
                }
                auto retType = Type::fromAnnotation(fnDecl->getReturnType());
                return substituteTypeParams(retType, typeSubst);
            } else if (funcTypeAnnotation) {
                for (size_t i = 0; i < call->getArgs().size(); ++i) {
                    const auto& arg = call->getArgs()[i];
                    if (arg.value) {
                        TypePtr pType = (i < funcTypeAnnotation->getParams().size())
                            ? Type::fromAnnotation(funcTypeAnnotation->getParams()[i].type.get())
                            : nullptr;
                        auto argType = inferType(*arg.value, scope, pType);
                        if (pType) {
                            checkAssignment(pType, argType, arg.value->getSpan(), "function argument");
                        }
                        bool isMut = arg.isMut || (i < funcTypeAnnotation->getParams().size() && funcTypeAnnotation->getParams()[i].isMut);
                        checkCallArg(arg, isMut);
                    }
                }
                for (const auto& v : mutatedVars) {
                    scope.resetNarrowedType(v);
                }
                return Type::fromAnnotation(funcTypeAnnotation->getReturnType());
            } else if (clsDecl) {
                for (size_t i = 0; i < call->getArgs().size(); ++i) {
                    const auto& arg = call->getArgs()[i];
                    if (arg.value) {
                        TypePtr pType = (i < clsDecl->getFields().size())
                            ? Type::fromAnnotation(clsDecl->getFields()[i].type.get())
                            : nullptr;
                        auto argType = inferType(*arg.value, scope, pType);
                        if (pType) {
                            checkAssignment(pType, argType, arg.value->getSpan(), "constructor argument");
                        }
                        checkCallArg(arg, arg.isMut);
                    }
                }
                for (const auto& v : mutatedVars) {
                    scope.resetNarrowedType(v);
                }
                return Type::custom(clsDecl->getName());
            }
        }

        for (const auto& arg : call->getArgs()) {
            if (arg.value) inferType(*arg.value, scope);
            checkCallArg(arg, arg.isMut);
        }
        for (const auto& v : mutatedVars) {
            scope.resetNarrowedType(v);
        }
        return Type::unknownType();
    }

    if (const auto* ternary = dynamic_cast<const ast::TernaryExpr*>(&expr)) {
        if (ternary->getCondition()) inferType(*ternary->getCondition(), scope);

        BranchNarrowing narrowing = analyzeCondition(ternary->getCondition(), scope);

        auto thenScope = scope.createChild(sema::ScopeKind::Block);
        for (const auto& fact : narrowing.thenFacts) {
            thenScope->setNarrowedType(fact.varName, fact.narrowedType);
        }
        auto thenType = ternary->getThenExpr() ? inferType(*ternary->getThenExpr(), *thenScope, contextualType) : Type::unknownType();

        auto elseScope = scope.createChild(sema::ScopeKind::Block);
        for (const auto& fact : narrowing.elseFacts) {
            elseScope->setNarrowedType(fact.varName, fact.narrowedType);
        }
        auto elseType = ternary->getElseExpr() ? inferType(*ternary->getElseExpr(), *elseScope, contextualType) : Type::unknownType();

        if ((thenType->isInt() && elseType->isFloat()) ||
            (thenType->isFloat() && elseType->isInt())) {
            reportImplicitWidening(thenType, elseType, ternary->getSpan(), "ternary branches");
        }
        return contextualType ? contextualType : Type::unionOf({thenType, elseType});
    }

    if (const auto* tpl = dynamic_cast<const ast::TemplateLitExpr*>(&expr)) {
        for (const auto& part : tpl->getParts()) {
            if (part.isExpr && part.expr) {
                inferType(*part.expr, scope);
            }
        }
        return Type::stringType();
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
        std::vector<TypePtr> paramTypes;
        checkTypeAnnotationValidity(closure->getReturnType());
        for (const auto& p : closure->getParams()) {
            checkTypeAnnotationValidity(p.type.get());
            if (p.isMut) {
                if (closure->isAsync()) {
                    reportAsyncMutableParameter(p.name, p.span);
                }
                if (p.defaultValue) {
                    reportDefaultArgMutableBorrow(p.name, p.defaultValue->getSpan().isValid() ? p.defaultValue->getSpan() : p.span);
                }
            }
            auto pType = Type::fromAnnotation(p.type.get());
            paramTypes.push_back(pType ? pType : Type::unknownType());
            auto pSym = std::make_shared<sema::Symbol>(p.name, sema::SymbolKind::Parameter, p.span);
            pSym->setTypeAnnotation(p.type.get());
            pSym->setMutable(p.isMut);
            pSym->setConst(!p.isMut);
            auto res = sema::ShadowChecker::defineWithDiagnostics(*fnScope, pSym, emitter_);
            if (!res.success) hasErrors_ = true;

            if (p.defaultValue) {
                auto defType = inferType(*p.defaultValue, *fnScope, pType);
                checkAssignment(pType, defType, p.defaultValue->getSpan(), "parameter default value");
            }
        }
        auto retType = Type::fromAnnotation(closure->getReturnType());
        if (closure->getBody()) {
            auto prevRet = currentFunctionReturnType_;
            currentFunctionReturnType_ = retType;
            checkZeroCoercion(*closure->getBody(), *fnScope);
            checkExhaustiveness(closure->getBody(), "<anonymous closure>", retType, closure->getSpan());
            currentFunctionReturnType_ = prevRet;
        }
        return Type::function(std::move(paramTypes), retType ? retType : Type::voidType(), closure->isAsync());
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
        if (const auto* objIdent = dynamic_cast<const ast::IdentExpr*>(member->getObject())) {
            if (objIdent->getName() == "this") {
                auto fieldSym = scope.lookup(member->getMemberName());
                if (fieldSym && fieldSym->isField() && fieldSym->getTypeAnnotation()) {
                    return Type::fromAnnotation(fieldSym->getTypeAnnotation());
                }
            }
        }
        auto objType = member->getObject() ? inferType(*member->getObject(), scope) : Type::unknownType();
        if (objType && (objType->isError() || (objType->isCustom() && static_cast<const CustomType*>(objType.get())->getName() == "error"))) {
            if (member->getMemberName() == "message" || member->getMemberName() == "name") {
                return Type::stringType();
            }
        }
        if (objType && objType->isCustom()) {
            auto clsName = static_cast<const CustomType*>(objType.get())->getName();
            auto clsSym = scope.lookup(clsName);
            if (clsSym && clsSym->getAstNode()) {
                const auto* clsDecl = dynamic_cast<const ast::ClassDecl*>(clsSym->getAstNode());
                while (clsDecl) {
                    for (const auto& f : clsDecl->getFields()) {
                        if (f.name == member->getMemberName()) {
                            return Type::fromAnnotation(f.type.get());
                        }
                    }
                    if (clsDecl->hasBaseClass()) {
                        auto baseSym = scope.lookup(clsDecl->getBaseClass());
                        clsDecl = (baseSym && baseSym->getAstNode()) ? dynamic_cast<const ast::ClassDecl*>(baseSym->getAstNode()) : nullptr;
                    } else {
                        clsDecl = nullptr;
                    }
                }
            }
        }
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
            auto blockScope = scope.createChild(sema::ScopeKind::Block);
            if (!zonk->getErrorVarName().empty() && zonk->getErrorVarName() != "_") {
                auto errSym = std::make_shared<sema::Symbol>(zonk->getErrorVarName(), sema::SymbolKind::Variable, zonk->getSpan());
                auto res = sema::ShadowChecker::defineWithDiagnostics(*blockScope, errSym, emitter_);
                if (!res.success) hasErrors_ = true;
                blockScope->setNarrowedType(zonk->getErrorVarName(), Type::errorType());
            }
            checkZeroCoercion(*zonk->getRecoveryBlock(), *blockScope);
        }
        return contextualType ? contextualType : pType;
    }

    if (const auto* aw = dynamic_cast<const ast::AwaitExpr*>(&expr)) {
        auto targetType = aw->getTarget() ? inferType(*aw->getTarget(), scope) : Type::unknownType();
        if (targetType && targetType->isFunction()) {
            const auto& fnType = static_cast<const FunctionType&>(*targetType);
            return fnType.getReturnType();
        }
        return targetType;
    }

    return Type::unknownType();
}

TypeChecker::ReachabilityResult TypeChecker::checkReachability(const ast::Stmt* stmt) {
    ReachabilityResult result;
    if (!stmt) return result;

    if (const auto* retStmt = dynamic_cast<const ast::ReturnStmt*>(stmt)) {
        if (retStmt->hasValue()) {
            checkClosuresInExpr(retStmt->getValue());
        }
        result.locallyTerminates = true;
        result.functionExits = true;
        return result;
    }

    if (const auto* zonkStmt = dynamic_cast<const ast::ZonkStmt*>(stmt)) {
        if (zonkStmt->getErrorExpr()) {
            checkClosuresInExpr(zonkStmt->getErrorExpr());
        }
        result.locallyTerminates = true;
        result.functionExits = true;
        return result;
    }

    if (const auto* yieldStmt = dynamic_cast<const ast::YieldStmt*>(stmt)) {
        if (yieldStmt->getValue()) {
            checkClosuresInExpr(yieldStmt->getValue());
        }
        result.locallyTerminates = true;
        result.functionExits = false;
        return result;
    }

    if (dynamic_cast<const ast::BreakStmt*>(stmt)) {
        result.locallyTerminates = true;
        result.functionExits = false;
        return result;
    }

    if (dynamic_cast<const ast::ContinueStmt*>(stmt)) {
        result.locallyTerminates = true;
        result.functionExits = false;
        return result;
    }

    if (const auto* blockStmt = dynamic_cast<const ast::BlockStmt*>(stmt)) {
        bool terminated = false;
        ReachabilityResult lastResult;

        for (const auto& s : blockStmt->getStatements()) {
            if (!s) continue;
            if (terminated) {
                reportUnreachableCode(s->getSpan());
                break;
            }
            auto sub = checkReachability(s.get());
            if (sub.locallyTerminates) {
                terminated = true;
                lastResult = sub;
            }
        }

        if (terminated) {
            return lastResult;
        }
        return result;
    }

    if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(stmt)) {
        if (ifStmt->getCondition()) {
            checkClosuresInExpr(ifStmt->getCondition());
        }

        auto thenRes = checkReachability(ifStmt->getThenBranch());
        ReachabilityResult elseRes;
        if (ifStmt->hasElseBranch()) {
            elseRes = checkReachability(ifStmt->getElseBranch());
            result.locallyTerminates = thenRes.locallyTerminates && elseRes.locallyTerminates;
            result.functionExits = thenRes.functionExits && elseRes.functionExits;
        } else {
            result.locallyTerminates = false;
            result.functionExits = false;
        }
        return result;
    }

    if (const auto* switchStmt = dynamic_cast<const ast::SwitchStmt*>(stmt)) {
        if (switchStmt->getDiscriminant()) {
            checkClosuresInExpr(switchStmt->getDiscriminant());
        }

        bool hasDefault = false;
        bool allFunctionExit = !switchStmt->getCases().empty();

        for (const auto& c : switchStmt->getCases()) {
            if (c.isDefault()) hasDefault = true;
            auto cRes = checkReachability(c.body.get());
            if (!cRes.functionExits) {
                allFunctionExit = false;
            }
        }

        if (hasDefault && allFunctionExit) {
            result.locallyTerminates = true;
            result.functionExits = true;
        } else {
            result.locallyTerminates = false;
            result.functionExits = false;
        }
        return result;
    }

    if (const auto* whileStmt = dynamic_cast<const ast::WhileStmt*>(stmt)) {
        if (whileStmt->getCondition()) {
            checkClosuresInExpr(whileStmt->getCondition());
        }

        checkReachability(whileStmt->getBody());

        bool isInf = isTrueCondition(whileStmt->getCondition()) &&
                     !hasBreakTargetingLoop(whileStmt->getBody(), whileStmt->getLabel());
        if (isInf) {
            result.locallyTerminates = true;
            result.functionExits = true;
        } else {
            result.locallyTerminates = false;
            result.functionExits = false;
        }
        return result;
    }

    if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(stmt)) {
        if (forStmt->getCondition()) {
            checkClosuresInExpr(forStmt->getCondition());
        }

        checkReachability(forStmt->getBody());

        bool isInf = (!forStmt->getCondition() || isTrueCondition(forStmt->getCondition())) &&
                     !hasBreakTargetingLoop(forStmt->getBody(), forStmt->getLabel());
        if (isInf) {
            result.locallyTerminates = true;
            result.functionExits = true;
        } else {
            result.locallyTerminates = false;
            result.functionExits = false;
        }
        return result;
    }

    if (const auto* forInStmt = dynamic_cast<const ast::ForInStmt*>(stmt)) {
        if (forInStmt->getCollection()) {
            checkClosuresInExpr(forInStmt->getCollection());
        }
        checkReachability(forInStmt->getBody());
        result.locallyTerminates = false;
        result.functionExits = false;
        return result;
    }

    if (const auto* exprStmt = dynamic_cast<const ast::ExprStmt*>(stmt)) {
        if (exprStmt->getExpression()) {
            checkClosuresInExpr(exprStmt->getExpression());
        }
        return result;
    }

    if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(stmt)) {
        if (const auto* decl = declStmt->getDeclaration()) {
            if (const auto* vd = dynamic_cast<const ast::VarDecl*>(decl)) {
                if (vd->hasInitializer()) checkClosuresInExpr(vd->getInitializer());
            } else if (const auto* cd = dynamic_cast<const ast::ConstDecl*>(decl)) {
                if (cd->getInitializer()) checkClosuresInExpr(cd->getInitializer());
            } else if (const auto* dd = dynamic_cast<const ast::DefineDecl*>(decl)) {
                if (dd->getValue()) checkClosuresInExpr(dd->getValue());
            }
        }
        return result;
    }

    return result;
}

bool TypeChecker::checkExhaustiveness(const ast::Stmt* body, std::string_view name, TypePtr returnType, diag::SourceSpan span) {
    if (!body) return false;

    auto termRes = checkReachability(body);

    if (returnType && !returnType->isVoid()) {
        if (!termRes.functionExits) {
            reportMissingReturn(name, returnType, span);
            return false;
        }
    }
    return true;
}

void TypeChecker::checkClosuresInExpr(const ast::Expr* expr) {
    if (!expr) return;

    if (const auto* closure = dynamic_cast<const ast::ClosureExpr*>(expr)) {
        auto retType = Type::fromAnnotation(closure->getReturnType());
        if (closure->getBody()) {
            checkExhaustiveness(closure->getBody(), "<anonymous closure>", retType, closure->getSpan());
        }
        return;
    }

    if (const auto* binary = dynamic_cast<const ast::BinaryExpr*>(expr)) {
        checkClosuresInExpr(binary->getLeft());
        checkClosuresInExpr(binary->getRight());
    } else if (const auto* unary = dynamic_cast<const ast::UnaryExpr*>(expr)) {
        checkClosuresInExpr(unary->getOperand());
    } else if (const auto* call = dynamic_cast<const ast::CallExpr*>(expr)) {
        checkClosuresInExpr(call->getCallee());
        for (const auto& arg : call->getArgs()) {
            checkClosuresInExpr(arg.value.get());
        }
    } else if (const auto* ternary = dynamic_cast<const ast::TernaryExpr*>(expr)) {
        checkClosuresInExpr(ternary->getCondition());
        checkClosuresInExpr(ternary->getThenExpr());
        checkClosuresInExpr(ternary->getElseExpr());
    } else if (const auto* arr = dynamic_cast<const ast::ArrayLitExpr*>(expr)) {
        for (const auto& el : arr->getElements()) {
            checkClosuresInExpr(el.get());
        }
    } else if (const auto* map = dynamic_cast<const ast::MapLitExpr*>(expr)) {
        for (const auto& entry : map->getEntries()) {
            checkClosuresInExpr(entry.key.get());
            checkClosuresInExpr(entry.value.get());
        }
    } else if (const auto* cast = dynamic_cast<const ast::TypeCastExpr*>(expr)) {
        checkClosuresInExpr(cast->getExpr());
    } else if (const auto* index = dynamic_cast<const ast::IndexExpr*>(expr)) {
        checkClosuresInExpr(index->getContainer());
        checkClosuresInExpr(index->getIndex());
    } else if (const auto* member = dynamic_cast<const ast::MemberExpr*>(expr)) {
        checkClosuresInExpr(member->getObject());
    } else if (const auto* typeofExpr = dynamic_cast<const ast::TypeofExpr*>(expr)) {
        checkClosuresInExpr(typeofExpr->getExpr());
    } else if (const auto* zonk = dynamic_cast<const ast::ZonkExpr*>(expr)) {
        checkClosuresInExpr(zonk->getPrimary());
        checkClosuresInExpr(zonk->getFallback());
        if (zonk->getRecoveryBlock()) {
            checkReachability(zonk->getRecoveryBlock());
        }
    } else if (const auto* tpl = dynamic_cast<const ast::TemplateLitExpr*>(expr)) {
        for (const auto& part : tpl->getParts()) {
            if (part.isExpr && part.expr) checkClosuresInExpr(part.expr.get());
        }
    }
}

} // namespace keety::typechecker
