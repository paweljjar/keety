#pragma once

#include <keety/ir/basic_block.hpp>
#include <keety/ir/function.hpp>
#include <keety/ir/instruction.hpp>
#include <keety/ir/module.hpp>
#include <keety/parser/parser.hpp>
#include <keety/typechecker/type.hpp>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace keety::ir {

/// Represents metadata about a detected recursive self-referential class field
struct RecursiveFieldInfo {
    std::string class_name;
    std::string field_name;
    typechecker::TypePtr field_type;
};

/// Configuration options for the Recursive Smart-Pointer Boxing Pass
struct RecursiveBoxingOptions {
    /// Whether to automatically unbox when reading from a recursive field in MemberLoadInst
    bool auto_unbox_reads{true};

    /// Whether to automatically box when writing to a recursive field in MemberStoreInst
    bool auto_box_writes{true};

    /// Whether to automatically box recursive fields in ClassConstructInst
    bool auto_box_constructs{true};
};

/// Pass 5: Recursive Smart-Pointer Boxing Optimization Pass
///
/// Detects self-referential class fields (e.g. `class Node { let next: Node | null; }`)
/// and cyclic class structures that would cause infinite-size or incomplete type errors
/// in C++ value types and std::variant.
/// Transparently inserts `BoxInst` and `UnboxInst` (`keet::deep_copy_ptr<T>`) at the KeetIR level.
class RecursiveBoxingPass {
public:
    explicit RecursiveBoxingPass(RecursiveBoxingOptions options = {})
        : options_(options) {}

    /// Registers a class definition and its fields for recursive dependency analysis
    void register_class(std::string name, std::vector<std::pair<std::string, typechecker::TypePtr>> fields);

    /// Scans an AST compilation unit and registers all class declarations
    void scan_classes(const parser::ModuleAST& ast);

    /// Runs the recursive boxing pass on a single function.
    /// Inserts BoxInst and UnboxInst instructions around accesses to self-referential fields.
    /// Returns the total number of boxing and unboxing instructions inserted.
    size_t run_on_function(Function& fn);

    /// Runs the recursive boxing pass across all functions in a module.
    /// Returns the total number of boxing and unboxing instructions inserted.
    size_t run_on_module(Module& mod);

    /// Convenience runner: scans classes from AST, then runs on all functions in the module.
    size_t run_on_module(Module& mod, const parser::ModuleAST& ast);

    /// Checks if a specific field in a class is recursive / self-referential
    [[nodiscard]] bool is_recursive_field(const std::string& class_name, const std::string& field_name) const;

    /// Checks if a class contains any recursive / self-referential fields
    [[nodiscard]] bool is_recursive_class(const std::string& class_name) const;

    /// Returns a list of all detected recursive fields across all registered classes
    [[nodiscard]] std::vector<RecursiveFieldInfo> get_recursive_fields() const;

    /// Resolves the class name for an object operand accessed with a member name
    [[nodiscard]] std::string resolve_class_name(const Operand& obj, const std::string& member_name, const Function& fn) const;

    [[nodiscard]] const RecursiveBoxingOptions& get_options() const noexcept { return options_; }
    void set_options(RecursiveBoxingOptions options) noexcept { options_ = options; }

private:
    RecursiveBoxingOptions options_;

    // Map: class_name -> list of (field_name, field_type)
    std::unordered_map<std::string, std::vector<std::pair<std::string, typechecker::TypePtr>>> class_fields_;

    // Set: "ClassName::fieldName"
    std::unordered_set<std::string> recursive_field_keys_;

    // Check if a type contains direct or indirect self-reference to class_name
    [[nodiscard]] bool is_type_recursive(const std::string& class_name, const typechecker::TypePtr& type) const;

    // Check if there is a path from from_class to target_class in class dependency graph
    [[nodiscard]] bool has_path_to(const std::string& from, const std::string& target, std::unordered_set<std::string>& visited) const;
};

} // namespace keety::ir
