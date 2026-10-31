#include <keety/ir/ast_lowerer.hpp>
#include <keety/typechecker/type.hpp>

#include <stdexcept>

namespace keety::ir {

AstLowerer::AstLowerer(diag::DiagnosticEmitter* emitter)
    : emitter_(emitter) {}

BasicBlock* AstLowerer::create_block(const std::string& prefix) {
    if (!current_fn_) {
        throw std::runtime_error("Cannot create basic block outside of a function context");
    }
    return current_fn_->create_basic_block(prefix);
}

void AstLowerer::emit(std::unique_ptr<Instruction> inst) {
    if (!current_bb_) {
        throw std::runtime_error("Cannot emit instruction into null basic block");
    }
    current_bb_->append_instruction(std::move(inst));
}

std::unique_ptr<Module> AstLowerer::lower_module(const parser::ModuleAST& module_ast) {
    auto mod = std::make_unique<Module>(module_ast.moduleName);
    current_module_ = mod.get();

    for (const auto& decl : module_ast.declarations) {
        if (!decl) continue;

        if (const auto* var_decl = dynamic_cast<const ast::VarDecl*>(decl.get())) {
            typechecker::TypePtr type = typechecker::Type::fromAnnotation(var_decl->getType());
            std::optional<Operand> init_val = std::nullopt;
            if (var_decl->hasInitializer()) {
                // If it's a literal or simple initializer, we can extract constant
                if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(var_decl->getInitializer())) {
                    init_val = lower_literal_expr(*lit);
                }
            }
            mod->add_global({var_decl->getName(), type, false, var_decl->isExported(), init_val});
        } else if (const auto* const_decl = dynamic_cast<const ast::ConstDecl*>(decl.get())) {
            typechecker::TypePtr type = typechecker::Type::fromAnnotation(const_decl->getType());
            std::optional<Operand> init_val = std::nullopt;
            if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(const_decl->getInitializer())) {
                init_val = lower_literal_expr(*lit);
            }
            mod->add_global({const_decl->getName(), type, true, const_decl->isExported(), init_val});
        } else if (const auto* def_decl = dynamic_cast<const ast::DefineDecl*>(decl.get())) {
            typechecker::TypePtr type = typechecker::Type::fromAnnotation(def_decl->getType());
            std::optional<Operand> init_val = std::nullopt;
            if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(def_decl->getValue())) {
                init_val = lower_literal_expr(*lit);
            }
            mod->add_global({def_decl->getName(), type, true, def_decl->isExported(), init_val});
        } else if (const auto* fn_decl = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            auto fn = lower_function(*fn_decl);
            mod->add_function(std::move(fn));
        }
    }

    mod->recompute_cfg();
    current_module_ = nullptr;
    return mod;
}

std::unique_ptr<Function> AstLowerer::lower_function(const ast::FnDecl& fn_decl) {
    typechecker::TypePtr return_type = fn_decl.getReturnType()
        ? typechecker::Type::fromAnnotation(fn_decl.getReturnType())
        : typechecker::Type::voidType();

    auto fn = std::make_unique<Function>(fn_decl.getName(), return_type, fn_decl.isAsync());
    current_fn_ = fn.get();

    for (const auto& param : fn_decl.getParams()) {
        typechecker::TypePtr param_type = typechecker::Type::fromAnnotation(param.type.get());
        fn->add_parameter({param.name, param_type, param.isMut, false, param.defaultValue != nullptr});
    }

    BasicBlock* entry = fn->create_basic_block("entry");
    fn->set_entry_block(entry);
    current_bb_ = entry;

    loop_stack_.clear();
    switch_stack_.clear();
    zonk_stack_.clear();
    break_order_.clear();

    if (fn_decl.getBody()) {
        lower_statement(*fn_decl.getBody());
    }

    if (current_bb_ && !current_bb_->has_terminator()) {
        if (return_type->getKind() == typechecker::TypeKind::Void) {
            emit(std::make_unique<ReturnInst>());
        } else {
            emit(std::make_unique<UnreachableInst>());
        }
    }

    fn->recompute_cfg_edges();
    current_fn_ = nullptr;
    current_bb_ = nullptr;
    return fn;
}

void AstLowerer::lower_statement(const ast::Stmt& stmt) {
    if (current_bb_ && current_bb_->has_terminator()) {
        // Current block is already terminated; any subsequent statement would be unreachable
        current_bb_ = create_block("unreachable");
    }

    if (const auto* block_stmt = dynamic_cast<const ast::BlockStmt*>(&stmt)) {
        lower_block_stmt(*block_stmt);
    } else if (const auto* decl_stmt = dynamic_cast<const ast::DeclStmt*>(&stmt)) {
        lower_decl_stmt(*decl_stmt);
    } else if (const auto* expr_stmt = dynamic_cast<const ast::ExprStmt*>(&stmt)) {
        lower_expr_stmt(*expr_stmt);
    } else if (const auto* if_stmt = dynamic_cast<const ast::IfStmt*>(&stmt)) {
        lower_if_stmt(*if_stmt);
    } else if (const auto* while_stmt = dynamic_cast<const ast::WhileStmt*>(&stmt)) {
        lower_while_stmt(*while_stmt);
    } else if (const auto* for_stmt = dynamic_cast<const ast::ForStmt*>(&stmt)) {
        lower_for_stmt(*for_stmt);
    } else if (const auto* for_in_stmt = dynamic_cast<const ast::ForInStmt*>(&stmt)) {
        lower_for_in_stmt(*for_in_stmt);
    } else if (const auto* switch_stmt = dynamic_cast<const ast::SwitchStmt*>(&stmt)) {
        lower_switch_stmt(*switch_stmt);
    } else if (const auto* return_stmt = dynamic_cast<const ast::ReturnStmt*>(&stmt)) {
        lower_return_stmt(*return_stmt);
    } else if (const auto* break_stmt = dynamic_cast<const ast::BreakStmt*>(&stmt)) {
        lower_break_stmt(*break_stmt);
    } else if (const auto* continue_stmt = dynamic_cast<const ast::ContinueStmt*>(&stmt)) {
        lower_continue_stmt(*continue_stmt);
    } else if (const auto* zonk_stmt = dynamic_cast<const ast::ZonkStmt*>(&stmt)) {
        lower_zonk_stmt(*zonk_stmt);
    } else if (const auto* yield_stmt = dynamic_cast<const ast::YieldStmt*>(&stmt)) {
        lower_yield_stmt(*yield_stmt);
    }
}

void AstLowerer::lower_block_stmt(const ast::BlockStmt& stmt) {
    for (const auto& inner : stmt.getStatements()) {
        if (inner) {
            lower_statement(*inner);
        }
    }
}

void AstLowerer::lower_decl_stmt(const ast::DeclStmt& stmt) {
    const auto* decl = stmt.getDeclaration();
    if (!decl) return;

    if (const auto* var_decl = dynamic_cast<const ast::VarDecl*>(decl)) {
        typechecker::TypePtr type = typechecker::Type::fromAnnotation(var_decl->getType());
        emit(std::make_unique<AllocaInst>(var_decl->getName(), type, true));

        if (var_decl->hasInitializer()) {
            Operand init_op = lower_expression(*var_decl->getInitializer());
            Operand dest = Operand::create_var(var_decl->getName(), type);
            emit(std::make_unique<StoreInst>(dest, init_op));
        }
    } else if (const auto* const_decl = dynamic_cast<const ast::ConstDecl*>(decl)) {
        typechecker::TypePtr type = typechecker::Type::fromAnnotation(const_decl->getType());
        emit(std::make_unique<AllocaInst>(const_decl->getName(), type, false));

        if (const_decl->getInitializer()) {
            Operand init_op = lower_expression(*const_decl->getInitializer());
            Operand dest = Operand::create_var(const_decl->getName(), type);
            emit(std::make_unique<StoreInst>(dest, init_op));
        }
    } else if (const auto* def_decl = dynamic_cast<const ast::DefineDecl*>(decl)) {
        typechecker::TypePtr type = typechecker::Type::fromAnnotation(def_decl->getType());
        emit(std::make_unique<AllocaInst>(def_decl->getName(), type, false));

        if (def_decl->getValue()) {
            Operand init_op = lower_expression(*def_decl->getValue());
            Operand dest = Operand::create_var(def_decl->getName(), type);
            emit(std::make_unique<StoreInst>(dest, init_op));
        }
    }
}

void AstLowerer::lower_expr_stmt(const ast::ExprStmt& stmt) {
    if (stmt.getExpression()) {
        lower_expression(*stmt.getExpression());
    }
}

void AstLowerer::lower_if_stmt(const ast::IfStmt& stmt) {
    if (stmt.hasInit()) {
        lower_statement(*stmt.getInit());
    }

    Operand cond = lower_expression(*stmt.getCondition());

    BasicBlock* then_bb = create_block("if.then");
    BasicBlock* else_bb = stmt.hasElseBranch() ? create_block("if.else") : nullptr;
    BasicBlock* merge_bb = create_block("if.end");

    BasicBlock* false_target = else_bb ? else_bb : merge_bb;
    emit(std::make_unique<CondBranchInst>(cond, then_bb, false_target));

    // Then block
    current_bb_ = then_bb;
    if (stmt.getThenBranch()) {
        lower_block_stmt(*stmt.getThenBranch());
    }
    if (current_bb_ && !current_bb_->has_terminator()) {
        emit(std::make_unique<BranchInst>(merge_bb));
    }

    // Else block
    if (else_bb) {
        current_bb_ = else_bb;
        lower_statement(*stmt.getElseBranch());
        if (current_bb_ && !current_bb_->has_terminator()) {
            emit(std::make_unique<BranchInst>(merge_bb));
        }
    }

    current_bb_ = merge_bb;
}

void AstLowerer::lower_while_stmt(const ast::WhileStmt& stmt) {
    if (stmt.hasInit()) {
        lower_statement(*stmt.getInit());
    }

    BasicBlock* cond_bb = create_block("while.cond");
    BasicBlock* body_bb = create_block("while.body");
    BasicBlock* exit_bb = create_block("while.end");

    emit(std::make_unique<BranchInst>(cond_bb));

    // Condition block
    current_bb_ = cond_bb;
    Operand cond = lower_expression(*stmt.getCondition());
    emit(std::make_unique<CondBranchInst>(cond, body_bb, exit_bb));

    // Body block
    current_bb_ = body_bb;
    size_t loop_idx = loop_stack_.size();
    loop_stack_.push_back({stmt.getLabel(), cond_bb, exit_bb});
    break_order_.emplace_back(BreakContextKind::Loop, loop_idx);

    if (stmt.getBody()) {
        lower_block_stmt(*stmt.getBody());
    }
    if (current_bb_ && !current_bb_->has_terminator()) {
        emit(std::make_unique<BranchInst>(cond_bb));
    }

    break_order_.pop_back();
    loop_stack_.pop_back();
    current_bb_ = exit_bb;
}

void AstLowerer::lower_for_stmt(const ast::ForStmt& stmt) {
    if (stmt.getInit()) {
        lower_statement(*stmt.getInit());
    }

    BasicBlock* cond_bb = create_block("for.cond");
    BasicBlock* body_bb = create_block("for.body");
    BasicBlock* post_bb = create_block("for.post");
    BasicBlock* exit_bb = create_block("for.end");

    emit(std::make_unique<BranchInst>(cond_bb));

    // Condition block
    current_bb_ = cond_bb;
    if (stmt.getCondition()) {
        Operand cond = lower_expression(*stmt.getCondition());
        emit(std::make_unique<CondBranchInst>(cond, body_bb, exit_bb));
    } else {
        emit(std::make_unique<BranchInst>(body_bb));
    }

    // Body block
    current_bb_ = body_bb;
    size_t loop_idx = loop_stack_.size();
    loop_stack_.push_back({stmt.getLabel(), post_bb, exit_bb});
    break_order_.emplace_back(BreakContextKind::Loop, loop_idx);

    if (stmt.getBody()) {
        lower_block_stmt(*stmt.getBody());
    }
    if (current_bb_ && !current_bb_->has_terminator()) {
        emit(std::make_unique<BranchInst>(post_bb));
    }

    break_order_.pop_back();
    loop_stack_.pop_back();

    // Post block
    current_bb_ = post_bb;
    if (stmt.getPost()) {
        lower_statement(*stmt.getPost());
    }
    if (current_bb_ && !current_bb_->has_terminator()) {
        emit(std::make_unique<BranchInst>(cond_bb));
    }

    current_bb_ = exit_bb;
}

void AstLowerer::lower_for_in_stmt(const ast::ForInStmt& stmt) {
    // Lower collection iteration:
    // coll_op = eval collection
    // alloca _idx: int = 0
    // cond: _idx < coll_op.length()
    // body: item = coll_op[_idx] ...
    // post: _idx = _idx + 1
    Operand coll_op = lower_expression(*stmt.getCollection());

    auto int_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Int);
    std::string idx_name = "_idx." + std::to_string(current_fn_->create_temp(int_type).get_temp_id());
    emit(std::make_unique<AllocaInst>(idx_name, int_type, true));
    emit(std::make_unique<StoreInst>(Operand::create_var(idx_name, int_type), Operand::create_int(0)));

    BasicBlock* cond_bb = create_block("forin.cond");
    BasicBlock* body_bb = create_block("forin.body");
    BasicBlock* post_bb = create_block("forin.post");
    BasicBlock* exit_bb = create_block("forin.end");

    emit(std::make_unique<BranchInst>(cond_bb));

    // Condition
    current_bb_ = cond_bb;
    Operand current_idx = current_fn_->create_temp(int_type);
    emit(std::make_unique<LoadInst>(current_idx, Operand::create_var(idx_name, int_type)));

    Operand len_op = current_fn_->create_temp(int_type);
    emit(std::make_unique<MethodCallInst>(len_op, coll_op, "length", std::vector<Operand>{}));

    auto bool_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Bool);
    Operand cmp_op = current_fn_->create_temp(bool_type);
    emit(std::make_unique<BinaryInst>(BinaryOpKind::Lt, cmp_op, current_idx, len_op));
    emit(std::make_unique<CondBranchInst>(cmp_op, body_bb, exit_bb));

    // Body
    current_bb_ = body_bb;
    size_t loop_idx = loop_stack_.size();
    loop_stack_.push_back({stmt.getLabel(), post_bb, exit_bb});
    break_order_.emplace_back(BreakContextKind::Loop, loop_idx);

    typechecker::TypePtr val_type = stmt.getValueType()
        ? typechecker::Type::fromAnnotation(stmt.getValueType())
        : typechecker::Type::unknownType();

    emit(std::make_unique<AllocaInst>(stmt.getValueName(), val_type, !stmt.isConst()));
    Operand item_val = current_fn_->create_temp(val_type);
    emit(std::make_unique<IndexLoadInst>(item_val, coll_op, current_idx));
    emit(std::make_unique<StoreInst>(Operand::create_var(stmt.getValueName(), val_type), item_val));

    if (stmt.getBody()) {
        lower_block_stmt(*stmt.getBody());
    }
    if (current_bb_ && !current_bb_->has_terminator()) {
        emit(std::make_unique<BranchInst>(post_bb));
    }

    break_order_.pop_back();
    loop_stack_.pop_back();

    // Post
    current_bb_ = post_bb;
    Operand inc_val = current_fn_->create_temp(int_type);
    emit(std::make_unique<BinaryInst>(BinaryOpKind::Add, inc_val, current_idx, Operand::create_int(1)));
    emit(std::make_unique<StoreInst>(Operand::create_var(idx_name, int_type), inc_val));
    emit(std::make_unique<BranchInst>(cond_bb));

    current_bb_ = exit_bb;
}

void AstLowerer::lower_switch_stmt(const ast::SwitchStmt& stmt) {
    if (stmt.hasInit()) {
        lower_statement(*stmt.getInit());
    }

    Operand disc = lower_expression(*stmt.getDiscriminant());

    BasicBlock* exit_bb = create_block("switch.end");
    size_t sw_idx = switch_stack_.size();
    switch_stack_.push_back({stmt.getLabel(), exit_bb});
    break_order_.emplace_back(BreakContextKind::Switch, sw_idx);

    const auto& cases = stmt.getCases();
    const ast::SwitchCase* default_case = nullptr;
    std::vector<const ast::SwitchCase*> value_cases;

    for (const auto& c : cases) {
        if (c.isDefault()) {
            default_case = &c;
        } else {
            value_cases.push_back(&c);
        }
    }

    BasicBlock* default_bb = default_case ? create_block("switch.default") : exit_bb;

    // Build case check sequence
    std::vector<BasicBlock*> case_body_bbs;
    for (size_t i = 0; i < value_cases.size(); ++i) {
        case_body_bbs.push_back(create_block("switch.case." + std::to_string(i)));
    }

    auto bool_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Bool);

    for (size_t i = 0; i < value_cases.size(); ++i) {
        const auto* c = value_cases[i];
        BasicBlock* next_check_bb = (i + 1 < value_cases.size())
            ? create_block("switch.check." + std::to_string(i + 1))
            : default_bb;

        // Multiple values in a single case branch: case (v1, v2)
        Operand combined_match;
        for (size_t v_idx = 0; v_idx < c->values.size(); ++v_idx) {
            Operand val_op = lower_expression(*c->values[v_idx]);
            Operand eq_op = current_fn_->create_temp(bool_type);
            emit(std::make_unique<BinaryInst>(BinaryOpKind::Eq, eq_op, disc, val_op));

            if (v_idx == 0) {
                combined_match = eq_op;
            } else {
                Operand or_op = current_fn_->create_temp(bool_type);
                emit(std::make_unique<BinaryInst>(BinaryOpKind::LogicOr, or_op, combined_match, eq_op));
                combined_match = or_op;
            }
        }

        emit(std::make_unique<CondBranchInst>(combined_match, case_body_bbs[i], next_check_bb));

        // Lower case body
        current_bb_ = case_body_bbs[i];
        if (c->body) {
            lower_block_stmt(*c->body);
        }
        if (current_bb_ && !current_bb_->has_terminator()) {
            emit(std::make_unique<BranchInst>(exit_bb));
        }

        current_bb_ = next_check_bb;
    }

    // Lower default case if present
    if (default_case) {
        current_bb_ = default_bb;
        if (default_case->body) {
            lower_block_stmt(*default_case->body);
        }
        if (current_bb_ && !current_bb_->has_terminator()) {
            emit(std::make_unique<BranchInst>(exit_bb));
        }
    }

    break_order_.pop_back();
    switch_stack_.pop_back();
    current_bb_ = exit_bb;
}

void AstLowerer::lower_return_stmt(const ast::ReturnStmt& stmt) {
    if (stmt.hasValue()) {
        Operand val = lower_expression(*stmt.getValue());
        emit(std::make_unique<ReturnInst>(val));
    } else {
        emit(std::make_unique<ReturnInst>());
    }
}

void AstLowerer::lower_break_stmt(const ast::BreakStmt& stmt) {
    BasicBlock* target = find_break_target(stmt.getTargetLabel());
    if (target) {
        emit(std::make_unique<BranchInst>(target));
    }
}

void AstLowerer::lower_continue_stmt(const ast::ContinueStmt& stmt) {
    BasicBlock* target = find_continue_target(stmt.getTargetLabel());
    if (target) {
        emit(std::make_unique<BranchInst>(target));
    }
}

void AstLowerer::lower_zonk_stmt(const ast::ZonkStmt& stmt) {
    if (stmt.getErrorExpr()) {
        Operand err = lower_expression(*stmt.getErrorExpr());
        emit(std::make_unique<ThrowInst>(err));
    }
}

void AstLowerer::lower_yield_stmt(const ast::YieldStmt& stmt) {
    if (!zonk_stack_.empty() && stmt.getValue()) {
        const auto& ctx = zonk_stack_.back();
        Operand val = lower_expression(*stmt.getValue());
        emit(std::make_unique<StoreInst>(ctx.result_var, val));
        emit(std::make_unique<BranchInst>(ctx.merge_target));
    }
}

BasicBlock* AstLowerer::find_break_target(const std::string& label) const {
    if (!label.empty()) {
        for (auto it = break_order_.rbegin(); it != break_order_.rend(); ++it) {
            if (it->first == BreakContextKind::Loop) {
                if (loop_stack_[it->second].label == label) {
                    return loop_stack_[it->second].break_target;
                }
            } else {
                if (switch_stack_[it->second].label == label) {
                    return switch_stack_[it->second].break_target;
                }
            }
        }
    } else if (!break_order_.empty()) {
        const auto& top = break_order_.back();
        if (top.first == BreakContextKind::Loop) {
            return loop_stack_[top.second].break_target;
        } else {
            return switch_stack_[top.second].break_target;
        }
    }
    return nullptr;
}

BasicBlock* AstLowerer::find_continue_target(const std::string& label) const {
    if (!label.empty()) {
        for (auto it = loop_stack_.rbegin(); it != loop_stack_.rend(); ++it) {
            if (it->label == label) {
                return it->continue_target;
            }
        }
    } else if (!loop_stack_.empty()) {
        return loop_stack_.back().continue_target;
    }
    return nullptr;
}

// -----------------------------------------------------------------------------
// Expression Lowering
// -----------------------------------------------------------------------------

Operand AstLowerer::lower_expression(const ast::Expr& expr) {
    if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(&expr)) {
        return lower_literal_expr(*lit);
    } else if (const auto* ident = dynamic_cast<const ast::IdentExpr*>(&expr)) {
        return lower_ident_expr(*ident);
    } else if (const auto* binary = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
        return lower_binary_expr(*binary);
    } else if (const auto* unary = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
        return lower_unary_expr(*unary);
    } else if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expr)) {
        return lower_call_expr(*call);
    } else if (const auto* member = dynamic_cast<const ast::MemberExpr*>(&expr)) {
        return lower_member_expr(*member);
    } else if (const auto* index = dynamic_cast<const ast::IndexExpr*>(&expr)) {
        return lower_index_expr(*index);
    } else if (const auto* arr = dynamic_cast<const ast::ArrayLitExpr*>(&expr)) {
        return lower_array_lit_expr(*arr);
    } else if (const auto* map = dynamic_cast<const ast::MapLitExpr*>(&expr)) {
        return lower_map_lit_expr(*map);
    } else if (const auto* ternary = dynamic_cast<const ast::TernaryExpr*>(&expr)) {
        return lower_ternary_expr(*ternary);
    } else if (const auto* typecast = dynamic_cast<const ast::TypeCastExpr*>(&expr)) {
        return lower_typecast_expr(*typecast);
    } else if (const auto* typeof_expr = dynamic_cast<const ast::TypeofExpr*>(&expr)) {
        return lower_typeof_expr(*typeof_expr);
    } else if (const auto* zonk_expr = dynamic_cast<const ast::ZonkExpr*>(&expr)) {
        return lower_zonk_expr(*zonk_expr);
    }
    return Operand::create_null();
}

Operand AstLowerer::lower_literal_expr(const ast::LiteralExpr& expr) {
    switch (expr.getLiteralKind()) {
        case lexer::TokenKind::IntLiteral: {
            int64_t val = 0;
            try {
                val = std::stoll(expr.getRawText());
            } catch (...) {
                try {
                    val = static_cast<int64_t>(std::stoull(expr.getRawText()));
                } catch (...) {}
            }
            return Operand::create_int(val);
        }
        case lexer::TokenKind::FloatLiteral:
            return Operand::create_float(std::stod(expr.getRawText()));
        case lexer::TokenKind::CharLiteral:
            return Operand::create_char(expr.getRawText().empty() ? '\0' : expr.getRawText()[0]);
        case lexer::TokenKind::StringLiteral:
        case lexer::TokenKind::TemplateNoSub:
            return Operand::create_string(expr.getRawText());
        case lexer::TokenKind::KwTrue:
            return Operand::create_bool(true);
        case lexer::TokenKind::KwFalse:
            return Operand::create_bool(false);
        case lexer::TokenKind::KwNull:
            return Operand::create_null();
        default:
            return Operand::create_null();
    }
}

Operand AstLowerer::lower_ident_expr(const ast::IdentExpr& expr) {
    return Operand::create_var(expr.getName(), nullptr);
}

Operand AstLowerer::lower_binary_expr(const ast::BinaryExpr& expr) {
    lexer::TokenKind op = expr.getOp();

    // Assignment
    if (op == lexer::TokenKind::Equal) {
        Operand rhs = lower_expression(*expr.getRight());
        if (const auto* ident = dynamic_cast<const ast::IdentExpr*>(expr.getLeft())) {
            Operand var_op = Operand::create_var(ident->getName(), rhs.get_type());
            emit(std::make_unique<StoreInst>(var_op, rhs));
            return rhs;
        } else if (const auto* index = dynamic_cast<const ast::IndexExpr*>(expr.getLeft())) {
            Operand cont = lower_expression(*index->getContainer());
            Operand idx = lower_expression(*index->getIndex());
            emit(std::make_unique<IndexStoreInst>(cont, idx, rhs));
            return rhs;
        } else if (const auto* member = dynamic_cast<const ast::MemberExpr*>(expr.getLeft())) {
            Operand obj = lower_expression(*member->getObject());
            emit(std::make_unique<MemberStoreInst>(obj, member->getMemberName(), rhs));
            return rhs;
        }
        return rhs;
    }

    // Logical Short-Circuiting
    if (op == lexer::TokenKind::AmpAmp) {
        auto bool_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Bool);
        std::string res_var = "_and_res." + std::to_string(current_fn_->create_temp(bool_type).get_temp_id());
        emit(std::make_unique<AllocaInst>(res_var, bool_type, true));

        Operand lhs = lower_expression(*expr.getLeft());
        emit(std::make_unique<StoreInst>(Operand::create_var(res_var, bool_type), lhs));

        BasicBlock* rhs_bb = create_block("and.rhs");
        BasicBlock* merge_bb = create_block("and.end");

        emit(std::make_unique<CondBranchInst>(lhs, rhs_bb, merge_bb));

        current_bb_ = rhs_bb;
        Operand rhs = lower_expression(*expr.getRight());
        emit(std::make_unique<StoreInst>(Operand::create_var(res_var, bool_type), rhs));
        emit(std::make_unique<BranchInst>(merge_bb));

        current_bb_ = merge_bb;
        Operand result = current_fn_->create_temp(bool_type);
        emit(std::make_unique<LoadInst>(result, Operand::create_var(res_var, bool_type)));
        return result;
    }

    if (op == lexer::TokenKind::PipePipe) {
        auto bool_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Bool);
        std::string res_var = "_or_res." + std::to_string(current_fn_->create_temp(bool_type).get_temp_id());
        emit(std::make_unique<AllocaInst>(res_var, bool_type, true));

        Operand lhs = lower_expression(*expr.getLeft());
        emit(std::make_unique<StoreInst>(Operand::create_var(res_var, bool_type), lhs));

        BasicBlock* rhs_bb = create_block("or.rhs");
        BasicBlock* merge_bb = create_block("or.end");

        emit(std::make_unique<CondBranchInst>(lhs, merge_bb, rhs_bb));

        current_bb_ = rhs_bb;
        Operand rhs = lower_expression(*expr.getRight());
        emit(std::make_unique<StoreInst>(Operand::create_var(res_var, bool_type), rhs));
        emit(std::make_unique<BranchInst>(merge_bb));

        current_bb_ = merge_bb;
        Operand result = current_fn_->create_temp(bool_type);
        emit(std::make_unique<LoadInst>(result, Operand::create_var(res_var, bool_type)));
        return result;
    }

    // Standard Binary Operation
    Operand lhs = lower_expression(*expr.getLeft());
    Operand rhs = lower_expression(*expr.getRight());

    BinaryOpKind ir_op = BinaryOpKind::Add;
    typechecker::TypePtr res_type = lhs.get_type();

    switch (op) {
        case lexer::TokenKind::Plus: ir_op = BinaryOpKind::Add; break;
        case lexer::TokenKind::Minus: ir_op = BinaryOpKind::Sub; break;
        case lexer::TokenKind::Star: ir_op = BinaryOpKind::Mul; break;
        case lexer::TokenKind::Slash: ir_op = BinaryOpKind::Div; break;
        case lexer::TokenKind::Percent: ir_op = BinaryOpKind::Mod; break;
        case lexer::TokenKind::EqualEqual:
            ir_op = BinaryOpKind::Eq;
            res_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Bool);
            break;
        case lexer::TokenKind::ExclaimEqual:
            ir_op = BinaryOpKind::Neq;
            res_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Bool);
            break;
        case lexer::TokenKind::Less:
            ir_op = BinaryOpKind::Lt;
            res_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Bool);
            break;
        case lexer::TokenKind::LessEqual:
            ir_op = BinaryOpKind::Lte;
            res_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Bool);
            break;
        case lexer::TokenKind::Greater:
            ir_op = BinaryOpKind::Gt;
            res_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Bool);
            break;
        case lexer::TokenKind::GreaterEqual:
            ir_op = BinaryOpKind::Gte;
            res_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Bool);
            break;
        case lexer::TokenKind::Pipe: ir_op = BinaryOpKind::BitOr; break;
        default: break;
    }

    Operand dest = current_fn_->create_temp(res_type);
    emit(std::make_unique<BinaryInst>(ir_op, dest, lhs, rhs));
    return dest;
}

Operand AstLowerer::lower_unary_expr(const ast::UnaryExpr& expr) {
    Operand operand = lower_expression(*expr.getOperand());
    UnaryOpKind ir_op = UnaryOpKind::Neg;
    typechecker::TypePtr res_type = operand.get_type();

    switch (expr.getOp()) {
        case lexer::TokenKind::Minus: ir_op = UnaryOpKind::Neg; break;
        case lexer::TokenKind::Exclaim:
            ir_op = UnaryOpKind::Not;
            res_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Bool);
            break;
        default: break;
    }

    Operand dest = current_fn_->create_temp(res_type);
    emit(std::make_unique<UnaryInst>(ir_op, dest, operand));
    return dest;
}

Operand AstLowerer::lower_call_expr(const ast::CallExpr& expr) {
    std::vector<Operand> args;
    for (const auto& arg : expr.getArgs()) {
        Operand arg_op = lower_expression(*arg.value);
        if (arg.isMut) {
            arg_op.set_mut_borrow(true);
        }
        args.push_back(std::move(arg_op));
    }

    Operand dest = current_fn_->create_temp(nullptr);

    if (const auto* member = dynamic_cast<const ast::MemberExpr*>(expr.getCallee())) {
        Operand receiver = lower_expression(*member->getObject());
        emit(std::make_unique<MethodCallInst>(dest, receiver, member->getMemberName(), std::move(args)));
    } else if (const auto* ident = dynamic_cast<const ast::IdentExpr*>(expr.getCallee())) {
        emit(std::make_unique<CallInst>(dest, ident->getName(), std::move(args)));
    } else {
        Operand callee_op = lower_expression(*expr.getCallee());
        emit(std::make_unique<CallInst>(dest, callee_op.to_string(), std::move(args)));
    }

    return dest;
}

Operand AstLowerer::lower_member_expr(const ast::MemberExpr& expr) {
    Operand obj = lower_expression(*expr.getObject());
    Operand dest = current_fn_->create_temp(nullptr);
    emit(std::make_unique<MemberLoadInst>(dest, obj, expr.getMemberName()));
    return dest;
}

Operand AstLowerer::lower_index_expr(const ast::IndexExpr& expr) {
    Operand container = lower_expression(*expr.getContainer());
    Operand index = lower_expression(*expr.getIndex());
    Operand dest = current_fn_->create_temp(nullptr);
    emit(std::make_unique<IndexLoadInst>(dest, container, index));
    return dest;
}

Operand AstLowerer::lower_array_lit_expr(const ast::ArrayLitExpr& expr) {
    std::vector<Operand> elements;
    for (const auto& el : expr.getElements()) {
        if (el) {
            elements.push_back(lower_expression(*el));
        }
    }
    Operand dest = current_fn_->create_temp(nullptr);
    emit(std::make_unique<ArrayConstructInst>(dest, std::move(elements)));
    return dest;
}

Operand AstLowerer::lower_map_lit_expr(const ast::MapLitExpr& expr) {
    std::vector<std::pair<Operand, Operand>> entries;
    for (const auto& entry : expr.getEntries()) {
        if (entry.key && entry.value) {
            entries.emplace_back(lower_expression(*entry.key), lower_expression(*entry.value));
        }
    }
    Operand dest = current_fn_->create_temp(nullptr);
    emit(std::make_unique<MapConstructInst>(dest, std::move(entries)));
    return dest;
}

Operand AstLowerer::lower_ternary_expr(const ast::TernaryExpr& expr) {
    Operand cond = lower_expression(*expr.getCondition());

    std::string res_name = "_ternary_res." + std::to_string(current_fn_->create_temp(nullptr).get_temp_id());
    emit(std::make_unique<AllocaInst>(res_name, nullptr, true));

    BasicBlock* then_bb = create_block("ternary.then");
    BasicBlock* else_bb = create_block("ternary.else");
    BasicBlock* merge_bb = create_block("ternary.end");

    emit(std::make_unique<CondBranchInst>(cond, then_bb, else_bb));

    current_bb_ = then_bb;
    Operand then_val = lower_expression(*expr.getThenExpr());
    emit(std::make_unique<StoreInst>(Operand::create_var(res_name, nullptr), then_val));
    emit(std::make_unique<BranchInst>(merge_bb));

    current_bb_ = else_bb;
    Operand else_val = lower_expression(*expr.getElseExpr());
    emit(std::make_unique<StoreInst>(Operand::create_var(res_name, nullptr), else_val));
    emit(std::make_unique<BranchInst>(merge_bb));

    current_bb_ = merge_bb;
    Operand result = current_fn_->create_temp(nullptr);
    emit(std::make_unique<LoadInst>(result, Operand::create_var(res_name, nullptr)));
    return result;
}

Operand AstLowerer::lower_typecast_expr(const ast::TypeCastExpr& expr) {
    Operand src = lower_expression(*expr.getExpr());
    typechecker::TypePtr target_type = typechecker::Type::fromAnnotation(expr.getTargetType());
    Operand dest = current_fn_->create_temp(target_type);
    emit(std::make_unique<TypeCastInst>(dest, src, target_type));
    return dest;
}

Operand AstLowerer::lower_typeof_expr(const ast::TypeofExpr& expr) {
    Operand src = lower_expression(*expr.getExpr());
    auto str_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::String);
    Operand dest = current_fn_->create_temp(str_type);
    emit(std::make_unique<TypeOfInst>(dest, src));
    return dest;
}

Operand AstLowerer::lower_zonk_expr(const ast::ZonkExpr& expr) {
    std::string res_name = "_zonk_res." + std::to_string(current_fn_->create_temp(nullptr).get_temp_id());
    emit(std::make_unique<AllocaInst>(res_name, nullptr, true));
    Operand res_var = Operand::create_var(res_name, nullptr);

    BasicBlock* try_bb = create_block("zonk.try");
    BasicBlock* catch_bb = create_block("zonk.catch");
    BasicBlock* merge_bb = create_block("zonk.end");

    emit(std::make_unique<BranchInst>(try_bb));

    // Try block (protected region)
    current_bb_ = try_bb;
    try_bb->set_landing_pad(catch_bb);

    if (expr.getPrimary()) {
        Operand primary_val = lower_expression(*expr.getPrimary());
        emit(std::make_unique<StoreInst>(res_var, primary_val));
    }
    if (current_bb_ && !current_bb_->has_terminator()) {
        emit(std::make_unique<BranchInst>(merge_bb));
    }

    // Catch / Landing pad block
    current_bb_ = catch_bb;

    if (expr.hasRecoveryBlock()) {
        std::string err_ident = expr.getErrorVarName().empty()
            ? ("_err." + std::to_string(current_fn_->create_temp(nullptr).get_temp_id()))
            : expr.getErrorVarName();

        auto err_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Error);
        Operand err_dest = Operand::create_var(err_ident, err_type);
        emit(std::make_unique<LandingPadInst>(err_dest));
        emit(std::make_unique<AllocaInst>(err_ident, err_type, false));

        zonk_stack_.push_back({res_var, merge_bb});
        lower_block_stmt(*expr.getRecoveryBlock());
        zonk_stack_.pop_back();

        if (current_bb_ && !current_bb_->has_terminator()) {
            emit(std::make_unique<BranchInst>(merge_bb));
        }
    } else if (expr.getFallback()) {
        std::string err_ident = "_err." + std::to_string(current_fn_->create_temp(nullptr).get_temp_id());
        auto err_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Error);
        Operand err_dest = Operand::create_var(err_ident, err_type);
        emit(std::make_unique<LandingPadInst>(err_dest));
        emit(std::make_unique<AllocaInst>(err_ident, err_type, false));

        Operand fallback_val = lower_expression(*expr.getFallback());
        emit(std::make_unique<StoreInst>(res_var, fallback_val));
        if (current_bb_ && !current_bb_->has_terminator()) {
            emit(std::make_unique<BranchInst>(merge_bb));
        }
    } else {
        std::string err_ident = "_err." + std::to_string(current_fn_->create_temp(nullptr).get_temp_id());
        auto err_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Error);
        Operand err_dest = Operand::create_var(err_ident, err_type);
        emit(std::make_unique<LandingPadInst>(err_dest));
        emit(std::make_unique<BranchInst>(merge_bb));
    }

    // Merge block
    current_bb_ = merge_bb;
    Operand result = current_fn_->create_temp(nullptr);
    emit(std::make_unique<LoadInst>(result, res_var));
    return result;
}

} // namespace keety::ir
