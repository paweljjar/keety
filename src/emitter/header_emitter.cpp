#include <keety/ast/expr.hpp>
#include <keety/ast/type.hpp>
#include <keety/emitter/header_emitter.hpp>
#include <keety/resolver/module_path_resolver.hpp>
#include <keety/typechecker/type.hpp>

#include <algorithm>
#include <cctype>
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
    collect_declarations(ast);
}

HeaderEmitter::HeaderEmitter(const ir::Module& ir_module, HeaderEmitterOptions options)
    : options_(std::move(options)), ir_module_(&ir_module) {
    if (!ir_module.get_name().empty()) {
        module_name_ = ir_module.get_name();
    }
    collect_ir_declarations(ir_module);
}

HeaderEmitter::HeaderEmitter(const parser::ModuleAST& ast, const ir::Module& ir_module, HeaderEmitterOptions options)
    : options_(std::move(options)), ast_(&ast), ir_module_(&ir_module) {
    if (!ast.moduleName.empty()) {
        module_name_ = ast.moduleName;
    } else if (!ir_module.get_name().empty()) {
        module_name_ = ir_module.get_name();
    }
    collect_imports(ast);
    if (!ast.declarations.empty()) {
        collect_declarations(ast);
        for (const auto& global : ir_module.get_globals()) {
            if (global.is_export) {
                add_const_view(global.name, to_cpp_type(global.type));
            }
        }
    } else {
        collect_ir_declarations(ir_module);
    }
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

std::string HeaderEmitter::sanitize_identifier(std::string_view name) {
    if (name.empty()) {
        return "main";
    }

    std::string sanitized;
    sanitized.reserve(name.size());

    if (std::isdigit(static_cast<unsigned char>(name.front()))) {
        sanitized.push_back('_');
    }

    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            sanitized.push_back(c);
        } else {
            sanitized.push_back('_');
        }
    }

    if (sanitized.empty()) {
        return "main";
    }
    return sanitized;
}

std::string HeaderEmitter::make_module_namespace(std::string_view module_name) {
    if (module_name.empty()) {
        return "keet_module_main";
    }

    std::string sanitized = sanitize_identifier(module_name);
    if (sanitized.rfind("keet_module_", 0) == 0) {
        return sanitized;
    }
    return "keet_module_" + sanitized;
}

std::string HeaderEmitter::get_namespace_name() const {
    if (!options_.custom_namespace.empty()) {
        return options_.custom_namespace;
    }
    return make_module_namespace(module_name_);
}

std::string HeaderEmitter::emit_namespace_begin() const {
    if (!options_.emit_namespace) {
        return "";
    }
    return "namespace " + get_namespace_name() + " {\n";
}

std::string HeaderEmitter::emit_namespace_end() const {
    if (!options_.emit_namespace) {
        return "";
    }
    return "} // namespace " + get_namespace_name() + "\n";
}

std::string HeaderEmitter::to_cpp_type(const ast::TypeAnnotation* type, const std::string& current_module) {
    if (!type) {
        return "void";
    }

    if (const auto* prim = dynamic_cast<const ast::PrimitiveType*>(type)) {
        switch (prim->getKind()) {
        case ast::PrimitiveTypeKind::Bool:   return "bool";
        case ast::PrimitiveTypeKind::Int:    return "int64_t";
        case ast::PrimitiveTypeKind::Float:  return "double";
        case ast::PrimitiveTypeKind::Char:   return "char";
        case ast::PrimitiveTypeKind::String: return "std::string";
        case ast::PrimitiveTypeKind::Void:   return "void";
        case ast::PrimitiveTypeKind::Error:  return "keet::error";
        case ast::PrimitiveTypeKind::Null:   return "std::monostate";
        }
    }

    if (const auto* arr = dynamic_cast<const ast::ArrayType*>(type)) {
        return "std::vector<" + to_cpp_type(arr->getElementType(), current_module) + ">";
    }

    if (const auto* map = dynamic_cast<const ast::MapType*>(type)) {
        return "std::unordered_map<" + to_cpp_type(map->getKeyType(), current_module) + ", " +
               to_cpp_type(map->getValueType(), current_module) + ">";
    }

    if (const auto* un = dynamic_cast<const ast::UnionType*>(type)) {
        std::string res = "std::variant<";
        for (size_t i = 0; i < un->getVariants().size(); ++i) {
            if (i > 0) res += ", ";
            res += to_cpp_type(un->getVariants()[i].get(), current_module);
        }
        res += ">";
        return res;
    }

    if (const auto* fn = dynamic_cast<const ast::FunctionType*>(type)) {
        std::string ret = to_cpp_type(fn->getReturnType(), current_module);
        std::string params;
        for (size_t i = 0; i < fn->getParams().size(); ++i) {
            if (i > 0) params += ", ";
            const auto& p = fn->getParams()[i];
            std::string pt = to_cpp_type(p.type.get(), current_module);
            if (p.isMut) {
                params += pt + "&";
            } else {
                params += pt;
            }
        }
        return "std::function<" + ret + "(" + params + ")>";
    }

    if (const auto* cust = dynamic_cast<const ast::CustomType*>(type)) {
        std::string prefix;
        if (cust->isQualified()) {
            prefix = make_module_namespace(cust->getModuleQualifier()) + "::";
        }
        std::string name = cust->getName();
        if (cust->isGeneric()) {
            name += "<";
            for (size_t i = 0; i < cust->getTypeArgs().size(); ++i) {
                if (i > 0) name += ", ";
                name += to_cpp_type(cust->getTypeArgs()[i].get(), current_module);
            }
            name += ">";
        }
        return prefix + name;
    }

    if (const auto* lit = dynamic_cast<const ast::LiteralType*>(type)) {
        switch (lit->getLiteralKind()) {
        case ast::LiteralTypeKind::Int:    return "int64_t";
        case ast::LiteralTypeKind::Float:  return "double";
        case ast::LiteralTypeKind::Bool:   return "bool";
        case ast::LiteralTypeKind::Char:   return "char";
        case ast::LiteralTypeKind::String: return "std::string";
        }
    }

    return "auto";
}

std::string HeaderEmitter::to_cpp_type(const typechecker::TypePtr& type, const std::string& current_module) {
    if (!type) {
        return "void";
    }

    switch (type->getKind()) {
    case typechecker::TypeKind::Int:    return "int64_t";
    case typechecker::TypeKind::Float:  return "double";
    case typechecker::TypeKind::Bool:   return "bool";
    case typechecker::TypeKind::Char:   return "char";
    case typechecker::TypeKind::String: return "std::string";
    case typechecker::TypeKind::Void:   return "void";
    case typechecker::TypeKind::Null:   return "std::monostate";
    case typechecker::TypeKind::Error:  return "keet::error";
    case typechecker::TypeKind::Array: {
        const auto* arr = dynamic_cast<const typechecker::ArrayType*>(type.get());
        return "std::vector<" + to_cpp_type(arr ? arr->getElementType() : nullptr, current_module) + ">";
    }
    case typechecker::TypeKind::Map: {
        const auto* map = dynamic_cast<const typechecker::MapType*>(type.get());
        return "std::unordered_map<" + to_cpp_type(map ? map->getKeyType() : nullptr, current_module) + ", " +
               to_cpp_type(map ? map->getValueType() : nullptr, current_module) + ">";
    }
    case typechecker::TypeKind::Union: {
        const auto* un = dynamic_cast<const typechecker::UnionType*>(type.get());
        if (!un) return "std::variant<std::monostate>";
        std::string res = "std::variant<";
        for (size_t i = 0; i < un->getVariants().size(); ++i) {
            if (i > 0) res += ", ";
            res += to_cpp_type(un->getVariants()[i], current_module);
        }
        res += ">";
        return res;
    }
    case typechecker::TypeKind::Function: {
        const auto* fn = dynamic_cast<const typechecker::FunctionType*>(type.get());
        if (!fn) return "std::function<void()>";
        std::string ret = to_cpp_type(fn->getReturnType(), current_module);
        std::string params;
        for (size_t i = 0; i < fn->getParamTypes().size(); ++i) {
            if (i > 0) params += ", ";
            params += to_cpp_type(fn->getParamTypes()[i], current_module);
        }
        return "std::function<" + ret + "(" + params + ")>";
    }
    case typechecker::TypeKind::Custom: {
        const auto* cust = dynamic_cast<const typechecker::CustomType*>(type.get());
        return cust ? cust->getName() : "auto";
    }
    case typechecker::TypeKind::Literal:
        return to_cpp_type(type->getBaseType(), current_module);
    default:
        return "auto";
    }
}

std::string HeaderEmitter::to_cpp_expr(const ast::Expr* expr) {
    if (!expr) {
        return "";
    }

    if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(expr)) {
        if (lit->getRawText() == "null") {
            return "nullptr";
        }
        if (lit->getRawText() == "9223372036854775808") {
            return "(9223372036854775807LL + 1LL)";
        }
        if (lit->getRawText() == "9223372036854775807") {
            return "9223372036854775807LL";
        }
        return lit->getRawText();
    }

    if (const auto* id = dynamic_cast<const ast::IdentExpr*>(expr)) {
        return id->getName();
    }

    if (const auto* un = dynamic_cast<const ast::UnaryExpr*>(expr)) {
        if (un->getOp() == lexer::TokenKind::Minus) {
            if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(un->getOperand())) {
                if (lit->getRawText() == "9223372036854775808") {
                    return "(-9223372036854775807LL - 1LL)";
                }
            }
        }
        std::string_view op_str = lexer::getTokenKindName(un->getOp());
        return std::string(op_str) + to_cpp_expr(un->getOperand());
    }

    if (const auto* bin = dynamic_cast<const ast::BinaryExpr*>(expr)) {
        if (bin->getOp() == lexer::TokenKind::Slash) {
            return "keet::div_safe(" + to_cpp_expr(bin->getLeft()) + ", " + to_cpp_expr(bin->getRight()) + ")";
        }
        if (bin->getOp() == lexer::TokenKind::Percent) {
            return "keet::mod_safe(" + to_cpp_expr(bin->getLeft()) + ", " + to_cpp_expr(bin->getRight()) + ")";
        }
        std::string_view op_str = lexer::getTokenKindName(bin->getOp());
        return to_cpp_expr(bin->getLeft()) + " " + std::string(op_str) + " " + to_cpp_expr(bin->getRight());
    }

    if (const auto* mem = dynamic_cast<const ast::MemberExpr*>(expr)) {
        return to_cpp_expr(mem->getObject()) + "." + mem->getMemberName();
    }

    return "";
}

std::string HeaderEmitter::format_param(const ast::Param& param, const std::vector<std::string>& type_params) {
    std::string type_str = param.type ? to_cpp_type(param.type.get()) : "auto";
    if (param.isMut) {
        return type_str + "& " + param.name;
    }
    bool is_tparam = false;
    for (const auto& tp : type_params) {
        if (type_str == tp) {
            is_tparam = true;
            break;
        }
    }
    if (is_tparam ||
        type_str == "std::string" ||
        type_str == "keet::error" ||
        type_str.rfind("std::vector<", 0) == 0 ||
        type_str.rfind("std::unordered_map<", 0) == 0 ||
        type_str.rfind("std::variant<", 0) == 0) {
        return "const " + type_str + "& " + param.name;
    }
    return type_str + " " + param.name;
}

std::string HeaderEmitter::format_function_prototype(const ast::FnDecl& fn) {
    std::string ret_type = fn.getReturnType() ? to_cpp_type(fn.getReturnType()) : "void";
    std::string params_str;
    for (size_t i = 0; i < fn.getParams().size(); ++i) {
        if (i > 0) params_str += ", ";
        params_str += format_param(fn.getParams()[i], fn.getTypeParams());
    }
    return ret_type + " " + fn.getName() + "(" + params_str + ");";
}

std::string HeaderEmitter::format_function_template(const ast::FnDecl& fn) {
    std::string tparams = "template <";
    for (size_t i = 0; i < fn.getTypeParams().size(); ++i) {
        if (i > 0) tparams += ", ";
        tparams += "typename " + fn.getTypeParams()[i];
    }
    tparams += ">\n    ";
    return tparams + format_function_prototype(fn);
}

std::string HeaderEmitter::format_function_prototype(const ir::Function& fn) {
    std::string ret_type = to_cpp_type(fn.get_return_type());
    std::string params_str;
    for (size_t i = 0; i < fn.get_parameters().size(); ++i) {
        if (i > 0) params_str += ", ";
        const auto& p = fn.get_parameters()[i];
        std::string pt = to_cpp_type(p.type);
        if (p.is_mut) {
            params_str += pt + "& " + p.name;
        } else if (p.is_const_ref ||
                   pt == "std::string" ||
                   pt.rfind("std::vector<", 0) == 0 ||
                   pt.rfind("std::unordered_map<", 0) == 0 ||
                   pt.rfind("std::variant<", 0) == 0) {
            params_str += "const " + pt + "& " + p.name;
        } else {
            params_str += pt + " " + p.name;
        }
    }
    return ret_type + " " + fn.get_name() + "(" + params_str + ");";
}

std::string HeaderEmitter::generate_enum_def(const ast::EnumDecl& enm) {
    std::ostringstream ss;
    std::string underlying = enm.getUnderlyingType() ? to_cpp_type(enm.getUnderlyingType()) : "int64_t";
    ss << "    enum class " << enm.getName() << " : " << underlying << " {\n";
    for (size_t i = 0; i < enm.getMembers().size(); ++i) {
        const auto& member = enm.getMembers()[i];
        ss << "        " << member.name;
        if (member.value) {
            ss << " = " << to_cpp_expr(member.value.get());
        }
        if (i + 1 < enm.getMembers().size()) {
            ss << ",";
        }
        ss << "\n";
    }
    ss << "    };";
    return ss.str();
}

std::string HeaderEmitter::generate_class_def(const ast::ClassDecl& cls) {
    std::ostringstream ss;
    std::string base;
    if (cls.hasBaseClass()) {
        base = " : public " + cls.getBaseClass();
    }
    ss << "    struct " << cls.getName() << base << " {\n";

    for (const auto& field : cls.getFields()) {
        std::string fld_type = field.type ? to_cpp_type(field.type.get()) : "auto";
        if (field.initializer) {
            ss << "        " << fld_type << " " << field.name << " = " << to_cpp_expr(field.initializer.get()) << ";\n";
        } else {
            ss << "        " << fld_type << " " << field.name << "{};\n";
        }
    }

    if (!cls.getFields().empty()) {
        ss << "\n";
    }
    ss << "        " << cls.getName() << "() = default;\n";

    for (const auto& method : cls.getMethods()) {
        std::string ret_type = method->getReturnType() ? to_cpp_type(method->getReturnType()) : "void";
        std::string params_str;
        for (size_t i = 0; i < method->getParams().size(); ++i) {
            if (i > 0) params_str += ", ";
            params_str += format_param(method->getParams()[i]);
        }
        ss << "        " << ret_type << " " << method->getName() << "(" << params_str << ");\n";
    }

    ss << "    };";
    return ss.str();
}

std::string HeaderEmitter::generate_errtype_def(const ast::ErrTypeDecl& err) {
    std::ostringstream ss;
    std::string base = (err.getBaseError().empty() || err.getBaseError() == "error") ? "keet::error" : err.getBaseError();
    ss << "    struct " << err.getName() << " : public " << base << " {\n";

    for (const auto& field : err.getFields()) {
        std::string fld_type = field.type ? to_cpp_type(field.type.get()) : "auto";
        if (field.initializer) {
            ss << "        " << fld_type << " " << field.name << " = " << to_cpp_expr(field.initializer.get()) << ";\n";
        } else {
            ss << "        " << fld_type << " " << field.name << "{};\n";
        }
    }

    if (!err.getFields().empty()) {
        ss << "\n";
        std::string ctor_params;
        std::string ctor_inits;
        for (const auto& field : err.getFields()) {
            std::string fld_type = field.type ? to_cpp_type(field.type.get()) : "auto";
            if (!ctor_params.empty()) ctor_params += ", ";
            ctor_params += fld_type + " " + field.name + " = {}";
            ctor_inits += ", " + field.name + "(" + field.name + ")";
        }
        ctor_params += ", std::string msg = \"" + err.getName() + "\"";
        ss << "        " << err.getName() << "(" << ctor_params << ")\n";
        ss << "            : " << base << "(\"" << err.getName() << "\", std::move(msg))" << ctor_inits << " {}\n";
    } else {
        ss << "        explicit " << err.getName() << "(std::string msg = \"" << err.getName() << "\")\n";
        ss << "            : " << base << "(\"" << err.getName() << "\", std::move(msg)) {}\n";
    }

    ss << "    };";
    return ss.str();
}

void HeaderEmitter::add_forward_decl(std::string decl) {
    if (decl.empty()) return;
    for (const auto& existing : forward_decls_) {
        if (existing == decl) return;
    }
    forward_decls_.push_back(std::move(decl));
}

void HeaderEmitter::add_constexpr_def(std::string def) {
    if (def.empty()) return;
    for (const auto& existing : constexpr_defs_) {
        if (existing == def) return;
    }
    constexpr_defs_.push_back(std::move(def));
}

void HeaderEmitter::add_enum_def(std::string def) {
    if (def.empty()) return;
    for (const auto& existing : enum_defs_) {
        if (existing == def) return;
    }
    enum_defs_.push_back(std::move(def));
}

void HeaderEmitter::add_class_def(std::string def) {
    if (def.empty()) return;
    for (const auto& existing : class_defs_) {
        if (existing == def) return;
    }
    class_defs_.push_back(std::move(def));
}

void HeaderEmitter::add_errtype_def(std::string def) {
    if (def.empty()) return;
    for (const auto& existing : errtype_defs_) {
        if (existing == def) return;
    }
    errtype_defs_.push_back(std::move(def));
}

void HeaderEmitter::add_const_view(std::string name, std::string cpp_type) {
    std::string entry = "extern " + cpp_type + " _internal_" + name + ";\n"
                        "    inline const " + cpp_type + "& " + name + " = _internal_" + name + ";";
    for (const auto& existing : const_views_) {
        if (existing == entry) return;
    }
    const_views_.push_back(std::move(entry));
}

void HeaderEmitter::add_function_template(std::string template_def) {
    if (template_def.empty()) return;
    for (const auto& existing : function_templates_) {
        if (existing == template_def) return;
    }
    function_templates_.push_back(std::move(template_def));
}

void HeaderEmitter::add_function_prototype(std::string prototype) {
    if (prototype.empty()) return;
    for (const auto& existing : function_prototypes_) {
        if (existing == prototype) return;
    }
    function_prototypes_.push_back(std::move(prototype));
}

void HeaderEmitter::collect_declarations(const parser::ModuleAST& ast) {
    collect_declarations(ast.declarations);
}

void HeaderEmitter::collect_declarations(const std::vector<std::unique_ptr<ast::Decl>>& decls) {
    for (const auto& decl : decls) {
        if (!decl) continue;

        if (const auto* def = dynamic_cast<const ast::DefineDecl*>(decl.get())) {
            std::string cpp_type = def->getType() ? to_cpp_type(def->getType()) : "auto";
            std::string val = def->getValue() ? to_cpp_expr(def->getValue()) : "{}";
            add_constexpr_def("inline constexpr " + cpp_type + " " + def->getName() + " = " + val + ";");
        } else if (const auto* enm = dynamic_cast<const ast::EnumDecl*>(decl.get())) {
            std::string underlying = enm->getUnderlyingType() ? to_cpp_type(enm->getUnderlyingType()) : "int64_t";
            add_forward_decl("enum class " + enm->getName() + " : " + underlying + ";");
            add_enum_def(generate_enum_def(*enm));
        } else if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            add_forward_decl("struct " + cls->getName() + ";");
            add_class_def(generate_class_def(*cls));
        } else if (const auto* err = dynamic_cast<const ast::ErrTypeDecl*>(decl.get())) {
            add_forward_decl("struct " + err->getName() + ";");
            add_errtype_def(generate_errtype_def(*err));
        } else if (const auto* var = dynamic_cast<const ast::VarDecl*>(decl.get())) {
            if (var->isExported()) {
                std::string cpp_type = var->getType() ? to_cpp_type(var->getType()) : "auto";
                add_const_view(var->getName(), cpp_type);
            }
        } else if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            if (fn->isGeneric()) {
                add_function_template(format_function_template(*fn));
            } else {
                add_function_prototype(format_function_prototype(*fn));
            }
        }
    }
}

void HeaderEmitter::collect_ir_declarations(const ir::Module& ir_mod) {
    for (const auto& global : ir_mod.get_globals()) {
        if (global.is_export) {
            add_const_view(global.name, to_cpp_type(global.type));
        }
    }
    for (const auto& fn : ir_mod.get_functions()) {
        if (fn) {
            add_function_prototype(format_function_prototype(*fn));
        }
    }
}

std::string HeaderEmitter::emit_forward_declarations() const {
    if (forward_decls_.empty()) {
        return "";
    }
    std::ostringstream ss;
    ss << "    // --- Forward Declarations ---\n";
    for (const auto& decl : forward_decls_) {
        ss << "    " << decl << "\n";
    }
    return ss.str();
}

std::string HeaderEmitter::emit_constexpr_definitions() const {
    if (constexpr_defs_.empty()) {
        return "";
    }
    std::ostringstream ss;
    ss << "    // --- Compile-Time Constants ---\n";
    for (const auto& def : constexpr_defs_) {
        ss << "    " << def << "\n";
    }
    return ss.str();
}

std::string HeaderEmitter::emit_enum_definitions() const {
    if (enum_defs_.empty()) {
        return "";
    }
    std::ostringstream ss;
    ss << "    // --- Enums ---\n";
    for (size_t i = 0; i < enum_defs_.size(); ++i) {
        if (i > 0) ss << "\n";
        ss << enum_defs_[i] << "\n";
    }
    return ss.str();
}

std::string HeaderEmitter::emit_class_definitions() const {
    if (class_defs_.empty()) {
        return "";
    }
    std::ostringstream ss;
    ss << "    // --- Classes and Structs ---\n";
    for (size_t i = 0; i < class_defs_.size(); ++i) {
        if (i > 0) ss << "\n";
        ss << class_defs_[i] << "\n";
    }
    return ss.str();
}

std::string HeaderEmitter::emit_errtype_definitions() const {
    if (errtype_defs_.empty()) {
        return "";
    }
    std::ostringstream ss;
    ss << "    // --- Error Types ---\n";
    for (size_t i = 0; i < errtype_defs_.size(); ++i) {
        if (i > 0) ss << "\n";
        ss << errtype_defs_[i] << "\n";
    }
    return ss.str();
}

std::string HeaderEmitter::emit_const_views() const {
    if (const_views_.empty()) {
        return "";
    }
    std::ostringstream ss;
    ss << "    // --- Exported Variable Const-View Accessor ---\n";
    for (const auto& view : const_views_) {
        ss << "    " << view << "\n";
    }
    return ss.str();
}

std::string HeaderEmitter::emit_function_templates() const {
    if (function_templates_.empty()) {
        return "";
    }
    std::ostringstream ss;
    ss << "    // --- Generic Function Templates ---\n";
    for (const auto& tmpl : function_templates_) {
        ss << "    " << tmpl << "\n";
    }
    return ss.str();
}

std::string HeaderEmitter::emit_function_prototypes() const {
    if (function_prototypes_.empty()) {
        return "";
    }
    std::ostringstream ss;
    ss << "    // --- Exported Function Prototypes ---\n";
    for (const auto& proto : function_prototypes_) {
        ss << "    " << proto << "\n";
    }
    return ss.str();
}

std::string HeaderEmitter::emit_namespace_body() const {
    std::ostringstream ss;
    bool has_prev = false;

    auto append_section = [&](const std::string& section) {
        if (!section.empty()) {
            if (has_prev) ss << "\n";
            ss << section;
            has_prev = true;
        }
    };

    append_section(emit_forward_declarations());
    append_section(emit_constexpr_definitions());
    append_section(emit_enum_definitions());
    append_section(emit_class_definitions());
    append_section(emit_errtype_definitions());
    append_section(emit_const_views());
    append_section(emit_function_templates());
    append_section(emit_function_prototypes());

    return ss.str();
}

std::string HeaderEmitter::emit() const {
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
    }

    return ss.str();
}

void HeaderEmitter::emit(std::ostream& os) const {
    os << emit();
}

} // namespace keety::emitter
