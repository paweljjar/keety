#include <keety/ir/function.hpp>

#include <sstream>
#include <unordered_set>

namespace keety::ir {

std::string FunctionParameter::to_string() const {
    std::string s;
    if (is_mut) {
        s += "mut ";
    }
    if (is_const_ref) {
        s += "const& ";
    }
    s += name + ": " + (type ? type->toString() : "void");
    return s;
}

BasicBlock* Function::create_basic_block(const std::string& prefix) {
    std::string label = prefix + "." + std::to_string(next_block_id_++);
    auto bb = std::make_unique<BasicBlock>(std::move(label), this);
    BasicBlock* ptr = bb.get();
    blocks_.push_back(std::move(bb));
    if (!entry_block_) {
        entry_block_ = ptr;
    }
    return ptr;
}

BasicBlock* Function::append_basic_block(std::unique_ptr<BasicBlock> bb) {
    if (!bb) return nullptr;
    bb->set_parent(this);
    BasicBlock* ptr = bb.get();
    blocks_.push_back(std::move(bb));
    if (!entry_block_) {
        entry_block_ = ptr;
    }
    return ptr;
}

BasicBlock* Function::find_basic_block(const std::string& label) const noexcept {
    for (const auto& bb : blocks_) {
        if (bb && bb->get_label() == label) {
            return bb.get();
        }
    }
    return nullptr;
}

Operand Function::create_temp(typechecker::TypePtr type, bool is_move) {
    return Operand::create_temp(next_temp_id_++, std::move(type), is_move);
}

void Function::recompute_cfg_edges() {
    for (auto& bb : blocks_) {
        if (bb) {
            bb->clear_predecessors();
            bb->clear_successors();
        }
    }

    for (auto& bb : blocks_) {
        if (!bb) continue;
        Instruction* term = bb->get_terminator();
        if (!term) continue;

        for (BasicBlock* succ : term->get_successors()) {
            if (succ) {
                bb->add_successor(succ);
                succ->add_predecessor(bb.get());
            }
        }
    }
}

bool Function::validate_cfg(std::string& error_message) const {
    if (blocks_.empty()) {
        error_message = "Function '" + name_ + "' has no basic blocks";
        return false;
    }

    if (!entry_block_) {
        error_message = "Function '" + name_ + "' has no entry block set";
        return false;
    }

    std::unordered_set<const BasicBlock*> block_set;
    for (const auto& bb : blocks_) {
        if (!bb) {
            error_message = "Function '" + name_ + "' contains null basic block pointer";
            return false;
        }
        block_set.insert(bb.get());
    }

    if (block_set.find(entry_block_) == block_set.end()) {
        error_message = "Entry block for function '" + name_ + "' is not part of its blocks list";
        return false;
    }

    for (const auto& bb : blocks_) {
        if (bb->empty()) {
            error_message = "Basic block '" + bb->get_label() + "' in function '" + name_ + "' is empty";
            return false;
        }

        const auto& insts = bb->get_instructions();
        for (size_t i = 0; i < insts.size(); ++i) {
            if (!insts[i]) {
                error_message = "Basic block '" + bb->get_label() + "' contains null instruction";
                return false;
            }
            if (insts[i]->is_terminator() && i + 1 < insts.size()) {
                error_message = "Basic block '" + bb->get_label() + "' has instructions after terminator";
                return false;
            }
        }

        Instruction* term = bb->get_terminator();
        if (!term) {
            error_message = "Basic block '" + bb->get_label() + "' in function '" + name_ + "' is missing a terminator";
            return false;
        }

        for (BasicBlock* succ : bb->get_successors()) {
            if (block_set.find(succ) == block_set.end()) {
                error_message = "Successor '" + succ->get_label() + "' of block '" + bb->get_label() + "' does not belong to function";
                return false;
            }
            const auto& succ_preds = succ->get_predecessors();
            if (std::find(succ_preds.begin(), succ_preds.end(), bb.get()) == succ_preds.end()) {
                error_message = "CFG edge asymmetry: block '" + bb->get_label() + "' has successor '" + succ->get_label() + "' but is not listed as predecessor";
                return false;
            }
        }

        for (BasicBlock* pred : bb->get_predecessors()) {
            if (block_set.find(pred) == block_set.end()) {
                error_message = "Predecessor '" + pred->get_label() + "' of block '" + bb->get_label() + "' does not belong to function";
                return false;
            }
            const auto& pred_succs = pred->get_successors();
            if (std::find(pred_succs.begin(), pred_succs.end(), bb.get()) == pred_succs.end()) {
                error_message = "CFG edge asymmetry: block '" + bb->get_label() + "' has predecessor '" + pred->get_label() + "' but is not listed as successor";
                return false;
            }
        }
    }

    return true;
}

std::string Function::to_string() const {
    std::ostringstream ss;
    if (is_async_) {
        ss << "async ";
    }
    ss << "fn @" << name_ << "(";
    for (size_t i = 0; i < parameters_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << parameters_[i].to_string();
    }
    ss << "): " << (return_type_ ? return_type_->toString() : "void") << " {\n";

    for (const auto& bb : blocks_) {
        if (bb) {
            ss << bb->to_string() << "\n";
        }
    }
    ss << "}\n";
    return ss.str();
}

} // namespace keety::ir
