#pragma once

#include <keety/ir/basic_block.hpp>
#include <keety/ir/function.hpp>
#include <keety/ir/instruction.hpp>
#include <keety/ir/module.hpp>

#include <cstddef>
#include <string>
#include <unordered_set>

namespace keety::ir {

/// Configuration options for the Pipeline Capacity Pre-Allocation Pass
struct PipelineReserveOptions {
    /// Recognized functional pipeline method names that produce collection transformations
    std::unordered_set<std::string> pipeline_methods{"map", "filter", "flatMap"};

    /// Whether to use integer constant capacity when source is a fixed-size literal array
    bool use_constant_capacity_for_literals{true};
};

/// Pass 4: Pipeline Capacity Pre-Allocation Optimization Pass
///
/// Recognizes array functional transformation pipelines (such as `.map()`, `.filter()`)
/// and inserts capacity pre-allocation directives (`reserve dest, src`) to prevent
/// dynamic geometric reallocations and memory overhead in generated C++ code.
class PipelineReservePass {
public:
    explicit PipelineReservePass(PipelineReserveOptions options = {})
        : options_(std::move(options)) {}

    /// Runs the pipeline capacity pre-allocation pass on a single function.
    /// Inserts `ReserveInst` instructions following functional pipeline stages.
    /// Returns the number of reserve instructions inserted.
    size_t run_on_function(Function& fn);

    /// Runs the pipeline capacity pre-allocation pass on all functions in a module.
    /// Returns the total number of reserve instructions inserted across the module.
    size_t run_on_module(Module& mod);

    /// Finds the root source collection operand for a pipeline receiver operand
    [[nodiscard]] Operand find_pipeline_root(const Operand& receiver, const Function& fn) const;

    /// Finds the instruction that produces a given operand value
    [[nodiscard]] const Instruction* find_producer(const Operand& op, const Function& fn) const;

    /// Checks if a method call instruction represents a pipeline transformation stage
    [[nodiscard]] bool is_pipeline_stage(const MethodCallInst& inst) const;

    [[nodiscard]] const PipelineReserveOptions& get_options() const noexcept { return options_; }
    void set_options(PipelineReserveOptions options) noexcept { options_ = std::move(options); }

private:
    PipelineReserveOptions options_;
};

} // namespace keety::ir
