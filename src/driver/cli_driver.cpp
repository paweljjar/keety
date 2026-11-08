#include <keety/driver/cli_driver.hpp>
#include <keety/driver/project_root.hpp>

#include <keety/emitter/cpp_emitter.hpp>
#include <keety/emitter/header_emitter.hpp>
#include <keety/ir/ast_lowerer.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>
#include <keety/resolver/cycle_detector.hpp>
#include <keety/resolver/module_graph.hpp>
#include <keety/resolver/module_path_resolver.hpp>
#include <keety/resolver/topological_sorter.hpp>
#include <keety/sema/const_view_checker.hpp>
#include <keety/sema/universe.hpp>
#include <keety/typechecker/type_checker.hpp>
#include <keety/fmt/formatter.hpp>
#include <keety/fmt/formatter_options.hpp>
#include <keety/lint/lint_config.hpp>
#include <keety/lint/lint_engine.hpp>
#include <keety/version.hpp>

#include <keet/json.hpp>
#include <keet/test.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <io.h>
#include <windows.h>
#endif

namespace keety::driver {

namespace {

std::filesystem::path get_executable_path() {
    std::error_code ec;
#if defined(__APPLE__)
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return std::filesystem::weakly_canonical(path, ec);
    }
    std::vector<char> dynamic_path(size);
    if (_NSGetExecutablePath(dynamic_path.data(), &size) == 0) {
        return std::filesystem::weakly_canonical(dynamic_path.data(), ec);
    }
#elif defined(_WIN32)
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (len > 0) {
        return std::filesystem::weakly_canonical(std::filesystem::path(path), ec);
    }
#elif defined(__linux__)
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        return std::filesystem::weakly_canonical(path, ec);
    }
#endif
    return {};
}

std::string quote_arg(const std::string& arg) {
    if (arg.empty()) {
        return "\"\"";
    }
    bool needs_quote = false;
    for (char c : arg) {
        if (c == ' ' || c == '\t' || c == '\"' || c == '(' || c == ')') {
            needs_quote = true;
            break;
        }
    }
    if (!needs_quote) {
        return arg;
    }
    std::string result = "\"";
    for (char c : arg) {
        if (c == '\"') {
            result += "\\\"";
        } else {
            result += c;
        }
    }
    result += "\"";
    return result;
}

std::filesystem::path find_runtime_directory() {
    // 1. Explicit environment variable override
    const char* env_dir = std::getenv("KEETY_RUNTIME_DIR");
    if (env_dir && std::filesystem::exists(std::filesystem::path(env_dir) / "keet" / "runtime.hpp")) {
        return env_dir;
    }

    // 2. Relative to running executable (resolves symlinks)
    auto exe_path = get_executable_path();
    if (!exe_path.empty()) {
        std::filesystem::path curr = exe_path.parent_path();
        for (int i = 0; i < 6; ++i) {
            std::filesystem::path candidate = curr / "runtime";
            if (std::filesystem::exists(candidate / "keet" / "runtime.hpp")) {
                return candidate;
            }
            std::filesystem::path inc_candidate = curr / "include";
            if (std::filesystem::exists(inc_candidate / "keet" / "runtime.hpp")) {
                return inc_candidate;
            }
            if (!curr.has_parent_path() || curr.parent_path() == curr) {
                break;
            }
            curr = curr.parent_path();
        }
    }

#if defined(KEETY_DEFAULT_RUNTIME_DIR)
    // 3. Compile-time default runtime directory from build tree
    std::filesystem::path default_dir = KEETY_DEFAULT_RUNTIME_DIR;
    if (std::filesystem::exists(default_dir / "keet" / "runtime.hpp")) {
        return default_dir;
    }
#endif

    // 4. Relative to current working directory
    std::filesystem::path curr = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i) {
        std::filesystem::path candidate = curr / "runtime";
        if (std::filesystem::exists(candidate / "keet" / "runtime.hpp")) {
            return candidate;
        }
        if (!curr.has_parent_path() || curr.parent_path() == curr) {
            break;
        }
        curr = curr.parent_path();
    }
    return "";
}

void dump_indent(std::ostream& os, int indent) {
    for (int i = 0; i < indent; ++i) {
        os << "  ";
    }
}

void dump_type(const ast::TypeAnnotation* type, std::ostream& os) {
    if (!type) {
        os << "<inferred>";
        return;
    }
    os << emitter::HeaderEmitter::to_cpp_type(type);
}

void dump_expr(const ast::Expr* expr, std::ostream& os, int indent);
void dump_stmt(const ast::Stmt* stmt, std::ostream& os, int indent);
void dump_decl(const ast::Decl* decl, std::ostream& os, int indent);

void dump_decl(const ast::Decl* decl, std::ostream& os, int indent) {
    if (!decl) return;
    dump_indent(os, indent);

    if (const auto* v = dynamic_cast<const ast::VarDecl*>(decl)) {
        os << (v->isExported() ? "ExportVarDecl " : "VarDecl ") << v->getName() << ": ";
        dump_type(v->getType(), os);
        os << "\n";
        if (v->getInitializer()) {
            dump_expr(v->getInitializer(), os, indent + 1);
        }
    } else if (const auto* c = dynamic_cast<const ast::ConstDecl*>(decl)) {
        os << (c->isExported() ? "ExportConstDecl " : "ConstDecl ") << c->getName() << ": ";
        dump_type(c->getType(), os);
        os << "\n";
        if (c->getInitializer()) {
            dump_expr(c->getInitializer(), os, indent + 1);
        }
    } else if (const auto* def = dynamic_cast<const ast::DefineDecl*>(decl)) {
        os << (def->isExported() ? "ExportDefineDecl " : "DefineDecl ") << def->getName() << ": ";
        dump_type(def->getType(), os);
        os << "\n";
        if (def->getValue()) {
            dump_expr(def->getValue(), os, indent + 1);
        }
    } else if (const auto* f = dynamic_cast<const ast::FnDecl*>(decl)) {
        os << (f->isExported() ? "ExportFnDecl " : "FnDecl ") << f->getName() << "(";
        for (size_t i = 0; i < f->getParams().size(); ++i) {
            if (i > 0) os << ", ";
            if (f->getParams()[i].isMut) os << "mut ";
            os << f->getParams()[i].name << ": ";
            dump_type(f->getParams()[i].type.get(), os);
        }
        os << "): ";
        dump_type(f->getReturnType(), os);
        os << "\n";
        if (f->getBody()) {
            dump_stmt(f->getBody(), os, indent + 1);
        }
    } else if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl)) {
        os << (cls->isExported() ? "ExportClassDecl " : "ClassDecl ") << cls->getName() << "\n";
        for (const auto& field : cls->getFields()) {
            dump_indent(os, indent + 1);
            os << "Field " << field.name << ": ";
            dump_type(field.type.get(), os);
            os << "\n";
        }
        for (const auto& method : cls->getMethods()) {
            dump_decl(method.get(), os, indent + 1);
        }
    } else if (const auto* enm = dynamic_cast<const ast::EnumDecl*>(decl)) {
        os << (enm->isExported() ? "ExportEnumDecl " : "EnumDecl ") << enm->getName() << "\n";
        for (const auto& member : enm->getMembers()) {
            dump_indent(os, indent + 1);
            os << "EnumMember " << member.name << "\n";
        }
    } else if (const auto* imp = dynamic_cast<const ast::ImportDecl*>(decl)) {
        os << "ImportDecl \"" << imp->getModulePath() << "\"\n";
    } else if (const auto* err = dynamic_cast<const ast::ErrTypeDecl*>(decl)) {
        os << (err->isExported() ? "ExportErrTypeDecl " : "ErrTypeDecl ") << err->getName() << "\n";
    } else {
        os << "Decl\n";
    }
}

void dump_stmt(const ast::Stmt* stmt, std::ostream& os, int indent) {
    if (!stmt) return;
    dump_indent(os, indent);

    if (const auto* b = dynamic_cast<const ast::BlockStmt*>(stmt)) {
        os << "BlockStmt\n";
        for (const auto& s : b->getStatements()) {
            dump_stmt(s.get(), os, indent + 1);
        }
    } else if (const auto* d = dynamic_cast<const ast::DeclStmt*>(stmt)) {
        os << "DeclStmt\n";
        dump_decl(d->getDeclaration(), os, indent + 1);
    } else if (const auto* e = dynamic_cast<const ast::ExprStmt*>(stmt)) {
        os << "ExprStmt\n";
        dump_expr(e->getExpression(), os, indent + 1);
    } else if (const auto* r = dynamic_cast<const ast::ReturnStmt*>(stmt)) {
        os << "ReturnStmt\n";
        if (r->hasValue()) {
            dump_expr(r->getValue(), os, indent + 1);
        }
    } else if (const auto* i = dynamic_cast<const ast::IfStmt*>(stmt)) {
        os << "IfStmt\n";
        if (i->getCondition()) {
            dump_indent(os, indent + 1);
            os << "Condition:\n";
            dump_expr(i->getCondition(), os, indent + 2);
        }
        if (i->getThenBranch()) {
            dump_indent(os, indent + 1);
            os << "Then:\n";
            dump_stmt(i->getThenBranch(), os, indent + 2);
        }
        if (i->getElseBranch()) {
            dump_indent(os, indent + 1);
            os << "Else:\n";
            dump_stmt(i->getElseBranch(), os, indent + 2);
        }
    } else if (const auto* w = dynamic_cast<const ast::WhileStmt*>(stmt)) {
        os << "WhileStmt\n";
        if (w->getCondition()) {
            dump_expr(w->getCondition(), os, indent + 1);
        }
        if (w->getBody()) {
            dump_stmt(w->getBody(), os, indent + 1);
        }
    } else if (const auto* f = dynamic_cast<const ast::ForInStmt*>(stmt)) {
        os << "ForInStmt " << f->getValueName() << "\n";
        if (f->getCollection()) {
            dump_expr(f->getCollection(), os, indent + 1);
        }
        if (f->getBody()) {
            dump_stmt(f->getBody(), os, indent + 1);
        }
    } else if (dynamic_cast<const ast::BreakStmt*>(stmt)) {
        os << "BreakStmt\n";
    } else if (dynamic_cast<const ast::ContinueStmt*>(stmt)) {
        os << "ContinueStmt\n";
    } else {
        os << "Stmt\n";
    }
}

void dump_expr(const ast::Expr* expr, std::ostream& os, int indent) {
    if (!expr) return;
    dump_indent(os, indent);

    if (const auto* l = dynamic_cast<const ast::LiteralExpr*>(expr)) {
        os << "LiteralExpr(" << l->getRawText() << ")\n";
    } else if (const auto* id = dynamic_cast<const ast::IdentExpr*>(expr)) {
        os << "IdentExpr(" << id->getName() << ")\n";
    } else if (const auto* b = dynamic_cast<const ast::BinaryExpr*>(expr)) {
        os << "BinaryExpr(" << lexer::getTokenKindName(b->getOp()) << ")\n";
        dump_expr(b->getLeft(), os, indent + 1);
        dump_expr(b->getRight(), os, indent + 1);
    } else if (const auto* u = dynamic_cast<const ast::UnaryExpr*>(expr)) {
        os << "UnaryExpr(" << lexer::getTokenKindName(u->getOp()) << ")\n";
        dump_expr(u->getOperand(), os, indent + 1);
    } else if (const auto* c = dynamic_cast<const ast::CallExpr*>(expr)) {
        os << "CallExpr\n";
        dump_expr(c->getCallee(), os, indent + 1);
        for (const auto& arg : c->getArgs()) {
            dump_expr(arg.value.get(), os, indent + 1);
        }
    } else if (const auto* m = dynamic_cast<const ast::MemberExpr*>(expr)) {
        os << "MemberExpr(." << m->getMemberName() << ")\n";
        dump_expr(m->getObject(), os, indent + 1);
    } else if (const auto* idx = dynamic_cast<const ast::IndexExpr*>(expr)) {
        os << "IndexExpr\n";
        dump_expr(idx->getContainer(), os, indent + 1);
        dump_expr(idx->getIndex(), os, indent + 1);
    } else if (const auto* arr = dynamic_cast<const ast::ArrayLitExpr*>(expr)) {
        os << "ArrayLitExpr\n";
        for (const auto& elem : arr->getElements()) {
            dump_expr(elem.get(), os, indent + 1);
        }
    } else if (const auto* t = dynamic_cast<const ast::TernaryExpr*>(expr)) {
        os << "TernaryExpr\n";
        dump_expr(t->getCondition(), os, indent + 1);
        dump_expr(t->getThenExpr(), os, indent + 1);
        dump_expr(t->getElseExpr(), os, indent + 1);
    } else {
        os << "Expr\n";
    }
}

std::string dump_ast(const parser::ModuleAST& ast) {
    std::ostringstream ss;
    ss << "ModuleAST \"" << ast.moduleName << "\"\n";
    for (const auto& decl : ast.declarations) {
        dump_decl(decl.get(), ss, 1);
    }
    return ss.str();
}

bool parse_opt_level(std::string_view val, OptimizationLevel& level) {
    if (val == "0" || val == "O0") {
        level = OptimizationLevel::O0;
        return true;
    }
    if (val == "1" || val == "O1") {
        level = OptimizationLevel::O1;
        return true;
    }
    if (val == "2" || val == "O2") {
        level = OptimizationLevel::O2;
        return true;
    }
    if (val == "3" || val == "O3") {
        level = OptimizationLevel::O3;
        return true;
    }
    if (val == "s" || val == "Os") {
        level = OptimizationLevel::Os;
        return true;
    }
    if (val == "z" || val == "Oz") {
        level = OptimizationLevel::Oz;
        return true;
    }
    return false;
}

bool apply_lint_fixes(std::string& source, const std::vector<keety::lint::LintDiagnostic>& diagnostics) {
    struct EditWithSpan {
        keety::diag::SourceSpan span;
        std::string replacement;
        std::string ruleCode;
    };
    std::vector<EditWithSpan> edits;
    for (const auto& d : diagnostics) {
        if (d.quickFix && d.quickFix->span.isValid()) {
            edits.push_back({d.quickFix->span, d.quickFix->replacement, d.ruleCode});
        }
    }
    if (edits.empty()) {
        return false;
    }
    // Sort descending by start offset, then end offset
    std::stable_sort(edits.begin(), edits.end(), [](const EditWithSpan& a, const EditWithSpan& b) {
        if (a.span.start.offset != b.span.start.offset) {
            return a.span.start.offset > b.span.start.offset;
        }
        return a.span.end.offset > b.span.end.offset;
    });

    bool applied = false;
    size_t lastStart = std::string::npos;
    for (const auto& edit : edits) {
        uint32_t start = edit.span.start.offset;
        uint32_t end = edit.span.end.offset;
        if (end <= source.size() && start <= end) {
            if (lastStart == std::string::npos || end <= lastStart) {
                size_t len = end - start;
                if (edit.ruleCode == "L001" && edit.replacement.empty()) {
                    size_t lineStart = start;
                    while (lineStart > 0 && source[lineStart - 1] != '\n' && (source[lineStart - 1] == ' ' || source[lineStart - 1] == '\t')) {
                        --lineStart;
                    }
                    if (lineStart == 0 || source[lineStart - 1] == '\n') {
                        len += (start - lineStart);
                        start = lineStart;
                    }
                    if (start + len < source.size() && source[start + len] == '\r' && start + len + 1 < source.size() && source[start + len + 1] == '\n') {
                        len += 2;
                    } else if (start + len < source.size() && source[start + len] == '\n') {
                        len += 1;
                    }
                }
                source.replace(start, len, edit.replacement);
                lastStart = start;
                applied = true;
            }
        }
    }
    return applied;
}

} // namespace

std::string_view to_string(CliCommand cmd) noexcept {
    switch (cmd) {
    case CliCommand::Check:   return "check";
    case CliCommand::Emit:    return "emit";
    case CliCommand::Build:   return "build";
    case CliCommand::Run:     return "run";
    case CliCommand::Test:    return "test";
    case CliCommand::Fmt:     return "fmt";
    case CliCommand::Lint:    return "lint";
    case CliCommand::Version: return "version";
    case CliCommand::Help:    return "help";
    case CliCommand::Unknown: return "unknown";
    }
    return "unknown";
}

std::string_view to_string(EmitTarget target) noexcept {
    switch (target) {
    case EmitTarget::None: return "none";
    case EmitTarget::Cpp:  return "cpp";
    case EmitTarget::Hpp:  return "hpp";
    case EmitTarget::Ast:  return "ast";
    case EmitTarget::Ir:   return "ir";
    }
    return "none";
}

CliOptions CliDriver::parse_args(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr) {
            args.emplace_back(argv[i]);
        }
    }
    return parse_args(args);
}

CliOptions CliDriver::parse_args(const std::vector<std::string>& args) {
    CliOptions options;

    if (args.empty()) {
        options.command = CliCommand::Help;
        return options;
    }

    size_t idx = 0;
    std::string first = args[0];

    if (first == "-h" || first == "--help" || first == "help") {
        options.command = CliCommand::Help;
        return options;
    }
    if (first == "-V" || first == "--version" || first == "version") {
        options.command = CliCommand::Version;
        return options;
    }

    if (first == "check") {
        options.command = CliCommand::Check;
        idx = 1;
    } else if (first == "emit") {
        options.command = CliCommand::Emit;
        idx = 1;
    } else if (first == "build") {
        options.command = CliCommand::Build;
        idx = 1;
    } else if (first == "run") {
        options.command = CliCommand::Run;
        idx = 1;
    } else if (first == "test") {
        options.command = CliCommand::Test;
        idx = 1;
    } else if (first == "fmt") {
        options.command = CliCommand::Fmt;
        idx = 1;
    } else if (first == "lint") {
        options.command = CliCommand::Lint;
        idx = 1;
    }

    bool fmt_write_explicit = false;

    for (; idx < args.size(); ++idx) {
        const std::string& arg = args[idx];

        if (arg == "--") {
            // All remaining args are forwarded to execution
            for (size_t r = idx + 1; r < args.size(); ++r) {
                options.run_args.push_back(args[r]);
            }
            break;
        }

        if (arg == "-v" || arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "--verbose-cpp") {
            options.verbose_cpp = true;
        } else if (arg == "-g" || arg == "--debug") {
            options.debug_info = true;
        } else if (arg == "--no-line-directives") {
            options.emit_line_directives = false;
        } else if (arg == "--benchmarks") {
            options.test_benchmarks = true;
        } else if (arg == "--conformance") {
            options.test_conformance = true;
        } else if (arg == "-f" || arg == "--filter") {
            if (idx + 1 < args.size()) {
                options.test_filter = args[++idx];
            }
        } else if (arg.rfind("--filter=", 0) == 0) {
            options.test_filter = arg.substr(9);
        } else if (arg.rfind("-f=", 0) == 0) {
            options.test_filter = arg.substr(3);
        } else if (arg == "--fail-fast") {
            options.test_fail_fast = true;
        } else if (arg == "--json") {
            options.test_json = true;
            options.diagnostic_format = DiagnosticFormat::Json;
        } else if (arg == "--tap") {
            options.test_tap = true;
        } else if (arg == "-h" || arg == "--help") {
            options.command = CliCommand::Help;
            return options;
        } else if (arg == "-V" || arg == "--version") {
            options.command = CliCommand::Version;
            return options;
        } else if (arg == "--no-color") {
            options.color_mode = diag::ColorMode::Never;
        } else if (arg == "--color=always") {
            options.color_mode = diag::ColorMode::Always;
        } else if (arg == "--color=never") {
            options.color_mode = diag::ColorMode::Never;
        } else if (arg == "--color=auto") {
            options.color_mode = diag::ColorMode::Auto;
        } else if (arg == "--diagnostic-format=json" || arg == "--json-diagnostics") {
            options.diagnostic_format = DiagnosticFormat::Json;
        } else if (arg == "--diagnostic-format=human") {
            options.diagnostic_format = DiagnosticFormat::Human;
        } else if (arg == "--wrapv") {
            options.wrapv = true;
        } else if (arg == "--cpp") {
            options.emit_target = EmitTarget::Cpp;
        } else if (arg == "--hpp") {
            options.emit_target = EmitTarget::Hpp;
        } else if (arg == "--ast") {
            options.emit_target = EmitTarget::Ast;
        } else if (arg == "--ir") {
            options.emit_target = EmitTarget::Ir;
        } else if (arg == "--output-dir") {
            if (idx + 1 < args.size()) {
                options.output_dir = args[++idx];
                options.output_is_dir = true;
            }
        } else if (arg.rfind("--output-dir=", 0) == 0) {
            options.output_dir = arg.substr(13);
            options.output_is_dir = true;
        } else if (arg == "-o" || arg == "--output") {
            if (idx + 1 < args.size()) {
                options.output_dir = args[++idx];
            }
        } else if (arg.rfind("--output=", 0) == 0) {
            options.output_dir = arg.substr(9);
        } else if (arg.rfind("-o=", 0) == 0) {
            options.output_dir = arg.substr(3);
        } else if (arg == "--cxx") {
            if (idx + 1 < args.size()) {
                options.cxx_compiler = args[++idx];
            }
        } else if (arg.rfind("--cxx=", 0) == 0) {
            options.cxx_compiler = arg.substr(6);
        } else if (arg == "-O0") {
            options.opt_level = OptimizationLevel::O0;
        } else if (arg == "-O1") {
            options.opt_level = OptimizationLevel::O1;
        } else if (arg == "-O2") {
            options.opt_level = OptimizationLevel::O2;
        } else if (arg == "-O3") {
            options.opt_level = OptimizationLevel::O3;
        } else if (arg == "-Os") {
            options.opt_level = OptimizationLevel::Os;
        } else if (arg == "-Oz") {
            options.opt_level = OptimizationLevel::Oz;
        } else if ((arg == "-O" || arg == "--opt") && idx + 1 < args.size()) {
            parse_opt_level(args[++idx], options.opt_level);
        } else if (arg.rfind("--opt=", 0) == 0) {
            parse_opt_level(arg.substr(6), options.opt_level);
        } else if (arg.rfind("-O=", 0) == 0) {
            parse_opt_level(arg.substr(3), options.opt_level);
        } else if (arg == "--check") {
            if (options.command == CliCommand::Fmt) {
                options.fmt_check = true;
            } else if (options.command == CliCommand::Lint) {
                options.lint_check = true;
            }
        } else if (arg == "--diff" && options.command == CliCommand::Fmt) {
            options.fmt_diff = true;
        } else if (arg == "--fix" && options.command == CliCommand::Lint) {
            options.lint_fix = true;
        } else if (arg == "--strict" && options.command == CliCommand::Lint) {
            options.lint_strict = true;
        } else if ((arg == "-w" || arg == "--write") && options.command == CliCommand::Fmt) {
            options.fmt_write = true;
            fmt_write_explicit = true;
        } else if (arg == "--no-write" && options.command == CliCommand::Fmt) {
            options.fmt_write = false;
            fmt_write_explicit = true;
        } else if ((arg == "-r" || arg == "--recursive") && options.command == CliCommand::Fmt) {
            options.fmt_recursive = true;
        } else if (arg == "--no-recursive" && options.command == CliCommand::Fmt) {
            options.fmt_recursive = false;
        } else if (arg == "-") {
            if (options.command == CliCommand::Fmt) {
                options.fmt_stdin = true;
            } else if (options.command == CliCommand::Lint) {
                options.lint_stdin = true;
            }
        } else if (!arg.empty() && arg[0] != '-') {
            if (options.command == CliCommand::Fmt) {
                options.fmt_paths.emplace_back(arg);
                if (options.input_file.empty()) {
                    options.input_file = arg;
                }
            } else if (options.command == CliCommand::Lint) {
                options.lint_paths.emplace_back(arg);
                if (options.input_file.empty()) {
                    options.input_file = arg;
                }
            } else if (options.input_file.empty()) {
                options.input_file = arg;
            }
        }
    }

    if (options.command == CliCommand::Fmt) {
        if (!fmt_write_explicit) {
            if (options.fmt_check || options.fmt_diff) {
                options.fmt_write = false;
            } else {
                options.fmt_write = true;
            }
        }
        if (options.fmt_paths.empty() && !options.fmt_stdin) {
#if defined(_WIN32)
            bool is_tty = _isatty(_fileno(stdin)) != 0;
#else
            bool is_tty = isatty(fileno(stdin)) != 0;
#endif
            if (!is_tty && !options.fmt_check && !options.fmt_diff) {
                options.fmt_stdin = true;
            }
        }
    }

    if (options.command == CliCommand::Lint) {
        if (options.lint_paths.empty() && !options.lint_stdin) {
#if defined(_WIN32)
            bool is_tty = _isatty(_fileno(stdin)) != 0;
#else
            bool is_tty = isatty(fileno(stdin)) != 0;
#endif
            if (!is_tty && !options.lint_check && !options.lint_fix) {
                options.lint_stdin = true;
            }
        }
    }

    if (options.command == CliCommand::Unknown && !options.input_file.empty()) {
        options.command = CliCommand::Check;
    }

    return options;
}

void CliDriver::print_version(std::ostream& out) {
    out << "keety - KeetScript compiler v" << keety::getVersionString() << "\n";
}

void CliDriver::print_help(std::ostream& out) {
    out << "Usage: keety <command> [options] [file.keet]\n\n"
        << "Commands:\n"
        << "  check <file>                         Fast IDE-grade semantic and type validation\n"
        << "  emit <file> (--cpp|--hpp|--ast|--ir) Emit generated C++ or intermediate representations\n"
        << "  build <file>                         Compile KeetScript source into a native binary\n"
        << "  run <file> [-- args]                 Compile and immediately execute the target binary\n"
        << "  test [path] [options]                Auto-discover and run unit tests, or run benchmarks/conformance\n"
        << "  fmt [paths...] [options]             Format KeetScript source files according to canonical style\n"
        << "  lint [paths...] [options]            Analyze KeetScript source files for bugs, style, and hygiene\n"
        << "  version                              Display keety version information\n"
        << "  help                                 Display this help message\n\n"
        << "Options:\n"
        << "  --check                              Check code format or lint without writing; exit 1 if violations found\n"
        << "  --diff                               Display unified diffs of formatting changes\n"
        << "  -w, --write                          Overwrite formatted files in place (default)\n"
        << "  --fix                                Automatically apply quick-fixes for lint violations in place\n"
        << "  --strict                             Treat all lint warnings as errors\n"
        << "  -o, --output-dir <dir>               Target output directory for emitted sources or binaries\n"
        << "  --cxx <compiler>                     Explicit native C++ compiler binary (clang++, g++, cl.exe)\n"
        << "  -O, --opt <0|1|2|3|s|z>              Optimization level (default: 3)\n"
        << "  --wrapv                              Enforce two's-complement overflow semantics\n"
        << "  -v, --verbose                        Enable verbose diagnostics and phase timings\n"
        << "  -f, --filter <pattern>               Filter test cases by name or suite\n"
        << "  --json                               Emit structured JSON test summary to standard output\n"
        << "  --tap                                Emit Test Anything Protocol (TAP version 13) output\n"
        << "  --fail-fast                          Abort test execution on the first failure\n"
        << "  --benchmarks                         Run performance benchmarks suite\n"
        << "  --conformance                        Run conformance test suite\n"
        << "  --no-color                           Disable ANSI color escape sequences in output\n"
        << "  --no-line-directives                 Suppress #line source mapping directives in emitted C++\n"
        << "  -g, --debug                          Emit native debug symbols (-g / /Z7)\n"
        << "  --diagnostic-format <human|json>     Format diagnostics as human-readable terminal text or JSON array\n"
        << "  --json-diagnostics                   Shorthand for --diagnostic-format=json\n"
        << "  -h, --help                           Display usage documentation\n"
        << "  -V, --version                        Display version number\n";
}

int CliDriver::execute_check(const CliOptions& options, std::ostream& out, std::ostream& err) {
    const bool is_json = (options.diagnostic_format == DiagnosticFormat::Json);

    if (options.input_file.empty()) {
        if (is_json) {
            out << "[{\"code\":\"E0000\",\"severity\":\"error\",\"message\":\"'keety check' requires a target input file.\"}]\n";
        } else {
            err << "error: 'keety check' requires a target input file.\n"
                << "usage: keety check <file.keet>\n";
        }
        return 1;
    }

    if (!std::filesystem::exists(options.input_file)) {
        if (is_json) {
            out << "[{\"code\":\"E1002\",\"severity\":\"error\",\"message\":\"input file not found: "
                << options.input_file.string() << "\"}]\n";
        } else {
            err << "error: input file not found: " << options.input_file.string() << "\n";
        }
        return 1;
    }

    diag::SourceManager sm;
    diag::DiagnosticEmitter emitter(sm, options.color_mode);

    std::filesystem::path project_root = findProjectRoot(options.input_file);

    resolver::ModulePathResolver path_resolver(project_root, &emitter);
    resolver::ModuleGraph graph;

    bool graph_ok = graph.buildGraph(options.input_file, sm, path_resolver, &emitter);
    if (!graph_ok || emitter.hasErrors()) {
        if (is_json) {
            emitter.emitJson(out);
        } else {
            for (const auto& d : emitter.getDiagnostics()) {
                err << emitter.format(d);
            }
        }
        return 1;
    }

    // Check for circular dependency cycles
    resolver::CycleDetector cycle_detector(graph, &emitter);
    if (!cycle_detector.detectCycles() || emitter.hasErrors()) {
        if (is_json) {
            emitter.emitJson(out);
        } else {
            for (const auto& d : emitter.getDiagnostics()) {
                err << emitter.format(d);
            }
        }
        return 1;
    }

    // Topologically sort module dependency graph
    resolver::TopologicalSorter sorter(graph);
    std::filesystem::path canonical_entry = std::filesystem::weakly_canonical(options.input_file);
    auto ordered_nodes = sorter.sortNodes(canonical_entry.string());

    if (options.verbose && !is_json) {
        out << "[verbose] Resolved " << ordered_nodes.size() << " module(s) in dependency order:\n";
        for (size_t i = 0; i < ordered_nodes.size(); ++i) {
            out << "[verbose]   [" << (i + 1) << "] " << ordered_nodes[i]->moduleName << "\n";
        }
    }

    auto universe = std::make_shared<sema::Scope>(sema::ScopeKind::Universe);
    sema::registerUniverseSymbols(*universe);

    std::unordered_map<std::string, std::shared_ptr<sema::Scope>> module_scopes;
    module_scopes["<stdlib>/io"] = sema::createStdlibModuleScope("io", universe);
    module_scopes["<stdlib>/fs"] = sema::createStdlibModuleScope("fs", universe);
    module_scopes["<stdlib>/sys"] = sema::createStdlibModuleScope("sys", universe);
    module_scopes["<stdlib>/math"] = sema::createStdlibModuleScope("math", universe);
    module_scopes["<stdlib>/time"] = sema::createStdlibModuleScope("time", universe);
    module_scopes["<stdlib>/json"] = sema::createStdlibModuleScope("json", universe);
    module_scopes["<stdlib>/net"] = sema::createStdlibModuleScope("net", universe);
    module_scopes["<stdlib>/test"] = sema::createStdlibModuleScope("test", universe);
    module_scopes["<stdlib>/crypto"] = sema::createStdlibModuleScope("crypto", universe);
    module_scopes["<stdlib>/http"] = sema::createStdlibModuleScope("http", universe);
    module_scopes["<stdlib>/net.http"] = sema::createStdlibModuleScope("net.http", universe);
    module_scopes["<stdlib>/ui"] = sema::createStdlibModuleScope("ui", universe);

    for (const auto* node : ordered_nodes) {
        if (!node || !node->ast) {
            continue;
        }

        if (options.verbose && !is_json) {
            out << "[verbose] Validating semantics and lowering IR: " << node->moduleName << "\n";
        }

        auto module_scope = std::make_shared<sema::Scope>(sema::ScopeKind::Module, universe);
        for (const auto& edge : node->outgoingEdges) {
            if (edge.toCanonicalPath == "<stdlib>/io" || edge.rawImportPath == "io") {
                auto ioModSym = std::make_shared<sema::Symbol>("io", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(ioModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/fs" || edge.rawImportPath == "fs") {
                auto fsModSym = std::make_shared<sema::Symbol>("fs", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(fsModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/sys" || edge.rawImportPath == "sys") {
                auto sysModSym = std::make_shared<sema::Symbol>("sys", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(sysModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/math" || edge.rawImportPath == "math") {
                auto mathModSym = std::make_shared<sema::Symbol>("math", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(mathModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/time" || edge.rawImportPath == "time") {
                auto timeModSym = std::make_shared<sema::Symbol>("time", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(timeModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/json" || edge.rawImportPath == "json") {
                auto jsonModSym = std::make_shared<sema::Symbol>("json", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(jsonModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/net" || edge.rawImportPath == "net") {
                auto netModSym = std::make_shared<sema::Symbol>("net", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(netModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/test" || edge.rawImportPath == "test") {
                auto testModSym = std::make_shared<sema::Symbol>("test", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(testModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/crypto" || edge.rawImportPath == "crypto") {
                auto cryptoModSym = std::make_shared<sema::Symbol>("crypto", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(cryptoModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/net.http" || edge.toCanonicalPath == "<stdlib>/http" ||
                       edge.rawImportPath == "net.http" || edge.rawImportPath == "http") {
                auto httpModSym = std::make_shared<sema::Symbol>("http", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(httpModSym, false);
            }
            if (edge.toCanonicalPath.rfind("<stdlib>/", 0) != 0) {
                auto it = module_scopes.find(edge.toCanonicalPath);
                if (it != module_scopes.end() && it->second) {
                    for (const auto& [name, sym] : it->second->getSymbols()) {
                        if (sym && sym->isExported()) {
                            if (sym->isVariable()) {
                                auto constView = sema::ConstViewChecker::importExportedSymbol(sym, edge.importSpan);
                                module_scope->define(constView, false);
                            } else {
                                module_scope->define(sym, false);
                            }
                        }
                    }
                }
            }
        }

        typechecker::TypeChecker checker(&emitter);
        checker.checkTypes(*node->ast, module_scope.get());
        module_scopes[node->canonicalPath] = module_scope;

        if (emitter.hasErrors()) {
            if (is_json) {
                emitter.emitJson(out);
            } else {
                for (const auto& d : emitter.getDiagnostics()) {
                    err << emitter.format(d);
                }
            }
            return 1;
        }

        ir::AstLowerer lowerer(&emitter);
        auto ir_mod = lowerer.lower_module(*node->ast);

        if (emitter.hasErrors()) {
            if (is_json) {
                emitter.emitJson(out);
            } else {
                for (const auto& d : emitter.getDiagnostics()) {
                    err << emitter.format(d);
                }
            }
            return 1;
        }
    }

    if (is_json) {
        emitter.emitJson(out);
        return emitter.hasErrors() ? 1 : 0;
    }

    // Output any warnings
    for (const auto& d : emitter.getDiagnostics()) {
        if (d.severity == diag::DiagnosticSeverity::Warning) {
            err << emitter.format(d);
        }
    }

    if (options.verbose) {
        out << "Checked '" << options.input_file.string() << "' successfully (0 errors, "
            << emitter.getWarningCount() << " warnings).\n";
    } else {
        out << "Checked '" << options.input_file.string() << "' successfully.\n";
    }

    return 0;
}

int CliDriver::execute_emit(const CliOptions& options, std::ostream& out, std::ostream& err) {
    if (options.emit_target == EmitTarget::None) {
        err << "error: 'keety emit' requires an emission target (--cpp, --hpp, --ast, or --ir).\n"
            << "usage: keety emit <file.keet> (--cpp | --hpp | --ast | --ir)\n";
        return 1;
    }

    if (options.input_file.empty()) {
        err << "error: 'keety emit' requires a target input file.\n"
            << "usage: keety emit <file.keet> (--cpp | --hpp | --ast | --ir)\n";
        return 1;
    }

    if (!std::filesystem::exists(options.input_file)) {
        err << "error: input file not found: " << options.input_file.string() << "\n";
        return 1;
    }

    diag::SourceManager sm;
    diag::DiagnosticEmitter emitter(sm, options.color_mode);

    std::filesystem::path project_root = findProjectRoot(options.input_file);

    resolver::ModulePathResolver path_resolver(project_root, &emitter);
    resolver::ModuleGraph graph;

    bool graph_ok = graph.buildGraph(options.input_file, sm, path_resolver, &emitter);
    if (!graph_ok || emitter.hasErrors()) {
        for (const auto& d : emitter.getDiagnostics()) {
            err << emitter.format(d);
        }
        return 1;
    }

    // Check for circular dependency cycles
    resolver::CycleDetector cycle_detector(graph, &emitter);
    if (!cycle_detector.detectCycles() || emitter.hasErrors()) {
        for (const auto& d : emitter.getDiagnostics()) {
            err << emitter.format(d);
        }
        return 1;
    }

    // Topologically sort module dependency graph
    resolver::TopologicalSorter sorter(graph);
    std::filesystem::path canonical_entry = std::filesystem::weakly_canonical(options.input_file);
    auto ordered_nodes = sorter.sortNodes(canonical_entry.string());

    auto universe = std::make_shared<sema::Scope>(sema::ScopeKind::Universe);
    sema::registerUniverseSymbols(*universe);

    const resolver::ModuleNode* target_node = nullptr;

    std::unordered_map<std::string, std::shared_ptr<sema::Scope>> module_scopes;
    module_scopes["<stdlib>/io"] = sema::createStdlibModuleScope("io", universe);
    module_scopes["<stdlib>/fs"] = sema::createStdlibModuleScope("fs", universe);
    module_scopes["<stdlib>/sys"] = sema::createStdlibModuleScope("sys", universe);
    module_scopes["<stdlib>/math"] = sema::createStdlibModuleScope("math", universe);
    module_scopes["<stdlib>/time"] = sema::createStdlibModuleScope("time", universe);
    module_scopes["<stdlib>/json"] = sema::createStdlibModuleScope("json", universe);
    module_scopes["<stdlib>/net"] = sema::createStdlibModuleScope("net", universe);
    module_scopes["<stdlib>/test"] = sema::createStdlibModuleScope("test", universe);
    module_scopes["<stdlib>/crypto"] = sema::createStdlibModuleScope("crypto", universe);
    module_scopes["<stdlib>/http"] = sema::createStdlibModuleScope("http", universe);
    module_scopes["<stdlib>/net.http"] = sema::createStdlibModuleScope("net.http", universe);
    module_scopes["<stdlib>/ui"] = sema::createStdlibModuleScope("ui", universe);

    for (const auto* node : ordered_nodes) {
        if (!node || !node->ast) {
            continue;
        }

        if (node->canonicalPath == canonical_entry.string()) {
            target_node = node;
        }

        auto module_scope = std::make_shared<sema::Scope>(sema::ScopeKind::Module, universe);
        for (const auto& edge : node->outgoingEdges) {
            if (edge.toCanonicalPath == "<stdlib>/io" || edge.rawImportPath == "io") {
                auto ioModSym = std::make_shared<sema::Symbol>("io", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(ioModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/fs" || edge.rawImportPath == "fs") {
                auto fsModSym = std::make_shared<sema::Symbol>("fs", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(fsModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/sys" || edge.rawImportPath == "sys") {
                auto sysModSym = std::make_shared<sema::Symbol>("sys", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(sysModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/math" || edge.rawImportPath == "math") {
                auto mathModSym = std::make_shared<sema::Symbol>("math", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(mathModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/time" || edge.rawImportPath == "time") {
                auto timeModSym = std::make_shared<sema::Symbol>("time", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(timeModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/json" || edge.rawImportPath == "json") {
                auto jsonModSym = std::make_shared<sema::Symbol>("json", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(jsonModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/net" || edge.rawImportPath == "net") {
                auto netModSym = std::make_shared<sema::Symbol>("net", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(netModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/test" || edge.rawImportPath == "test") {
                auto testModSym = std::make_shared<sema::Symbol>("test", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(testModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/crypto" || edge.rawImportPath == "crypto") {
                auto cryptoModSym = std::make_shared<sema::Symbol>("crypto", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(cryptoModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/net.http" || edge.toCanonicalPath == "<stdlib>/http" ||
                       edge.rawImportPath == "net.http" || edge.rawImportPath == "http") {
                auto httpModSym = std::make_shared<sema::Symbol>("http", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(httpModSym, false);
            }
            if (edge.toCanonicalPath.rfind("<stdlib>/", 0) != 0) {
                auto it = module_scopes.find(edge.toCanonicalPath);
                if (it != module_scopes.end() && it->second) {
                    for (const auto& [name, sym] : it->second->getSymbols()) {
                        if (sym && sym->isExported()) {
                            if (sym->isVariable()) {
                                auto constView = sema::ConstViewChecker::importExportedSymbol(sym, edge.importSpan);
                                module_scope->define(constView, false);
                            } else {
                                module_scope->define(sym, false);
                            }
                        }
                    }
                }
            }
        }

        typechecker::TypeChecker checker(&emitter);
        checker.checkTypes(*node->ast, module_scope.get());
        module_scopes[node->canonicalPath] = module_scope;

        if (emitter.hasErrors()) {
            for (const auto& d : emitter.getDiagnostics()) {
                err << emitter.format(d);
            }
            return 1;
        }
    }

    if (!target_node || !target_node->ast) {
        err << "error: failed to resolve target compilation unit.\n";
        return 1;
    }

    // Generate output content based on requested emit_target
    std::string content;
    switch (options.emit_target) {
    case EmitTarget::Ast: {
        content = dump_ast(*target_node->ast);
        break;
    }
    case EmitTarget::Ir: {
        ir::AstLowerer lowerer(&emitter);
        auto ir_mod = lowerer.lower_module(*target_node->ast);
        if (emitter.hasErrors() || !ir_mod) {
            for (const auto& d : emitter.getDiagnostics()) {
                err << emitter.format(d);
            }
            return 1;
        }
        content = ir_mod->to_string();
        break;
    }
    case EmitTarget::Hpp: {
        ir::AstLowerer lowerer(&emitter);
        auto ir_mod = lowerer.lower_module(*target_node->ast);
        if (emitter.hasErrors() || !ir_mod) {
            for (const auto& d : emitter.getDiagnostics()) {
                err << emitter.format(d);
            }
            return 1;
        }
        emitter::HeaderEmitterOptions h_opts;
        h_opts.emit_line_directives = options.emit_line_directives;
        emitter::HeaderEmitter header_emitter(*target_node->ast, *ir_mod, h_opts);
        content = header_emitter.emit();
        break;
    }
    case EmitTarget::Cpp: {
        ir::AstLowerer lowerer(&emitter);
        auto ir_mod = lowerer.lower_module(*target_node->ast);
        if (emitter.hasErrors() || !ir_mod) {
            for (const auto& d : emitter.getDiagnostics()) {
                err << emitter.format(d);
            }
            return 1;
        }
        emitter::CppEmitterOptions cpp_opts;
        cpp_opts.emit_line_directives = options.emit_line_directives;
        cpp_opts.source_manager = &sm;
        cpp_opts.source_file_path = target_node->canonicalPath;
        emitter::CppEmitter cpp_emitter(*target_node->ast, *ir_mod, cpp_opts);
        content = cpp_emitter.emit();
        break;
    }
    case EmitTarget::None:
        break;
    }

    // Write to output directory or stdout
    if (options.output_dir.empty()) {
        out << content;
    } else {
        std::filesystem::path dest;
        if (options.output_is_dir ||
            std::filesystem::is_directory(options.output_dir) ||
            options.output_dir.string().back() == '/' ||
            options.output_dir.string().back() == '\\') {
            std::string ext;
            switch (options.emit_target) {
            case EmitTarget::Cpp: ext = ".cpp"; break;
            case EmitTarget::Hpp: ext = ".hpp"; break;
            case EmitTarget::Ast: ext = ".ast"; break;
            case EmitTarget::Ir:  ext = ".ir";  break;
            default: ext = ".txt"; break;
            }
            dest = options.output_dir / (options.input_file.stem().string() + ext);
        } else {
            dest = options.output_dir;
        }

        if (dest.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(dest.parent_path(), ec);
        }

        std::ofstream dest_file(dest);
        if (!dest_file.is_open()) {
            err << "error: failed to open output file for writing: " << dest.string() << "\n";
            return 1;
        }
        dest_file << content;

        if (options.verbose) {
            out << "Emitted " << to_string(options.emit_target) << " to " << dest.string() << "\n";
        }
    }

    return 0;
}

std::optional<std::filesystem::path> CliDriver::build_binary(
    const CliOptions& options, std::ostream& out, std::ostream& err) {

    if (options.input_file.empty()) {
        err << "error: 'keety " << to_string(options.command) << "' requires a target input file.\n"
            << "usage: keety " << to_string(options.command) << " <file.keet>\n";
        return std::nullopt;
    }

    if (!std::filesystem::exists(options.input_file)) {
        err << "error: input file not found: " << options.input_file.string() << "\n";
        return std::nullopt;
    }

    if (options.verbose) {
        out << "[verbose] Building project from entry: " << options.input_file.string() << "\n";
    }

    diag::SourceManager sm;
    diag::DiagnosticEmitter emitter(sm, options.color_mode);

    std::filesystem::path project_root = findProjectRoot(options.input_file);

    if (options.verbose) {
        out << "[verbose] Project root: " << project_root.string() << "\n";
    }

    resolver::ModulePathResolver path_resolver(project_root, &emitter);
    resolver::ModuleGraph graph;

    bool graph_ok = graph.buildGraph(options.input_file, sm, path_resolver, &emitter);
    if (!graph_ok || emitter.hasErrors()) {
        for (const auto& d : emitter.getDiagnostics()) {
            err << emitter.format(d);
        }
        return std::nullopt;
    }

    // Check for circular dependency cycles
    resolver::CycleDetector cycle_detector(graph, &emitter);
    if (!cycle_detector.detectCycles() || emitter.hasErrors()) {
        for (const auto& d : emitter.getDiagnostics()) {
            err << emitter.format(d);
        }
        return std::nullopt;
    }

    // Topologically sort module dependency graph
    resolver::TopologicalSorter sorter(graph);
    std::filesystem::path canonical_entry = std::filesystem::weakly_canonical(options.input_file);
    auto ordered_nodes = sorter.sortNodes(canonical_entry.string());

    if (options.verbose) {
        out << "[verbose] Resolved " << ordered_nodes.size() << " module(s) in dependency order:\n";
        for (size_t i = 0; i < ordered_nodes.size(); ++i) {
            out << "[verbose]   [" << (i + 1) << "] " << ordered_nodes[i]->moduleName << "\n";
        }
    }

    auto universe = std::make_shared<sema::Scope>(sema::ScopeKind::Universe);
    sema::registerUniverseSymbols(*universe);

    std::unordered_map<std::string, std::shared_ptr<sema::Scope>> module_scopes;
    module_scopes["<stdlib>/io"] = sema::createStdlibModuleScope("io", universe);
    module_scopes["<stdlib>/fs"] = sema::createStdlibModuleScope("fs", universe);
    module_scopes["<stdlib>/sys"] = sema::createStdlibModuleScope("sys", universe);
    module_scopes["<stdlib>/math"] = sema::createStdlibModuleScope("math", universe);
    module_scopes["<stdlib>/time"] = sema::createStdlibModuleScope("time", universe);
    module_scopes["<stdlib>/json"] = sema::createStdlibModuleScope("json", universe);
    module_scopes["<stdlib>/net"] = sema::createStdlibModuleScope("net", universe);
    module_scopes["<stdlib>/test"] = sema::createStdlibModuleScope("test", universe);
    module_scopes["<stdlib>/crypto"] = sema::createStdlibModuleScope("crypto", universe);
    module_scopes["<stdlib>/http"] = sema::createStdlibModuleScope("http", universe);
    module_scopes["<stdlib>/net.http"] = sema::createStdlibModuleScope("net.http", universe);
    module_scopes["<stdlib>/ui"] = sema::createStdlibModuleScope("ui", universe);

    // 1. Semantic and type checking across all modules
    for (const auto* node : ordered_nodes) {
        if (!node || !node->ast) {
            continue;
        }

        auto module_scope = std::make_shared<sema::Scope>(sema::ScopeKind::Module, universe);
        for (const auto& edge : node->outgoingEdges) {
            if (edge.toCanonicalPath == "<stdlib>/io" || edge.rawImportPath == "io") {
                auto ioModSym = std::make_shared<sema::Symbol>("io", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(ioModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/fs" || edge.rawImportPath == "fs") {
                auto fsModSym = std::make_shared<sema::Symbol>("fs", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(fsModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/sys" || edge.rawImportPath == "sys") {
                auto sysModSym = std::make_shared<sema::Symbol>("sys", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(sysModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/math" || edge.rawImportPath == "math") {
                auto mathModSym = std::make_shared<sema::Symbol>("math", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(mathModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/time" || edge.rawImportPath == "time") {
                auto timeModSym = std::make_shared<sema::Symbol>("time", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(timeModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/json" || edge.rawImportPath == "json") {
                auto jsonModSym = std::make_shared<sema::Symbol>("json", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(jsonModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/net" || edge.rawImportPath == "net") {
                auto netModSym = std::make_shared<sema::Symbol>("net", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(netModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/test" || edge.rawImportPath == "test") {
                auto testModSym = std::make_shared<sema::Symbol>("test", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(testModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/crypto" || edge.rawImportPath == "crypto") {
                auto cryptoModSym = std::make_shared<sema::Symbol>("crypto", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(cryptoModSym, false);
            } else if (edge.toCanonicalPath == "<stdlib>/net.http" || edge.toCanonicalPath == "<stdlib>/http" ||
                       edge.rawImportPath == "net.http" || edge.rawImportPath == "http") {
                auto httpModSym = std::make_shared<sema::Symbol>("http", sema::SymbolKind::Module, edge.importSpan);
                module_scope->define(httpModSym, false);
            }
            if (edge.toCanonicalPath.rfind("<stdlib>/", 0) != 0) {
                auto it = module_scopes.find(edge.toCanonicalPath);
                if (it != module_scopes.end() && it->second) {
                    for (const auto& [name, sym] : it->second->getSymbols()) {
                        if (sym && sym->isExported()) {
                            if (sym->isVariable()) {
                                auto constView = sema::ConstViewChecker::importExportedSymbol(sym, edge.importSpan);
                                module_scope->define(constView, false);
                            } else {
                                module_scope->define(sym, false);
                            }
                        }
                    }
                }
            }
        }

        typechecker::TypeChecker checker(&emitter);
        checker.checkTypes(*node->ast, module_scope.get());
        module_scopes[node->canonicalPath] = module_scope;

        if (emitter.hasErrors()) {
            for (const auto& d : emitter.getDiagnostics()) {
                err << emitter.format(d);
            }
            return std::nullopt;
        }
    }

    // 2. Determine build directory and target binary path
    std::filesystem::path target_binary;
    std::filesystem::path build_dir;

    std::string exec_ext = ToolchainDriver::get_executable_extension();

    std::filesystem::path effective_output_dir = options.output_dir;
    bool effective_output_is_dir = options.output_is_dir;

    if (effective_output_dir.empty()) {
        auto configured_build_dir = resolveBuildDirectory(options.input_file);
        if (configured_build_dir.has_value() && !configured_build_dir->empty()) {
            effective_output_dir = *configured_build_dir;
            effective_output_is_dir = true;
        }
    }

    if (!effective_output_dir.empty()) {
        std::error_code ec;
        if (effective_output_is_dir ||
            std::filesystem::is_directory(effective_output_dir) ||
            effective_output_dir.string().back() == '/' ||
            effective_output_dir.string().back() == '\\') {
            std::filesystem::create_directories(effective_output_dir, ec);
            target_binary = effective_output_dir / (options.input_file.stem().string() + exec_ext);
            build_dir = effective_output_dir / ".keety_build";
        } else {
            target_binary = effective_output_dir;
            build_dir = target_binary.has_parent_path()
                ? (target_binary.parent_path() / ".keety_build")
                : (std::filesystem::current_path() / ".keety_build");
        }
    } else {
        std::filesystem::path parent = options.input_file.has_parent_path()
            ? options.input_file.parent_path()
            : std::filesystem::current_path();
        target_binary = parent / (options.input_file.stem().string() + exec_ext);
        build_dir = parent / ".keety_build";
    }

    std::error_code ec;
    std::filesystem::create_directories(build_dir, ec);

    if (options.verbose) {
        out << "[verbose] Target binary: " << target_binary.string() << "\n";
        out << "[verbose] Build directory: " << build_dir.string() << "\n";
    }

    // 3. Emit C++ headers and implementations
    std::vector<std::filesystem::path> generated_cpp_files;

    for (const auto* node : ordered_nodes) {
        if (!node || !node->ast) {
            continue;
        }

        ir::AstLowerer lowerer(&emitter);
        auto ir_mod = lowerer.lower_module(*node->ast);
        if (emitter.hasErrors() || !ir_mod) {
            for (const auto& d : emitter.getDiagnostics()) {
                err << emitter.format(d);
            }
            return std::nullopt;
        }

        // Header emission
        emitter::HeaderEmitterOptions h_opts;
        h_opts.emit_line_directives = options.emit_line_directives;
        emitter::HeaderEmitter header_emitter(*node->ast, *ir_mod, h_opts);
        std::string hpp_content = header_emitter.emit();
        std::filesystem::path hpp_path = build_dir / (node->moduleName + ".hpp");
        std::ofstream hpp_file(hpp_path);
        hpp_file << hpp_content;

        // C++ source emission
        emitter::CppEmitterOptions cpp_opts;
        cpp_opts.header_include = "\"" + node->moduleName + ".hpp\"";
        cpp_opts.emit_line_directives = options.emit_line_directives;
        cpp_opts.source_manager = &sm;
        cpp_opts.source_file_path = node->canonicalPath;
        if (node->canonicalPath == canonical_entry.string()) {
            cpp_opts.is_root_module = true;
        }
        emitter::CppEmitter cpp_emitter(*node->ast, *ir_mod, cpp_opts);
        std::string cpp_content = cpp_emitter.emit();
        std::filesystem::path cpp_path = build_dir / (node->moduleName + ".cpp");
        std::ofstream cpp_file(cpp_path);
        cpp_file << cpp_content;

        if (options.verbose) {
            out << "[verbose] Emitted sources: " << hpp_path.filename().string()
                << ", " << cpp_path.filename().string() << "\n";
        }

        generated_cpp_files.push_back(cpp_path);
    }

    // 4. Native Toolchain compilation & linking
    ToolchainDriver toolchain_driver;
    if (!options.cxx_compiler.empty()) {
        auto custom_tc = ToolchainDetector::probe(options.cxx_compiler);
        if (!custom_tc || !custom_tc->is_valid()) {
            err << "error: specified C++ compiler '" << options.cxx_compiler << "' is invalid or not found.\n";
            return std::nullopt;
        }
        toolchain_driver.set_toolchain(*custom_tc);
        if (options.verbose) {
            out << "[verbose] Using specified C++ toolchain: " << custom_tc->compiler_path.string()
                << " (" << to_string(custom_tc->kind) << " " << custom_tc->version << ")\n";
        }
    } else {
        if (options.verbose && toolchain_driver.has_valid_toolchain()) {
            const auto& tc = toolchain_driver.get_toolchain();
            out << "[verbose] Using host C++ toolchain: " << tc.compiler_path.string()
                << " (" << to_string(tc.kind) << " " << tc.version << ")\n";
        }
    }

    if (!toolchain_driver.has_valid_toolchain()) {
        err << "error: no suitable native C++ toolchain found (clang++, g++, cl.exe).\n";
        return std::nullopt;
    }

    CompileOptions comp_opts;
    comp_opts.opt_level = options.opt_level;
    comp_opts.wrapv = options.wrapv;
    comp_opts.cpp_standard = "c++20";
    comp_opts.compile_only = true;
    comp_opts.debug_symbols = options.debug_info || (options.opt_level == OptimizationLevel::O0);
    comp_opts.include_directories.push_back(build_dir);

    if (options.verbose) {
        out << "[verbose] Optimization level: " << to_string(options.opt_level)
            << " (flag: " << to_compiler_flag(options.opt_level, toolchain_driver.get_toolchain().kind) << ")\n";
        out << "[verbose] Debug symbols: " << (comp_opts.debug_symbols ? "enabled" : "disabled") << "\n";
        out << "[verbose] Two's-complement overflow semantics: " << (options.wrapv ? "enabled" : "disabled") << "\n";
    }

    auto runtime_dir = find_runtime_directory();
    if (!runtime_dir.empty()) {
        comp_opts.include_directories.push_back(runtime_dir);
        if (options.verbose) {
            out << "[verbose] KeetScript runtime include directory: " << runtime_dir.string() << "\n";
        }
    } else {
        err << "error: KeetScript runtime directory could not be located.\n"
            << "  --> Expected to find 'keet/runtime.hpp'\n"
            << "  --> Set the KEETY_RUNTIME_DIR environment variable to the path containing 'keet/runtime.hpp'\n";
        return std::nullopt;
    }

    std::vector<std::filesystem::path> object_files;
    std::string obj_ext = ToolchainDriver::get_object_file_extension(toolchain_driver.get_toolchain().kind);

    for (const auto& cpp_src : generated_cpp_files) {
        std::filesystem::path obj_path = build_dir / (cpp_src.stem().string() + obj_ext);
        if (options.verbose) {
            out << "[verbose] Compiling: " << cpp_src.filename().string() << " -> " << obj_path.filename().string() << "\n";
        }
        auto comp_res = toolchain_driver.compile(cpp_src, obj_path, comp_opts);
        if (!comp_res.success) {
            if (options.verbose || options.verbose_cpp) {
                err << "error: compilation failed for '" << cpp_src.filename().string() << "':\n"
                    << comp_res.output << "\n";
            } else {
                err << "error[ICE]: internal compiler error: generated C++ failed to compile for '"
                    << cpp_src.filename().string() << "'\n"
                    << "  --> Please file a bug report at https://github.com/keetscript/keety/issues\n"
                    << "      Run with '--verbose-cpp' or '--verbose' to view raw backend compiler backtrace.\n";
            }
            return std::nullopt;
        }
        object_files.push_back(obj_path);
    }

    LinkOptions link_opts;
    link_opts.kind = BinaryKind::Executable;
    link_opts.debug_symbols = comp_opts.debug_symbols;
#if defined(_WIN32)
    link_opts.libraries.push_back("ws2_32");
    link_opts.libraries.push_back("bcrypt");
#endif
    if (options.verbose) {
        out << "[verbose] Linking " << object_files.size() << " object file(s) into "
            << target_binary.filename().string() << "\n";
    }
    auto link_res = toolchain_driver.link_executable(object_files, target_binary, link_opts);
    if (!link_res.success) {
        if (options.verbose || options.verbose_cpp) {
            err << "error: linking failed for '" << target_binary.filename().string() << "':\n"
                << link_res.output << "\n";
        } else {
            err << "error[ICE]: internal compiler error: backend linking failed for '"
                << target_binary.filename().string() << "'\n"
                << "  --> Please file a bug report at https://github.com/keetscript/keety/issues\n"
                << "      Run with '--verbose-cpp' or '--verbose' to view raw backend linker backtrace.\n";
        }
        return std::nullopt;
    }

    if (options.verbose) {
        out << "[verbose] Linking finished: " << target_binary.string() << "\n";
    }

    return target_binary;
}

int CliDriver::execute_build(const CliOptions& options, std::ostream& out, std::ostream& err) {
    auto bin = build_binary(options, out, err);
    if (!bin) {
        return 1;
    }
    if (options.verbose) {
        out << "Built executable: " << bin->string() << "\n";
    }
    return 0;
}

int CliDriver::execute_run(const CliOptions& options, std::ostream& out, std::ostream& err) {
    auto bin = build_binary(options, out, err);
    if (!bin) {
        return 1;
    }

    std::string cmd = quote_arg(bin->string());
    for (const auto& a : options.run_args) {
        cmd += " " + quote_arg(a);
    }

    if (options.verbose) {
        out << "[verbose] Executing: " << cmd << "\n";
    }

    int run_status = std::system(cmd.c_str());
    int exit_code = run_status;
#if !defined(_WIN32)
    if (WIFEXITED(run_status)) {
        exit_code = WEXITSTATUS(run_status);
    }
#endif
    if (options.verbose) {
        out << "[verbose] Process exited with status code: " << exit_code << "\n";
    }
    return exit_code;
}

std::vector<std::filesystem::path> CliDriver::discover_test_files(const std::filesystem::path& target_path) {
    std::vector<std::filesystem::path> results;
    std::error_code ec;

    std::filesystem::path target = target_path.empty() ? std::filesystem::current_path() : target_path;
    if (!std::filesystem::exists(target, ec)) {
        return results;
    }

    if (std::filesystem::is_regular_file(target, ec)) {
        if (target.extension() == ".keet") {
            results.push_back(target);
        }
        return results;
    }

    if (!std::filesystem::is_directory(target, ec)) {
        return results;
    }

    for (std::filesystem::recursive_directory_iterator it(target, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (ec) break;

        const auto& entry = *it;
        auto filename = entry.path().filename().string();

        if (entry.is_directory(ec)) {
            // Skip hidden directories (.git, .keety_build, etc.), build, node_modules, cmake-build-*
            if (!filename.empty() && filename[0] == '.') {
                it.disable_recursion_pending();
                continue;
            }
            if (filename == "build" || filename == "node_modules" || filename.rfind("cmake-build-", 0) == 0) {
                it.disable_recursion_pending();
                continue;
            }
            continue;
        }

        if (entry.is_regular_file(ec) && (entry.path().extension() == ".keet" || entry.path().extension() == ".kml")) {
            bool matches = false;
            // 1. *.test.keet or *.test.kml
            if ((filename.size() > 10 && filename.rfind(".test.keet") == filename.size() - 10) ||
                (filename.size() > 9 && filename.rfind(".test.kml") == filename.size() - 9)) {
                matches = true;
            }
            // 2. test_*.keet or test_*.kml
            else if (filename.rfind("test_", 0) == 0) {
                matches = true;
            }
            // 3. Inside a tests/ directory containing import test;
            else {
                bool inside_tests_dir = false;
                for (const auto& part : entry.path()) {
                    if (part == "tests") {
                        inside_tests_dir = true;
                        break;
                    }
                }
                if (inside_tests_dir) {
                    std::ifstream ifs(entry.path());
                    if (ifs.is_open()) {
                        std::string line;
                        while (std::getline(ifs, line)) {
                            if (line.find("import test") != std::string::npos ||
                                line.find("from \"test\"") != std::string::npos ||
                                line.find("from 'test'") != std::string::npos) {
                                matches = true;
                                break;
                            }
                        }
                    }
                }
            }

            if (matches) {
                results.push_back(entry.path());
            }
        }
    }

    std::sort(results.begin(), results.end());
    return results;
}

int CliDriver::execute_test(const CliOptions& options, std::ostream& out, std::ostream& err) {
    bool run_benchmarks = options.test_benchmarks;
    bool run_conformance = options.test_conformance;

    if (run_benchmarks || run_conformance) {
        int overall_status = 0;

        if (run_benchmarks) {
            if (options.verbose) {
                out << "[verbose] Running reference performance benchmarks...\n";
            }
            std::vector<std::filesystem::path> candidates = {
                "./build/benchmarks/keety_benchmarks",
                "./benchmarks/keety_benchmarks",
                "keety_benchmarks"
            };
            std::filesystem::path bench_bin;
            for (const auto& c : candidates) {
                if (std::filesystem::exists(c)) {
                    bench_bin = c;
                    break;
                }
            }

            if (!bench_bin.empty()) {
                std::string cmd = quote_arg(bench_bin.string());
                if (options.verbose) {
                    cmd += " --verbose";
                }
                for (const auto& a : options.run_args) {
                    cmd += " " + quote_arg(a);
                }
                int status = std::system(cmd.c_str());
                if (status != 0) {
                    overall_status = 1;
                }
            } else {
                std::string cmd = "ctest --test-dir build -R bench_numeric_vector --output-on-failure";
                int status = std::system(cmd.c_str());
                if (status != 0) {
                    overall_status = 1;
                }
            }
        }

        if (run_conformance) {
            if (options.verbose) {
                out << "[verbose] Running conformance test suite...\n";
            }
            std::string cmd = "ctest --test-dir build -R conformance --output-on-failure";
            int status = std::system(cmd.c_str());
            if (status != 0) {
                overall_status = 1;
            }
        }

        return overall_status;
    }

    // Native KeetScript Test Discovery and Runner
    std::filesystem::path target = options.input_file.empty() ? std::filesystem::current_path() : options.input_file;
    auto test_files = discover_test_files(target);

    if (test_files.empty()) {
        if (options.test_json) {
            out << "{\n"
                << "  \"total\": 0,\n"
                << "  \"passed\": 0,\n"
                << "  \"failed\": 0,\n"
                << "  \"skipped\": 0,\n"
                << "  \"totalDurationNs\": 0,\n"
                << "  \"testFiles\": {\n"
                << "    \"total\": 0,\n"
                << "    \"passed\": 0,\n"
                << "    \"failed\": 0\n"
                << "  },\n"
                << "  \"results\": []\n"
                << "}\n";
        } else if (options.test_tap) {
            out << "TAP version 13\n1..0\n";
        } else {
            out << "No test files found in " << target.string() << ".\n";
        }
        return 0;
    }

    bool use_color = false;
    if (options.color_mode == diag::ColorMode::Always) {
        use_color = true;
    } else if (options.color_mode == diag::ColorMode::Never) {
        use_color = false;
    } else {
        const char* no_color = std::getenv("NO_COLOR");
        if (!no_color || no_color[0] == '\0') {
#if defined(_WIN32)
            use_color = _isatty(_fileno(stdout)) != 0;
#else
            use_color = isatty(fileno(stdout)) != 0;
#endif
        }
    }

    const char* passBadge = use_color ? "\033[30;42m PASS \033[0m" : " PASS ";
    const char* failBadge = use_color ? "\033[30;41m FAIL \033[0m" : " FAIL ";
    const char* cPass = use_color ? "\033[32m" : "";
    const char* cFail = use_color ? "\033[31m" : "";
    const char* cDim = use_color ? "\033[90m" : "";
    const char* cBold = use_color ? "\033[1m" : "";
    const char* cReset = use_color ? "\033[0m" : "";

    struct AggregatedCaseResult {
        std::string file;
        keet::test::TestCaseResult result;
    };
    std::vector<AggregatedCaseResult> all_results;

    int total_files = static_cast<int>(test_files.size());
    int files_passed = 0;
    int files_failed = 0;
    int total_cases_passed = 0;
    int total_cases_failed = 0;
    int total_cases_skipped = 0;
    int total_cases = 0;
    auto suite_start = std::chrono::steady_clock::now();

    for (const auto& test_path : test_files) {
        CliOptions single_opts = options;
        single_opts.command = CliCommand::Build;
        single_opts.input_file = test_path;

        std::ostringstream build_out;
        std::ostringstream build_err;
        auto bin = build_binary(single_opts, build_out, build_err);

        if (!bin) {
            files_failed++;
            keet::test::TestCaseResult comp_res;
            comp_res.name = "compilation";
            comp_res.suite = "";
            comp_res.passed = false;
            comp_res.durationNs = 0;
            keet::test::TestFailure tf;
            tf.message = "compilation failed";
            tf.file = test_path.string();
            tf.line = 0;
            tf.errorType = "CompilationError";
            comp_res.failure = std::move(tf);
            all_results.push_back({test_path.filename().string(), std::move(comp_res)});
            total_cases_failed++;
            total_cases++;

            if (!options.test_json && !options.test_tap) {
                out << failBadge << " " << test_path.string() << " (compilation failed)\n";
                std::string err_str = build_err.str();
                if (!err_str.empty()) {
                    err << err_str << "\n";
                }
            }
            if (options.test_fail_fast) {
                break;
            }
            continue;
        }

        std::string cmd = quote_arg(bin->string()) + " --keet-test-json";
        if (!options.test_filter.empty()) {
            cmd += " --filter " + quote_arg(options.test_filter);
        }
        if (options.test_fail_fast) {
            cmd += " --fail-fast";
        }

        std::string exec_output;
        int exit_code = 0;
#if defined(_WIN32)
        std::string full_cmd = "\"" + cmd + " 2>&1\"";
        FILE* pipe = _popen(full_cmd.c_str(), "r");
#else
        std::string full_cmd = cmd + " 2>&1";
        FILE* pipe = popen(full_cmd.c_str(), "r");
#endif
        if (!pipe) {
            files_failed++;
            keet::test::TestCaseResult proc_res;
            proc_res.name = "process";
            proc_res.suite = "";
            proc_res.passed = false;
            proc_res.durationNs = 0;
            keet::test::TestFailure tf;
            tf.message = "failed to launch test process";
            tf.file = test_path.string();
            tf.line = 0;
            tf.errorType = "ProcessError";
            proc_res.failure = std::move(tf);
            all_results.push_back({test_path.filename().string(), std::move(proc_res)});
            total_cases_failed++;
            total_cases++;

            if (!options.test_json && !options.test_tap) {
                out << failBadge << " " << test_path.string() << " (failed to launch test process)\n\n";
            }
            if (options.test_fail_fast) break;
            continue;
        }

        std::array<char, 512> buf;
        while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
            exec_output += buf.data();
        }
#if defined(_WIN32)
        int raw_status = _pclose(pipe);
        exit_code = raw_status;
#else
        int raw_status = pclose(pipe);
        exit_code = WIFEXITED(raw_status) ? WEXITSTATUS(raw_status) : raw_status;
#endif

        bool parsed_ok = false;
        keet::test::TestSummary summary;
        try {
            size_t json_start = exec_output.find('{');
            if (json_start != std::string::npos) {
                std::string_view json_view = std::string_view(exec_output).substr(json_start);
                ::keet::json::detail::Parser json_p(json_view);
                auto root = json_p.parse_document();
                if (root.is_object()) {
                    const auto& obj = std::get<keet::json::detail::JsonNode::Object>(root.value);
                    if (obj.count("total") && obj.count("passed") && obj.count("results")) {
                        summary.total = static_cast<int>(std::get<int64_t>(obj.at("total").value));
                        summary.passed = static_cast<int>(std::get<int64_t>(obj.at("passed").value));
                        summary.failed = static_cast<int>(std::get<int64_t>(obj.at("failed").value));
                        if (obj.count("skipped")) {
                            summary.skipped = static_cast<int>(std::get<int64_t>(obj.at("skipped").value));
                        }
                        if (obj.count("totalDurationNs")) {
                            summary.totalDurationNs = std::get<int64_t>(obj.at("totalDurationNs").value);
                        }
                        if (obj.at("results").is_array()) {
                            const auto& res_arr = std::get<keet::json::detail::JsonNode::Array>(obj.at("results").value);
                            for (const auto& item : res_arr) {
                                if (!item.is_object()) continue;
                                const auto& r_obj = std::get<keet::json::detail::JsonNode::Object>(item.value);
                                keet::test::TestCaseResult r;
                                if (r_obj.count("name") && r_obj.at("name").is_string()) {
                                    r.name = std::get<std::string>(r_obj.at("name").value);
                                }
                                if (r_obj.count("suite") && r_obj.at("suite").is_string()) {
                                    r.suite = std::get<std::string>(r_obj.at("suite").value);
                                }
                                if (r_obj.count("passed") && r_obj.at("passed").is_bool()) {
                                    r.passed = std::get<bool>(r_obj.at("passed").value);
                                }
                                if (r_obj.count("durationNs") && r_obj.at("durationNs").is_int()) {
                                    r.durationNs = std::get<int64_t>(r_obj.at("durationNs").value);
                                }
                                if (r_obj.count("failure") && r_obj.at("failure").is_object()) {
                                    const auto& f_obj = std::get<keet::json::detail::JsonNode::Object>(r_obj.at("failure").value);
                                    keet::test::TestFailure tf;
                                    if (f_obj.count("message") && f_obj.at("message").is_string()) tf.message = std::get<std::string>(f_obj.at("message").value);
                                    if (f_obj.count("expected") && f_obj.at("expected").is_string()) tf.expected = std::get<std::string>(f_obj.at("expected").value);
                                    if (f_obj.count("actual") && f_obj.at("actual").is_string()) tf.actual = std::get<std::string>(f_obj.at("actual").value);
                                    if (f_obj.count("file") && f_obj.at("file").is_string()) tf.file = std::get<std::string>(f_obj.at("file").value);
                                    if (f_obj.count("line") && f_obj.at("line").is_int()) tf.line = static_cast<int>(std::get<int64_t>(f_obj.at("line").value));
                                    if (f_obj.count("errorType") && f_obj.at("errorType").is_string()) tf.errorType = std::get<std::string>(f_obj.at("errorType").value);
                                    r.failure = std::move(tf);
                                }
                                summary.results.push_back(std::move(r));
                            }
                        }
                        parsed_ok = true;
                    }
                }
            }
        } catch (...) {
            parsed_ok = false;
        }

        if (parsed_ok) {
            bool file_passed = (exit_code == 0 && summary.failed == 0);
            if (file_passed) {
                files_passed++;
            } else {
                files_failed++;
            }
            total_cases_passed += summary.passed;
            total_cases_failed += summary.failed;
            total_cases_skipped += summary.skipped;
            total_cases += summary.total;

            for (const auto& res : summary.results) {
                all_results.push_back({test_path.filename().string(), res});
            }

            if (!options.test_json && !options.test_tap) {
                int64_t fileDurationMs = summary.totalDurationNs / 1'000'000;
                if (file_passed) {
                    out << passBadge << " " << test_path.string() << " ("
                        << summary.total << (summary.total == 1 ? " test, " : " tests, ");
                    if (summary.skipped > 0) {
                        out << summary.skipped << " skipped, ";
                    }
                    out << fileDurationMs << "ms)\n";
                } else {
                    out << failBadge << " " << test_path.string() << " ("
                        << summary.failed << " failed, "
                        << summary.passed << " passed, ";
                    if (summary.skipped > 0) {
                        out << summary.skipped << " skipped, ";
                    }
                    out << fileDurationMs << "ms)\n";
                }

                for (const auto& res : summary.results) {
                    int64_t caseMs = res.durationNs / 1'000'000;
                    if (res.passed) {
                        out << "   " << cPass << "✓" << cReset << " " << res.name
                            << " " << cDim << "(" << caseMs << "ms)" << cReset << "\n";
                    } else {
                        out << "   " << cFail << "✕" << cReset << " " << res.name
                            << " " << cDim << "(" << caseMs << "ms)" << cReset << "\n";
                        if (res.failure) {
                            const auto& f = *res.failure;
                            out << "     " << cFail << f.errorType << ": " << f.message << cReset << "\n";
                            if (!f.expected.empty() || !f.actual.empty()) {
                                out << "       " << cDim << "Expected: " << cReset << cPass << f.expected << cReset << "\n";
                                out << "       " << cDim << "Actual:   " << cReset << cFail << f.actual << cReset << "\n";
                            }
                            if (!f.file.empty()) {
                                out << "       " << cDim << "--> " << f.file;
                                if (f.line > 0) out << ":" << f.line;
                                out << cReset << "\n";
                            }
                        }
                    }
                }
                out << "\n";
            }
        } else {
            files_failed++;
            keet::test::TestCaseResult exec_res;
            exec_res.name = "execution";
            exec_res.suite = "";
            exec_res.passed = false;
            exec_res.durationNs = 0;
            keet::test::TestFailure tf;
            tf.message = "execution failed with exit code " + std::to_string(exit_code);
            tf.file = test_path.string();
            tf.line = 0;
            tf.errorType = "ExecutionError";
            exec_res.failure = std::move(tf);
            all_results.push_back({test_path.filename().string(), std::move(exec_res)});
            total_cases_failed++;
            total_cases++;

            if (!options.test_json && !options.test_tap) {
                out << failBadge << " " << test_path.string() << " (execution failed with exit code " << exit_code << ")\n";
                if (!exec_output.empty()) {
                    out << exec_output << "\n";
                }
                out << "\n";
            }
        }

        if (options.test_fail_fast && (files_failed > 0 || total_cases_failed > 0)) {
            break;
        }
    }

    auto suite_end = std::chrono::steady_clock::now();
    double suiteDurationMs = std::chrono::duration<double, std::milli>(suite_end - suite_start).count();

    if (options.test_json) {
        out << "{\n";
        out << "  \"total\": " << total_cases << ",\n";
        out << "  \"passed\": " << total_cases_passed << ",\n";
        out << "  \"failed\": " << total_cases_failed << ",\n";
        out << "  \"skipped\": " << total_cases_skipped << ",\n";
        out << "  \"totalDurationNs\": " << static_cast<int64_t>(suiteDurationMs * 1'000'000.0) << ",\n";
        out << "  \"testFiles\": {\n";
        out << "    \"total\": " << total_files << ",\n";
        out << "    \"passed\": " << files_passed << ",\n";
        out << "    \"failed\": " << files_failed << "\n";
        out << "  },\n";
        out << "  \"results\": [\n";
        for (size_t i = 0; i < all_results.size(); ++i) {
            const auto& ar = all_results[i];
            const auto& r = ar.result;
            out << "    {\n";
            out << "      \"name\": \"" << keet::test::jsonEscape(r.name) << "\",\n";
            out << "      \"suite\": \"" << keet::test::jsonEscape(r.suite) << "\",\n";
            out << "      \"file\": \"" << keet::test::jsonEscape(ar.file) << "\",\n";
            out << "      \"passed\": " << (r.passed ? "true" : "false") << ",\n";
            out << "      \"durationNs\": " << r.durationNs;
            if (r.failure) {
                out << ",\n      \"failure\": {\n";
                out << "        \"message\": \"" << keet::test::jsonEscape(r.failure->message) << "\",\n";
                out << "        \"expected\": \"" << keet::test::jsonEscape(r.failure->expected) << "\",\n";
                out << "        \"actual\": \"" << keet::test::jsonEscape(r.failure->actual) << "\",\n";
                out << "        \"file\": \"" << keet::test::jsonEscape(r.failure->file) << "\",\n";
                out << "        \"line\": " << r.failure->line << ",\n";
                out << "        \"errorType\": \"" << keet::test::jsonEscape(r.failure->errorType) << "\"\n";
                out << "      }\n";
            } else {
                out << "\n";
            }
            out << "    }" << (i + 1 < all_results.size() ? "," : "") << "\n";
        }
        out << "  ]\n";
        out << "}\n";
    } else if (options.test_tap) {
        out << "TAP version 13\n";
        out << "1.." << all_results.size() << "\n";
        for (size_t i = 0; i < all_results.size(); ++i) {
            const auto& ar = all_results[i];
            const auto& r = ar.result;
            std::string fullName = ar.file;
            if (!r.suite.empty()) {
                fullName += " > " + r.suite;
            }
            fullName += " > " + r.name;

            int64_t caseMs = r.durationNs / 1'000'000;
            if (r.passed) {
                out << "ok " << (i + 1) << " - " << fullName << " # time=" << caseMs << "ms\n";
            } else {
                out << "not ok " << (i + 1) << " - " << fullName << " # time=" << caseMs << "ms\n";
                if (r.failure) {
                    out << "  ---\n";
                    out << "  message: \"" << keet::test::jsonEscape(r.failure->message) << "\"\n";
                    if (!r.failure->expected.empty()) {
                        out << "  expected: \"" << keet::test::jsonEscape(r.failure->expected) << "\"\n";
                    }
                    if (!r.failure->actual.empty()) {
                        out << "  actual: \"" << keet::test::jsonEscape(r.failure->actual) << "\"\n";
                    }
                    if (!r.failure->file.empty()) {
                        out << "  file: \"" << keet::test::jsonEscape(r.failure->file) << "\"\n";
                    }
                    if (r.failure->line > 0) {
                        out << "  line: " << r.failure->line << "\n";
                    }
                    out << "  ...\n";
                }
            }
        }
    } else {
        out << cBold << "Test Files  " << cReset;
        if (files_failed == 0) {
            out << cPass << files_passed << " passed" << cReset << " (" << total_files << " total)\n";
        } else {
            out << cPass << files_passed << " passed" << cReset << ", "
                << cFail << files_failed << " failed" << cReset << " (" << total_files << " total)\n";
        }

        out << cBold << "Tests       " << cReset;
        if (total_cases_failed == 0) {
            out << cPass << total_cases_passed << " passed" << cReset << " (" << total_cases << " total)\n";
        } else {
            out << cPass << total_cases_passed << " passed" << cReset << ", "
                << cFail << total_cases_failed << " failed" << cReset << " (" << total_cases << " total)\n";
        }

        std::ostringstream dur_ss;
        dur_ss << std::fixed << std::setprecision(1) << suiteDurationMs;
        out << cBold << "Duration    " << cReset << dur_ss.str() << "ms\n";
    }

    return (files_failed > 0 || total_cases_failed > 0) ? 1 : 0;
}

std::vector<std::filesystem::path> CliDriver::discover_keet_files(const std::filesystem::path& target_path) {
    std::vector<std::filesystem::path> results;
    std::error_code ec;

    std::filesystem::path target = target_path.empty() ? std::filesystem::current_path() : target_path;
    if (!std::filesystem::exists(target, ec)) {
        return results;
    }

    if (std::filesystem::is_regular_file(target, ec)) {
        results.push_back(target);
        return results;
    }

    if (!std::filesystem::is_directory(target, ec)) {
        return results;
    }

    for (std::filesystem::recursive_directory_iterator it(target, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (ec) break;

        const auto& entry = *it;
        auto filename = entry.path().filename().string();

        if (entry.is_directory(ec)) {
            // Skip hidden directories (.git, .keety_build, etc.), build, node_modules, cmake-build-*
            if (!filename.empty() && filename[0] == '.') {
                it.disable_recursion_pending();
                continue;
            }
            if (filename == "build" || filename == "node_modules" || filename.rfind("cmake-build-", 0) == 0) {
                it.disable_recursion_pending();
                continue;
            }
            continue;
        }

        if (entry.is_regular_file(ec) && (entry.path().extension() == ".keet" || entry.path().extension() == ".kml")) {
            results.push_back(entry.path());
        }
    }

    std::sort(results.begin(), results.end());
    return results;
}

int CliDriver::execute_fmt(const CliOptions& options, std::ostream& out, std::ostream& err, std::istream& in) {
    keety::fmt::FormatterOptions fmt_opts;
    keety::fmt::Formatter formatter(fmt_opts);

    // Stdin mode
    if (options.fmt_stdin) {
        std::string original((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        diag::SourceManager sm;
        diag::DiagnosticEmitter emitter(sm, options.color_mode);
        std::string formatted = formatter.format(original, "<stdin>", &emitter);

        if (emitter.hasErrors()) {
            if (options.diagnostic_format == DiagnosticFormat::Json) {
                emitter.emitJson(out);
            } else {
                for (const auto& d : emitter.getDiagnostics()) {
                    err << emitter.format(d);
                }
            }
            return 1;
        }

        bool changed = (original != formatted);
        if (options.fmt_check) {
            if (changed) {
                err << "<stdin>\n";
                if (options.fmt_diff) {
                    std::string diff = keety::fmt::Formatter::generateDiff(original, formatted, "<stdin>");
                    out << diff;
                }
                return 1;
            }
            return 0;
        }

        if (options.fmt_diff) {
            if (changed) {
                std::string diff = keety::fmt::Formatter::generateDiff(original, formatted, "<stdin>");
                out << diff;
            }
            return 0;
        }

        out << formatted;
        return 0;
    }

    // File / Directory mode
    std::vector<std::filesystem::path> search_paths = options.fmt_paths;
    if (search_paths.empty()) {
        search_paths.emplace_back(".");
    }

    std::vector<std::filesystem::path> all_files;
    bool not_found_error = false;

    for (const auto& p : search_paths) {
        std::error_code ec;
        if (!std::filesystem::exists(p, ec)) {
            err << "error: path not found: " << p.string() << "\n";
            not_found_error = true;
            continue;
        }
        auto files = discover_keet_files(p);
        all_files.insert(all_files.end(), files.begin(), files.end());
    }

    if (not_found_error) {
        return 1;
    }

    // Deduplicate discovered files
    std::sort(all_files.begin(), all_files.end());
    all_files.erase(std::unique(all_files.begin(), all_files.end()), all_files.end());

    if (all_files.empty()) {
        if (options.verbose) {
            out << "[verbose] No .keet files found to format.\n";
        }
        return 0;
    }

    int unformatted_count = 0;
    int error_count = 0;

    for (const auto& file_path : all_files) {
        std::ifstream ifs(file_path, std::ios::binary);
        if (!ifs) {
            err << "error: unable to read file: " << file_path.string() << "\n";
            ++error_count;
            continue;
        }
        std::string original((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        diag::SourceManager sm;
        diag::DiagnosticEmitter emitter(sm, options.color_mode);
        std::string formatted = formatter.format(original, file_path.string(), &emitter);

        if (emitter.hasErrors()) {
            if (options.diagnostic_format == DiagnosticFormat::Json) {
                emitter.emitJson(out);
            } else {
                for (const auto& d : emitter.getDiagnostics()) {
                    err << emitter.format(d);
                }
            }
            ++error_count;
            continue;
        }

        bool changed = (original != formatted);
        if (changed) {
            ++unformatted_count;

            if (options.fmt_check) {
                err << file_path.string() << "\n";
            }

            if (options.fmt_diff) {
                std::string diff = keety::fmt::Formatter::generateDiff(original, formatted, file_path.string());
                out << diff;
            }

            if (options.fmt_write) {
                std::ofstream ofs(file_path, std::ios::binary | std::ios::trunc);
                if (!ofs) {
                    err << "error: failed to write to " << file_path.string() << "\n";
                    ++error_count;
                    continue;
                }
                ofs << formatted;
                ofs.close();

                if (options.verbose) {
                    out << "[verbose] Formatted " << file_path.string() << "\n";
                }
            }
        } else {
            if (options.verbose) {
                out << "[verbose] Clean " << file_path.string() << "\n";
            }
        }
    }

    if (error_count > 0) {
        return 1;
    }

    if (options.fmt_check && unformatted_count > 0) {
        return 1;
    }

    return 0;
}

int CliDriver::execute_lint(const CliOptions& options, std::ostream& out, std::ostream& err, std::istream& in) {
    // 1. Stdin mode
    if (options.lint_stdin) {
        std::string original((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        lint::LintConfig config = lint::LintConfig::discover(std::filesystem::current_path());
        if (options.lint_strict) {
            config.setStrict(true);
        }

        lint::LintEngine engine(config);
        engine.registerDefaultRules();

        if (options.lint_fix) {
            std::string content = original;
            for (int iter = 0; iter < 10; ++iter) {
                auto diags = engine.lintSource(content, "<stdin>");
                if (!apply_lint_fixes(content, diags)) {
                    break;
                }
            }
            out << content;

            auto remaining = engine.lintSource(content, "<stdin>");
            diag::SourceManager sm;
            sm.addBuffer("<stdin>", content);
            diag::DiagnosticEmitter emitter(sm, options.color_mode);
            for (const auto& d : remaining) {
                emitter.report(d.toCompilerDiagnostic());
            }

            if (emitter.hasErrors()) {
                return 1;
            }
            if (options.lint_check && emitter.getWarningCount() > 0) {
                return 1;
            }
            return 0;
        }

        diag::SourceManager sm;
        auto fileId = sm.addBuffer("<stdin>", original);
        lexer::Lexer lexer(fileId, original);
        lexer.setCaptureComments(true);
        diag::DiagnosticEmitter parseEmitter(sm, options.color_mode);
        parser::Parser parser(lexer, &parseEmitter);
        auto moduleAst = parser.parseModule("<stdin>");
        auto comments = lexer.getCapturedComments();

        if (parseEmitter.hasErrors()) {
            if (options.diagnostic_format == DiagnosticFormat::Json) {
                parseEmitter.emitJson(out);
            } else {
                for (const auto& d : parseEmitter.getDiagnostics()) {
                    err << parseEmitter.format(d);
                }
            }
            return 1;
        }

        auto diags = engine.lintModule(moduleAst, comments, sm, original, "<stdin>");

        diag::DiagnosticEmitter emitter(sm, options.color_mode);
        for (const auto& d : diags) {
            emitter.report(d.toCompilerDiagnostic());
        }

        if (options.diagnostic_format == DiagnosticFormat::Json) {
            emitter.emitJson(out);
        } else {
            for (const auto& d : emitter.getDiagnostics()) {
                err << emitter.format(d);
            }
        }

        if (emitter.hasErrors()) {
            return 1;
        }
        if (options.lint_check && emitter.getWarningCount() > 0) {
            return 1;
        }
        return 0;
    }

    // 2. File / Directory mode
    std::vector<std::filesystem::path> search_paths = options.lint_paths;
    if (search_paths.empty()) {
        search_paths.emplace_back(".");
    }

    std::vector<std::filesystem::path> all_files;
    bool not_found_error = false;

    for (const auto& p : search_paths) {
        std::error_code ec;
        if (!std::filesystem::exists(p, ec)) {
            err << "error: path not found: " << p.string() << "\n";
            not_found_error = true;
            continue;
        }
        auto files = discover_keet_files(p);
        all_files.insert(all_files.end(), files.begin(), files.end());
    }

    if (not_found_error) {
        return 1;
    }

    // Deduplicate discovered files
    std::sort(all_files.begin(), all_files.end());
    all_files.erase(std::unique(all_files.begin(), all_files.end()), all_files.end());

    if (all_files.empty()) {
        if (options.verbose) {
            out << "[verbose] No .keet files found to lint.\n";
        }
        if (options.diagnostic_format == DiagnosticFormat::Json) {
            out << "[]\n";
        }
        return 0;
    }

    diag::SourceManager sm;
    diag::DiagnosticEmitter emitter(sm, options.color_mode);
    int read_write_errors = 0;

    for (const auto& file_path : all_files) {
        lint::LintConfig config = lint::LintConfig::discover(file_path.parent_path());
        if (options.lint_strict) {
            config.setStrict(true);
        }
        if (config.isPathIgnored(file_path)) {
            continue;
        }

        std::ifstream ifs(file_path, std::ios::binary);
        if (!ifs) {
            err << "error: unable to read file: " << file_path.string() << "\n";
            ++read_write_errors;
            continue;
        }
        std::string source((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        lint::LintEngine engine(config);
        engine.registerDefaultRules();

        if (options.lint_fix) {
            bool fileChanged = false;
            for (int iter = 0; iter < 10; ++iter) {
                auto diags = engine.lintSource(source, file_path.string());
                if (!apply_lint_fixes(source, diags)) {
                    break;
                }
                fileChanged = true;
            }
            if (fileChanged) {
                std::ofstream ofs(file_path, std::ios::binary | std::ios::trunc);
                if (!ofs) {
                    err << "error: failed to write to " << file_path.string() << "\n";
                    ++read_write_errors;
                } else {
                    ofs << source;
                    ofs.close();
                    if (options.verbose) {
                        out << "[verbose] Fixed " << file_path.string() << "\n";
                    }
                }
            }
        }

        auto fileId = sm.addBuffer(file_path.string(), source);
        lexer::Lexer lexer(fileId, source);
        lexer.setCaptureComments(true);
        diag::DiagnosticEmitter parseEmitter(sm, options.color_mode);
        parser::Parser parser(lexer, &parseEmitter);
        auto moduleAst = parser.parseModule(file_path.string());
        auto comments = lexer.getCapturedComments();

        if (parseEmitter.hasErrors()) {
            for (const auto& d : parseEmitter.getDiagnostics()) {
                emitter.report(d);
                if (options.diagnostic_format != DiagnosticFormat::Json) {
                    err << emitter.format(d);
                }
            }
            continue;
        }

        auto diags = engine.lintModule(moduleAst, comments, sm, source, file_path.string());

        for (const auto& d : diags) {
            auto compDiag = d.toCompilerDiagnostic();
            emitter.report(compDiag);
            if (options.diagnostic_format != DiagnosticFormat::Json) {
                err << emitter.format(compDiag);
            }
        }

        if (options.verbose && diags.empty() && options.diagnostic_format != DiagnosticFormat::Json) {
            out << "[verbose] Clean " << file_path.string() << "\n";
        }
    }

    if (options.diagnostic_format == DiagnosticFormat::Json) {
        emitter.emitJson(out);
    }

    if (read_write_errors > 0 || emitter.hasErrors()) {
        return 1;
    }
    if (options.lint_check && emitter.getWarningCount() > 0) {
        return 1;
    }
    return 0;
}

int CliDriver::run(const CliOptions& options, std::ostream& out, std::ostream& err, std::istream& in) {
    switch (options.command) {
    case CliCommand::Help:
        print_help(out);
        return 0;
    case CliCommand::Version:
        print_version(out);
        return 0;
    case CliCommand::Check:
        return execute_check(options, out, err);
    case CliCommand::Emit:
        return execute_emit(options, out, err);
    case CliCommand::Build:
        return execute_build(options, out, err);
    case CliCommand::Run:
        return execute_run(options, out, err);
    case CliCommand::Test:
        return execute_test(options, out, err);
    case CliCommand::Fmt:
        return execute_fmt(options, out, err, in);
    case CliCommand::Lint:
        return execute_lint(options, out, err, in);
    case CliCommand::Unknown:
        err << "error: unknown command or missing arguments.\n"
            << "Run 'keety --help' for usage.\n";
        return 1;
    }
    return 1;
}

int CliDriver::run(int argc, char* argv[], std::ostream& out, std::ostream& err, std::istream& in) {
    auto options = parse_args(argc, argv);
    return run(options, out, err, in);
}

} // namespace keety::driver
