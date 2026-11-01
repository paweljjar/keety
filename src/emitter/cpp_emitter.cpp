#include <keety/ast/decl.hpp>
#include <keety/ast/expr.hpp>
#include <keety/ast/stmt.hpp>
#include <keety/emitter/cpp_emitter.hpp>
#include <keety/ir/basic_block.hpp>
#include <keety/ir/instruction.hpp>
#include <keety/lexer/token.hpp>

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
    if (!ast.declarations.empty()) {
        collect_declarations(ast);
        for (const auto& global : ir_module.get_globals()) {
            if (!is_module_var(global.name)) {
                register_module_var(global.name);
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
    } else {
        collect_ir_declarations(ir_module);
    }
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

bool CppEmitter::is_module_var(const std::string& name) const {
    for (const auto& v : module_vars_) {
        if (v == name) return true;
    }
    return false;
}

void CppEmitter::register_module_var(std::string name) {
    if (name.empty()) return;
    for (const auto& existing : module_vars_) {
        if (existing == name) return;
    }
    module_vars_.push_back(std::move(name));
}

void CppEmitter::add_storage_allocation(std::string name, std::string cpp_type, std::string initial_value) {
    register_module_var(name);
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

void CppEmitter::add_function_definition(std::string function_def) {
    if (function_def.empty()) return;
    for (const auto& existing : function_definitions_) {
        if (existing == function_def) return;
    }
    function_definitions_.push_back(std::move(function_def));
}

void CppEmitter::collect_declarations(const parser::ModuleAST& ast) {
    collect_declarations(ast.declarations);
}

void CppEmitter::collect_declarations(const std::vector<std::unique_ptr<ast::Decl>>& decls) {
    // 0. Preliminary pass: scan for standard library imports (e.g. io)
    for (const auto& decl : decls) {
        if (!decl) continue;
        if (const auto* imp = dynamic_cast<const ast::ImportDecl*>(decl.get())) {
            const std::string& p = imp->getModulePath();
            if (p == "io" || p == "std.io" || p == "keet.io") {
                has_io_import_ = true;
                add_standard_header("<keet/runtime.hpp>");
                for (const auto& s : imp->getSymbols()) {
                    imported_io_symbols_.insert(s.getEffectiveName());
                }
            } else if (p == "fs" || p == "std.fs" || p == "keet.fs") {
                has_fs_import_ = true;
                add_standard_header("<keet/runtime.hpp>");
                for (const auto& s : imp->getSymbols()) {
                    imported_fs_symbols_.insert(s.getEffectiveName());
                }
            }
        }
    }

    // 1. First pass: register and allocate storage for all module-level variables
    for (const auto& decl : decls) {
        if (!decl) continue;
        if (const auto* var = dynamic_cast<const ast::VarDecl*>(decl.get())) {
            register_module_var(var->getName());
            std::string cpp_type = var->getType() ? HeaderEmitter::to_cpp_type(var->getType()) : "auto";
            std::string init_val = var->getInitializer() ? HeaderEmitter::to_cpp_expr(var->getInitializer()) : "{}";
            add_storage_allocation(var->getName(), cpp_type, init_val);
        } else if (const auto* cst = dynamic_cast<const ast::ConstDecl*>(decl.get())) {
            register_module_var(cst->getName());
            std::string cpp_type = cst->getType() ? HeaderEmitter::to_cpp_type(cst->getType()) : "auto";
            std::string init_val = cst->getInitializer() ? HeaderEmitter::to_cpp_expr(cst->getInitializer()) : "{}";
            add_storage_allocation(cst->getName(), "const " + cpp_type, init_val);
        }
    }

    // 2. Second pass: translate functions and class methods
    for (const auto& decl : decls) {
        if (!decl) continue;
        if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            if (fn->getName() == "main") {
                has_main_function_ = true;
                main_has_args_ = !fn->getParams().empty();
                if (fn->getReturnType()) {
                    if (const auto* prim = dynamic_cast<const ast::PrimitiveType*>(fn->getReturnType())) {
                        main_returns_int_ = (prim->getKind() == ast::PrimitiveTypeKind::Int);
                    }
                }
            }
            if (fn->getBody()) {
                add_function_definition(translate_function(*fn));
            }
        } else if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            for (const auto& method : cls->getMethods()) {
                if (method && method->getBody()) {
                    add_function_definition(translate_method(cls->getName(), *method));
                }
            }
        }
    }
}

void CppEmitter::collect_ir_declarations(const ir::Module& ir_module) {
    for (const auto& global : ir_module.get_globals()) {
        register_module_var(global.name);
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

    for (const auto& fn : ir_module.get_functions()) {
        if (fn) {
            if (fn->get_name() == "main" || fn->get_name() == "_keet_main") {
                has_main_function_ = true;
                main_has_args_ = !fn->get_parameters().empty();
                if (fn->get_return_type()) {
                    main_returns_int_ = fn->get_return_type()->isInt();
                }
            }
            add_function_definition(translate_ir_function(*fn));
        }
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

std::string CppEmitter::to_cpp_ir_operand(const ir::Operand& operand) const {
    if (operand.is_constant()) {
        return to_cpp_operand(operand);
    }
    if (operand.is_temp()) {
        return "_t" + std::to_string(operand.get_temp_id());
    }
    if (operand.is_global() || is_module_var(operand.get_name())) {
        return make_internal_var_name(operand.get_name());
    }
    return operand.get_name();
}

std::string CppEmitter::translate_expression(const ast::Expr* expr) const {
    if (!expr) {
        return "";
    }

    if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(expr)) {
        if (lit->getRawText() == "null") {
            return "std::monostate{}";
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
        if (id->getName() == "this") {
            return "this";
        }
        if (id->getName() == "main") {
            return "_keet_main";
        }
        auto nit = narrowed_vars_.find(id->getName());
        if (nit != narrowed_vars_.end()) {
            return "std::get<" + nit->second + ">(" + id->getName() + ")";
        }
        if (is_module_var(id->getName())) {
            return make_internal_var_name(id->getName());
        }
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
        if (un->isPrefix()) {
            return std::string(op_str) + translate_expression(un->getOperand());
        }
        return translate_expression(un->getOperand()) + std::string(op_str);
    }

    if (const auto* bin = dynamic_cast<const ast::BinaryExpr*>(expr)) {
        if (bin->getOp() == lexer::TokenKind::Equal) {
            if (const auto* id = dynamic_cast<const ast::IdentExpr*>(bin->getLeft())) {
                narrowed_vars_.erase(id->getName());
                std::string lhs = is_module_var(id->getName()) ? make_internal_var_name(id->getName()) : id->getName();
                return lhs + " = " + translate_expression(bin->getRight());
            }
        }
        if (in_zonk_expr_ && bin->getOp() == lexer::TokenKind::Slash) {
            return "keet::div_safe(" + translate_expression(bin->getLeft()) + ", " + translate_expression(bin->getRight()) + ")";
        }
        if (in_zonk_expr_ && bin->getOp() == lexer::TokenKind::Percent) {
            return "keet::mod_safe(" + translate_expression(bin->getLeft()) + ", " + translate_expression(bin->getRight()) + ")";
        }
        std::string_view op_str = lexer::getTokenKindName(bin->getOp());
        return translate_expression(bin->getLeft()) + " " + std::string(op_str) + " " + translate_expression(bin->getRight());
    }

    if (const auto* tern = dynamic_cast<const ast::TernaryExpr*>(expr)) {
        std::string cond_s = translate_expression(tern->getCondition());
        std::string then_s = translate_expression(tern->getThenExpr());
        std::string else_s = translate_expression(tern->getElseExpr());
        return "((" + cond_s + ") ? keet::branch_type_t<decltype(" + then_s + "), decltype(" + else_s + ")>(" + then_s + ") : " +
               "keet::branch_type_t<decltype(" + then_s + "), decltype(" + else_s + ")>(" + else_s + "))";
    }

    if (const auto* idx = dynamic_cast<const ast::IndexExpr*>(expr)) {
        return translate_expression(idx->getContainer()) + "[" + translate_expression(idx->getIndex()) + "]";
    }

    if (const auto* mem = dynamic_cast<const ast::MemberExpr*>(expr)) {
        if (const auto* obj_id = dynamic_cast<const ast::IdentExpr*>(mem->getObject())) {
            if (obj_id->getName() == "this") {
                return "this->" + mem->getMemberName();
            }
            if (obj_id->getName() == "Color") {
                return "::keet::io::Color::" + mem->getMemberName();
            }
        }
        if (const auto* obj_mem = dynamic_cast<const ast::MemberExpr*>(mem->getObject())) {
            if (const auto* root_id = dynamic_cast<const ast::IdentExpr*>(obj_mem->getObject())) {
                if (root_id->getName() == "io" && obj_mem->getMemberName() == "Color") {
                    return "::keet::io::Color::" + mem->getMemberName();
                }
            }
        }
        return translate_expression(mem->getObject()) + "." + mem->getMemberName();
    }

    if (const auto* call = dynamic_cast<const ast::CallExpr*>(expr)) {
        std::string args_str;
        for (size_t i = 0; i < call->getArgs().size(); ++i) {
            if (i > 0) args_str += ", ";
            args_str += translate_expression(call->getArgs()[i].value.get());
        }

        if (const auto* mem = dynamic_cast<const ast::MemberExpr*>(call->getCallee())) {
            std::string obj_s;
            bool is_this = false;
            if (const auto* obj_id = dynamic_cast<const ast::IdentExpr*>(mem->getObject())) {
                if (obj_id->getName() == "this") {
                    is_this = true;
                    obj_s = "this";
                }
            }
            if (!is_this) {
                obj_s = translate_expression(mem->getObject());
            }

            std::string m_name = mem->getMemberName();
            if (obj_s == "io") {
                if (m_name == "print" || m_name == "println" || m_name == "eprint" || m_name == "eprintln" ||
                    m_name == "readln" || m_name == "readLine" || m_name == "eof" ||
                    m_name == "input" || m_name == "flush" || m_name == "red" ||
                    m_name == "green" || m_name == "yellow" || m_name == "blue" ||
                    m_name == "magenta" || m_name == "cyan" || m_name == "white" ||
                    m_name == "gray" || m_name == "bold") {
                    return "::keet::io::" + m_name + "(" + args_str + ")";
                }
            }
            if (obj_s == "fs") {
                if (m_name == "readText" ||
                    m_name == "readLines" ||
                    m_name == "writeText" ||
                    m_name == "appendText" ||
                    m_name == "exists" ||
                    m_name == "isFile" ||
                    m_name == "isDir" ||
                    m_name == "fileSize" ||
                    m_name == "mkdir" || m_name == "remove" ||
                    m_name == "removeAll" ||
                    m_name == "copy" || m_name == "rename" ||
                    m_name == "listDir" ||
                    m_name == "cwd" || m_name == "join" ||
                    m_name == "fileName" ||
                    m_name == "extension" ||
                    m_name == "parentDir") {
                    return "::keet::fs::" + m_name + "(" + args_str + ")";
                }
            }
            // Container member mapping
            if (m_name == "push") {
                return obj_s + ".push_back(" + args_str + ")";
            }
            if (m_name == "pop") {
                return obj_s + ".pop_back()";
            }
            if (m_name == "len" || m_name == "length") {
                return obj_s + ".size()";
            }
            if (m_name == "has") {
                return "(" + obj_s + ".count(" + args_str + ") > 0)";
            }
            if (m_name == "clear") {
                return obj_s + ".clear()";
            }
            if (m_name == "reserve") {
                return obj_s + ".reserve(" + args_str + ")";
            }
            if (m_name == "filter") {
                return "keet::filter(" + obj_s + ", " + args_str + ")";
            }
            if (m_name == "map") {
                return "keet::map(" + obj_s + ", " + args_str + ")";
            }
            if (m_name == "reduce") {
                return "keet::reduce(" + obj_s + ", " + args_str + ")";
            }
            if (m_name == "clamp") {
                return "keet::clamp(" + obj_s + ", " + args_str + ")";
            }
            if (m_name == "toString") {
                return "keet::to_string(" + obj_s + ")";
            }
            if (m_name == "tryInt") {
                return "keet::try_int(" + obj_s + ")";
            }
            if (m_name == "tryFloat") {
                return "keet::try_float(" + obj_s + ")";
            }
            if (m_name == "trim") {
                return "keet::trim(" + obj_s + ")";
            }

            if (is_this) {
                return "this->" + m_name + "(" + args_str + ")";
            }
            return obj_s + "." + m_name + "(" + args_str + ")";
        }

        if (const auto* callee_id = dynamic_cast<const ast::IdentExpr*>(call->getCallee())) {
            const std::string& name = callee_id->getName();
            if (has_io_import_ && (imported_io_symbols_.count(name) ||
                name == "print" || name == "println" || name == "eprint" || name == "eprintln" ||
                name == "readln" || name == "readLine" || name == "eof" ||
                name == "input" || name == "flush" || name == "red" || name == "green" ||
                name == "yellow" || name == "blue" || name == "magenta" || name == "cyan" ||
                name == "white" || name == "gray" || name == "bold")) {
                return "::keet::io::" + name + "(" + args_str + ")";
            }
            if (has_fs_import_ && (imported_fs_symbols_.count(name) ||
                name == "readText" ||
                name == "readLines" ||
                name == "writeText" ||
                name == "appendText" ||
                name == "exists" ||
                name == "isFile" ||
                name == "isDir" ||
                name == "fileSize" ||
                name == "mkdir" || name == "remove" ||
                name == "removeAll" ||
                name == "copy" || name == "rename" ||
                name == "listDir" ||
                name == "cwd" || name == "join" ||
                name == "fileName" ||
                name == "extension" ||
                name == "parentDir")) {
                return "::keet::fs::" + name + "(" + args_str + ")";
            }
        }

        return translate_expression(call->getCallee()) + "(" + args_str + ")";
    }

    if (const auto* closure = dynamic_cast<const ast::ClosureExpr*>(expr)) {
        std::string raw_ret = closure->getReturnType() ? HeaderEmitter::to_cpp_type(closure->getReturnType()) : "void";
        if (closure->isAsync()) {
            std::string res = "[=](";
            for (size_t i = 0; i < closure->getParams().size(); ++i) {
                if (i > 0) res += ", ";
                const auto& p = closure->getParams()[i];
                if (p.type) {
                    res += HeaderEmitter::to_cpp_type(p.type.get()) + " " + p.name;
                } else {
                    res += "auto " + p.name;
                }
            }
            res += ") -> keet::Task<" + raw_ret + "> {\n";
            res += "            return keet::spawn([=]() -> " + raw_ret + " {\n";
            if (closure->getBody()) {
                res += translate_block(closure->getBody(), 4);
            }
            res += "            });\n";
            res += "        }";
            return res;
        }

        std::string res = "[&](";
        for (size_t i = 0; i < closure->getParams().size(); ++i) {
            if (i > 0) res += ", ";
            const auto& p = closure->getParams()[i];
            if (p.type) {
                std::string pt = HeaderEmitter::to_cpp_type(p.type.get());
                if (p.isMut) {
                    res += pt + "& " + p.name;
                } else {
                    res += pt + " " + p.name;
                }
            } else {
                if (p.isMut) {
                    res += "auto& " + p.name;
                } else {
                    res += "auto " + p.name;
                }
            }
        }
        res += ") ";
        if (closure->getReturnType()) {
            res += "-> " + raw_ret + " ";
        }
        res += "{\n";
        if (closure->getBody()) {
            res += translate_block(closure->getBody(), 1);
        }
        res += "}";
        return res;
    }

    if (const auto* cast = dynamic_cast<const ast::TypeCastExpr*>(expr)) {
        std::string target_t = HeaderEmitter::to_cpp_type(cast->getTargetType());
        if (cast->isAssertion()) {
            if (const auto* id = dynamic_cast<const ast::IdentExpr*>(cast->getExpr())) {
                std::string var_name = is_module_var(id->getName()) ? make_internal_var_name(id->getName()) : id->getName();
                return "std::get<" + target_t + ">(" + var_name + ")";
            }
            return "std::get<" + target_t + ">(" + translate_expression(cast->getExpr()) + ")";
        }
        if (target_t == "int64_t") {
            return "keet::to_int(" + translate_expression(cast->getExpr()) + ")";
        }
        return "static_cast<" + target_t + ">(" + translate_expression(cast->getExpr()) + ")";
    }

    if (const auto* top = dynamic_cast<const ast::TypeofExpr*>(expr)) {
        return "keet::type_of(" + translate_expression(top->getExpr()) + ")";
    }

    if (const auto* arr = dynamic_cast<const ast::ArrayLitExpr*>(expr)) {
        std::string res = "{";
        for (size_t i = 0; i < arr->getElements().size(); ++i) {
            if (i > 0) res += ", ";
            res += translate_expression(arr->getElements()[i].get());
        }
        res += "}";
        return res;
    }

    if (const auto* map = dynamic_cast<const ast::MapLitExpr*>(expr)) {
        std::string res = "{";
        for (size_t i = 0; i < map->getEntries().size(); ++i) {
            if (i > 0) res += ", ";
            res += "{" + translate_expression(map->getEntries()[i].key.get()) + ", " +
                   translate_expression(map->getEntries()[i].value.get()) + "}";
        }
        res += "}";
        return res;
    }

    if (const auto* tpl = dynamic_cast<const ast::TemplateLitExpr*>(expr)) {
        if (tpl->getParts().empty()) {
            return "std::string(\"\")";
        }
        std::string res = "(";
        bool first = true;
        for (const auto& part : tpl->getParts()) {
            if (!first) res += " + ";
            first = false;
            if (part.isExpr) {
                res += "keet::to_string(" + translate_expression(part.expr.get()) + ")";
            } else {
                std::ostringstream ss;
                ss << std::quoted(part.text);
                res += "std::string(" + ss.str() + ")";
            }
        }
        res += ")";
        return res;
    }

    if (const auto* znk = dynamic_cast<const ast::ZonkExpr*>(expr)) {
        bool prev_zonk = in_zonk_expr_;
        in_zonk_expr_ = true;
        std::string prim_str = translate_expression(znk->getPrimary());
        in_zonk_expr_ = prev_zonk;
        if (znk->hasRecoveryBlock()) {
            std::string err_v = znk->getErrorVarName().empty() ? "err" : znk->getErrorVarName();
            std::string res = "([&]() -> std::decay_t<decltype(" + prim_str + ")> {\n"
                              "            try {\n"
                              "                return " + prim_str + ";\n"
                              "            } catch (const keet::error& " + err_v + ") {\n";
            res += translate_block(znk->getRecoveryBlock(), 4);
            res += "            } catch (const std::exception& e) {\n";
            res += "                const keet::error " + err_v + "(\"Error\", e.what());\n";
            res += translate_block(znk->getRecoveryBlock(), 4);
            res += "            }\n"
                   "        })()";
            return res;
        }
        if (znk->getFallback()) {
            return "([&]() -> std::decay_t<decltype(" + prim_str + ")> { try { return " + prim_str +
                   "; } catch (...) { return " + translate_expression(znk->getFallback()) + "; } })()";
        }
    }

    if (const auto* aw = dynamic_cast<const ast::AwaitExpr*>(expr)) {
        return "(" + translate_expression(aw->getTarget()) + ").get()";
    }

    return "";
}

std::string CppEmitter::translate_block(const ast::BlockStmt* block, int indent) const {
    if (!block) return "";
    std::string res;
    for (const auto& s : block->getStatements()) {
        res += translate_statement(s.get(), indent);
    }
    return res;
}

std::string CppEmitter::translate_statement(const ast::Stmt* stmt, int indent) const {
    if (!stmt) return "";
    std::string indent_str(indent * 4, ' ');

    if (const auto* blk = dynamic_cast<const ast::BlockStmt*>(stmt)) {
        return translate_block(blk, indent);
    }

    if (const auto* decl = dynamic_cast<const ast::DeclStmt*>(stmt)) {
        if (const auto* var = dynamic_cast<const ast::VarDecl*>(decl->getDeclaration())) {
            std::string type_s = var->getType() ? HeaderEmitter::to_cpp_type(var->getType()) : "auto";
            std::string init_s;
            if (var->getInitializer()) {
                init_s = " = " + translate_expression(var->getInitializer());
            } else {
                init_s = "{}";
            }
            return indent_str + type_s + " " + var->getName() + init_s + ";\n";
        }
        if (const auto* cst = dynamic_cast<const ast::ConstDecl*>(decl->getDeclaration())) {
            std::string type_s = cst->getType() ? HeaderEmitter::to_cpp_type(cst->getType()) : "auto";
            std::string init_s = cst->getInitializer() ? " = " + translate_expression(cst->getInitializer()) : "{}";
            return indent_str + "const " + type_s + " " + cst->getName() + init_s + ";\n";
        }
        return "";
    }

    if (const auto* expr_stmt = dynamic_cast<const ast::ExprStmt*>(stmt)) {
        return indent_str + translate_expression(expr_stmt->getExpression()) + ";\n";
    }

    if (const auto* ret = dynamic_cast<const ast::ReturnStmt*>(stmt)) {
        if (ret->hasValue()) {
            return indent_str + "return " + translate_expression(ret->getValue()) + ";\n";
        }
        return indent_str + "return;\n";
    }

    if (const auto* yld = dynamic_cast<const ast::YieldStmt*>(stmt)) {
        if (yld->getValue()) {
            return indent_str + "return " + translate_expression(yld->getValue()) + ";\n";
        }
        return indent_str + "return;\n";
    }

    if (const auto* brk = dynamic_cast<const ast::BreakStmt*>(stmt)) {
        if (brk->hasTargetLabel()) {
            return indent_str + "goto " + brk->getTargetLabel() + "_break;\n";
        }
        return indent_str + "break;\n";
    }

    if (const auto* cnt = dynamic_cast<const ast::ContinueStmt*>(stmt)) {
        if (cnt->hasTargetLabel()) {
            return indent_str + "goto " + cnt->getTargetLabel() + "_continue;\n";
        }
        return indent_str + "continue;\n";
    }

    if (const auto* znk = dynamic_cast<const ast::ZonkStmt*>(stmt)) {
        const auto* err_expr = znk->getErrorExpr();
        if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(err_expr)) {
            if (lit->getLiteralKind() == lexer::TokenKind::StringLiteral) {
                return indent_str + "throw keet::error(\"Error\", " + lit->getRawText() + ");\n";
            }
        }
        return indent_str + "throw " + translate_expression(err_expr) + ";\n";
    }

    if (const auto* if_stmt = dynamic_cast<const ast::IfStmt*>(stmt)) {
        std::string narrowed_var;
        std::string narrowed_type;
        if (const auto* bin = dynamic_cast<const ast::BinaryExpr*>(if_stmt->getCondition())) {
            if (bin->getOp() == lexer::TokenKind::EqualEqual) {
                const ast::TypeofExpr* top = dynamic_cast<const ast::TypeofExpr*>(bin->getLeft());
                const ast::LiteralExpr* lit = dynamic_cast<const ast::LiteralExpr*>(bin->getRight());
                if (!top) {
                    top = dynamic_cast<const ast::TypeofExpr*>(bin->getRight());
                    lit = dynamic_cast<const ast::LiteralExpr*>(bin->getLeft());
                }
                if (top && lit && lit->getLiteralKind() == lexer::TokenKind::StringLiteral) {
                    if (const auto* id = dynamic_cast<const ast::IdentExpr*>(top->getExpr())) {
                        narrowed_var = id->getName();
                        std::string tname = lit->getRawText();
                        if (tname.size() >= 2 && (tname.front() == '"' || tname.front() == '\'')) {
                            tname = tname.substr(1, tname.size() - 2);
                        }
                        if (tname == "int") narrowed_type = "int64_t";
                        else if (tname == "string") narrowed_type = "std::string";
                        else if (tname == "float") narrowed_type = "double";
                        else if (tname == "bool") narrowed_type = "bool";
                        else if (tname == "char") narrowed_type = "char";
                    }
                }
            } else if (bin->getOp() == lexer::TokenKind::ExclaimEqual) {
                const ast::IdentExpr* id = dynamic_cast<const ast::IdentExpr*>(bin->getLeft());
                const ast::LiteralExpr* lit = dynamic_cast<const ast::LiteralExpr*>(bin->getRight());
                if (!id) {
                    id = dynamic_cast<const ast::IdentExpr*>(bin->getRight());
                    lit = dynamic_cast<const ast::LiteralExpr*>(bin->getLeft());
                }
                if (id && lit && lit->getRawText() == "null") {
                    if (if_stmt->hasInit()) {
                        if (const auto* decl = dynamic_cast<const ast::DeclStmt*>(if_stmt->getInit())) {
                            if (const auto* var = dynamic_cast<const ast::VarDecl*>(decl->getDeclaration())) {
                                if (var->getName() == id->getName() && var->getType()) {
                                    if (const auto* un = dynamic_cast<const ast::UnionType*>(var->getType())) {
                                        for (const auto& v : un->getVariants()) {
                                            if (const auto* prim = dynamic_cast<const ast::PrimitiveType*>(v.get())) {
                                                if (prim->getKind() != ast::PrimitiveTypeKind::Null) {
                                                    narrowed_var = id->getName();
                                                    narrowed_type = HeaderEmitter::to_cpp_type(v.get());
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        std::string res = indent_str + "if (";
        if (if_stmt->hasInit()) {
            std::string init_s = translate_statement(if_stmt->getInit(), 0);
            if (!init_s.empty() && init_s.back() == '\n') init_s.pop_back();
            res += init_s + " ";
        }
        res += translate_expression(if_stmt->getCondition()) + ") {\n";

        std::string prev_narrowed;
        bool had_prev = false;
        if (!narrowed_var.empty() && !narrowed_type.empty()) {
            auto it = narrowed_vars_.find(narrowed_var);
            if (it != narrowed_vars_.end()) {
                prev_narrowed = it->second;
                had_prev = true;
            }
            narrowed_vars_[narrowed_var] = narrowed_type;
        }

        res += translate_block(if_stmt->getThenBranch(), indent + 1);

        if (!narrowed_var.empty() && !narrowed_type.empty()) {
            if (had_prev) {
                narrowed_vars_[narrowed_var] = prev_narrowed;
            } else {
                narrowed_vars_.erase(narrowed_var);
            }
        }

        res += indent_str + "}";
        if (if_stmt->hasElseBranch()) {
            if (const auto* else_if = dynamic_cast<const ast::IfStmt*>(if_stmt->getElseBranch())) {
                std::string elif_str = translate_statement(else_if, indent);
                size_t first_non_space = elif_str.find_first_not_of(' ');
                if (first_non_space != std::string::npos) {
                    elif_str = elif_str.substr(first_non_space);
                }
                res += " else " + elif_str;
                return res;
            } else if (const auto* else_block = dynamic_cast<const ast::BlockStmt*>(if_stmt->getElseBranch())) {
                res += " else {\n";
                res += translate_block(else_block, indent + 1);
                res += indent_str + "}\n";
                return res;
            }
        }
        res += "\n";
        return res;
    }

    if (const auto* while_stmt = dynamic_cast<const ast::WhileStmt*>(stmt)) {
        std::string res = indent_str + "while (";
        if (while_stmt->hasInit()) {
            std::string init_s = translate_statement(while_stmt->getInit(), 0);
            if (!init_s.empty() && init_s.back() == '\n') init_s.pop_back();
            res += init_s + " ";
        }
        res += translate_expression(while_stmt->getCondition()) + ") {\n";
        res += translate_block(while_stmt->getBody(), indent + 1);
        if (while_stmt->hasLabel()) {
            res += indent_str + "    " + while_stmt->getLabel() + "_continue:;\n";
        }
        res += indent_str + "}\n";
        if (while_stmt->hasLabel()) {
            res += indent_str + while_stmt->getLabel() + "_break:;\n";
        }
        return res;
    }

    if (const auto* for_stmt = dynamic_cast<const ast::ForStmt*>(stmt)) {
        std::string res = indent_str + "for (";
        if (for_stmt->getInit()) {
            std::string init_s = translate_statement(for_stmt->getInit(), 0);
            if (!init_s.empty() && init_s.back() == '\n') init_s.pop_back();
            res += init_s + " ";
        } else {
            res += "; ";
        }
        if (for_stmt->getCondition()) {
            res += translate_expression(for_stmt->getCondition());
        }
        res += "; ";
        if (for_stmt->getPost()) {
            std::string post_s = translate_statement(for_stmt->getPost(), 0);
            if (!post_s.empty() && post_s.back() == '\n') post_s.pop_back();
            if (!post_s.empty() && post_s.back() == ';') post_s.pop_back();
            res += post_s;
        }
        res += ") {\n";
        res += translate_block(for_stmt->getBody(), indent + 1);
        if (for_stmt->hasLabel()) {
            res += indent_str + "    " + for_stmt->getLabel() + "_continue:;\n";
        }
        res += indent_str + "}\n";
        if (for_stmt->hasLabel()) {
            res += indent_str + for_stmt->getLabel() + "_break:;\n";
        }
        return res;
    }

    if (const auto* for_in = dynamic_cast<const ast::ForInStmt*>(stmt)) {
        std::string res = indent_str + "for (";
        std::string binding;
        if (for_in->isKeyValue()) {
            binding = for_in->isConst() ? "const auto& [" + for_in->getKeyName() + ", " + for_in->getValueName() + "]"
                                        : "auto& [" + for_in->getKeyName() + ", " + for_in->getValueName() + "]";
        } else {
            binding = for_in->isConst() ? "const auto& " + for_in->getValueName()
                                        : "auto& " + for_in->getValueName();
        }
        res += binding + " : " + translate_expression(for_in->getCollection()) + ") {\n";
        res += translate_block(for_in->getBody(), indent + 1);
        if (for_in->hasLabel()) {
            res += indent_str + "    " + for_in->getLabel() + "_continue:;\n";
        }
        res += indent_str + "}\n";
        if (for_in->hasLabel()) {
            res += indent_str + for_in->getLabel() + "_break:;\n";
        }
        return res;
    }

    if (const auto* sw = dynamic_cast<const ast::SwitchStmt*>(stmt)) {
        std::string res = indent_str + "switch (";
        if (sw->hasInit()) {
            std::string init_s = translate_statement(sw->getInit(), 0);
            if (!init_s.empty() && init_s.back() == '\n') init_s.pop_back();
            res += init_s + " ";
        }
        res += translate_expression(sw->getDiscriminant()) + ") {\n";
        for (const auto& c : sw->getCases()) {
            if (c.isDefault()) {
                res += indent_str + "default: {\n";
                res += translate_block(c.body.get(), indent + 1);
                res += indent_str + "    break;\n";
                res += indent_str + "}\n";
            } else {
                for (const auto& v : c.values) {
                    res += indent_str + "case " + translate_expression(v.get()) + ":\n";
                }
                res += indent_str + "{\n";
                res += translate_block(c.body.get(), indent + 1);
                res += indent_str + "    break;\n";
                res += indent_str + "}\n";
            }
        }
        res += indent_str + "}\n";
        if (sw->hasLabel()) {
            res += indent_str + sw->getLabel() + "_break:;\n";
        }
        return res;
    }

    return "";
}

std::string CppEmitter::translate_function(const ast::FnDecl& fn) const {
    std::string raw_ret = fn.getReturnType() ? HeaderEmitter::to_cpp_type(fn.getReturnType()) : "void";
    std::string ret_type = fn.isAsync() ? ("keet::Task<" + raw_ret + ">") : raw_ret;
    std::string params_str;
    for (size_t i = 0; i < fn.getParams().size(); ++i) {
        if (i > 0) params_str += ", ";
        params_str += HeaderEmitter::format_param(fn.getParams()[i], fn.getTypeParams());
    }

    std::string fn_name = (fn.getName() == "main") ? "_keet_main" : fn.getName();
    std::ostringstream ss;
    if (!fn.getTypeParams().empty()) {
        ss << "    template <";
        for (size_t i = 0; i < fn.getTypeParams().size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "typename " << fn.getTypeParams()[i];
        }
        ss << ">\n";
    }
    ss << "    " << ret_type << " " << fn_name << "(" << params_str << ") {\n";
    if (fn.isAsync()) {
        ss << "        return keet::spawn([=]() -> " + raw_ret + " {\n";
        if (fn.getBody()) {
            if (const auto* block = dynamic_cast<const ast::BlockStmt*>(fn.getBody())) {
                ss << translate_block(block, 3);
            } else {
                ss << translate_statement(fn.getBody(), 3);
            }
        }
        ss << "        });\n";
    } else {
        if (fn.getBody()) {
            if (const auto* block = dynamic_cast<const ast::BlockStmt*>(fn.getBody())) {
                ss << translate_block(block, 2);
            } else {
                ss << translate_statement(fn.getBody(), 2);
            }
        }
    }
    ss << "    }";
    return ss.str();
}

std::string CppEmitter::translate_method(std::string_view class_name, const ast::FnDecl& fn) const {
    std::string ret_type = fn.getReturnType() ? HeaderEmitter::to_cpp_type(fn.getReturnType()) : "void";
    std::string params_str;
    for (size_t i = 0; i < fn.getParams().size(); ++i) {
        if (i > 0) params_str += ", ";
        params_str += HeaderEmitter::format_param(fn.getParams()[i], fn.getTypeParams());
    }

    std::ostringstream ss;
    ss << "    " << ret_type << " " << class_name << "::" << fn.getName() << "(" << params_str << ") {\n";
    if (fn.getBody()) {
        if (const auto* block = dynamic_cast<const ast::BlockStmt*>(fn.getBody())) {
            ss << translate_block(block, 2);
        } else {
            ss << translate_statement(fn.getBody(), 2);
        }
    }
    ss << "    }";
    return ss.str();
}

std::string CppEmitter::translate_ir_function(const ir::Function& fn) const {
    std::string ret_type = HeaderEmitter::to_cpp_type(fn.get_return_type());
    std::string params_str;
    for (size_t i = 0; i < fn.get_parameters().size(); ++i) {
        if (i > 0) params_str += ", ";
        const auto& p = fn.get_parameters()[i];
        std::string pt = HeaderEmitter::to_cpp_type(p.type);
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

    std::string fn_name = (fn.get_name() == "main") ? "_keet_main" : fn.get_name();
    std::ostringstream ss;
    ss << "    " << ret_type << " " << fn_name << "(" << params_str << ") {\n";

    for (size_t b_idx = 0; b_idx < fn.get_blocks().size(); ++b_idx) {
        const auto& bb = fn.get_blocks()[b_idx];
        if (fn.get_blocks().size() > 1 && b_idx > 0) {
            ss << "    " << bb->get_label() << ":;\n";
        }
        for (const auto& inst : bb->get_instructions()) {
            if (!inst) continue;
            switch (inst->get_opcode()) {
            case ir::Opcode::Alloca: {
                const auto* al = dynamic_cast<const ir::AllocaInst*>(inst.get());
                std::string t = HeaderEmitter::to_cpp_type(al->get_type());
                ss << "        " << t << " " << al->get_name() << "{};\n";
                break;
            }
            case ir::Opcode::Load: {
                const auto* ld = dynamic_cast<const ir::LoadInst*>(inst.get());
                ss << "        auto " << to_cpp_ir_operand(ld->get_dest()) << " = " << to_cpp_ir_operand(ld->get_src()) << ";\n";
                break;
            }
            case ir::Opcode::Store: {
                const auto* st = dynamic_cast<const ir::StoreInst*>(inst.get());
                ss << "        " << to_cpp_ir_operand(st->get_dest()) << " = " << to_cpp_ir_operand(st->get_val()) << ";\n";
                break;
            }
            case ir::Opcode::Assign: {
                const auto* asg = dynamic_cast<const ir::AssignInst*>(inst.get());
                ss << "        " << to_cpp_ir_operand(asg->get_dest()) << " = " << to_cpp_ir_operand(asg->get_val()) << ";\n";
                break;
            }
            case ir::Opcode::Binary: {
                const auto* bin = dynamic_cast<const ir::BinaryInst*>(inst.get());
                std::string op_str;
                switch (bin->get_op()) {
                case ir::BinaryOpKind::Add: op_str = "+"; break;
                case ir::BinaryOpKind::Sub: op_str = "-"; break;
                case ir::BinaryOpKind::Mul: op_str = "*"; break;
                case ir::BinaryOpKind::Div: op_str = "/"; break;
                case ir::BinaryOpKind::Mod: op_str = "%"; break;
                case ir::BinaryOpKind::Eq: op_str = "=="; break;
                case ir::BinaryOpKind::Neq: op_str = "!="; break;
                case ir::BinaryOpKind::Lt: op_str = "<"; break;
                case ir::BinaryOpKind::Lte: op_str = "<="; break;
                case ir::BinaryOpKind::Gt: op_str = ">"; break;
                case ir::BinaryOpKind::Gte: op_str = ">="; break;
                case ir::BinaryOpKind::LogicAnd: op_str = "&&"; break;
                case ir::BinaryOpKind::LogicOr: op_str = "||"; break;
                case ir::BinaryOpKind::BitAnd: op_str = "&"; break;
                case ir::BinaryOpKind::BitOr: op_str = "|"; break;
                case ir::BinaryOpKind::BitXor: op_str = "^"; break;
                case ir::BinaryOpKind::Shl: op_str = "<<"; break;
                case ir::BinaryOpKind::Shr: op_str = ">>"; break;
                }
                ss << "        auto " << to_cpp_ir_operand(bin->get_dest()) << " = "
                   << to_cpp_ir_operand(bin->get_lhs()) << " " << op_str << " " << to_cpp_ir_operand(bin->get_rhs()) << ";\n";
                break;
            }
            case ir::Opcode::Unary: {
                const auto* un = dynamic_cast<const ir::UnaryInst*>(inst.get());
                std::string op_str;
                switch (un->get_op()) {
                case ir::UnaryOpKind::Neg: op_str = "-"; break;
                case ir::UnaryOpKind::Not: op_str = "!"; break;
                case ir::UnaryOpKind::BitNot: op_str = "~"; break;
                }
                ss << "        auto " << to_cpp_ir_operand(un->get_dest()) << " = "
                   << op_str << to_cpp_ir_operand(un->get_val()) << ";\n";
                break;
            }
            case ir::Opcode::Return: {
                const auto* ret = dynamic_cast<const ir::ReturnInst*>(inst.get());
                if (ret->has_value()) {
                    ss << "        return " << to_cpp_ir_operand(*ret->get_value()) << ";\n";
                } else {
                    ss << "        return;\n";
                }
                break;
            }
            case ir::Opcode::Throw: {
                const auto* th = dynamic_cast<const ir::ThrowInst*>(inst.get());
                ss << "        throw " << to_cpp_ir_operand(th->get_error()) << ";\n";
                break;
            }
            case ir::Opcode::Branch: {
                const auto* br = dynamic_cast<const ir::BranchInst*>(inst.get());
                if (br->get_target()) {
                    ss << "        goto " << br->get_target()->get_label() << ";\n";
                }
                break;
            }
            case ir::Opcode::CondBranch: {
                const auto* cbr = dynamic_cast<const ir::CondBranchInst*>(inst.get());
                ss << "        if (" << to_cpp_ir_operand(cbr->get_condition()) << ") {\n";
                if (cbr->get_true_target()) {
                    ss << "            goto " << cbr->get_true_target()->get_label() << ";\n";
                }
                ss << "        } else {\n";
                if (cbr->get_false_target()) {
                    ss << "            goto " << cbr->get_false_target()->get_label() << ";\n";
                }
                ss << "        }\n";
                break;
            }
            case ir::Opcode::Call: {
                const auto* call = dynamic_cast<const ir::CallInst*>(inst.get());
                std::string callee = call->get_callee();
                if (!callee.empty() && callee.front() == '@') {
                    callee = callee.substr(1);
                }
                std::string call_expr;
                if (callee.rfind("keet.io.", 0) == 0) {
                    std::string func_name = callee.substr(8);
                    call_expr = "::keet::io::" + func_name + "(";
                } else if (callee.rfind("keet.fs.", 0) == 0) {
                    std::string func_name = callee.substr(8);
                    call_expr = "::keet::fs::" + func_name + "(";
                } else {
                    call_expr = callee + "(";
                }
                for (size_t i = 0; i < call->get_args().size(); ++i) {
                    if (i > 0) call_expr += ", ";
                    call_expr += to_cpp_ir_operand(call->get_args()[i]);
                }
                call_expr += ")";
                if (call->get_dest().has_value()) {
                    ss << "        auto " << to_cpp_ir_operand(*call->get_dest()) << " = " << call_expr << ";\n";
                } else {
                    ss << "        " << call_expr << ";\n";
                }
                break;
            }
            case ir::Opcode::MethodCall: {
                const auto* mcall = dynamic_cast<const ir::MethodCallInst*>(inst.get());
                std::string call_expr = to_cpp_ir_operand(mcall->get_receiver()) + "." + mcall->get_method() + "(";
                for (size_t i = 0; i < mcall->get_args().size(); ++i) {
                    if (i > 0) call_expr += ", ";
                    call_expr += to_cpp_ir_operand(mcall->get_args()[i]);
                }
                call_expr += ")";
                if (mcall->get_dest().has_value()) {
                    ss << "        auto " << to_cpp_ir_operand(*mcall->get_dest()) << " = " << call_expr << ";\n";
                } else {
                    ss << "        " << call_expr << ";\n";
                }
                break;
            }
            default:
                break;
            }
        }
    }
    ss << "    }";
    return ss.str();
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

std::string CppEmitter::emit_function_definitions() const {
    if (function_definitions_.empty()) {
        return "";
    }
    std::ostringstream ss;
    for (size_t i = 0; i < function_definitions_.size(); ++i) {
        if (i > 0) ss << "\n";
        ss << function_definitions_[i] << "\n";
    }
    return ss.str();
}

bool CppEmitter::is_root_module() const noexcept {
    if (manual_root_override_) {
        return options_.is_root_module;
    }
    return options_.is_root_module || has_main_function_;
}

std::string CppEmitter::emit_main_wrapper() const {
    std::ostringstream ss;
    ss << "int main(int argc, char* argv[]) {\n";
    ss << "    try {\n";

    std::string ns = get_namespace_name();
    if (main_has_args_ || !has_main_function_) {
        ss << "        std::vector<std::string> args;\n";
        ss << "        args.reserve(argc);\n";
        ss << "        for (int i = 0; i < argc; ++i) {\n";
        ss << "            args.emplace_back(argv[i]);\n";
        ss << "        }\n";
    }

    if (has_main_function_) {
        std::string call_str = ns + "::_keet_main(";
        if (main_has_args_) {
            call_str += "std::move(args)";
        }
        call_str += ");";

        if (main_returns_int_) {
            ss << "        return static_cast<int>(" << call_str.substr(0, call_str.size() - 1) << ");\n";
        } else {
            ss << "        " << call_str << "\n";
            ss << "        return 0;\n";
        }
    } else {
        ss << "        return 0;\n";
    }

    ss << "    } catch (const keet::error& err) {\n";
    ss << "        std::cerr << \"Uncaught error: \" << err.toString() << \"\\n\";\n";
    ss << "        return 1;\n";
    ss << "    } catch (const std::exception& ex) {\n";
    ss << "        std::cerr << \"Uncaught native C++ exception: \" << ex.what() << \"\\n\";\n";
    ss << "        return 1;\n";
    ss << "    }\n";
    ss << "}\n";

    return ss.str();
}

std::string CppEmitter::emit_namespace_body() const {
    std::ostringstream ss;
    bool has_prev = false;

    std::string allocs = emit_storage_allocations();
    if (!allocs.empty()) {
        ss << allocs;
        has_prev = true;
    }

    std::string funcs = emit_function_definitions();
    if (!funcs.empty()) {
        if (has_prev) ss << "\n";
        ss << funcs;
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

    if (is_root_module()) {
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
