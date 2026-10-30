#include <keety/emitter/cpp_emitter.hpp>

#include <iomanip>
#include <sstream>
#include <type_traits>

namespace keety::emitter {

CppEmitter::CppEmitter(CppEmitterOptions options)
    : options_(std::move(options)) {}

CppEmitter::CppEmitter(const parser::ModuleAST& ast, CppEmitterOptions options)
    : options_(std::move(options)), ast_(&ast) {
    if (!ast.moduleName.empty()) {
        module_name_ = ast.moduleName;
    }
    collect_declarations(ast);
}

CppEmitter::CppEmitter(const ir::Module& ir_module, CppEmitterOptions options)
    : options_(std::move(options)), ir_module_(&ir_module) {
    if (!ir_module.get_name().empty()) {
        module_name_ = ir_module.get_name();
    }
    collect_ir_declarations(ir_module);
}

CppEmitter::CppEmitter(const parser::ModuleAST& ast, const ir::Module& ir_module, CppEmitterOptions options)
    : options_(std::move(options)), ast_(&ast), ir_module_(&ir_module) {
    if (!ast.moduleName.empty()) {
        module_name_ = ast.moduleName;
    } else if (!ir_module.get_name().empty()) {
        module_name_ = ir_module.get_name();
    }
    collect_declarations(ast);
    collect_ir_declarations(ir_module);
}

void CppEmitter::add_standard_header(std::string header) {
    for (const auto& existing : options_.standard_headers) {
        if (existing == header) return;
    }
    options_.standard_headers.push_back(std::move(header));
}

std::string CppEmitter::get_header_include() const {
    if (!options_.header_include.empty()) {
        return options_.header_include;
    }
    return "\"" + module_name_ + ".hpp\"";
}

std::string CppEmitter::make_module_namespace(std::string_view module_name) {
    return HeaderEmitter::make_module_namespace(module_name);
}

std::string CppEmitter::make_internal_var_name(std::string_view var_name) {
    if (var_name.empty()) {
        return "_internal_";
    }
    if (var_name.rfind("_internal_", 0) == 0) {
        return std::string(var_name);
    }
    return "_internal_" + std::string(var_name);
}

std::string CppEmitter::get_namespace_name() const {
    if (!options_.custom_namespace.empty()) {
        return options_.custom_namespace;
    }
    return make_module_namespace(module_name_);
}

void CppEmitter::add_storage_allocation(std::string name, std::string cpp_type, std::string initial_value) {
    std::string internal_name = make_internal_var_name(name);
    std::string entry;
    if (initial_value.empty() || initial_value == "{}") {
        entry = cpp_type + " " + internal_name + " = {};";
    } else {
        entry = cpp_type + " " + internal_name + " = " + initial_value + ";";
    }
    for (const auto& existing : storage_allocations_) {
        if (existing == entry) return;
    }
    storage_allocations_.push_back(std::move(entry));
}

void CppEmitter::collect_declarations(const parser::ModuleAST& ast) {
    collect_declarations(ast.declarations);
}

void CppEmitter::collect_declarations(const std::vector<std::unique_ptr<ast::Decl>>& decls) {
    for (const auto& decl : decls) {
        if (!decl) continue;

        if (const auto* var = dynamic_cast<const ast::VarDecl*>(decl.get())) {
            std::string cpp_type = var->getType() ? HeaderEmitter::to_cpp_type(var->getType()) : "auto";
            std::string init_val = var->getInitializer() ? HeaderEmitter::to_cpp_expr(var->getInitializer()) : "{}";
            add_storage_allocation(var->getName(), cpp_type, init_val);
        } else if (const auto* cst = dynamic_cast<const ast::ConstDecl*>(decl.get())) {
            std::string cpp_type = cst->getType() ? HeaderEmitter::to_cpp_type(cst->getType()) : "auto";
            std::string init_val = cst->getInitializer() ? HeaderEmitter::to_cpp_expr(cst->getInitializer()) : "{}";
            add_storage_allocation(cst->getName(), "const " + cpp_type, init_val);
        }
    }
}

void CppEmitter::collect_ir_declarations(const ir::Module& ir_module) {
    for (const auto& global : ir_module.get_globals()) {
        std::string cpp_type = HeaderEmitter::to_cpp_type(global.type);
        if (global.is_const) {
            cpp_type = "const " + cpp_type;
        }
        std::string init_val = "{}";
        if (global.initial_value.has_value()) {
            init_val = to_cpp_operand(*global.initial_value);
        }
        add_storage_allocation(global.name, cpp_type, init_val);
    }
}

std::string CppEmitter::to_cpp_operand(const ir::Operand& operand) {
    if (operand.is_constant()) {
        return std::visit([](const auto& val) -> std::string {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, double>) {
                std::ostringstream ss;
                ss << val;
                std::string s = ss.str();
                if (s.find('.') == std::string::npos && s.find('e') == std::string::npos && s.find('E') == std::string::npos) {
                    s += ".0";
                }
                return s;
            } else if constexpr (std::is_same_v<T, bool>) {
                return val ? "true" : "false";
            } else if constexpr (std::is_same_v<T, char>) {
                switch (val) {
                case '\'': return "'\\''";
                case '\\': return "'\\\\'";
                case '\n': return "'\\n'";
                case '\t': return "'\\t'";
                case '\r': return "'\\r'";
                default:   return std::string("'") + val + "'";
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                std::string escaped;
                for (char c : val) {
                    switch (c) {
                    case '"':  escaped += "\\\""; break;
                    case '\\': escaped += "\\\\"; break;
                    case '\n': escaped += "\\n"; break;
                    case '\t': escaped += "\\t"; break;
                    case '\r': escaped += "\\r"; break;
                    default:   escaped += c; break;
                    }
                }
                return "\"" + escaped + "\"";
            } else {
                return "nullptr";
            }
        }, operand.get_constant());
    }

    if (operand.is_global()) {
        return make_internal_var_name(operand.get_name());
    }

    return operand.get_name();
}

std::string CppEmitter::emit_preamble() const {
    std::ostringstream ss;
    if (options_.emit_banner && !options_.banner_text.empty()) {
        ss << options_.banner_text << "\n";
    }

    std::string header_inc = get_header_include();
    if (!header_inc.empty()) {
        if (header_inc.front() == '<' || header_inc.front() == '"') {
            ss << "#include " << header_inc << "\n";
        } else {
            ss << "#include \"" << header_inc << "\"\n";
        }
    }

    for (const auto& sh : options_.standard_headers) {
        if (!sh.empty()) {
            if (sh.front() == '<' || sh.front() == '"') {
                ss << "#include " << sh << "\n";
            } else {
                ss << "#include <" << sh << ">\n";
            }
        }
    }

    return ss.str();
}

std::string CppEmitter::emit_namespace_begin() const {
    if (!options_.emit_namespace) {
        return "";
    }
    return "namespace " + get_namespace_name() + " {\n";
}

std::string CppEmitter::emit_namespace_end() const {
    if (!options_.emit_namespace) {
        return "";
    }
    return "} // namespace " + get_namespace_name() + "\n";
}

std::string CppEmitter::emit_storage_allocations() const {
    if (storage_allocations_.empty()) {
        return "";
    }

    std::ostringstream ss;
    if (storage_allocations_.size() == 1) {
        ss << "    // Storage definition for module-level variable\n";
    } else {
        ss << "    // Storage definitions for module-level variables\n";
    }
    for (const auto& alloc : storage_allocations_) {
        ss << "    " << alloc << "\n";
    }
    return ss.str();
}

std::string CppEmitter::emit_main_wrapper() const {
    // Stub for root entry point (Task 5.2.3)
    return "";
}

std::string CppEmitter::emit_namespace_body() const {
    std::ostringstream ss;
    std::string allocs = emit_storage_allocations();
    if (!allocs.empty()) {
        ss << allocs;
    }
    return ss.str();
}

std::string CppEmitter::emit() const {
    std::ostringstream ss;
    std::string preamble = emit_preamble();
    ss << preamble;

    if (options_.emit_namespace) {
        if (!preamble.empty()) {
            ss << "\n";
        }
        ss << emit_namespace_begin();
        std::string body = emit_namespace_body();
        if (!body.empty()) {
            ss << body;
        }
        ss << emit_namespace_end();
    } else {
        std::string body = emit_namespace_body();
        if (!body.empty()) {
            if (!preamble.empty()) {
                ss << "\n";
            }
            ss << body;
        }
    }

    if (options_.is_root_module) {
        std::string main_wrap = emit_main_wrapper();
        if (!main_wrap.empty()) {
            ss << "\n" << main_wrap;
        }
    }

    return ss.str();
}

void CppEmitter::emit(std::ostream& os) const {
    os << emit();
}

} // namespace keety::emitter
