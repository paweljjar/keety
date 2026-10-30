#pragma once

#include <keety/ir/basic_block.hpp>
#include <keety/ir/operand.hpp>
#include <keety/typechecker/type.hpp>

#include <memory>
#include <string>
#include <vector>

namespace keety::ir {

/// Represents a function parameter in KeetIR
struct FunctionParameter {
    std::string name;
    typechecker::TypePtr type;
    bool is_mut{false};
    bool is_const_ref{false}; ///< Pass 3 optimization hint
    bool has_default_val{false};

    [[nodiscard]] std::string to_string() const;
};

/// Represents a function in KeetIR, containing a Control Flow Graph of Basic Blocks
class Function {
public:
    Function(std::string name, typechecker::TypePtr return_type, bool is_async = false)
        : name_(std::move(name)), return_type_(std::move(return_type)), is_async_(is_async) {}

    [[nodiscard]] const std::string& get_name() const noexcept { return name_; }
    void set_name(std::string name) noexcept { name_ = std::move(name); }

    [[nodiscard]] typechecker::TypePtr get_return_type() const noexcept { return return_type_; }
    void set_return_type(typechecker::TypePtr type) noexcept { return_type_ = std::move(type); }

    [[nodiscard]] bool is_async() const noexcept { return is_async_; }
    void set_async(bool is_async) noexcept { is_async_ = is_async; }

    [[nodiscard]] const std::vector<FunctionParameter>& get_parameters() const noexcept { return parameters_; }
    [[nodiscard]] std::vector<FunctionParameter>& get_parameters() noexcept { return parameters_; }
    void add_parameter(FunctionParameter param) { parameters_.push_back(std::move(param)); }

    [[nodiscard]] const std::vector<std::unique_ptr<BasicBlock>>& get_blocks() const noexcept { return blocks_; }
    [[nodiscard]] std::vector<std::unique_ptr<BasicBlock>>& get_blocks() noexcept { return blocks_; }

    BasicBlock* create_basic_block(const std::string& prefix = "bb");
    BasicBlock* append_basic_block(std::unique_ptr<BasicBlock> bb);

    [[nodiscard]] BasicBlock* get_entry_block() const noexcept { return entry_block_; }
    void set_entry_block(BasicBlock* bb) noexcept { entry_block_ = bb; }

    [[nodiscard]] BasicBlock* find_basic_block(const std::string& label) const noexcept;

    /// Generates a new unique virtual register / temporary operand
    Operand create_temp(typechecker::TypePtr type, bool is_move = false);

    /// Recomputes CFG predecessor and successor edges across all basic blocks based on terminator instructions
    void recompute_cfg_edges();

    /// Validates CFG integrity (valid terminators, edge symmetry, block membership)
    [[nodiscard]] bool validate_cfg(std::string& error_message) const;

    [[nodiscard]] std::string to_string() const;

private:
    std::string name_;
    typechecker::TypePtr return_type_;
    std::vector<FunctionParameter> parameters_;
    bool is_async_{false};
    std::vector<std::unique_ptr<BasicBlock>> blocks_;
    BasicBlock* entry_block_{nullptr};
    size_t next_temp_id_{0};
    size_t next_block_id_{0};
};

} // namespace keety::ir
