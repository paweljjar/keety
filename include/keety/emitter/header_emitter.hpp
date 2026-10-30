#pragma once

#include <keety/ast/decl.hpp>
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

    // Full header generation
    [[nodiscard]] std::string emit() const;
    void emit(std::ostream& os) const;

private:
    std::string module_name_{"main"};
    HeaderEmitterOptions options_;
    const parser::ModuleAST* ast_{nullptr};
    const ir::Module* ir_module_{nullptr};
};

} // namespace keety::emitter
