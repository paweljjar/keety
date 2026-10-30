#include <keety/ir/instruction.hpp>
#include <keety/ir/basic_block.hpp>

#include <sstream>

namespace keety::ir {

std::string to_string(BinaryOpKind op) {
    switch (op) {
        case BinaryOpKind::Add: return "add";
        case BinaryOpKind::Sub: return "sub";
        case BinaryOpKind::Mul: return "mul";
        case BinaryOpKind::Div: return "div";
        case BinaryOpKind::Mod: return "mod";
        case BinaryOpKind::Eq: return "eq";
        case BinaryOpKind::Neq: return "neq";
        case BinaryOpKind::Lt: return "lt";
        case BinaryOpKind::Lte: return "lte";
        case BinaryOpKind::Gt: return "gt";
        case BinaryOpKind::Gte: return "gte";
        case BinaryOpKind::LogicAnd: return "land";
        case BinaryOpKind::LogicOr: return "lor";
        case BinaryOpKind::BitAnd: return "and";
        case BinaryOpKind::BitOr: return "or";
        case BinaryOpKind::BitXor: return "xor";
        case BinaryOpKind::Shl: return "shl";
        case BinaryOpKind::Shr: return "shr";
    }
    return "unknown_binary";
}

std::string to_string(UnaryOpKind op) {
    switch (op) {
        case UnaryOpKind::Neg: return "neg";
        case UnaryOpKind::Not: return "not";
        case UnaryOpKind::BitNot: return "bitnot";
    }
    return "unknown_unary";
}

std::string to_string(Opcode op) {
    switch (op) {
        case Opcode::Branch: return "branch";
        case Opcode::CondBranch: return "cond_branch";
        case Opcode::Switch: return "switch";
        case Opcode::Return: return "return";
        case Opcode::Throw: return "throw";
        case Opcode::Unreachable: return "unreachable";
        case Opcode::Alloca: return "alloca";
        case Opcode::Load: return "load";
        case Opcode::Store: return "store";
        case Opcode::Assign: return "assign";
        case Opcode::Binary: return "binary";
        case Opcode::Unary: return "unary";
        case Opcode::Call: return "call";
        case Opcode::MethodCall: return "method_call";
        case Opcode::ArrayConstruct: return "array_construct";
        case Opcode::MapConstruct: return "map_construct";
        case Opcode::ClassConstruct: return "class_construct";
        case Opcode::IndexLoad: return "index_load";
        case Opcode::IndexStore: return "index_store";
        case Opcode::MemberLoad: return "member_load";
        case Opcode::MemberStore: return "member_store";
        case Opcode::Reserve: return "reserve";
        case Opcode::TypeOf: return "typeof";
        case Opcode::TypeCast: return "type_cast";
        case Opcode::Box: return "box";
        case Opcode::Unbox: return "unbox";
        case Opcode::LandingPad: return "landing_pad";
        case Opcode::Invoke: return "invoke";
    }
    return "unknown_opcode";
}

// -----------------------------------------------------------------------------
// BranchInst
// -----------------------------------------------------------------------------

std::vector<BasicBlock*> BranchInst::get_successors() const {
    if (target_) {
        return {target_};
    }
    return {};
}

std::string BranchInst::to_string() const {
    std::string s = "br ";
    if (target_) {
        s += "@" + target_->get_label();
    } else {
        s += "@<null_bb>";
    }
    return s;
}

std::unique_ptr<Instruction> BranchInst::clone() const {
    return std::make_unique<BranchInst>(target_, get_span());
}

// -----------------------------------------------------------------------------
// CondBranchInst
// -----------------------------------------------------------------------------

std::vector<BasicBlock*> CondBranchInst::get_successors() const {
    std::vector<BasicBlock*> succs;
    if (true_target_) succs.push_back(true_target_);
    if (false_target_ && false_target_ != true_target_) succs.push_back(false_target_);
    return succs;
}

std::string CondBranchInst::to_string() const {
    std::ostringstream ss;
    ss << "br " << condition_.to_string() << ", ";
    ss << (true_target_ ? "@" + true_target_->get_label() : "@<null_bb>") << ", ";
    ss << (false_target_ ? "@" + false_target_->get_label() : "@<null_bb>");
    return ss.str();
}

std::unique_ptr<Instruction> CondBranchInst::clone() const {
    return std::make_unique<CondBranchInst>(condition_, true_target_, false_target_, get_span());
}

// -----------------------------------------------------------------------------
// SwitchInst
// -----------------------------------------------------------------------------

std::vector<BasicBlock*> SwitchInst::get_successors() const {
    std::vector<BasicBlock*> succs;
    for (const auto& [val, target] : cases_) {
        if (target && std::find(succs.begin(), succs.end(), target) == succs.end()) {
            succs.push_back(target);
        }
    }
    if (default_target_ && std::find(succs.begin(), succs.end(), default_target_) == succs.end()) {
        succs.push_back(default_target_);
    }
    return succs;
}

std::vector<Operand*> SwitchInst::get_operands() {
    std::vector<Operand*> ops = {&condition_};
    for (auto& [val, target] : cases_) {
        ops.push_back(&val);
    }
    return ops;
}

std::vector<const Operand*> SwitchInst::get_operands() const {
    std::vector<const Operand*> ops = {&condition_};
    for (const auto& [val, target] : cases_) {
        ops.push_back(&val);
    }
    return ops;
}

std::string SwitchInst::to_string() const {
    std::ostringstream ss;
    ss << "switch " << condition_.to_string() << " [\n";
    for (const auto& [val, target] : cases_) {
        ss << "    " << val.to_string() << " -> @" << (target ? target->get_label() : "<null_bb>") << "\n";
    }
    ss << "    default -> @" << (default_target_ ? default_target_->get_label() : "<null_bb>") << "\n  ]";
    return ss.str();
}

std::unique_ptr<Instruction> SwitchInst::clone() const {
    auto copy = std::make_unique<SwitchInst>(condition_, default_target_, get_span());
    for (const auto& [val, target] : cases_) {
        copy->add_case(val, target);
    }
    return copy;
}

// -----------------------------------------------------------------------------
// ReturnInst
// -----------------------------------------------------------------------------

std::vector<Operand*> ReturnInst::get_operands() {
    if (value_.has_value()) {
        return {&(*value_)};
    }
    return {};
}

std::vector<const Operand*> ReturnInst::get_operands() const {
    if (value_.has_value()) {
        return {&(*value_)};
    }
    return {};
}

std::string ReturnInst::to_string() const {
    if (value_.has_value()) {
        return "ret " + value_->to_string();
    }
    return "ret";
}

std::unique_ptr<Instruction> ReturnInst::clone() const {
    return std::make_unique<ReturnInst>(value_, get_span());
}

// -----------------------------------------------------------------------------
// ThrowInst
// -----------------------------------------------------------------------------

std::string ThrowInst::to_string() const {
    return "throw " + error_.to_string();
}

std::unique_ptr<Instruction> ThrowInst::clone() const {
    return std::make_unique<ThrowInst>(error_, get_span());
}

// -----------------------------------------------------------------------------
// UnreachableInst
// -----------------------------------------------------------------------------

std::string UnreachableInst::to_string() const {
    return "unreachable";
}

std::unique_ptr<Instruction> UnreachableInst::clone() const {
    return std::make_unique<UnreachableInst>(get_span());
}

// -----------------------------------------------------------------------------
// InvokeInst
// -----------------------------------------------------------------------------

std::vector<BasicBlock*> InvokeInst::get_successors() const {
    std::vector<BasicBlock*> succs;
    if (normal_target_) succs.push_back(normal_target_);
    if (unwind_target_ && unwind_target_ != normal_target_) succs.push_back(unwind_target_);
    return succs;
}

std::vector<Operand*> InvokeInst::get_operands() {
    std::vector<Operand*> ops;
    if (dest_.has_value()) {
        ops.push_back(&*dest_);
    }
    for (auto& arg : args_) {
        ops.push_back(&arg);
    }
    return ops;
}

std::vector<const Operand*> InvokeInst::get_operands() const {
    std::vector<const Operand*> ops;
    if (dest_.has_value()) {
        ops.push_back(&*dest_);
    }
    for (const auto& arg : args_) {
        ops.push_back(&arg);
    }
    return ops;
}

std::string InvokeInst::to_string() const {
    std::ostringstream ss;
    if (dest_.has_value()) {
        ss << dest_->to_string() << " = ";
    }
    ss << "invoke @" << callee_ << "(";
    for (size_t i = 0; i < args_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << args_[i].to_string();
    }
    ss << ") to " << (normal_target_ ? "@" + normal_target_->get_label() : "@<null_bb>");
    ss << " unwind " << (unwind_target_ ? "@" + unwind_target_->get_label() : "@<null_bb>");
    return ss.str();
}

std::unique_ptr<Instruction> InvokeInst::clone() const {
    return std::make_unique<InvokeInst>(dest_, callee_, args_, normal_target_, unwind_target_, get_span());
}

// -----------------------------------------------------------------------------
// AllocaInst
// -----------------------------------------------------------------------------

std::string AllocaInst::to_string() const {
    std::ostringstream ss;
    ss << "alloca " << (is_mut_ ? "mut " : "") << name_ << ": " << (type_ ? type_->toString() : "void");
    return ss.str();
}

std::unique_ptr<Instruction> AllocaInst::clone() const {
    return std::make_unique<AllocaInst>(name_, type_, is_mut_, get_span());
}

// -----------------------------------------------------------------------------
// LoadInst
// -----------------------------------------------------------------------------

std::string LoadInst::to_string() const {
    return dest_.to_string() + " = load " + src_.to_string();
}

std::unique_ptr<Instruction> LoadInst::clone() const {
    return std::make_unique<LoadInst>(dest_, src_, get_span());
}

// -----------------------------------------------------------------------------
// StoreInst
// -----------------------------------------------------------------------------

std::string StoreInst::to_string() const {
    return "store " + dest_.to_string() + ", " + val_.to_string();
}

std::unique_ptr<Instruction> StoreInst::clone() const {
    return std::make_unique<StoreInst>(dest_, val_, get_span());
}

// -----------------------------------------------------------------------------
// AssignInst
// -----------------------------------------------------------------------------

std::string AssignInst::to_string() const {
    return dest_.to_string() + " = " + val_.to_string();
}

std::unique_ptr<Instruction> AssignInst::clone() const {
    return std::make_unique<AssignInst>(dest_, val_, get_span());
}

// -----------------------------------------------------------------------------
// BinaryInst
// -----------------------------------------------------------------------------

std::string BinaryInst::to_string() const {
    return dest_.to_string() + " = " + ir::to_string(op_) + " " + lhs_.to_string() + ", " + rhs_.to_string();
}

std::unique_ptr<Instruction> BinaryInst::clone() const {
    return std::make_unique<BinaryInst>(op_, dest_, lhs_, rhs_, get_span());
}

// -----------------------------------------------------------------------------
// UnaryInst
// -----------------------------------------------------------------------------

std::string UnaryInst::to_string() const {
    return dest_.to_string() + " = " + ir::to_string(op_) + " " + val_.to_string();
}

std::unique_ptr<Instruction> UnaryInst::clone() const {
    return std::make_unique<UnaryInst>(op_, dest_, val_, get_span());
}

// -----------------------------------------------------------------------------
// CallInst
// -----------------------------------------------------------------------------

std::vector<Operand*> CallInst::get_operands() {
    std::vector<Operand*> ops;
    if (dest_.has_value()) {
        ops.push_back(&*dest_);
    }
    for (auto& arg : args_) {
        ops.push_back(&arg);
    }
    return ops;
}

std::vector<const Operand*> CallInst::get_operands() const {
    std::vector<const Operand*> ops;
    if (dest_.has_value()) {
        ops.push_back(&*dest_);
    }
    for (const auto& arg : args_) {
        ops.push_back(&arg);
    }
    return ops;
}

std::string CallInst::to_string() const {
    std::ostringstream ss;
    if (dest_.has_value()) {
        ss << dest_->to_string() << " = ";
    }
    ss << "call @" << callee_ << "(";
    for (size_t i = 0; i < args_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << args_[i].to_string();
    }
    ss << ")";
    return ss.str();
}

std::unique_ptr<Instruction> CallInst::clone() const {
    return std::make_unique<CallInst>(dest_, callee_, args_, get_span());
}

// -----------------------------------------------------------------------------
// MethodCallInst
// -----------------------------------------------------------------------------

std::vector<Operand*> MethodCallInst::get_operands() {
    std::vector<Operand*> ops;
    if (dest_.has_value()) {
        ops.push_back(&*dest_);
    }
    ops.push_back(&receiver_);
    for (auto& arg : args_) {
        ops.push_back(&arg);
    }
    return ops;
}

std::vector<const Operand*> MethodCallInst::get_operands() const {
    std::vector<const Operand*> ops;
    if (dest_.has_value()) {
        ops.push_back(&*dest_);
    }
    ops.push_back(&receiver_);
    for (const auto& arg : args_) {
        ops.push_back(&arg);
    }
    return ops;
}

std::string MethodCallInst::to_string() const {
    std::ostringstream ss;
    if (dest_.has_value()) {
        ss << dest_->to_string() << " = ";
    }
    ss << "call_method " << receiver_.to_string() << "." << method_ << "(";
    for (size_t i = 0; i < args_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << args_[i].to_string();
    }
    ss << ")";
    return ss.str();
}

std::unique_ptr<Instruction> MethodCallInst::clone() const {
    return std::make_unique<MethodCallInst>(dest_, receiver_, method_, args_, get_span());
}

// -----------------------------------------------------------------------------
// ArrayConstructInst
// -----------------------------------------------------------------------------

std::vector<Operand*> ArrayConstructInst::get_operands() {
    std::vector<Operand*> ops = {&dest_};
    for (auto& el : elements_) {
        ops.push_back(&el);
    }
    return ops;
}

std::vector<const Operand*> ArrayConstructInst::get_operands() const {
    std::vector<const Operand*> ops = {&dest_};
    for (const auto& el : elements_) {
        ops.push_back(&el);
    }
    return ops;
}

std::string ArrayConstructInst::to_string() const {
    std::ostringstream ss;
    ss << dest_.to_string() << " = array_construct [";
    for (size_t i = 0; i < elements_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << elements_[i].to_string();
    }
    ss << "]";
    if (reserved_capacity_ > 0) {
        ss << " [reserve: " << reserved_capacity_ << "]";
    }
    return ss.str();
}

std::unique_ptr<Instruction> ArrayConstructInst::clone() const {
    return std::make_unique<ArrayConstructInst>(dest_, elements_, reserved_capacity_, get_span());
}

// -----------------------------------------------------------------------------
// MapConstructInst
// -----------------------------------------------------------------------------

std::vector<Operand*> MapConstructInst::get_operands() {
    std::vector<Operand*> ops = {&dest_};
    for (auto& [k, v] : entries_) {
        ops.push_back(&k);
        ops.push_back(&v);
    }
    return ops;
}

std::vector<const Operand*> MapConstructInst::get_operands() const {
    std::vector<const Operand*> ops = {&dest_};
    for (const auto& [k, v] : entries_) {
        ops.push_back(&k);
        ops.push_back(&v);
    }
    return ops;
}

std::string MapConstructInst::to_string() const {
    std::ostringstream ss;
    ss << dest_.to_string() << " = map_construct {";
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << entries_[i].first.to_string() << ": " << entries_[i].second.to_string();
    }
    ss << "}";
    return ss.str();
}

std::unique_ptr<Instruction> MapConstructInst::clone() const {
    return std::make_unique<MapConstructInst>(dest_, entries_, get_span());
}

// -----------------------------------------------------------------------------
// ClassConstructInst
// -----------------------------------------------------------------------------

std::vector<Operand*> ClassConstructInst::get_operands() {
    std::vector<Operand*> ops = {&dest_};
    for (auto& [field, val] : fields_) {
        ops.push_back(&val);
    }
    return ops;
}

std::vector<const Operand*> ClassConstructInst::get_operands() const {
    std::vector<const Operand*> ops = {&dest_};
    for (const auto& [field, val] : fields_) {
        ops.push_back(&val);
    }
    return ops;
}

std::string ClassConstructInst::to_string() const {
    std::ostringstream ss;
    ss << dest_.to_string() << " = class_construct " << class_name_ << " {";
    for (size_t i = 0; i < fields_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << fields_[i].first << ": " << fields_[i].second.to_string();
    }
    ss << "}";
    return ss.str();
}

std::unique_ptr<Instruction> ClassConstructInst::clone() const {
    return std::make_unique<ClassConstructInst>(dest_, class_name_, fields_, get_span());
}

// -----------------------------------------------------------------------------
// Index and Member Instructions
// -----------------------------------------------------------------------------

std::string IndexLoadInst::to_string() const {
    return dest_.to_string() + " = " + container_.to_string() + "[" + index_.to_string() + "]";
}

std::unique_ptr<Instruction> IndexLoadInst::clone() const {
    return std::make_unique<IndexLoadInst>(dest_, container_, index_, get_span());
}

std::string IndexStoreInst::to_string() const {
    return container_.to_string() + "[" + index_.to_string() + "] = " + val_.to_string();
}

std::unique_ptr<Instruction> IndexStoreInst::clone() const {
    return std::make_unique<IndexStoreInst>(container_, index_, val_, get_span());
}

std::string MemberLoadInst::to_string() const {
    return dest_.to_string() + " = " + obj_.to_string() + "." + member_;
}

std::unique_ptr<Instruction> MemberLoadInst::clone() const {
    return std::make_unique<MemberLoadInst>(dest_, obj_, member_, get_span());
}

std::string MemberStoreInst::to_string() const {
    return obj_.to_string() + "." + member_ + " = " + val_.to_string();
}

std::unique_ptr<Instruction> MemberStoreInst::clone() const {
    return std::make_unique<MemberStoreInst>(obj_, member_, val_, get_span());
}

std::string ReserveInst::to_string() const {
    return "reserve " + collection_.to_string() + ", " + capacity_.to_string();
}

std::unique_ptr<Instruction> ReserveInst::clone() const {
    return std::make_unique<ReserveInst>(collection_, capacity_, get_span());
}

// -----------------------------------------------------------------------------
// Type and Pattern Operations
// -----------------------------------------------------------------------------

std::string TypeOfInst::to_string() const {
    return dest_.to_string() + " = typeof " + src_.to_string();
}

std::unique_ptr<Instruction> TypeOfInst::clone() const {
    return std::make_unique<TypeOfInst>(dest_, src_, get_span());
}

std::string TypeCastInst::to_string() const {
    return dest_.to_string() + " = type_cast<" + (target_type_ ? target_type_->toString() : "unknown") + "> " + src_.to_string();
}

std::unique_ptr<Instruction> TypeCastInst::clone() const {
    return std::make_unique<TypeCastInst>(dest_, src_, target_type_, get_span());
}

std::string BoxInst::to_string() const {
    return dest_.to_string() + " = box " + src_.to_string();
}

std::unique_ptr<Instruction> BoxInst::clone() const {
    return std::make_unique<BoxInst>(dest_, src_, get_span());
}

std::string UnboxInst::to_string() const {
    return dest_.to_string() + " = unbox " + src_.to_string();
}

std::unique_ptr<Instruction> UnboxInst::clone() const {
    return std::make_unique<UnboxInst>(dest_, src_, get_span());
}

// -----------------------------------------------------------------------------
// LandingPadInst
// -----------------------------------------------------------------------------

std::string LandingPadInst::to_string() const {
    return error_dest_.to_string() + " = landing_pad";
}

std::unique_ptr<Instruction> LandingPadInst::clone() const {
    return std::make_unique<LandingPadInst>(error_dest_, get_span());
}

} // namespace keety::ir
