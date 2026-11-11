#pragma once

#include <keety/ast/decl.hpp>
#include <keety/ast/type.hpp>
#include <keety/emitter/header_emitter.hpp>
#include <keety/emitter/line_directive_tracker.hpp>
#include <keety/ir/module.hpp>
#include <keety/ir/operand.hpp>
#include <keety/parser/parser.hpp>

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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
    bool is_root_module{false};                 ///< Emits main() process entry wrapper if true
    bool emit_line_directives{true};            ///< Emits #line directives for source-to-source mapping
    const diag::SourceManager* source_manager{nullptr}; ///< SourceManager to resolve FileId to paths
    std::string source_file_path{""};           ///< Fallback or override source file path
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
    [[nodiscard]] bool emits_line_directives() const noexcept { return options_.emit_line_directives; }
    void set_emit_line_directives(bool emit) noexcept { options_.emit_line_directives = emit; }
    void set_source_manager(const diag::SourceManager* sm) noexcept { options_.source_manager = sm; }
    void set_source_file_path(std::string path) { options_.source_file_path = std::move(path); }
    [[nodiscard]] const diag::SourceMap& get_source_map() const noexcept { return line_tracker_.get_source_map(); }

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
    void add_storage_allocation(std::string name, std::string cpp_type, std::string initial_value = "{}", std::string line_directive = "");
    [[nodiscard]] const std::vector<std::string>& get_storage_allocations() const noexcept { return storage_allocations_; }
    void clear_storage_allocations() noexcept { storage_allocations_.clear(); }

    [[nodiscard]] std::string format_line(const diag::SourceSpan& span) const;

    // Function definition management
    void add_function_definition(std::string function_def);
    [[nodiscard]] const std::vector<std::string>& get_function_definitions() const noexcept { return function_definitions_; }
    void clear_function_definitions() noexcept { function_definitions_.clear(); }

    // AST / IR declaration ingestion
    void collect_declarations(const parser::ModuleAST& ast);
    void collect_declarations(const std::vector<std::unique_ptr<ast::Decl>>& decls);
    void collect_ir_declarations(const ir::Module& ir_module);

    // Function translation
    [[nodiscard]] std::string translate_function(const ast::FnDecl& fn) const;
    [[nodiscard]] std::string translate_method(std::string_view class_name, const ast::FnDecl& fn) const;
    [[nodiscard]] std::string translate_statement(const ast::Stmt* stmt, int indent = 1) const;
    [[nodiscard]] std::string translate_expression(const ast::Expr* expr) const;
    [[nodiscard]] std::string translate_block(const ast::BlockStmt* block, int indent = 1) const;
    [[nodiscard]] std::string translate_ir_function(const ir::Function& fn) const;

    [[nodiscard]] bool is_module_var(const std::string& name) const;
    void register_module_var(std::string name);

    // Emission sections
    [[nodiscard]] std::string emit_preamble() const;
    [[nodiscard]] std::string emit_namespace_begin() const;
    [[nodiscard]] std::string emit_storage_allocations() const;
    [[nodiscard]] std::string emit_function_definitions() const;
    [[nodiscard]] std::string emit_namespace_body() const;
    [[nodiscard]] std::string emit_namespace_end() const;
    [[nodiscard]] std::string emit_main_wrapper() const;

    [[nodiscard]] bool is_root_module() const noexcept;
    void set_root_module(bool is_root) noexcept {
        options_.is_root_module = is_root;
        manual_root_override_ = true;
    }
    [[nodiscard]] bool has_main_function() const noexcept { return has_main_function_; }

    [[nodiscard]] std::string emit() const;
    void emit(std::ostream& os) const;

    // KeetIR Operand to C++ formatting
    [[nodiscard]] std::string to_cpp_ir_operand(const ir::Operand& operand) const;
    [[nodiscard]] static std::string to_cpp_operand(const ir::Operand& operand);
    // Flow-based narrowing facts
    struct NarrowingFact {
        std::string var_name;
        std::string cpp_type;
    };

    struct BranchNarrowing {
        std::vector<NarrowingFact> then_facts;
        std::vector<NarrowingFact> else_facts;
    };

    [[nodiscard]] BranchNarrowing analyze_narrowing_condition(const ast::Expr* condition) const;
    [[nodiscard]] std::string get_non_null_cpp_type(const std::string& var_name) const;
    void collect_local_var_types(const ast::Stmt* stmt) const;

private:
    std::string module_name_{"main"};
    CppEmitterOptions options_;
    const parser::ModuleAST* ast_{nullptr};
    const ir::Module* ir_module_{nullptr};

    std::vector<std::string> storage_allocations_;
    std::vector<std::string> function_definitions_;
    std::vector<std::string> module_vars_;

    bool has_main_function_{false};
    bool main_has_args_{false};
    bool main_returns_int_{false};
    bool manual_root_override_{false};
    mutable bool in_zonk_expr_{false};
    mutable std::unordered_map<std::string, std::string> narrowed_vars_{};
    mutable std::unordered_map<std::string, const ast::TypeAnnotation*> var_types_{};
    std::unordered_map<std::string, const ast::TypeAnnotation*> global_var_types_{};
    bool has_io_import_{false};
    std::unordered_set<std::string> imported_io_symbols_{};
    bool has_fs_import_{false};
    std::unordered_set<std::string> imported_fs_symbols_{};
    bool has_sys_import_{false};
    std::unordered_set<std::string> imported_sys_symbols_{};
    bool has_math_import_{false};
    std::unordered_set<std::string> imported_math_symbols_{};
    bool has_time_import_{false};
    std::unordered_set<std::string> imported_time_symbols_{};
    std::unordered_set<std::string> imported_time_constants_{};
    bool has_json_import_{false};
    std::unordered_set<std::string> json_aliases_{};
    std::unordered_set<std::string> imported_json_symbols_{};
    bool has_net_import_{false};
    std::unordered_set<std::string> net_aliases_{};
    std::unordered_set<std::string> imported_net_symbols_{};
    bool has_test_import_{false};
    std::unordered_set<std::string> test_aliases_{};
    std::unordered_set<std::string> imported_test_symbols_{};
    bool has_crypto_import_{false};
    std::unordered_set<std::string> crypto_aliases_{};
    std::unordered_set<std::string> imported_crypto_symbols_{};
    bool has_http_import_{false};
    std::unordered_set<std::string> http_aliases_{};
    std::unordered_set<std::string> imported_http_symbols_{};
    bool has_ui_import_{false};
    std::unordered_set<std::string> ui_aliases_{};
    std::unordered_set<std::string> imported_ui_symbols_{};
    std::vector<std::string> test_function_names_{};
    std::unordered_set<std::string> user_imported_modules_{};
    mutable LineDirectiveTracker line_tracker_;
};

} // namespace keety::emitter
