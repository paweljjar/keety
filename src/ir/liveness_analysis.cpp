#include <keety/ir/liveness_analysis.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>

namespace keety::ir {

// -----------------------------------------------------------------------------
// LiveRange
// -----------------------------------------------------------------------------

bool LiveRange::is_last_use(const Instruction* inst) const noexcept {
    return std::find(last_uses.begin(), last_uses.end(), inst) != last_uses.end();
}

bool LiveRange::is_defined_at(const Instruction* inst) const noexcept {
    return std::find(definitions.begin(), definitions.end(), inst) != definitions.end();
}

bool LiveRange::is_used_at(const Instruction* inst) const noexcept {
    return std::find(uses.begin(), uses.end(), inst) != uses.end();
}

// -----------------------------------------------------------------------------
// LivenessResult
// -----------------------------------------------------------------------------

const BlockLiveness* LivenessResult::get_block_liveness(const BasicBlock* bb) const noexcept {
    auto it = block_liveness_.find(bb);
    if (it != block_liveness_.end()) {
        return &it->second;
    }
    return nullptr;
}

const LiveRange* LivenessResult::get_live_range(const std::string& var_name) const noexcept {
    auto it = live_ranges_.find(var_name);
    if (it != live_ranges_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool LivenessResult::is_last_use(const Instruction* inst, const std::string& var_name) const noexcept {
    auto it = live_ranges_.find(var_name);
    if (it != live_ranges_.end()) {
        return it->second.is_last_use(inst);
    }
    return false;
}

std::vector<std::string> LivenessResult::get_last_used_vars(const Instruction* inst) const {
    auto it = last_uses_by_instruction_.find(inst);
    if (it != last_uses_by_instruction_.end()) {
        return it->second;
    }
    return {};
}

bool LivenessResult::is_live_in(const BasicBlock* bb, const std::string& var_name) const noexcept {
    const auto* bl = get_block_liveness(bb);
    if (bl) {
        return bl->live_in.find(var_name) != bl->live_in.end();
    }
    return false;
}

bool LivenessResult::is_live_out(const BasicBlock* bb, const std::string& var_name) const noexcept {
    const auto* bl = get_block_liveness(bb);
    if (bl) {
        return bl->live_out.find(var_name) != bl->live_out.end();
    }
    return false;
}

void LivenessResult::set_block_liveness(const BasicBlock* bb, BlockLiveness liveness) {
    block_liveness_[bb] = std::move(liveness);
}

void LivenessResult::add_definition(const std::string& var_name, const Instruction* inst) {
    auto& range = live_ranges_[var_name];
    range.variable_name = var_name;
    range.definitions.push_back(inst);
}

void LivenessResult::add_use(const std::string& var_name, const Instruction* inst) {
    auto& range = live_ranges_[var_name];
    range.variable_name = var_name;
    range.uses.push_back(inst);
}

void LivenessResult::add_last_use(const std::string& var_name, const Instruction* inst) {
    auto& range = live_ranges_[var_name];
    range.variable_name = var_name;
    range.last_uses.push_back(inst);
    last_uses_by_instruction_[inst].push_back(var_name);
}

std::string LivenessResult::to_string() const {
    std::ostringstream ss;
    ss << "; Liveness Analysis Result for function: @" << (function_ ? function_->get_name() : "<unknown>") << "\n";

    if (function_) {
        for (const auto& bb : function_->get_blocks()) {
            if (!bb) continue;
            auto it = block_liveness_.find(bb.get());
            if (it != block_liveness_.end()) {
                ss << "  " << bb->get_label() << ":\n";
                ss << "    LiveIn:  {";
                bool first = true;
                for (const auto& v : it->second.live_in) {
                    if (!first) ss << ", ";
                    ss << v;
                    first = false;
                }
                ss << "}\n    LiveOut: {";
                first = true;
                for (const auto& v : it->second.live_out) {
                    if (!first) ss << ", ";
                    ss << v;
                    first = false;
                }
                ss << "}\n";
            }
        }
    }

    ss << "; Variable Live Ranges:\n";
    for (const auto& [var, range] : live_ranges_) {
        ss << "  " << var << ": defs=" << range.definitions.size()
           << ", uses=" << range.uses.size()
           << ", last_uses=" << range.last_uses.size() << "\n";
    }

    return ss.str();
}

void LivenessResult::dump(std::ostream& os) const {
    os << to_string();
}

// -----------------------------------------------------------------------------
// LivenessAnalysis Pass
// -----------------------------------------------------------------------------

static void collect_operand_if_var(const Operand& op, std::vector<std::string>& out) {
    if (op.is_variable()) {
        if (std::find(out.begin(), out.end(), op.get_name()) == out.end()) {
            out.push_back(op.get_name());
        }
    }
}

std::vector<std::string> LivenessAnalysis::get_defined_vars(const Instruction& inst) {
    std::vector<std::string> defs;
    switch (inst.get_opcode()) {
        case Opcode::Alloca: {
            const auto& a = static_cast<const AllocaInst&>(inst);
            defs.push_back(a.get_name());
            break;
        }
        case Opcode::Store: {
            const auto& s = static_cast<const StoreInst&>(inst);
            if (s.get_dest().is_variable()) {
                defs.push_back(s.get_dest().get_name());
            }
            break;
        }
        case Opcode::Assign: {
            const auto& a = static_cast<const AssignInst&>(inst);
            if (a.get_dest().is_variable()) {
                defs.push_back(a.get_dest().get_name());
            }
            break;
        }
        case Opcode::LandingPad: {
            const auto& lp = static_cast<const LandingPadInst&>(inst);
            if (lp.get_error_dest().is_variable()) {
                defs.push_back(lp.get_error_dest().get_name());
            }
            break;
        }
        default:
            break;
    }
    return defs;
}

std::vector<std::string> LivenessAnalysis::get_used_vars(const Instruction& inst) {
    std::vector<std::string> uses;

    switch (inst.get_opcode()) {
        case Opcode::Alloca:
        case Opcode::LandingPad:
        case Opcode::Branch:
        case Opcode::Unreachable:
            break;

        case Opcode::Store: {
            const auto& s = static_cast<const StoreInst&>(inst);
            collect_operand_if_var(s.get_val(), uses);
            break;
        }
        case Opcode::Assign: {
            const auto& a = static_cast<const AssignInst&>(inst);
            collect_operand_if_var(a.get_val(), uses);
            break;
        }
        case Opcode::Load: {
            const auto& l = static_cast<const LoadInst&>(inst);
            collect_operand_if_var(l.get_src(), uses);
            break;
        }
        case Opcode::Binary: {
            const auto& b = static_cast<const BinaryInst&>(inst);
            collect_operand_if_var(b.get_lhs(), uses);
            collect_operand_if_var(b.get_rhs(), uses);
            break;
        }
        case Opcode::Unary: {
            const auto& u = static_cast<const UnaryInst&>(inst);
            collect_operand_if_var(u.get_val(), uses);
            break;
        }
        case Opcode::Call: {
            const auto& c = static_cast<const CallInst&>(inst);
            for (const auto& arg : c.get_args()) {
                collect_operand_if_var(arg, uses);
            }
            break;
        }
        case Opcode::MethodCall: {
            const auto& m = static_cast<const MethodCallInst&>(inst);
            collect_operand_if_var(m.get_receiver(), uses);
            for (const auto& arg : m.get_args()) {
                collect_operand_if_var(arg, uses);
            }
            break;
        }
        case Opcode::IndexLoad: {
            const auto& i = static_cast<const IndexLoadInst&>(inst);
            collect_operand_if_var(i.get_container(), uses);
            collect_operand_if_var(i.get_index(), uses);
            break;
        }
        case Opcode::IndexStore: {
            const auto& i = static_cast<const IndexStoreInst&>(inst);
            collect_operand_if_var(i.get_container(), uses);
            collect_operand_if_var(i.get_index(), uses);
            collect_operand_if_var(i.get_val(), uses);
            break;
        }
        case Opcode::MemberLoad: {
            const auto& m = static_cast<const MemberLoadInst&>(inst);
            collect_operand_if_var(m.get_obj(), uses);
            break;
        }
        case Opcode::MemberStore: {
            const auto& m = static_cast<const MemberStoreInst&>(inst);
            collect_operand_if_var(m.get_obj(), uses);
            collect_operand_if_var(m.get_val(), uses);
            break;
        }
        case Opcode::ArrayConstruct: {
            const auto& a = static_cast<const ArrayConstructInst&>(inst);
            for (const auto& el : a.get_elements()) {
                collect_operand_if_var(el, uses);
            }
            break;
        }
        case Opcode::MapConstruct: {
            const auto& m = static_cast<const MapConstructInst&>(inst);
            for (const auto& [k, v] : m.get_entries()) {
                collect_operand_if_var(k, uses);
                collect_operand_if_var(v, uses);
            }
            break;
        }
        case Opcode::ClassConstruct: {
            const auto& c = static_cast<const ClassConstructInst&>(inst);
            for (const auto& [f, val] : c.get_fields()) {
                collect_operand_if_var(val, uses);
            }
            break;
        }
        case Opcode::CondBranch: {
            const auto& b = static_cast<const CondBranchInst&>(inst);
            collect_operand_if_var(b.get_condition(), uses);
            break;
        }
        case Opcode::Switch: {
            const auto& s = static_cast<const SwitchInst&>(inst);
            collect_operand_if_var(s.get_condition(), uses);
            for (const auto& [val, target] : s.get_cases()) {
                collect_operand_if_var(val, uses);
            }
            break;
        }
        case Opcode::Return: {
            const auto& r = static_cast<const ReturnInst&>(inst);
            if (r.has_value()) {
                collect_operand_if_var(*r.get_value(), uses);
            }
            break;
        }
        case Opcode::Throw: {
            const auto& t = static_cast<const ThrowInst&>(inst);
            collect_operand_if_var(t.get_error(), uses);
            break;
        }
        case Opcode::Invoke: {
            const auto& inv = static_cast<const InvokeInst&>(inst);
            for (const auto& arg : inv.get_args()) {
                collect_operand_if_var(arg, uses);
            }
            break;
        }
        case Opcode::TypeOf: {
            const auto& to = static_cast<const TypeOfInst&>(inst);
            collect_operand_if_var(to.get_src(), uses);
            break;
        }
        case Opcode::TypeCast: {
            const auto& tc = static_cast<const TypeCastInst&>(inst);
            collect_operand_if_var(tc.get_src(), uses);
            break;
        }
        case Opcode::Box: {
            const auto& b = static_cast<const BoxInst&>(inst);
            collect_operand_if_var(b.get_src(), uses);
            break;
        }
        case Opcode::Unbox: {
            const auto& u = static_cast<const UnboxInst&>(inst);
            collect_operand_if_var(u.get_src(), uses);
            break;
        }
        case Opcode::Reserve: {
            const auto& res = static_cast<const ReserveInst&>(inst);
            collect_operand_if_var(res.get_collection(), uses);
            collect_operand_if_var(res.get_capacity(), uses);
            break;
        }
    }

    return uses;
}

LivenessResult LivenessAnalysis::analyze(const Function& fn) const {
    LivenessResult result(&fn);

    const auto& blocks = fn.get_blocks();
    if (blocks.empty()) {
        return result;
    }

    // Phase 1: Compute defs and upward-exposed uses for each basic block
    std::unordered_map<const BasicBlock*, BlockLiveness> block_data;
    for (const auto& bb : blocks) {
        if (!bb) continue;

        BlockLiveness bl;
        for (const auto& inst : bb->get_instructions()) {
            if (!inst) continue;

            auto inst_uses = get_used_vars(*inst);
            for (const auto& u : inst_uses) {
                // If not defined earlier in this block, it's an upward-exposed use
                if (bl.defs.find(u) == bl.defs.end()) {
                    bl.uses.insert(u);
                }
                result.add_use(u, inst.get());
            }

            auto inst_defs = get_defined_vars(*inst);
            for (const auto& d : inst_defs) {
                bl.defs.insert(d);
                result.add_definition(d, inst.get());
            }
        }

        block_data[bb.get()] = std::move(bl);
    }

    // Phase 2: Iterative backwards dataflow fixed-point
    bool changed = true;
    while (changed) {
        changed = false;

        // Iterate in reverse order through basic blocks
        for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
            const BasicBlock* bb = it->get();
            if (!bb) continue;

            auto& bl = block_data[bb];

            // LiveOut(B) = Union of LiveIn(S) for all successors S
            std::unordered_set<std::string> new_out;
            for (const BasicBlock* succ : bb->get_successors()) {
                if (succ) {
                    const auto& succ_in = block_data[succ].live_in;
                    new_out.insert(succ_in.begin(), succ_in.end());
                }
            }

            // LiveIn(B) = Uses(B) U (LiveOut(B) \ Defs(B))
            std::unordered_set<std::string> new_in = bl.uses;
            for (const auto& var : new_out) {
                if (bl.defs.find(var) == bl.defs.end()) {
                    new_in.insert(var);
                }
            }

            if (new_in != bl.live_in || new_out != bl.live_out) {
                bl.live_in = std::move(new_in);
                bl.live_out = std::move(new_out);
                changed = true;
            }
        }
    }

    // Store block liveness in result
    for (const auto& [bb, bl] : block_data) {
        result.set_block_liveness(bb, bl);
    }

    // Phase 3: Instruction-by-instruction backwards scan within each block
    // to identify exact live ranges and last-use points
    for (const auto& bb : blocks) {
        if (!bb) continue;

        const auto& bl = block_data[bb.get()];
        std::unordered_set<std::string> current_live = bl.live_out;

        const auto& insts = bb->get_instructions();
        for (int i = static_cast<int>(insts.size()) - 1; i >= 0; --i) {
            const auto& inst = insts[i];
            if (!inst) continue;

            // Definitions: variable is defined here, so it is killed going backwards
            auto inst_defs = get_defined_vars(*inst);
            for (const auto& d : inst_defs) {
                current_live.erase(d);
            }

            // Uses: variable is read here
            auto inst_uses = get_used_vars(*inst);
            for (const auto& u : inst_uses) {
                // If 'u' is NOT in current_live, then this instruction is a LAST USE point!
                if (current_live.find(u) == current_live.end()) {
                    result.add_last_use(u, inst.get());
                    current_live.insert(u);
                }
            }
        }
    }

    return result;
}

std::unordered_map<const Function*, LivenessResult> LivenessAnalysis::analyze_module(const Module& mod) const {
    std::unordered_map<const Function*, LivenessResult> results;
    for (const auto& fn : mod.get_functions()) {
        if (fn) {
            results[fn.get()] = analyze(*fn);
        }
    }
    return results;
}

} // namespace keety::ir
