#include <keety/ir/pipeline_reserve.hpp>

namespace keety::ir {

bool PipelineReservePass::is_pipeline_stage(const MethodCallInst& inst) const {
    if (!inst.get_dest().has_value()) {
        return false;
    }
    return options_.pipeline_methods.find(inst.get_method()) != options_.pipeline_methods.end();
}

const Instruction* PipelineReservePass::find_producer(const Operand& op, const Function& fn) const {
    for (const auto& bb : fn.get_blocks()) {
        if (!bb) continue;

        for (const auto& inst : bb->get_instructions()) {
            if (!inst) continue;

            switch (inst->get_opcode()) {
                case Opcode::MethodCall: {
                    const auto* mc = static_cast<const MethodCallInst*>(inst.get());
                    if (mc->get_dest().has_value() && *mc->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::Call: {
                    const auto* c = static_cast<const CallInst*>(inst.get());
                    if (c->get_dest().has_value() && *c->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::Invoke: {
                    const auto* inv = static_cast<const InvokeInst*>(inst.get());
                    if (inv->get_dest().has_value() && *inv->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::Binary: {
                    const auto* b = static_cast<const BinaryInst*>(inst.get());
                    if (b->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::Unary: {
                    const auto* u = static_cast<const UnaryInst*>(inst.get());
                    if (u->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::ArrayConstruct: {
                    const auto* a = static_cast<const ArrayConstructInst*>(inst.get());
                    if (a->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::MapConstruct: {
                    const auto* m = static_cast<const MapConstructInst*>(inst.get());
                    if (m->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::ClassConstruct: {
                    const auto* cc = static_cast<const ClassConstructInst*>(inst.get());
                    if (cc->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::IndexLoad: {
                    const auto* il = static_cast<const IndexLoadInst*>(inst.get());
                    if (il->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::MemberLoad: {
                    const auto* ml = static_cast<const MemberLoadInst*>(inst.get());
                    if (ml->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::Load: {
                    const auto* l = static_cast<const LoadInst*>(inst.get());
                    if (l->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::Store: {
                    const auto* s = static_cast<const StoreInst*>(inst.get());
                    if (s->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::Assign: {
                    const auto* a = static_cast<const AssignInst*>(inst.get());
                    if (a->get_dest() == op) {
                        return inst.get();
                    }
                    break;
                }
                case Opcode::Alloca: {
                    const auto* al = static_cast<const AllocaInst*>(inst.get());
                    if (op.is_variable() && al->get_name() == op.get_name()) {
                        return inst.get();
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    return nullptr;
}

Operand PipelineReservePass::find_pipeline_root(const Operand& receiver, const Function& fn) const {
    Operand current = receiver;
    std::unordered_set<std::string> visited;

    while (true) {
        std::string key = current.to_string();
        if (visited.find(key) != visited.end()) {
            // Prevent cycles
            break;
        }
        visited.insert(key);

        const Instruction* producer = find_producer(current, fn);
        if (!producer) {
            break;
        }

        if (producer->get_opcode() == Opcode::MethodCall) {
            const auto* mcall = static_cast<const MethodCallInst*>(producer);
            if (options_.pipeline_methods.find(mcall->get_method()) != options_.pipeline_methods.end()) {
                current = mcall->get_receiver();
                continue;
            }
        }
        break;
    }

    return current;
}

size_t PipelineReservePass::run_on_function(Function& fn) {
    size_t count = 0;

    for (const auto& bb : fn.get_blocks()) {
        if (!bb) continue;

        for (size_t i = 0; i < bb->get_instructions().size(); ++i) {
            const auto& inst = bb->get_instructions()[i];
            if (!inst || inst->get_opcode() != Opcode::MethodCall) {
                continue;
            }

            const auto* mcall = static_cast<const MethodCallInst*>(inst.get());
            if (!is_pipeline_stage(*mcall)) {
                continue;
            }

            // Check if already followed by a reserve instruction for this dest
            Operand dest = *mcall->get_dest();
            if (i + 1 < bb->get_instructions().size()) {
                const auto& next_inst = bb->get_instructions()[i + 1];
                if (next_inst && next_inst->get_opcode() == Opcode::Reserve) {
                    const auto* res_inst = static_cast<const ReserveInst*>(next_inst.get());
                    if (res_inst->get_collection() == dest) {
                        continue;
                    }
                }
            }

            // Resolve root source collection
            Operand root = find_pipeline_root(mcall->get_receiver(), fn);

            // Compute capacity operand
            Operand capacity = root;
            if (options_.use_constant_capacity_for_literals) {
                const Instruction* root_producer = find_producer(root, fn);
                if (root_producer && root_producer->get_opcode() == Opcode::ArrayConstruct) {
                    const auto* arr_inst = static_cast<const ArrayConstructInst*>(root_producer);
                    capacity = Operand::create_int(static_cast<int64_t>(arr_inst->get_elements().size()));
                }
            }

            // Insert ReserveInst right after this method call
            bb->insert_instruction(i + 1, std::make_unique<ReserveInst>(dest, capacity));
            ++count;
            ++i; // Advance past the newly inserted reserve instruction
        }
    }

    return count;
}

size_t PipelineReservePass::run_on_module(Module& mod) {
    size_t total = 0;
    for (const auto& fn : mod.get_functions()) {
        if (fn) {
            total += run_on_function(*fn);
        }
    }
    return total;
}

} // namespace keety::ir
