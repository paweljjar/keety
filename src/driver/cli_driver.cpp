#include <keety/driver/cli_driver.hpp>

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
#include <keety/version.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace keety::driver {

namespace {

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
    const char* env_dir = std::getenv("KEETY_RUNTIME_DIR");
    if (env_dir && std::filesystem::exists(std::filesystem::path(env_dir) / "keet" / "runtime.hpp")) {
        return env_dir;
    }
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

} // namespace

std::string_view to_string(CliCommand cmd) noexcept {
    switch (cmd) {
    case CliCommand::Check:   return "check";
    case CliCommand::Emit:    return "emit";
    case CliCommand::Build:   return "build";
    case CliCommand::Run:     return "run";
    case CliCommand::Test:    return "test";
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
    }

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
        } else if (arg == "--benchmarks") {
            options.test_benchmarks = true;
        } else if (arg == "--conformance") {
            options.test_conformance = true;
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
        } else if (!arg.empty() && arg[0] != '-') {
            if (options.input_file.empty()) {
                options.input_file = arg;
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
    out << "Usage: keety <command> [options] <file.keet>\n\n"
        << "Commands:\n"
        << "  check <file>                         Fast IDE-grade semantic and type validation\n"
        << "  emit <file> (--cpp|--hpp|--ast|--ir) Emit generated C++ or intermediate representations\n"
        << "  build <file>                         Compile KeetScript source into a native binary\n"
        << "  run <file> [-- args]                 Compile and immediately execute the target binary\n"
        << "  test [--benchmarks|--conformance]    Run performance benchmarks or conformance test suite\n"
        << "  version                              Display keety version information\n"
        << "  help                                 Display this help message\n\n"
        << "Options:\n"
        << "  -o, --output-dir <dir>               Target output directory for emitted sources or binaries\n"
        << "  --cxx <compiler>                     Explicit native C++ compiler binary (clang++, g++, cl.exe)\n"
        << "  -O, --opt <0|1|2|3|s|z>              Optimization level (default: 3)\n"
        << "  --wrapv                              Enforce two's-complement overflow semantics\n"
        << "  -v, --verbose                        Enable verbose diagnostics and phase timings\n"
        << "  --no-color                           Disable ANSI color escape sequences in output\n"
        << "  -h, --help                           Display usage documentation\n"
        << "  -V, --version                        Display version number\n";
}

int CliDriver::execute_check(const CliOptions& options, std::ostream& out, std::ostream& err) {
    if (options.input_file.empty()) {
        err << "error: 'keety check' requires a target input file.\n"
            << "usage: keety check <file.keet>\n";
        return 1;
    }

    if (!std::filesystem::exists(options.input_file)) {
        err << "error: input file not found: " << options.input_file.string() << "\n";
        return 1;
    }

    diag::SourceManager sm;
    diag::DiagnosticEmitter emitter(sm, options.color_mode);

    std::filesystem::path project_root = options.input_file.has_parent_path()
        ? options.input_file.parent_path()
        : std::filesystem::current_path();

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

    for (const auto* node : ordered_nodes) {
        if (!node || !node->ast) {
            continue;
        }

        if (options.verbose) {
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

        ir::AstLowerer lowerer(&emitter);
        auto ir_mod = lowerer.lower_module(*node->ast);

        if (emitter.hasErrors()) {
            for (const auto& d : emitter.getDiagnostics()) {
                err << emitter.format(d);
            }
            return 1;
        }
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

    std::filesystem::path project_root = options.input_file.has_parent_path()
        ? options.input_file.parent_path()
        : std::filesystem::current_path();

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
        emitter::HeaderEmitter header_emitter(*target_node->ast, *ir_mod);
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
        emitter::CppEmitter cpp_emitter(*target_node->ast, *ir_mod);
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

    std::filesystem::path project_root = options.input_file.has_parent_path()
        ? options.input_file.parent_path()
        : std::filesystem::current_path();

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

    if (!options.output_dir.empty()) {
        std::error_code ec;
        if (options.output_is_dir ||
            std::filesystem::is_directory(options.output_dir) ||
            options.output_dir.string().back() == '/' ||
            options.output_dir.string().back() == '\\') {
            std::filesystem::create_directories(options.output_dir, ec);
            target_binary = options.output_dir / (options.input_file.stem().string() + exec_ext);
            build_dir = options.output_dir / ".keety_build";
        } else {
            target_binary = options.output_dir;
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
        emitter::HeaderEmitter header_emitter(*node->ast, *ir_mod);
        std::string hpp_content = header_emitter.emit();
        std::filesystem::path hpp_path = build_dir / (node->moduleName + ".hpp");
        std::ofstream hpp_file(hpp_path);
        hpp_file << hpp_content;

        // C++ source emission
        emitter::CppEmitterOptions cpp_opts;
        cpp_opts.header_include = "\"" + node->moduleName + ".hpp\"";
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
    comp_opts.cpp_standard = "c++17";
    comp_opts.compile_only = true;
    comp_opts.include_directories.push_back(build_dir);

    if (options.verbose) {
        out << "[verbose] Optimization level: " << to_string(options.opt_level)
            << " (flag: " << to_compiler_flag(options.opt_level, toolchain_driver.get_toolchain().kind) << ")\n";
        out << "[verbose] Two's-complement overflow semantics: " << (options.wrapv ? "enabled" : "disabled") << "\n";
    }

    auto runtime_dir = find_runtime_directory();
    if (!runtime_dir.empty()) {
        comp_opts.include_directories.push_back(runtime_dir);
        if (options.verbose) {
            out << "[verbose] KeetScript runtime include directory: " << runtime_dir.string() << "\n";
        }
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
            err << "error: compilation failed for '" << cpp_src.filename().string() << "':\n"
                << comp_res.output << "\n";
            return std::nullopt;
        }
        object_files.push_back(obj_path);
    }

    LinkOptions link_opts;
    link_opts.kind = BinaryKind::Executable;
    if (options.verbose) {
        out << "[verbose] Linking " << object_files.size() << " object file(s) into "
            << target_binary.filename().string() << "\n";
    }
    auto link_res = toolchain_driver.link_executable(object_files, target_binary, link_opts);
    if (!link_res.success) {
        err << "error: linking failed for '" << target_binary.filename().string() << "':\n"
            << link_res.output << "\n";
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

int CliDriver::execute_test(const CliOptions& options, std::ostream& out, std::ostream& err) {
    (void)err;
    bool run_benchmarks = options.test_benchmarks;
    bool run_conformance = options.test_conformance;

    if (!run_benchmarks && !run_conformance) {
        run_benchmarks = true;
        run_conformance = true;
    }

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

int CliDriver::run(const CliOptions& options, std::ostream& out, std::ostream& err) {
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
    case CliCommand::Unknown:
        err << "error: unknown command or missing arguments.\n"
            << "Run 'keety --help' for usage.\n";
        return 1;
    }
    return 1;
}

int CliDriver::run(int argc, char* argv[], std::ostream& out, std::ostream& err) {
    auto options = parse_args(argc, argv);
    return run(options, out, err);
}

} // namespace keety::driver
