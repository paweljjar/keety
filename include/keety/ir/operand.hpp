#pragma once

#include <keety/typechecker/type.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace keety::ir {

class BasicBlock;

/// Classification of operands in KeetIR
enum class OperandKind {
    Variable,  ///< Named local variable (e.g. x)
    Temp,      ///< Virtual register / temporary value (e.g. %0)
    Constant,  ///< Compile-time constant literal
    Block,     ///< Reference to a BasicBlock target
    Global     ///< Module-level global / export variable
};

/// Type-safe literal constant value
using ConstantValue = std::variant<
    int64_t,
    double,
    bool,
    char,
    std::string,
    std::nullptr_t
>;

/// Represents an operand (value, variable, constant, or label) in KeetIR
class Operand {
public:
    Operand() : kind_(OperandKind::Constant), constant_val_(nullptr) {}

    static Operand create_var(std::string name, typechecker::TypePtr type, bool is_move = false);
    static Operand create_temp(size_t id, typechecker::TypePtr type, bool is_move = false);
    static Operand create_int(int64_t val);
    static Operand create_float(double val);
    static Operand create_bool(bool val);
    static Operand create_char(char val);
    static Operand create_string(std::string val);
    static Operand create_null();
    static Operand create_block(BasicBlock* bb);
    static Operand create_global(std::string name, typechecker::TypePtr type);

    [[nodiscard]] OperandKind get_kind() const noexcept { return kind_; }
    [[nodiscard]] bool is_variable() const noexcept { return kind_ == OperandKind::Variable; }
    [[nodiscard]] bool is_temp() const noexcept { return kind_ == OperandKind::Temp; }
    [[nodiscard]] bool is_constant() const noexcept { return kind_ == OperandKind::Constant; }
    [[nodiscard]] bool is_block() const noexcept { return kind_ == OperandKind::Block; }
    [[nodiscard]] bool is_global() const noexcept { return kind_ == OperandKind::Global; }

    [[nodiscard]] const std::string& get_name() const noexcept { return name_; }
    [[nodiscard]] size_t get_temp_id() const noexcept { return temp_id_; }
    [[nodiscard]] typechecker::TypePtr get_type() const noexcept { return type_; }
    [[nodiscard]] const ConstantValue& get_constant() const noexcept { return constant_val_; }
    [[nodiscard]] BasicBlock* get_block() const noexcept { return block_target_; }

    [[nodiscard]] bool is_move() const noexcept { return is_move_; }
    void set_move(bool move) noexcept { is_move_ = move; }

    [[nodiscard]] bool is_mut_borrow() const noexcept { return is_mut_borrow_; }
    void set_mut_borrow(bool mut) noexcept { is_mut_borrow_ = mut; }

    [[nodiscard]] std::string to_string() const;

    bool operator==(const Operand& other) const;
    bool operator!=(const Operand& other) const { return !(*this == other); }

private:
    OperandKind kind_;
    std::string name_{};
    size_t temp_id_{0};
    typechecker::TypePtr type_{nullptr};
    ConstantValue constant_val_{nullptr};
    BasicBlock* block_target_{nullptr};
    bool is_move_{false};
    bool is_mut_borrow_{false};
};

} // namespace keety::ir
