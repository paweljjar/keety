#pragma once

#include <keety/ir/instruction.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace keety::ir {

class Function;

/// Represents a single Basic Block in the Control Flow Graph (CFG)
class BasicBlock {
public:
    explicit BasicBlock(std::string label, Function* parent = nullptr)
        : label_(std::move(label)), parent_(parent) {}

    [[nodiscard]] const std::string& get_label() const noexcept { return label_; }
    void set_label(std::string label) noexcept { label_ = std::move(label); }

    [[nodiscard]] Function* get_parent() const noexcept { return parent_; }
    void set_parent(Function* parent) noexcept { parent_ = parent; }

    [[nodiscard]] const std::vector<std::unique_ptr<Instruction>>& get_instructions() const noexcept {
        return instructions_;
    }
    [[nodiscard]] std::vector<std::unique_ptr<Instruction>>& get_instructions() noexcept {
        return instructions_;
    }

    void append_instruction(std::unique_ptr<Instruction> inst) {
        if (inst) {
            inst->set_parent(this);
            instructions_.push_back(std::move(inst));
        }
    }

    void insert_instruction(size_t index, std::unique_ptr<Instruction> inst) {
        if (inst) {
            inst->set_parent(this);
            if (index >= instructions_.size()) {
                instructions_.push_back(std::move(inst));
            } else {
                instructions_.insert(instructions_.begin() + static_cast<std::ptrdiff_t>(index), std::move(inst));
            }
        }
    }

    void remove_instruction(size_t index) {
        if (index < instructions_.size()) {
            instructions_.erase(instructions_.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    [[nodiscard]] bool empty() const noexcept { return instructions_.empty(); }
    [[nodiscard]] size_t size() const noexcept { return instructions_.size(); }

    [[nodiscard]] Instruction* get_terminator() const noexcept {
        if (!instructions_.empty() && instructions_.back()->is_terminator()) {
            return instructions_.back().get();
        }
        return nullptr;
    }

    [[nodiscard]] bool has_terminator() const noexcept {
        return get_terminator() != nullptr;
    }

    void set_terminator(std::unique_ptr<Instruction> term) {
        if (has_terminator()) {
            instructions_.pop_back();
        }
        append_instruction(std::move(term));
    }

    // Predecessors and Successors in CFG
    [[nodiscard]] const std::vector<BasicBlock*>& get_predecessors() const noexcept { return predecessors_; }
    [[nodiscard]] const std::vector<BasicBlock*>& get_successors() const noexcept { return successors_; }

    void add_predecessor(BasicBlock* bb) {
        if (bb && std::find(predecessors_.begin(), predecessors_.end(), bb) == predecessors_.end()) {
            predecessors_.push_back(bb);
        }
    }

    void remove_predecessor(BasicBlock* bb) {
        predecessors_.erase(
            std::remove(predecessors_.begin(), predecessors_.end(), bb),
            predecessors_.end()
        );
    }

    void add_successor(BasicBlock* bb) {
        if (bb && std::find(successors_.begin(), successors_.end(), bb) == successors_.end()) {
            successors_.push_back(bb);
        }
    }

    void remove_successor(BasicBlock* bb) {
        successors_.erase(
            std::remove(successors_.begin(), successors_.end(), bb),
            successors_.end()
        );
    }

    void clear_predecessors() noexcept { predecessors_.clear(); }
    void clear_successors() noexcept { successors_.clear(); }

    [[nodiscard]] std::string to_string() const;

private:
    std::string label_;
    Function* parent_{nullptr};
    std::vector<std::unique_ptr<Instruction>> instructions_;
    std::vector<BasicBlock*> predecessors_;
    std::vector<BasicBlock*> successors_;
};

} // namespace keety::ir
