#pragma once

#include <keety/diag/source_location.hpp>
#include <keety/ir/operand.hpp>
#include <keety/typechecker/type.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace keety::ir {

class BasicBlock;

/// Binary operation kinds supported in KeetIR
enum class BinaryOpKind {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Eq,
    Neq,
    Lt,
    Lte,
    Gt,
    Gte,
    LogicAnd,
    LogicOr,
    BitAnd,
    BitOr,
    BitXor,
    Shl,
    Shr
};

[[nodiscard]] std::string to_string(BinaryOpKind op);

/// Unary operation kinds supported in KeetIR
enum class UnaryOpKind {
    Neg,
    Not,
    BitNot
};

[[nodiscard]] std::string to_string(UnaryOpKind op);

/// Operation codes for KeetIR instructions
enum class Opcode {
    // Control Flow Terminators
    Branch,
    CondBranch,
    Switch,
    Return,
    Throw,
    Unreachable,

    // Variables and Memory
    Alloca,
    Load,
    Store,
    Assign,

    // Computation
    Binary,
    Unary,

    // Calls and Invocations
    Call,
    MethodCall,

    // Collections and Aggregates
    ArrayConstruct,
    MapConstruct,
    ClassConstruct,
    IndexLoad,
    IndexStore,
    MemberLoad,
    MemberStore,
    Reserve,

    // Type and Pattern Operations
    TypeOf,
    TypeCast,
    Box,
    Unbox,

    // Exception and Landing Pad
    LandingPad
};

[[nodiscard]] std::string to_string(Opcode op);

/// Abstract base class for all instructions in KeetIR
class Instruction {
public:
    explicit Instruction(Opcode opcode, diag::SourceSpan span = {})
        : opcode_(opcode), span_(span) {}
    virtual ~Instruction() = default;

    [[nodiscard]] Opcode get_opcode() const noexcept { return opcode_; }
    [[nodiscard]] diag::SourceSpan get_span() const noexcept { return span_; }
    void set_span(diag::SourceSpan span) noexcept { span_ = span; }

    [[nodiscard]] BasicBlock* get_parent() const noexcept { return parent_; }
    void set_parent(BasicBlock* parent) noexcept { parent_ = parent; }

    [[nodiscard]] virtual bool is_terminator() const noexcept { return false; }
    [[nodiscard]] virtual std::vector<BasicBlock*> get_successors() const { return {}; }
    [[nodiscard]] virtual std::vector<Operand*> get_operands() = 0;
    [[nodiscard]] virtual std::vector<const Operand*> get_operands() const = 0;

    [[nodiscard]] virtual std::string to_string() const = 0;
    [[nodiscard]] virtual std::unique_ptr<Instruction> clone() const = 0;

private:
    Opcode opcode_;
    diag::SourceSpan span_;
    BasicBlock* parent_{nullptr};
};

// -----------------------------------------------------------------------------
// Control Flow Terminators
// -----------------------------------------------------------------------------

/// Unconditional branch: br target
class BranchInst : public Instruction {
public:
    explicit BranchInst(BasicBlock* target, diag::SourceSpan span = {})
        : Instruction(Opcode::Branch, span), target_(target) {}

    [[nodiscard]] bool is_terminator() const noexcept override { return true; }
    [[nodiscard]] BasicBlock* get_target() const noexcept { return target_; }
    void set_target(BasicBlock* target) noexcept { target_ = target; }

    [[nodiscard]] std::vector<BasicBlock*> get_successors() const override;
    [[nodiscard]] std::vector<Operand*> get_operands() override { return {}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    BasicBlock* target_;
};

/// Conditional branch: br cond, true_target, false_target
class CondBranchInst : public Instruction {
public:
    CondBranchInst(Operand condition, BasicBlock* true_target, BasicBlock* false_target, diag::SourceSpan span = {})
        : Instruction(Opcode::CondBranch, span),
          condition_(std::move(condition)),
          true_target_(true_target),
          false_target_(false_target) {}

    [[nodiscard]] bool is_terminator() const noexcept override { return true; }
    [[nodiscard]] const Operand& get_condition() const noexcept { return condition_; }
    [[nodiscard]] Operand& get_condition() noexcept { return condition_; }
    [[nodiscard]] BasicBlock* get_true_target() const noexcept { return true_target_; }
    [[nodiscard]] BasicBlock* get_false_target() const noexcept { return false_target_; }

    void set_true_target(BasicBlock* target) noexcept { true_target_ = target; }
    void set_false_target(BasicBlock* target) noexcept { false_target_ = target; }

    [[nodiscard]] std::vector<BasicBlock*> get_successors() const override;
    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&condition_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&condition_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand condition_;
    BasicBlock* true_target_;
    BasicBlock* false_target_;
};

/// Multi-way branch: switch cond, [val -> target], default_target
class SwitchInst : public Instruction {
public:
    SwitchInst(Operand condition, BasicBlock* default_target, diag::SourceSpan span = {})
        : Instruction(Opcode::Switch, span),
          condition_(std::move(condition)),
          default_target_(default_target) {}

    void add_case(Operand val, BasicBlock* target) {
        cases_.emplace_back(std::move(val), target);
    }

    [[nodiscard]] bool is_terminator() const noexcept override { return true; }
    [[nodiscard]] const Operand& get_condition() const noexcept { return condition_; }
    [[nodiscard]] Operand& get_condition() noexcept { return condition_; }
    [[nodiscard]] BasicBlock* get_default_target() const noexcept { return default_target_; }
    [[nodiscard]] const std::vector<std::pair<Operand, BasicBlock*>>& get_cases() const noexcept { return cases_; }

    [[nodiscard]] std::vector<BasicBlock*> get_successors() const override;
    [[nodiscard]] std::vector<Operand*> get_operands() override;
    [[nodiscard]] std::vector<const Operand*> get_operands() const override;

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand condition_;
    BasicBlock* default_target_;
    std::vector<std::pair<Operand, BasicBlock*>> cases_;
};

/// Return statement: ret [value]
class ReturnInst : public Instruction {
public:
    explicit ReturnInst(std::optional<Operand> value = std::nullopt, diag::SourceSpan span = {})
        : Instruction(Opcode::Return, span), value_(std::move(value)) {}

    [[nodiscard]] bool is_terminator() const noexcept override { return true; }
    [[nodiscard]] bool has_value() const noexcept { return value_.has_value(); }
    [[nodiscard]] const std::optional<Operand>& get_value() const noexcept { return value_; }
    [[nodiscard]] std::optional<Operand>& get_value() noexcept { return value_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override;
    [[nodiscard]] std::vector<const Operand*> get_operands() const override;

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    std::optional<Operand> value_;
};

/// Error throw / zonk raise: throw error
class ThrowInst : public Instruction {
public:
    explicit ThrowInst(Operand error, diag::SourceSpan span = {})
        : Instruction(Opcode::Throw, span), error_(std::move(error)) {}

    [[nodiscard]] bool is_terminator() const noexcept override { return true; }
    [[nodiscard]] const Operand& get_error() const noexcept { return error_; }
    [[nodiscard]] Operand& get_error() noexcept { return error_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&error_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&error_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand error_;
};

/// Unreachable instruction: marks impossible execution paths
class UnreachableInst : public Instruction {
public:
    explicit UnreachableInst(diag::SourceSpan span = {})
        : Instruction(Opcode::Unreachable, span) {}

    [[nodiscard]] bool is_terminator() const noexcept override { return true; }
    [[nodiscard]] std::vector<Operand*> get_operands() override { return {}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;
};

// -----------------------------------------------------------------------------
// Variables, Allocations and Memory Operations
// -----------------------------------------------------------------------------

/// Allocation / local variable declaration: alloca var_name: type (mut?)
class AllocaInst : public Instruction {
public:
    AllocaInst(std::string name, typechecker::TypePtr type, bool is_mut, diag::SourceSpan span = {})
        : Instruction(Opcode::Alloca, span),
          name_(std::move(name)),
          type_(std::move(type)),
          is_mut_(is_mut) {}

    [[nodiscard]] const std::string& get_name() const noexcept { return name_; }
    [[nodiscard]] typechecker::TypePtr get_type() const noexcept { return type_; }
    [[nodiscard]] bool is_mut() const noexcept { return is_mut_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    std::string name_;
    typechecker::TypePtr type_;
    bool is_mut_;
};

/// Load variable to virtual register: dest = load src
class LoadInst : public Instruction {
public:
    LoadInst(Operand dest, Operand src, diag::SourceSpan span = {})
        : Instruction(Opcode::Load, span),
          dest_(std::move(dest)),
          src_(std::move(src)) {}

    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const Operand& get_src() const noexcept { return src_; }
    [[nodiscard]] Operand& get_src() noexcept { return src_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&dest_, &src_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&dest_, &src_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand dest_;
    Operand src_;
};

/// Store value into variable: store dest, val
class StoreInst : public Instruction {
public:
    StoreInst(Operand dest, Operand val, diag::SourceSpan span = {})
        : Instruction(Opcode::Store, span),
          dest_(std::move(dest)),
          val_(std::move(val)) {}

    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const Operand& get_val() const noexcept { return val_; }
    [[nodiscard]] Operand& get_val() noexcept { return val_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&dest_, &val_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&dest_, &val_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand dest_;
    Operand val_;
};

/// Assignment: dest = val
class AssignInst : public Instruction {
public:
    AssignInst(Operand dest, Operand val, diag::SourceSpan span = {})
        : Instruction(Opcode::Assign, span),
          dest_(std::move(dest)),
          val_(std::move(val)) {}

    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const Operand& get_val() const noexcept { return val_; }
    [[nodiscard]] Operand& get_val() noexcept { return val_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&dest_, &val_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&dest_, &val_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand dest_;
    Operand val_;
};

// -----------------------------------------------------------------------------
// Computation Instructions
// -----------------------------------------------------------------------------

/// Binary arithmetic or logic: dest = op lhs, rhs
class BinaryInst : public Instruction {
public:
    BinaryInst(BinaryOpKind op, Operand dest, Operand lhs, Operand rhs, diag::SourceSpan span = {})
        : Instruction(Opcode::Binary, span),
          op_(op),
          dest_(std::move(dest)),
          lhs_(std::move(lhs)),
          rhs_(std::move(rhs)) {}

    [[nodiscard]] BinaryOpKind get_op() const noexcept { return op_; }
    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const Operand& get_lhs() const noexcept { return lhs_; }
    [[nodiscard]] Operand& get_lhs() noexcept { return lhs_; }
    [[nodiscard]] const Operand& get_rhs() const noexcept { return rhs_; }
    [[nodiscard]] Operand& get_rhs() noexcept { return rhs_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&dest_, &lhs_, &rhs_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&dest_, &lhs_, &rhs_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    BinaryOpKind op_;
    Operand dest_;
    Operand lhs_;
    Operand rhs_;
};

/// Unary arithmetic or logic: dest = op val
class UnaryInst : public Instruction {
public:
    UnaryInst(UnaryOpKind op, Operand dest, Operand val, diag::SourceSpan span = {})
        : Instruction(Opcode::Unary, span),
          op_(op),
          dest_(std::move(dest)),
          val_(std::move(val)) {}

    [[nodiscard]] UnaryOpKind get_op() const noexcept { return op_; }
    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const Operand& get_val() const noexcept { return val_; }
    [[nodiscard]] Operand& get_val() noexcept { return val_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&dest_, &val_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&dest_, &val_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    UnaryOpKind op_;
    Operand dest_;
    Operand val_;
};

// -----------------------------------------------------------------------------
// Invocations and Calls
// -----------------------------------------------------------------------------

/// Function call: [dest =] call callee(args...)
class CallInst : public Instruction {
public:
    CallInst(std::optional<Operand> dest, std::string callee, std::vector<Operand> args, diag::SourceSpan span = {})
        : Instruction(Opcode::Call, span),
          dest_(std::move(dest)),
          callee_(std::move(callee)),
          args_(std::move(args)) {}

    [[nodiscard]] const std::optional<Operand>& get_dest() const noexcept { return dest_; }
    [[nodiscard]] std::optional<Operand>& get_dest() noexcept { return dest_; }
    [[nodiscard]] const std::string& get_callee() const noexcept { return callee_; }
    [[nodiscard]] const std::vector<Operand>& get_args() const noexcept { return args_; }
    [[nodiscard]] std::vector<Operand>& get_args() noexcept { return args_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override;
    [[nodiscard]] std::vector<const Operand*> get_operands() const override;

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    std::optional<Operand> dest_;
    std::string callee_;
    std::vector<Operand> args_;
};

/// Method call: [dest =] method_call receiver.method(args...)
class MethodCallInst : public Instruction {
public:
    MethodCallInst(std::optional<Operand> dest, Operand receiver, std::string method, std::vector<Operand> args, diag::SourceSpan span = {})
        : Instruction(Opcode::MethodCall, span),
          dest_(std::move(dest)),
          receiver_(std::move(receiver)),
          method_(std::move(method)),
          args_(std::move(args)) {}

    [[nodiscard]] const std::optional<Operand>& get_dest() const noexcept { return dest_; }
    [[nodiscard]] std::optional<Operand>& get_dest() noexcept { return dest_; }
    [[nodiscard]] const Operand& get_receiver() const noexcept { return receiver_; }
    [[nodiscard]] Operand& get_receiver() noexcept { return receiver_; }
    [[nodiscard]] const std::string& get_method() const noexcept { return method_; }
    [[nodiscard]] const std::vector<Operand>& get_args() const noexcept { return args_; }
    [[nodiscard]] std::vector<Operand>& get_args() noexcept { return args_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override;
    [[nodiscard]] std::vector<const Operand*> get_operands() const override;

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    std::optional<Operand> dest_;
    Operand receiver_;
    std::string method_;
    std::vector<Operand> args_;
};

// -----------------------------------------------------------------------------
// Collections, Aggregates and Indexing
// -----------------------------------------------------------------------------

/// Array construct: dest = array_construct [elements...] [reserve: n]
class ArrayConstructInst : public Instruction {
public:
    ArrayConstructInst(Operand dest, std::vector<Operand> elements, size_t reserved_capacity = 0, diag::SourceSpan span = {})
        : Instruction(Opcode::ArrayConstruct, span),
          dest_(std::move(dest)),
          elements_(std::move(elements)),
          reserved_capacity_(reserved_capacity) {}

    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const std::vector<Operand>& get_elements() const noexcept { return elements_; }
    [[nodiscard]] std::vector<Operand>& get_elements() noexcept { return elements_; }
    [[nodiscard]] size_t get_reserved_capacity() const noexcept { return reserved_capacity_; }
    void set_reserved_capacity(size_t cap) noexcept { reserved_capacity_ = cap; }

    [[nodiscard]] std::vector<Operand*> get_operands() override;
    [[nodiscard]] std::vector<const Operand*> get_operands() const override;

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand dest_;
    std::vector<Operand> elements_;
    size_t reserved_capacity_{0};
};

/// Map construct: dest = map_construct {k1: v1, ...}
class MapConstructInst : public Instruction {
public:
    MapConstructInst(Operand dest, std::vector<std::pair<Operand, Operand>> entries, diag::SourceSpan span = {})
        : Instruction(Opcode::MapConstruct, span),
          dest_(std::move(dest)),
          entries_(std::move(entries)) {}

    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const std::vector<std::pair<Operand, Operand>>& get_entries() const noexcept { return entries_; }
    [[nodiscard]] std::vector<std::pair<Operand, Operand>>& get_entries() noexcept { return entries_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override;
    [[nodiscard]] std::vector<const Operand*> get_operands() const override;

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand dest_;
    std::vector<std::pair<Operand, Operand>> entries_;
};

/// Class / struct construct: dest = class_construct ClassName { field1: v1, ... }
class ClassConstructInst : public Instruction {
public:
    ClassConstructInst(Operand dest, std::string class_name, std::vector<std::pair<std::string, Operand>> fields, diag::SourceSpan span = {})
        : Instruction(Opcode::ClassConstruct, span),
          dest_(std::move(dest)),
          class_name_(std::move(class_name)),
          fields_(std::move(fields)) {}

    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const std::string& get_class_name() const noexcept { return class_name_; }
    [[nodiscard]] const std::vector<std::pair<std::string, Operand>>& get_fields() const noexcept { return fields_; }
    [[nodiscard]] std::vector<std::pair<std::string, Operand>>& get_fields() noexcept { return fields_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override;
    [[nodiscard]] std::vector<const Operand*> get_operands() const override;

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand dest_;
    std::string class_name_;
    std::vector<std::pair<std::string, Operand>> fields_;
};

/// Index load: dest = container[index]
class IndexLoadInst : public Instruction {
public:
    IndexLoadInst(Operand dest, Operand container, Operand index, diag::SourceSpan span = {})
        : Instruction(Opcode::IndexLoad, span),
          dest_(std::move(dest)),
          container_(std::move(container)),
          index_(std::move(index)) {}

    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const Operand& get_container() const noexcept { return container_; }
    [[nodiscard]] Operand& get_container() noexcept { return container_; }
    [[nodiscard]] const Operand& get_index() const noexcept { return index_; }
    [[nodiscard]] Operand& get_index() noexcept { return index_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&dest_, &container_, &index_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&dest_, &container_, &index_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand dest_;
    Operand container_;
    Operand index_;
};

/// Index store: container[index] = val
class IndexStoreInst : public Instruction {
public:
    IndexStoreInst(Operand container, Operand index, Operand val, diag::SourceSpan span = {})
        : Instruction(Opcode::IndexStore, span),
          container_(std::move(container)),
          index_(std::move(index)),
          val_(std::move(val)) {}

    [[nodiscard]] const Operand& get_container() const noexcept { return container_; }
    [[nodiscard]] Operand& get_container() noexcept { return container_; }
    [[nodiscard]] const Operand& get_index() const noexcept { return index_; }
    [[nodiscard]] Operand& get_index() noexcept { return index_; }
    [[nodiscard]] const Operand& get_val() const noexcept { return val_; }
    [[nodiscard]] Operand& get_val() noexcept { return val_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&container_, &index_, &val_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&container_, &index_, &val_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand container_;
    Operand index_;
    Operand val_;
};

/// Member load: dest = obj.member
class MemberLoadInst : public Instruction {
public:
    MemberLoadInst(Operand dest, Operand obj, std::string member, diag::SourceSpan span = {})
        : Instruction(Opcode::MemberLoad, span),
          dest_(std::move(dest)),
          obj_(std::move(obj)),
          member_(std::move(member)) {}

    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const Operand& get_obj() const noexcept { return obj_; }
    [[nodiscard]] Operand& get_obj() noexcept { return obj_; }
    [[nodiscard]] const std::string& get_member() const noexcept { return member_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&dest_, &obj_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&dest_, &obj_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand dest_;
    Operand obj_;
    std::string member_;
};

/// Member store: obj.member = val
class MemberStoreInst : public Instruction {
public:
    MemberStoreInst(Operand obj, std::string member, Operand val, diag::SourceSpan span = {})
        : Instruction(Opcode::MemberStore, span),
          obj_(std::move(obj)),
          member_(std::move(member)),
          val_(std::move(val)) {}

    [[nodiscard]] const Operand& get_obj() const noexcept { return obj_; }
    [[nodiscard]] Operand& get_obj() noexcept { return obj_; }
    [[nodiscard]] const std::string& get_member() const noexcept { return member_; }
    [[nodiscard]] const Operand& get_val() const noexcept { return val_; }
    [[nodiscard]] Operand& get_val() noexcept { return val_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&obj_, &val_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&obj_, &val_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand obj_;
    std::string member_;
    Operand val_;
};

/// Capacity pre-allocation hint (for pipeline optimizations): reserve collection, capacity
class ReserveInst : public Instruction {
public:
    ReserveInst(Operand collection, Operand capacity, diag::SourceSpan span = {})
        : Instruction(Opcode::Reserve, span),
          collection_(std::move(collection)),
          capacity_(std::move(capacity)) {}

    [[nodiscard]] const Operand& get_collection() const noexcept { return collection_; }
    [[nodiscard]] Operand& get_collection() noexcept { return collection_; }
    [[nodiscard]] const Operand& get_capacity() const noexcept { return capacity_; }
    [[nodiscard]] Operand& get_capacity() noexcept { return capacity_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&collection_, &capacity_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&collection_, &capacity_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand collection_;
    Operand capacity_;
};

// -----------------------------------------------------------------------------
// Type and Pattern Operations
// -----------------------------------------------------------------------------

/// Type reflection: dest = typeof src
class TypeOfInst : public Instruction {
public:
    TypeOfInst(Operand dest, Operand src, diag::SourceSpan span = {})
        : Instruction(Opcode::TypeOf, span),
          dest_(std::move(dest)),
          src_(std::move(src)) {}

    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const Operand& get_src() const noexcept { return src_; }
    [[nodiscard]] Operand& get_src() noexcept { return src_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&dest_, &src_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&dest_, &src_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand dest_;
    Operand src_;
};

/// Explicit type cast / narrowing: dest = cast<T> src
class TypeCastInst : public Instruction {
public:
    TypeCastInst(Operand dest, Operand src, typechecker::TypePtr target_type, diag::SourceSpan span = {})
        : Instruction(Opcode::TypeCast, span),
          dest_(std::move(dest)),
          src_(std::move(src)),
          target_type_(std::move(target_type)) {}

    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const Operand& get_src() const noexcept { return src_; }
    [[nodiscard]] Operand& get_src() noexcept { return src_; }
    [[nodiscard]] typechecker::TypePtr get_target_type() const noexcept { return target_type_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&dest_, &src_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&dest_, &src_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand dest_;
    Operand src_;
    typechecker::TypePtr target_type_;
};

/// Recursive smart-pointer boxing: dest = box src (keet::deep_copy_ptr<T>)
class BoxInst : public Instruction {
public:
    BoxInst(Operand dest, Operand src, diag::SourceSpan span = {})
        : Instruction(Opcode::Box, span),
          dest_(std::move(dest)),
          src_(std::move(src)) {}

    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const Operand& get_src() const noexcept { return src_; }
    [[nodiscard]] Operand& get_src() noexcept { return src_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&dest_, &src_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&dest_, &src_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand dest_;
    Operand src_;
};

/// Recursive smart-pointer unboxing: dest = unbox src (*ptr)
class UnboxInst : public Instruction {
public:
    UnboxInst(Operand dest, Operand src, diag::SourceSpan span = {})
        : Instruction(Opcode::Unbox, span),
          dest_(std::move(dest)),
          src_(std::move(src)) {}

    [[nodiscard]] const Operand& get_dest() const noexcept { return dest_; }
    [[nodiscard]] Operand& get_dest() noexcept { return dest_; }
    [[nodiscard]] const Operand& get_src() const noexcept { return src_; }
    [[nodiscard]] Operand& get_src() noexcept { return src_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&dest_, &src_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&dest_, &src_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand dest_;
    Operand src_;
};

// -----------------------------------------------------------------------------
// Exception and Landing Pad
// -----------------------------------------------------------------------------

/// Landing pad for zonk / exception recovery: dest = landing_pad
class LandingPadInst : public Instruction {
public:
    explicit LandingPadInst(Operand error_dest, diag::SourceSpan span = {})
        : Instruction(Opcode::LandingPad, span),
          error_dest_(std::move(error_dest)) {}

    [[nodiscard]] const Operand& get_error_dest() const noexcept { return error_dest_; }
    [[nodiscard]] Operand& get_error_dest() noexcept { return error_dest_; }

    [[nodiscard]] std::vector<Operand*> get_operands() override { return {&error_dest_}; }
    [[nodiscard]] std::vector<const Operand*> get_operands() const override { return {&error_dest_}; }

    [[nodiscard]] std::string to_string() const override;
    [[nodiscard]] std::unique_ptr<Instruction> clone() const override;

private:
    Operand error_dest_;
};

} // namespace keety::ir
