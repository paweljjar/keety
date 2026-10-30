#include <keety/ir/basic_block.hpp>

#include <sstream>

namespace keety::ir {

std::string BasicBlock::to_string() const {
    std::ostringstream ss;
    ss << label_ << ":\n";
    if (!predecessors_.empty()) {
        ss << "  ; preds: ";
        for (size_t i = 0; i < predecessors_.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "@" << (predecessors_[i] ? predecessors_[i]->get_label() : "<null_bb>");
        }
        ss << "\n";
    }

    for (const auto& inst : instructions_) {
        if (inst) {
            ss << "  " << inst->to_string() << "\n";
        }
    }
    return ss.str();
}

} // namespace keety::ir
