#pragma once

#include <keety/ir/function.hpp>
#include <keety/ir/operand.hpp>
#include <keety/typechecker/type.hpp>

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace keety::ir {

/// Represents a module-level global variable
struct GlobalVariable {
    std::string name;
    typechecker::TypePtr type;
    bool is_const{false};
    bool is_export{false};
    std::optional<Operand> initial_value{std::nullopt};

    [[nodiscard]] std::string to_string() const;
};

/// Represents an entire compiled compilation unit / module in KeetIR
class Module {
public:
    explicit Module(std::string name) : name_(std::move(name)) {}

    [[nodiscard]] const std::string& get_name() const noexcept { return name_; }
    void set_name(std::string name) noexcept { name_ = std::move(name); }

    [[nodiscard]] const std::vector<GlobalVariable>& get_globals() const noexcept { return globals_; }
    [[nodiscard]] std::vector<GlobalVariable>& get_globals() noexcept { return globals_; }
    void add_global(GlobalVariable global) { globals_.push_back(std::move(global)); }

    [[nodiscard]] const GlobalVariable* find_global(const std::string& name) const noexcept;

    [[nodiscard]] const std::vector<std::unique_ptr<Function>>& get_functions() const noexcept { return functions_; }
    [[nodiscard]] std::vector<std::unique_ptr<Function>>& get_functions() noexcept { return functions_; }

    Function* add_function(std::unique_ptr<Function> fn) {
        if (fn) {
            functions_.push_back(std::move(fn));
            return functions_.back().get();
        }
        return nullptr;
    }

    [[nodiscard]] Function* find_function(const std::string& name) const noexcept;

    /// Recomputes CFG edges across all functions in the module
    void recompute_cfg();

    /// Validates CFG integrity across all functions
    [[nodiscard]] bool validate(std::string& error_message) const;

    [[nodiscard]] std::string to_string() const;
    void dump(std::ostream& os) const;

private:
    std::string name_;
    std::vector<GlobalVariable> globals_;
    std::vector<std::unique_ptr<Function>> functions_;
};

} // namespace keety::ir
