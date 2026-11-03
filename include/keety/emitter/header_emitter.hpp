#pragma once

#include <keety/ast/decl.hpp>
#include <keety/emitter/line_directive_tracker.hpp>
#include <keety/ir/module.hpp>
#include <keety/parser/parser.hpp>

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace keety::emitter {

/// Configuration options for C++ header emission
struct HeaderEmitterOptions {
    bool emit_banner{true};
    bool emit_pragma_once{true};
    bool emit_runtime_header{true};
    bool sort_imported_headers{false};
    bool emit_namespace{true};
    std::string custom_namespace{""};
    std::string runtime_header_path{"<keet/runtime.hpp>"};
    std::vector<std::string> standard_headers = {
        "<algorithm>",
        "<cmath>",
        "<cstdint>",
        "<exception>",
        "<functional>",
        "<memory>",
        "<string>",
        "<unordered_map>",
        "<utility>",
        "<variant>",
        "<vector>"
    };
    std::vector<std::string> imported_headers{};
    bool emit_line_directives{true};            ///< Emits #line directives for source-to-source mapping
    const diag::SourceManager* source_manager{nullptr}; ///< SourceManager to resolve FileId to paths
    std::string source_file_path{""};           ///< Fallback or override source file path
};

/// Emits deterministic, standard-compliant C++17 module header files (<module>.hpp).
class HeaderEmitter {
public:
    explicit HeaderEmitter(HeaderEmitterOptions options = {});
    explicit HeaderEmitter(const parser::ModuleAST& ast, HeaderEmitterOptions options = {});
    explicit HeaderEmitter(const ir::Module& ir_module, HeaderEmitterOptions options = {});
    HeaderEmitter(const parser::ModuleAST& ast, const ir::Module& ir_module, HeaderEmitterOptions options = {});

    // Module name accessors
    [[nodiscard]] const std::string& get_module_name() const noexcept { return module_name_; }
    void set_module_name(std::string name) { module_name_ = std::move(name); }

    [[nodiscard]] const parser::ModuleAST* get_ast() const noexcept { return ast_; }
    [[nodiscard]] const ir::Module* get_ir_module() const noexcept { return ir_module_; }

    // Configuration accessors
    [[nodiscard]] const HeaderEmitterOptions& get_options() const noexcept { return options_; }
    HeaderEmitterOptions& get_options() noexcept { return options_; }
    [[nodiscard]] bool emits_line_directives() const noexcept { return options_.emit_line_directives; }
    void set_emit_line_directives(bool emit) noexcept { options_.emit_line_directives = emit; }
    void set_source_manager(const diag::SourceManager* sm) noexcept { options_.source_manager = sm; }
    void set_source_file_path(std::string path) { options_.source_file_path = std::move(path); }

    // Header management
    void add_standard_header(std::string header);
    void add_imported_header(std::string header);
    [[nodiscard]] const std::vector<std::string>& get_imported_headers() const noexcept {
        return options_.imported_headers;
    }

    // AST imports ingestion
    void collect_imports(const parser::ModuleAST& ast);
    void collect_imports(const std::vector<std::unique_ptr<ast::Decl>>& decls);

    // Path resolution helper for module imports
    [[nodiscard]] static std::string resolve_header_include_path(std::string_view import_path);

    // Section emission
    [[nodiscard]] std::string emit_banner() const;
    [[nodiscard]] std::string emit_header_guard() const;
    [[nodiscard]] std::string emit_standard_includes() const;
    [[nodiscard]] std::string emit_imported_includes() const;
    [[nodiscard]] std::string emit_preamble() const;

    // Namespace encapsulation
    [[nodiscard]] static std::string sanitize_identifier(std::string_view name);
    [[nodiscard]] static std::string make_module_namespace(std::string_view module_name);
    [[nodiscard]] std::string get_namespace_name() const;
    [[nodiscard]] std::string emit_namespace_begin() const;
    [[nodiscard]] std::string emit_namespace_end() const;
    [[nodiscard]] std::string emit_namespace_body() const;

    // Type and Expression mapping
    [[nodiscard]] static std::string to_cpp_type(const ast::TypeAnnotation* type, const std::string& current_module = "");
    [[nodiscard]] static std::string to_cpp_type(const typechecker::TypePtr& type, const std::string& current_module = "");
    [[nodiscard]] static std::string to_cpp_expr(const ast::Expr* expr);
    [[nodiscard]] static std::string format_param(const ast::Param& param, const std::vector<std::string>& type_params = {});
    [[nodiscard]] static std::string format_function_prototype(const ast::FnDecl& fn);
    [[nodiscard]] static std::string format_function_template(const ast::FnDecl& fn);
    [[nodiscard]] static std::string format_function_prototype(const ir::Function& fn);

    // Declaration emission sections
    [[nodiscard]] std::string emit_forward_declarations() const;
    [[nodiscard]] std::string emit_constexpr_definitions() const;
    [[nodiscard]] std::string emit_enum_definitions() const;
    [[nodiscard]] std::string emit_class_definitions() const;
    [[nodiscard]] std::string emit_errtype_definitions() const;
    [[nodiscard]] std::string emit_const_views() const;
    [[nodiscard]] std::string emit_function_templates() const;
    [[nodiscard]] std::string emit_function_prototypes() const;

    // AST / IR declaration ingestion
    void collect_declarations(const parser::ModuleAST& ast);
    void collect_declarations(const std::vector<std::unique_ptr<ast::Decl>>& decls);
    void collect_ir_declarations(const ir::Module& ir_module);

    // Programmatic declaration addition
    void add_forward_decl(std::string decl);
    void add_constexpr_def(std::string def);
    void add_enum_def(std::string def);
    void add_class_def(std::string def);
    void add_errtype_def(std::string def);
    void add_const_view(std::string name, std::string cpp_type);
    void add_function_template(std::string template_def);
    void add_function_prototype(std::string prototype);

    // Accessors for emitted declarations
    [[nodiscard]] const std::vector<std::string>& get_forward_decls() const noexcept { return forward_decls_; }
    [[nodiscard]] const std::vector<std::string>& get_constexpr_defs() const noexcept { return constexpr_defs_; }
    [[nodiscard]] const std::vector<std::string>& get_enum_defs() const noexcept { return enum_defs_; }
    [[nodiscard]] const std::vector<std::string>& get_class_defs() const noexcept { return class_defs_; }
    [[nodiscard]] const std::vector<std::string>& get_errtype_defs() const noexcept { return errtype_defs_; }
    [[nodiscard]] const std::vector<std::string>& get_const_views() const noexcept { return const_views_; }
    [[nodiscard]] const std::vector<std::string>& get_function_templates() const noexcept { return function_templates_; }
    [[nodiscard]] const std::vector<std::string>& get_function_prototypes() const noexcept { return function_prototypes_; }

    [[nodiscard]] std::string format_line(const diag::SourceSpan& span) const;

    // Full header generation
    [[nodiscard]] std::string emit() const;
    void emit(std::ostream& os) const;

private:
    [[nodiscard]] static std::string generate_enum_def(const ast::EnumDecl& enm);
    [[nodiscard]] static std::string generate_class_def(const ast::ClassDecl& cls);
    [[nodiscard]] static std::string generate_errtype_def(const ast::ErrTypeDecl& err);

    std::string module_name_{"main"};
    HeaderEmitterOptions options_;
    const parser::ModuleAST* ast_{nullptr};
    const ir::Module* ir_module_{nullptr};

    std::vector<std::string> forward_decls_{};
    std::vector<std::string> constexpr_defs_{};
    std::vector<std::string> enum_defs_{};
    std::vector<std::string> class_defs_{};
    std::vector<std::string> errtype_defs_{};
    std::vector<std::string> const_views_{};
    std::vector<std::string> function_templates_{};
    std::vector<std::string> function_prototypes_{};
    mutable LineDirectiveTracker line_tracker_;
};

} // namespace keety::emitter
