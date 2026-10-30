#pragma once

#include <keety/ir/basic_block.hpp>
#include <keety/ir/function.hpp>
#include <keety/ir/instruction.hpp>
#include <keety/ir/module.hpp>

#include <iosfwd>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace keety::ir {

/// Liveness sets for a single basic block in the CFG
struct BlockLiveness {
    std::unordered_set<std::string> defs;     ///< Variables defined before any read in block
    std::unordered_set<std::string> uses;     ///< Variables read before any definition in block
    std::unordered_set<std::string> live_in;  ///< Variables live at block entry
    std::unordered_set<std::string> live_out; ///< Variables live at block exit
};

/// Live range tracking for an individual variable within a function
struct LiveRange {
    std::string variable_name;
    std::vector<const Instruction*> definitions;
    std::vector<const Instruction*> uses;
    std::vector<const Instruction*> last_uses;

    [[nodiscard]] bool is_last_use(const Instruction* inst) const noexcept;
    [[nodiscard]] bool is_defined_at(const Instruction* inst) const noexcept;
    [[nodiscard]] bool is_used_at(const Instruction* inst) const noexcept;
};

/// Comprehensive result of static liveness analysis on a Function
class LivenessResult {
public:
    explicit LivenessResult(const Function* function = nullptr)
        : function_(function) {}

    [[nodiscard]] const Function* get_function() const noexcept { return function_; }

    [[nodiscard]] const std::unordered_map<const BasicBlock*, BlockLiveness>& get_block_liveness() const noexcept {
        return block_liveness_;
    }
    [[nodiscard]] const BlockLiveness* get_block_liveness(const BasicBlock* bb) const noexcept;

    [[nodiscard]] const std::unordered_map<std::string, LiveRange>& get_live_ranges() const noexcept {
        return live_ranges_;
    }
    [[nodiscard]] const LiveRange* get_live_range(const std::string& var_name) const noexcept;

    /// Checks if a given variable is used for the last time at a specific instruction
    [[nodiscard]] bool is_last_use(const Instruction* inst, const std::string& var_name) const noexcept;

    /// Returns all variables whose last use occurs at the given instruction
    [[nodiscard]] std::vector<std::string> get_last_used_vars(const Instruction* inst) const;

    /// Checks if a variable is live at the entry of a basic block
    [[nodiscard]] bool is_live_in(const BasicBlock* bb, const std::string& var_name) const noexcept;

    /// Checks if a variable is live at the exit of a basic block
    [[nodiscard]] bool is_live_out(const BasicBlock* bb, const std::string& var_name) const noexcept;

    /// Serializes liveness information for diagnostics or IR inspection
    [[nodiscard]] std::string to_string() const;
    void dump(std::ostream& os) const;

    // Internal builder methods
    void set_block_liveness(const BasicBlock* bb, BlockLiveness liveness);
    void add_definition(const std::string& var_name, const Instruction* inst);
    void add_use(const std::string& var_name, const Instruction* inst);
    void add_last_use(const std::string& var_name, const Instruction* inst);

private:
    const Function* function_{nullptr};
    std::unordered_map<const BasicBlock*, BlockLiveness> block_liveness_;
    std::unordered_map<std::string, LiveRange> live_ranges_;
    std::unordered_map<const Instruction*, std::vector<std::string>> last_uses_by_instruction_;
};

/// Static Liveness Analysis Pass (Pass 1)
class LivenessAnalysis {
public:
    LivenessAnalysis() = default;

    /// Computes static liveness analysis for a single function
    [[nodiscard]] LivenessResult analyze(const Function& fn) const;

    /// Computes static liveness analysis for all functions in a module
    [[nodiscard]] std::unordered_map<const Function*, LivenessResult> analyze_module(const Module& mod) const;

    /// Extracts variable names defined by an instruction
    [[nodiscard]] static std::vector<std::string> get_defined_vars(const Instruction& inst);

    /// Extracts variable names read/used by an instruction
    [[nodiscard]] static std::vector<std::string> get_used_vars(const Instruction& inst);
};

} // namespace keety::ir
