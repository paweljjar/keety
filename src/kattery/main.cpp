#include <keety/version.hpp>
#include <keety/pkg/diagnostics.hpp>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void printVersion(std::ostream& out) {
    out << "kattery - KeetScript package manager v" << keety::getVersionString() << "\n";
}

void printHelp(std::ostream& out) {
    out << "Usage: kattery <command> [options]\n\n"
        << "The official package manager for KeetScript programs.\n\n"
        << "Commands:\n"
        << "  init [--name <n>] [--yes]            Scaffold a new pedigree.json manifest\n"
        << "  add, adopt <name> <spec> [-D]        Add a dependency (git or local path)\n"
        << "  remove, release, rm <name>           Remove an installed dependency\n"
        << "  install, i [--frozen] [--production] Install all dependencies from pedigree.lock\n"
        << "  update, up [<name>]                  Update locked git dependencies to latest ref\n"
        << "  list, ls [--tree]                    List installed packages\n"
        << "  run, r <script> [-- <args...>]       Run a custom script defined in pedigree.json\n"
        << "  build [<file>]                       Compile the application entry point\n"
        << "  test                                 Execute the project test suite\n\n"
        << "Options:\n"
        << "  -V, --version                        Display kattery version\n"
        << "  -h, --help                           Display this help message\n"
        << "  --cwd <dir>                          Set the working directory\n"
        << "  -w, --workspace <name>               Target a specific workspace member\n"
        << "  -v, --verbose                        Enable verbose diagnostics\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string_view> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (args.empty() || args[0] == "-h" || args[0] == "--help" || args[0] == "help") {
        printHelp(std::cout);
        return 0;
    }

    if (args[0] == "-V" || args[0] == "--version" || args[0] == "version") {
        printVersion(std::cout);
        return 0;
    }

    std::string_view cmd = args[0];
    if (cmd == "init" || cmd == "add" || cmd == "adopt" || cmd == "remove" ||
        cmd == "release" || cmd == "rm" || cmd == "install" || cmd == "i" ||
        cmd == "update" || cmd == "up" || cmd == "list" || cmd == "ls" ||
        cmd == "run" || cmd == "r" || cmd == "build" || cmd == "test" || cmd == "t") {
        std::cerr << "kattery: command '" << cmd << "' will be enabled in Phase 3/4.\n";
        return 0;
    }

    std::cerr << "kattery: unrecognized command '" << cmd << "'. Run 'kattery --help' for usage.\n";
    return 1;
}
