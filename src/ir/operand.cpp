#include <keety/ir/operand.hpp>
#include <keety/ir/basic_block.hpp>

#include <iomanip>
#include <sstream>

namespace keety::ir {

Operand Operand::create_var(std::string name, typechecker::TypePtr type, bool is_move) {
    Operand op;
    op.kind_ = OperandKind::Variable;
    op.name_ = std::move(name);
    op.type_ = std::move(type);
    op.is_move_ = is_move;
    return op;
}

Operand Operand::create_temp(size_t id, typechecker::TypePtr type, bool is_move) {
    Operand op;
    op.kind_ = OperandKind::Temp;
    op.temp_id_ = id;
    op.name_ = "%" + std::to_string(id);
    op.type_ = std::move(type);
    op.is_move_ = is_move;
    return op;
}

Operand Operand::create_int(int64_t val) {
    Operand op;
    op.kind_ = OperandKind::Constant;
    op.constant_val_ = val;
    op.type_ = std::make_shared<typechecker::Type>(typechecker::TypeKind::Int);
    return op;
}

Operand Operand::create_float(double val) {
    Operand op;
    op.kind_ = OperandKind::Constant;
    op.constant_val_ = val;
    op.type_ = std::make_shared<typechecker::Type>(typechecker::TypeKind::Float);
    return op;
}

Operand Operand::create_bool(bool val) {
    Operand op;
    op.kind_ = OperandKind::Constant;
    op.constant_val_ = val;
    op.type_ = std::make_shared<typechecker::Type>(typechecker::TypeKind::Bool);
    return op;
}

Operand Operand::create_char(char val) {
    Operand op;
    op.kind_ = OperandKind::Constant;
    op.constant_val_ = val;
    op.type_ = std::make_shared<typechecker::Type>(typechecker::TypeKind::Char);
    return op;
}

Operand Operand::create_string(std::string val) {
    Operand op;
    op.kind_ = OperandKind::Constant;
    op.constant_val_ = std::move(val);
    op.type_ = std::make_shared<typechecker::Type>(typechecker::TypeKind::String);
    return op;
}

Operand Operand::create_null() {
    Operand op;
    op.kind_ = OperandKind::Constant;
    op.constant_val_ = nullptr;
    op.type_ = std::make_shared<typechecker::Type>(typechecker::TypeKind::Null);
    return op;
}

Operand Operand::create_block(BasicBlock* bb) {
    Operand op;
    op.kind_ = OperandKind::Block;
    op.block_target_ = bb;
    if (bb) {
        op.name_ = "@" + bb->get_label();
    }
    return op;
}

Operand Operand::create_global(std::string name, typechecker::TypePtr type) {
    Operand op;
    op.kind_ = OperandKind::Global;
    op.name_ = "@" + name;
    op.type_ = std::move(type);
    return op;
}

std::string Operand::to_string() const {
    std::string prefix;
    if (is_mut_borrow_) {
        prefix += "mut ";
    }
    if (is_move_) {
        prefix += "move ";
    }

    switch (kind_) {
        case OperandKind::Variable:
            return prefix + name_;
        case OperandKind::Temp:
            return prefix + name_;
        case OperandKind::Block:
            if (block_target_) {
                return "@" + block_target_->get_label();
            }
            return "@<null_bb>";
        case OperandKind::Global:
            return prefix + name_;
        case OperandKind::Constant: {
            std::ostringstream ss;
            std::visit([&ss](const auto& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, int64_t>) {
                    ss << val;
                } else if constexpr (std::is_same_v<T, double>) {
                    ss << val;
                    if (val == static_cast<int64_t>(val)) {
                        ss << ".0";
                    }
                } else if constexpr (std::is_same_v<T, bool>) {
                    ss << (val ? "true" : "false");
                } else if constexpr (std::is_same_v<T, char>) {
                    ss << "'" << val << "'";
                } else if constexpr (std::is_same_v<T, std::string>) {
                    ss << std::quoted(val);
                } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    ss << "null";
                }
            }, constant_val_);
            return prefix + ss.str();
        }
    }
    return prefix + "<unknown>";
}

bool Operand::operator==(const Operand& other) const {
    if (kind_ != other.kind_) {
        return false;
    }
    if (is_move_ != other.is_move_ || is_mut_borrow_ != other.is_mut_borrow_) {
        return false;
    }
    switch (kind_) {
        case OperandKind::Variable:
        case OperandKind::Global:
            return name_ == other.name_;
        case OperandKind::Temp:
            return temp_id_ == other.temp_id_;
        case OperandKind::Block:
            return block_target_ == other.block_target_;
        case OperandKind::Constant:
            return constant_val_ == other.constant_val_;
    }
    return false;
}

} // namespace keety::ir
