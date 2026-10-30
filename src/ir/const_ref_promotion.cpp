#include <keety/ir/const_ref_promotion.hpp>

namespace keety::ir {

namespace {

// Standard mutating collection methods in KeetScript
static const std::unordered_set<std::string> kMutatingMethods = {
    "push", "pop", "insert", "remove", "clear", "shift", "unshift",
    "reverse", "sort", "fill", "set", "delete"
};

} // namespace

bool ConstRefPromotionPass::is_composite_type(const typechecker::TypePtr& type) const {
    if (!type) {
        return false;
    }

    switch (type->getKind()) {
        case typechecker::TypeKind::String:
        case typechecker::TypeKind::Array:
        case typechecker::TypeKind::Map:
        case typechecker::TypeKind::Custom:
            return true;
        case typechecker::TypeKind::Union:
            return options_.include_unions;
        default:
            return false;
    }
}

bool ConstRefPromotionPass::is_mutated(const std::string& param_name, const Function& fn) const {
    for (const auto& bb : fn.get_blocks()) {
        if (!bb) continue;

        for (const auto& inst : bb->get_instructions()) {
            if (!inst) continue;

            // Check if parameter is consumed by move
            for (const auto* op : inst->get_operands()) {
                if (op && op->is_variable() && op->get_name() == param_name && op->is_move()) {
                    return true;
                }
            }

            switch (inst->get_opcode()) {
                case Opcode::Store: {
                    const auto& store = static_cast<const StoreInst&>(*inst);
                    if (store.get_dest().is_variable() && store.get_dest().get_name() == param_name) {
                        return true;
                    }
                    break;
                }
                case Opcode::Assign: {
                    const auto& assign = static_cast<const AssignInst&>(*inst);
                    if (assign.get_dest().is_variable() && assign.get_dest().get_name() == param_name) {
                        return true;
                    }
                    break;
                }
                case Opcode::IndexStore: {
                    const auto& idx_store = static_cast<const IndexStoreInst&>(*inst);
                    if (idx_store.get_container().is_variable() && idx_store.get_container().get_name() == param_name) {
                        return true;
                    }
                    break;
                }
                case Opcode::MemberStore: {
                    const auto& mem_store = static_cast<const MemberStoreInst&>(*inst);
                    if (mem_store.get_obj().is_variable() && mem_store.get_obj().get_name() == param_name) {
                        return true;
                    }
                    break;
                }
                case Opcode::Call: {
                    const auto& call = static_cast<const CallInst&>(*inst);
                    for (const auto& arg : call.get_args()) {
                        if (arg.is_variable() && arg.get_name() == param_name && arg.is_mut_borrow()) {
                            return true;
                        }
                    }
                    break;
                }
                case Opcode::Invoke: {
                    const auto& inv = static_cast<const InvokeInst&>(*inst);
                    for (const auto& arg : inv.get_args()) {
                        if (arg.is_variable() && arg.get_name() == param_name && arg.is_mut_borrow()) {
                            return true;
                        }
                    }
                    break;
                }
                case Opcode::MethodCall: {
                    const auto& mcall = static_cast<const MethodCallInst&>(*inst);
                    // Check if passed as mutable borrow argument
                    for (const auto& arg : mcall.get_args()) {
                        if (arg.is_variable() && arg.get_name() == param_name && arg.is_mut_borrow()) {
                            return true;
                        }
                    }
                    // Check if receiver is param_name and method modifies the collection in place
                    if (mcall.get_receiver().is_variable() && mcall.get_receiver().get_name() == param_name) {
                        if (kMutatingMethods.find(mcall.get_method()) != kMutatingMethods.end()) {
                            return true;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    return false;
}

bool ConstRefPromotionPass::is_promotable(const FunctionParameter& param, const Function& fn) const {
    if (param.is_mut || param.is_const_ref) {
        return false;
    }
    if (!is_composite_type(param.type)) {
        return false;
    }
    if (is_mutated(param.name, fn)) {
        return false;
    }
    return true;
}

size_t ConstRefPromotionPass::run_on_function(Function& fn) {
    size_t count = 0;
    for (auto& param : fn.get_parameters()) {
        if (is_promotable(param, fn)) {
            param.is_const_ref = true;
            ++count;
        }
    }
    return count;
}

size_t ConstRefPromotionPass::run_on_module(Module& mod) {
    size_t total = 0;
    for (const auto& fn : mod.get_functions()) {
        if (fn) {
            total += run_on_function(*fn);
        }
    }
    return total;
}

} // namespace keety::ir
