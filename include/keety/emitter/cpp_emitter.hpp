#pragma once

#include <keety/ast/decl.hpp>
#include <keety/emitter/header_emitter.hpp>
#include <keety/ir/module.hpp>
#include <keety/ir/operand.hpp>
#include <keety/parser/parser.hpp>

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace keety::emitter {

/// Configuration options for C++ implementation emission (<module>.cpp)
struct CppEmitterOptions {
    bool emit_banner{true};
    std::string banner_text{"// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT."};
    std::string header_include{""};             ///< Path to corresponding header, defaults to "<module>.hpp"
    bool emit_namespace{true};
    std::string custom_namespace{""};
    std::vector<std::string> standard_headers = {
        "<iostream>"
    };
    bool is_root_module{false};                 ///< Emits main() process wrapper if true (Task 5.2.3)
};

/// Emits standard-compliant C++17 module implementation files (<module>.cpp).
class CppEmitter {
public:
    explicit CppEmitter(CppEmitterOptions options = {});
    explicit CppEmitter(const parser::ModuleAST& ast, CppEmitterOptions options = {});
    explicit CppEmitter(const ir::Module& ir_module, CppEmitterOptions options = {});
    CppEmitter(const parser::ModuleAST& ast, const ir::Module& ir_module, CppEmitterOptions options = {});

    // Module name accessors
    [[nodiscard]] const std::string& get_module_name() const noexcept { return module_name_; }
    void set_module_name(std::string name) { module_name_ = std::move(name); }

    [[nodiscard]] const parser::ModuleAST* get_ast() const noexcept { return ast_; }
    [[nodiscard]] const ir::Module* get_ir_module() const noexcept { return ir_module_; }

    // Configuration accessors
    [[nodiscard]] const CppEmitterOptions& get_options() const noexcept { return options_; }
    CppEmitterOptions& get_options() noexcept { return options_; }

    // Header include management
    [[nodiscard]] std::string get_header_include() const;
    void set_header_include(std::string header) { options_.header_include = std::move(header); }

    void add_standard_header(std::string header);
    [[nodiscard]] const std::vector<std::string>& get_standard_headers() const noexcept { return options_.standard_headers; }

    // Namespace helpers
    [[nodiscard]] std::string get_namespace_name() const;
    [[nodiscard]] static std::string make_module_namespace(std::string_view module_name);
    [[nodiscard]] static std::string make_internal_var_name(std::string_view var_name);

    // Storage allocation management
    void add_storage_allocation(std::string name, std::string cpp_type, std::string initial_value = "{}");
    [[nodiscard]] const std::vector<std::string>& get_storage_allocations() const noexcept { return storage_allocations_; }
    void clear_storage_allocations() noexcept { storage_allocations_.clear(); }

    // AST / IR declaration ingestion
    void collect_declarations(const parser::ModuleAST& ast);
    void collect_declarations(const std::vector<std::unique_ptr<ast::Decl>>& decls);
    void collect_ir_declarations(const ir::Module& ir_module);

    // Emission sections
    [[nodiscard]] std::string emit_preamble() const;
    [[nodiscard]] std::string emit_namespace_begin() const;
    [[nodiscard]] std::string emit_storage_allocations() const;
    [[nodiscard]] std::string emit_namespace_body() const;
    [[nodiscard]] std::string emit_namespace_end() const;
    [[nodiscard]] std::string emit_main_wrapper() const;

    [[nodiscard]] std::string emit() const;
    void emit(std::ostream& os) const;

    // KeetIR Operand to C++ formatting
    [[nodiscard]] static std::string to_cpp_operand(const ir::Operand& operand);

private:
    std::string module_name_{"main"};
    CppEmitterOptions options_;
    const parser::ModuleAST* ast_{nullptr};
    const ir::Module* ir_module_{nullptr};

    std::vector<std::string> storage_allocations_;
};

} // namespace keety::emitter
