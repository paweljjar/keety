#include <keety/ir/move_insertion.hpp>

namespace keety::ir {

typechecker::TypePtr MoveInsertionPass::resolve_variable_type(const std::string& var_name, const Function& fn) const {
    for (const auto& param : fn.get_parameters()) {
        if (param.name == var_name && param.type) {
            return param.type;
        }
    }

    for (const auto& bb : fn.get_blocks()) {
        if (!bb) continue;
        for (const auto& inst : bb->get_instructions()) {
            if (inst && inst->get_opcode() == Opcode::Alloca) {
                const auto& alloca = static_cast<const AllocaInst&>(*inst);
                if (alloca.get_name() == var_name && alloca.get_type()) {
                    return alloca.get_type();
                }
            }
        }
    }

    return nullptr;
}

bool MoveInsertionPass::is_rvalue_eligible(const Operand& op, const Function& fn) const {
    if (!op.is_variable()) {
        return false;
    }
    if (op.is_global() || op.is_mut_borrow()) {
        return false;
    }

    // Mutable reference parameters must not be moved from
    for (const auto& param : fn.get_parameters()) {
        if (param.name == op.get_name() && param.is_mut) {
            return false;
        }
    }

    if (!options_.move_scalars) {
        typechecker::TypePtr type = op.get_type();
        if (!type) {
            type = resolve_variable_type(op.get_name(), fn);
        }
        if (type) {
            if (type->isPrimitiveInt() || type->isPrimitiveFloat() ||
                type->isPrimitiveBool() || type->isPrimitiveChar()) {
                return false;
            }
        }
    }

    return true;
}

bool MoveInsertionPass::try_mark_move(Operand& op, const Instruction* inst, const Function& fn, const LivenessResult& liveness) {
    if (!is_rvalue_eligible(op, fn)) {
        return false;
    }
    if (op.is_move()) {
        return false;
    }
    if (liveness.is_last_use(inst, op.get_name())) {
        op.set_move(true);
        return true;
    }
    return false;
}

size_t MoveInsertionPass::run_on_function(Function& fn, const LivenessResult& liveness) {
    size_t move_count = 0;

    for (const auto& bb : fn.get_blocks()) {
        if (!bb) continue;

        for (const auto& inst : bb->get_instructions()) {
            if (!inst) continue;

            switch (inst->get_opcode()) {
                case Opcode::Return: {
                    auto& ret = static_cast<ReturnInst&>(*inst);
                    if (ret.has_value()) {
                        if (try_mark_move(*ret.get_value(), inst.get(), fn, liveness)) {
                            ++move_count;
                        }
                    }
                    break;
                }
                case Opcode::Throw: {
                    auto& thr = static_cast<ThrowInst&>(*inst);
                    if (try_mark_move(thr.get_error(), inst.get(), fn, liveness)) {
                        ++move_count;
                    }
                    break;
                }
                case Opcode::Store: {
                    auto& store = static_cast<StoreInst&>(*inst);
                    if (try_mark_move(store.get_val(), inst.get(), fn, liveness)) {
                        ++move_count;
                    }
                    break;
                }
                case Opcode::Assign: {
                    auto& assign = static_cast<AssignInst&>(*inst);
                    if (try_mark_move(assign.get_val(), inst.get(), fn, liveness)) {
                        ++move_count;
                    }
                    break;
                }
                case Opcode::Call: {
                    auto& call = static_cast<CallInst&>(*inst);
                    std::unordered_map<std::string, size_t> var_counts;
                    for (const auto& arg : call.get_args()) {
                        if (arg.is_variable()) {
                            var_counts[arg.get_name()]++;
                        }
                    }
                    for (auto& arg : call.get_args()) {
                        if (arg.is_variable() && var_counts[arg.get_name()] == 1) {
                            if (try_mark_move(arg, inst.get(), fn, liveness)) {
                                ++move_count;
                            }
                        }
                    }
                    break;
                }
                case Opcode::MethodCall: {
                    auto& mcall = static_cast<MethodCallInst&>(*inst);
                    std::unordered_map<std::string, size_t> var_counts;
                    for (const auto& arg : mcall.get_args()) {
                        if (arg.is_variable()) {
                            var_counts[arg.get_name()]++;
                        }
                    }
                    for (auto& arg : mcall.get_args()) {
                        if (arg.is_variable() && var_counts[arg.get_name()] == 1) {
                            if (try_mark_move(arg, inst.get(), fn, liveness)) {
                                ++move_count;
                            }
                        }
                    }
                    break;
                }
                case Opcode::Invoke: {
                    auto& inv = static_cast<InvokeInst&>(*inst);
                    std::unordered_map<std::string, size_t> var_counts;
                    for (const auto& arg : inv.get_args()) {
                        if (arg.is_variable()) {
                            var_counts[arg.get_name()]++;
                        }
                    }
                    for (auto& arg : inv.get_args()) {
                        if (arg.is_variable() && var_counts[arg.get_name()] == 1) {
                            if (try_mark_move(arg, inst.get(), fn, liveness)) {
                                ++move_count;
                            }
                        }
                    }
                    break;
                }
                case Opcode::ArrayConstruct: {
                    auto& arr = static_cast<ArrayConstructInst&>(*inst);
                    std::unordered_map<std::string, size_t> var_counts;
                    for (const auto& el : arr.get_elements()) {
                        if (el.is_variable()) {
                            var_counts[el.get_name()]++;
                        }
                    }
                    for (auto& el : arr.get_elements()) {
                        if (el.is_variable() && var_counts[el.get_name()] == 1) {
                            if (try_mark_move(el, inst.get(), fn, liveness)) {
                                ++move_count;
                            }
                        }
                    }
                    break;
                }
                case Opcode::MapConstruct: {
                    auto& map_inst = static_cast<MapConstructInst&>(*inst);
                    std::unordered_map<std::string, size_t> var_counts;
                    for (const auto& [k, v] : map_inst.get_entries()) {
                        if (k.is_variable()) var_counts[k.get_name()]++;
                        if (v.is_variable()) var_counts[v.get_name()]++;
                    }
                    for (auto& [k, v] : map_inst.get_entries()) {
                        if (k.is_variable() && var_counts[k.get_name()] == 1) {
                            if (try_mark_move(k, inst.get(), fn, liveness)) {
                                ++move_count;
                            }
                        }
                        if (v.is_variable() && var_counts[v.get_name()] == 1) {
                            if (try_mark_move(v, inst.get(), fn, liveness)) {
                                ++move_count;
                            }
                        }
                    }
                    break;
                }
                case Opcode::ClassConstruct: {
                    auto& cls = static_cast<ClassConstructInst&>(*inst);
                    std::unordered_map<std::string, size_t> var_counts;
                    for (const auto& [f, val] : cls.get_fields()) {
                        if (val.is_variable()) {
                            var_counts[val.get_name()]++;
                        }
                    }
                    for (auto& [f, val] : cls.get_fields()) {
                        if (val.is_variable() && var_counts[val.get_name()] == 1) {
                            if (try_mark_move(val, inst.get(), fn, liveness)) {
                                ++move_count;
                            }
                        }
                    }
                    break;
                }
                case Opcode::IndexStore: {
                    auto& idx_store = static_cast<IndexStoreInst&>(*inst);
                    if (try_mark_move(idx_store.get_val(), inst.get(), fn, liveness)) {
                        ++move_count;
                    }
                    break;
                }
                case Opcode::MemberStore: {
                    auto& mem_store = static_cast<MemberStoreInst&>(*inst);
                    if (try_mark_move(mem_store.get_val(), inst.get(), fn, liveness)) {
                        ++move_count;
                    }
                    break;
                }
                case Opcode::Load: {
                    auto& load = static_cast<LoadInst&>(*inst);
                    if (try_mark_move(load.get_src(), inst.get(), fn, liveness)) {
                        ++move_count;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    return move_count;
}

size_t MoveInsertionPass::run_on_function(Function& fn) {
    LivenessAnalysis analyzer;
    LivenessResult liveness = analyzer.analyze(fn);
    return run_on_function(fn, liveness);
}

size_t MoveInsertionPass::run_on_module(Module& mod) {
    size_t total = 0;
    for (const auto& fn : mod.get_functions()) {
        if (fn) {
            total += run_on_function(*fn);
        }
    }
    return total;
}

} // namespace keety::ir
