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
#include <keety/sema/universe.hpp>
#include <keety/typechecker/type_checker.hpp>
#include <keety/version.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

namespace keety::driver {

namespace {

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

} // namespace

std::string_view to_string(CliCommand cmd) noexcept {
    switch (cmd) {
    case CliCommand::Check:   return "check";
    case CliCommand::Emit:    return "emit";
    case CliCommand::Build:   return "build";
    case CliCommand::Run:     return "run";
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
        } else if (arg == "-o" || arg == "--output-dir") {
            if (idx + 1 < args.size()) {
                options.output_dir = args[++idx];
            }
        } else if (arg == "--cxx") {
            if (idx + 1 < args.size()) {
                options.cxx_compiler = args[++idx];
            }
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
        } else if (arg == "--opt" && idx + 1 < args.size()) {
            std::string opt_val = args[++idx];
            if (opt_val == "0") options.opt_level = OptimizationLevel::O0;
            else if (opt_val == "1") options.opt_level = OptimizationLevel::O1;
            else if (opt_val == "2") options.opt_level = OptimizationLevel::O2;
            else if (opt_val == "3") options.opt_level = OptimizationLevel::O3;
            else if (opt_val == "s") options.opt_level = OptimizationLevel::Os;
            else if (opt_val == "z") options.opt_level = OptimizationLevel::Oz;
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

    auto universe = std::make_shared<sema::Scope>(sema::ScopeKind::Universe);
    sema::registerUniverseSymbols(*universe);

    for (const auto* node : ordered_nodes) {
        if (!node || !node->ast) {
            continue;
        }

        auto module_scope = std::make_shared<sema::Scope>(sema::ScopeKind::Module, universe);
        typechecker::TypeChecker checker(&emitter);
        checker.checkTypes(*node->ast, module_scope.get());

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

    for (const auto* node : ordered_nodes) {
        if (!node || !node->ast) {
            continue;
        }

        if (node->canonicalPath == canonical_entry.string()) {
            target_node = node;
        }

        auto module_scope = std::make_shared<sema::Scope>(sema::ScopeKind::Module, universe);
        typechecker::TypeChecker checker(&emitter);
        checker.checkTypes(*node->ast, module_scope.get());

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
        if (std::filesystem::is_directory(options.output_dir) ||
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
        err << "error: 'keety build' is not yet implemented.\n";
        return 1;
    case CliCommand::Run:
        err << "error: 'keety run' is not yet implemented.\n";
        return 1;
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
