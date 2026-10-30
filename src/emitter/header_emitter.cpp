#include <keety/emitter/header_emitter.hpp>
#include <keety/resolver/module_path_resolver.hpp>

#include <algorithm>
#include <ostream>
#include <sstream>

namespace keety::emitter {

HeaderEmitter::HeaderEmitter(HeaderEmitterOptions options)
    : options_(std::move(options)) {}

HeaderEmitter::HeaderEmitter(const parser::ModuleAST& ast, HeaderEmitterOptions options)
    : options_(std::move(options)), ast_(&ast) {
    if (!ast.moduleName.empty()) {
        module_name_ = ast.moduleName;
    }
    collect_imports(ast);
}

HeaderEmitter::HeaderEmitter(const ir::Module& ir_module, HeaderEmitterOptions options)
    : options_(std::move(options)), ir_module_(&ir_module) {
    if (!ir_module.get_name().empty()) {
        module_name_ = ir_module.get_name();
    }
}

HeaderEmitter::HeaderEmitter(const parser::ModuleAST& ast, const ir::Module& ir_module, HeaderEmitterOptions options)
    : options_(std::move(options)), ast_(&ast), ir_module_(&ir_module) {
    if (!ast.moduleName.empty()) {
        module_name_ = ast.moduleName;
    } else if (!ir_module.get_name().empty()) {
        module_name_ = ir_module.get_name();
    }
    collect_imports(ast);
}

void HeaderEmitter::add_standard_header(std::string header) {
    if (header.empty()) {
        return;
    }
    if (header.front() != '<' || header.back() != '>') {
        header = "<" + header + ">";
    }
    for (const auto& existing : options_.standard_headers) {
        if (existing == header) {
            return;
        }
    }
    options_.standard_headers.push_back(std::move(header));
}

void HeaderEmitter::add_imported_header(std::string header) {
    if (header.empty()) {
        return;
    }
    std::string formatted = resolve_header_include_path(header);
    for (const auto& existing : options_.imported_headers) {
        if (existing == formatted) {
            return;
        }
    }
    options_.imported_headers.push_back(std::move(formatted));
    if (options_.sort_imported_headers) {
        std::sort(options_.imported_headers.begin(), options_.imported_headers.end());
    }
}

void HeaderEmitter::collect_imports(const parser::ModuleAST& ast) {
    collect_imports(ast.declarations);
}

void HeaderEmitter::collect_imports(const std::vector<std::unique_ptr<ast::Decl>>& decls) {
    for (const auto& decl : decls) {
        if (const auto* import_decl = dynamic_cast<const ast::ImportDecl*>(decl.get())) {
            add_imported_header(import_decl->getModulePath());
        }
    }
}

std::string HeaderEmitter::resolve_header_include_path(std::string_view import_path) {
    if (import_path.empty()) {
        return "";
    }

    if ((import_path.front() == '"' && import_path.back() == '"') ||
        (import_path.front() == '<' && import_path.back() == '>')) {
        return std::string(import_path);
    }

    if (resolver::ModulePathResolver::isStandardLibraryModule(import_path)) {
        return "<keet/std/" + std::string(import_path) + ".hpp>";
    }

    std::string path_str(import_path);
    if (path_str.ends_with(".h") || path_str.ends_with(".hpp") ||
        path_str.ends_with(".hxx") || path_str.ends_with(".hh")) {
        return "\"" + path_str + "\"";
    }

    if (path_str.ends_with(".keet")) {
        path_str = path_str.substr(0, path_str.size() - 5) + ".hpp";
        return "\"" + path_str + "\"";
    }

    path_str += ".hpp";
    return "\"" + path_str + "\"";
}

std::string HeaderEmitter::emit_banner() const {
    if (!options_.emit_banner) {
        return "";
    }
    return "// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.\n";
}

std::string HeaderEmitter::emit_header_guard() const {
    if (!options_.emit_pragma_once) {
        return "";
    }
    return "#pragma once\n";
}

std::string HeaderEmitter::emit_standard_includes() const {
    std::ostringstream ss;
    for (const auto& header : options_.standard_headers) {
        ss << "#include " << header << "\n";
    }
    if (options_.emit_runtime_header && !options_.runtime_header_path.empty()) {
        if (!options_.standard_headers.empty()) {
            ss << "\n";
        }
        ss << "#include " << options_.runtime_header_path << "\n";
    }
    return ss.str();
}

std::string HeaderEmitter::emit_imported_includes() const {
    if (options_.imported_headers.empty()) {
        return "";
    }
    std::ostringstream ss;
    for (const auto& header : options_.imported_headers) {
        ss << "#include " << header << "\n";
    }
    return ss.str();
}

std::string HeaderEmitter::emit_preamble() const {
    std::ostringstream ss;

    std::string banner = emit_banner();
    if (!banner.empty()) {
        ss << banner;
    }

    std::string guard = emit_header_guard();
    if (!guard.empty()) {
        ss << guard;
    }

    std::string std_inc = emit_standard_includes();
    if (!std_inc.empty()) {
        if (!banner.empty() || !guard.empty()) {
            ss << "\n";
        }
        ss << std_inc;
    }

    std::string imp_inc = emit_imported_includes();
    if (!imp_inc.empty()) {
        if (!banner.empty() || !guard.empty() || !std_inc.empty()) {
            ss << "\n";
        }
        ss << imp_inc;
    }

    return ss.str();
}

std::string HeaderEmitter::emit() const {
    return emit_preamble();
}

void HeaderEmitter::emit(std::ostream& os) const {
    os << emit();
}

} // namespace keety::emitter
