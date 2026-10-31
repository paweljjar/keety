#include <keety/driver/cli_driver.hpp>

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

#include <iostream>

namespace keety::driver {

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
        // Default to check if file given without explicit command
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
        err << "error: 'keety emit' is not yet implemented.\n";
        return 1;
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
