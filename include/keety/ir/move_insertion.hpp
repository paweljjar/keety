#pragma once

#include <keety/ir/function.hpp>
#include <keety/ir/instruction.hpp>
#include <keety/ir/liveness_analysis.hpp>
#include <keety/ir/module.hpp>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace keety::ir {

/// Configuration options for the Move Insertion Optimization Pass
struct MoveInsertionOptions {
    /// Whether to insert std::move for scalar primitive variables (int, float, bool, char).
    /// Defaults to true so all eligible local variables and value parameters are moved.
    bool move_scalars{true};
};

/// Pass 2: Last-Use Move Insertion Optimization Pass
///
/// Analyzes variable lifetimes using static liveness analysis results
/// and marks final reads of r-value eligible variables as moves (`std::move`).
class MoveInsertionPass {
public:
    explicit MoveInsertionPass(MoveInsertionOptions options = {})
        : options_(options) {}

    /// Runs the move insertion pass on a single function using existing liveness analysis results.
    /// Returns the number of operands transformed to moves.
    size_t run_on_function(Function& fn, const LivenessResult& liveness);

    /// Computes static liveness analysis and runs the move insertion pass on a single function.
    /// Returns the number of operands transformed to moves.
    size_t run_on_function(Function& fn);

    /// Runs the move insertion pass on all functions in a module.
    /// Returns the total number of operands transformed to moves.
    size_t run_on_module(Module& mod);

    /// Checks if a variable operand in a function is eligible for move conversion.
    [[nodiscard]] bool is_rvalue_eligible(const Operand& op, const Function& fn) const;

    [[nodiscard]] const MoveInsertionOptions& get_options() const noexcept { return options_; }
    void set_options(MoveInsertionOptions options) noexcept { options_ = options; }

private:
    MoveInsertionOptions options_;

    /// Helper to attempt marking an operand as a move if it is eligible and is at a last-use point
    bool try_mark_move(Operand& op, const Instruction* inst, const Function& fn, const LivenessResult& liveness);

    /// Helper to resolve the type of a variable in a function
    [[nodiscard]] typechecker::TypePtr resolve_variable_type(const std::string& var_name, const Function& fn) const;
};

} // namespace keety::ir
