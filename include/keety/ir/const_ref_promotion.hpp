#pragma once

#include <keety/ir/function.hpp>
#include <keety/ir/instruction.hpp>
#include <keety/ir/module.hpp>
#include <keety/typechecker/type.hpp>

#include <cstddef>
#include <string>
#include <unordered_set>

namespace keety::ir {

/// Configuration options for the Const-Reference Parameter Promotion Pass
struct ConstRefPromotionOptions {
    /// Whether union types should also be considered composite (default: true)
    bool include_unions{true};
};

/// Pass 3: Const-Reference Parameter Promotion Optimization Pass
///
/// Scans function signatures and bodies to detect composite value parameters
/// (string, array, map, class, union) that are never mutated within the function.
/// Promotes them to `const T&` (via `is_const_ref = true`) in generated C++ signatures,
/// eliminating unnecessary deep copies while preserving language-level value semantics.
class ConstRefPromotionPass {
public:
    explicit ConstRefPromotionPass(ConstRefPromotionOptions options = {})
        : options_(options) {}

    /// Runs the const-reference parameter promotion pass on a single function.
    /// Promotes eligible unmutated composite parameters by setting `is_const_ref = true`.
    /// Returns the number of parameters promoted.
    size_t run_on_function(Function& fn);

    /// Runs the promotion pass on all functions in a module.
    /// Returns the total number of parameters promoted across the module.
    size_t run_on_module(Module& mod);

    /// Checks if a type is a composite type eligible for const-reference promotion.
    [[nodiscard]] bool is_composite_type(const typechecker::TypePtr& type) const;

    /// Checks if a named parameter is mutated anywhere within the function body.
    [[nodiscard]] bool is_mutated(const std::string& param_name, const Function& fn) const;

    /// Checks if a function parameter is eligible for const-reference promotion.
    [[nodiscard]] bool is_promotable(const FunctionParameter& param, const Function& fn) const;

    [[nodiscard]] const ConstRefPromotionOptions& get_options() const noexcept { return options_; }
    void set_options(ConstRefPromotionOptions options) noexcept { options_ = options; }

private:
    ConstRefPromotionOptions options_;
};

} // namespace keety::ir
