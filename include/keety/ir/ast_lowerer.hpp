#pragma once

#include <keety/ast/decl.hpp>
#include <keety/ast/expr.hpp>
#include <keety/ast/stmt.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/ir/function.hpp>
#include <keety/ir/instruction.hpp>
#include <keety/ir/module.hpp>
#include <keety/ir/operand.hpp>
#include <keety/parser/parser.hpp>

#include <memory>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace keety::ir {

/// Context entry for active loops during lowering
struct LoopContext {
    std::string label;
    BasicBlock* continue_target{nullptr};
    BasicBlock* break_target{nullptr};
};

/// Context entry for active switch statements during lowering
struct SwitchContext {
    std::string label;
    BasicBlock* break_target{nullptr};
};

/// Context entry for active zonk recovery blocks during lowering
struct ZonkContext {
    Operand result_var;
    BasicBlock* merge_target{nullptr};
};

/// Lowers validated AST nodes into KeetIR Control Flow Graphs and instructions
class AstLowerer {
public:
    explicit AstLowerer(diag::DiagnosticEmitter* emitter = nullptr);

    /// Lowers an entire parsed module AST into a KeetIR Module
    std::unique_ptr<Module> lower_module(const parser::ModuleAST& module_ast);

    /// Lowers an individual function declaration into a KeetIR Function
    std::unique_ptr<Function> lower_function(const ast::FnDecl& fn_decl);

    /// Lowers an AST statement in the current basic block
    void lower_statement(const ast::Stmt& stmt);

    /// Lowers an AST expression, emitting instructions and returning the resulting Operand
    Operand lower_expression(const ast::Expr& expr);

    // Context accessors
    void set_current_function(Function* fn) noexcept { current_fn_ = fn; }
    [[nodiscard]] Function* get_current_function() const noexcept { return current_fn_; }

    void set_current_block(BasicBlock* bb) noexcept { current_bb_ = bb; }
    [[nodiscard]] BasicBlock* get_current_block() const noexcept { return current_bb_; }

    BasicBlock* create_block(const std::string& prefix = "bb");
    void emit(std::unique_ptr<Instruction> inst);

    void set_emitter(diag::DiagnosticEmitter* emitter) noexcept { emitter_ = emitter; }
    [[nodiscard]] diag::DiagnosticEmitter* get_emitter() const noexcept { return emitter_; }

    [[nodiscard]] diag::SourceSpan get_current_span() const noexcept { return current_span_; }
    void set_current_span(diag::SourceSpan span) noexcept { current_span_ = span; }

private:
    diag::DiagnosticEmitter* emitter_{nullptr};
    Module* current_module_{nullptr};
    Function* current_fn_{nullptr};
    BasicBlock* current_bb_{nullptr};
    diag::SourceSpan current_span_{};

    // Control flow stacks for break/continue
    std::vector<LoopContext> loop_stack_;
    std::vector<SwitchContext> switch_stack_;
    std::vector<ZonkContext> zonk_stack_;
    enum class BreakContextKind { Loop, Switch };
    std::vector<std::pair<BreakContextKind, size_t>> break_order_;

    // Statements
    void lower_block_stmt(const ast::BlockStmt& stmt);
    void lower_decl_stmt(const ast::DeclStmt& stmt);
    void lower_expr_stmt(const ast::ExprStmt& stmt);
    void lower_if_stmt(const ast::IfStmt& stmt);
    void lower_while_stmt(const ast::WhileStmt& stmt);
    void lower_for_stmt(const ast::ForStmt& stmt);
    void lower_for_in_stmt(const ast::ForInStmt& stmt);
    void lower_switch_stmt(const ast::SwitchStmt& stmt);
    void lower_return_stmt(const ast::ReturnStmt& stmt);
    void lower_break_stmt(const ast::BreakStmt& stmt);
    void lower_continue_stmt(const ast::ContinueStmt& stmt);
    void lower_zonk_stmt(const ast::ZonkStmt& stmt);
    void lower_yield_stmt(const ast::YieldStmt& stmt);

    // Expressions
    Operand lower_literal_expr(const ast::LiteralExpr& expr);
    Operand lower_ident_expr(const ast::IdentExpr& expr);
    Operand lower_binary_expr(const ast::BinaryExpr& expr);
    Operand lower_unary_expr(const ast::UnaryExpr& expr);
    Operand lower_call_expr(const ast::CallExpr& expr);
    Operand lower_member_expr(const ast::MemberExpr& expr);
    Operand lower_index_expr(const ast::IndexExpr& expr);
    Operand lower_array_lit_expr(const ast::ArrayLitExpr& expr);
    Operand lower_map_lit_expr(const ast::MapLitExpr& expr);
    Operand lower_ternary_expr(const ast::TernaryExpr& expr);
    Operand lower_typecast_expr(const ast::TypeCastExpr& expr);
    Operand lower_typeof_expr(const ast::TypeofExpr& expr);
    Operand lower_zonk_expr(const ast::ZonkExpr& expr);

    // Helpers
    BasicBlock* find_break_target(const std::string& label) const;
    BasicBlock* find_continue_target(const std::string& label) const;

    bool has_io_import_{false};
    std::unordered_set<std::string> imported_io_symbols_;
    bool has_fs_import_{false};
    std::unordered_set<std::string> imported_fs_symbols_;
    bool has_sys_import_{false};
    std::unordered_set<std::string> imported_sys_symbols_;
    bool has_math_import_{false};
    std::unordered_set<std::string> imported_math_symbols_;
    std::unordered_set<std::string> imported_math_constants_;
    bool has_time_import_{false};
    std::unordered_set<std::string> imported_time_symbols_;
    std::unordered_set<std::string> imported_time_constants_;
    bool has_json_import_{false};
    std::unordered_set<std::string> json_aliases_;
    std::unordered_set<std::string> imported_json_symbols_;
    bool has_net_import_{false};
    std::unordered_set<std::string> net_aliases_;
    std::unordered_set<std::string> imported_net_symbols_;
    bool has_crypto_import_{false};
    std::unordered_set<std::string> crypto_aliases_;
    std::unordered_set<std::string> imported_crypto_symbols_;
    bool has_http_import_{false};
    std::unordered_set<std::string> http_aliases_;
    std::unordered_set<std::string> imported_http_symbols_;
};

} // namespace keety::ir
