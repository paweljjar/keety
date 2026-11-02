#include <keety/pkg/cli.hpp>
#include <keety/pkg/diagnostics.hpp>
#include <keety/pkg/git_client.hpp>
#include <keety/pkg/installer.hpp>
#include <keety/pkg/lockfile.hpp>
#include <keety/pkg/manifest.hpp>
#include <keety/pkg/script_runner.hpp>
#include <keety/pkg/source_spec.hpp>
#include <keety/pkg/workspace.hpp>
#include <keety/version.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace keety::pkg {

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
        << "  build [<file>] [-- <args...>]        Compile the application entry point\n"
        << "  test, t [-- <args...>]               Execute the project test suite\n\n"
        << "Options:\n"
        << "  -V, --version                        Display kattery version\n"
        << "  -h, --help                           Display this help message\n"
        << "  --cwd <dir>                          Set the working directory\n"
        << "  -w, --workspace <name>               Target a specific workspace member\n"
        << "  --workspaces                         Run command in all workspace members\n"
        << "  --keep-going                         Continue executing remaining workspace members on error\n"
        << "  -D, --dev                            Add as a development dependency\n"
        << "  --frozen                             Fail if pedigree.lock is out of date\n"
        << "  --production                         Skip installing devDependencies\n"
        << "  -v, --verbose                        Enable verbose diagnostics\n";
}

std::string sanitizePackageName(std::string_view dirName) {
    std::string clean;
    for (char c : dirName) {
        if ((c >= 'A' && c <= 'Z')) {
            clean.push_back(static_cast<char>(c + ('a' - 'A')));
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
            clean.push_back(c);
        } else {
            clean.push_back('-');
        }
    }
    while (!clean.empty() && (clean.front() == '-' || clean.front() == '_')) {
        clean.erase(clean.begin());
    }
    if (clean.empty() || clean.front() < 'a' || clean.front() > 'z') {
        clean = "app-" + clean;
    }
    if (!isValidPackageName(clean)) {
        return "my-app";
    }
    return clean;
}

int handleInit(
    const std::filesystem::path& cwd,
    const std::vector<std::string>& positional,
    const std::optional<std::string>& explicitName,
    std::ostream& out
) {
    (void)positional;
    std::filesystem::path manifestPath = cwd / "pedigree.json";
    std::error_code ec;
    if (std::filesystem::exists(manifestPath, ec)) {
        throw PkgError(
            ErrorCode::PackageAlreadyExists,
            "pedigree.json already exists in '" + cwd.string() + "'",
            "To reinitialize, remove the existing pedigree.json first"
        );
    }

    std::string name;
    if (explicitName.has_value() && !explicitName->empty()) {
        validatePackageName(*explicitName, "init --name");
        name = *explicitName;
    } else {
        name = sanitizePackageName(cwd.filename().string());
    }

    Manifest m;
    m.name = name;
    m.version = "0.1.0";
    m.entry = "src/main.keet";
    m.scripts["start"] = "keety run src/main.keet --output-dir build";

    m.save(manifestPath);

    std::filesystem::path gitignorePath = cwd / ".gitignore";
    if (!std::filesystem::exists(gitignorePath, ec)) {
        std::ofstream gitignore(gitignorePath);
        gitignore << "# KeetScript package and build artifacts\n"
                  << "build/\n"
                  << "keet_modules/\n"
                  << ".keety_build/\n";
    }

    std::filesystem::path entryPath = cwd / m.entry;
    if (!std::filesystem::exists(entryPath, ec)) {
        std::filesystem::create_directories(entryPath.parent_path(), ec);
        std::ofstream entryFile(entryPath);
        entryFile << "import io;\n\n"
                  << "fn main(): void {\n"
                  << "    io.println(\"Hello, World!\");\n"
                  << "}\n";
    }

    out << "Initialized new KeetScript project '" << name << "' in " << cwd.string() << "\n";
    return 0;
}

int handleAdd(
    const std::filesystem::path& cwd,
    const std::vector<std::string>& positional,
    const std::optional<std::string>& workspaceMember,
    bool isDev,
    std::ostream& out
) {
    if (positional.size() < 2) {
        throw PkgError(
            ErrorCode::MissingArgument,
            "missing package name or source specification",
            "Usage: kattery add <name> <spec> [-D] [-w <member>]"
        );
    }

    const std::string& name = positional[0];
    validatePackageName(name, "add <name>");

    std::string rawSpec = positional[1];
    // Normalize bare git URLs
    if (rawSpec.rfind("https://", 0) == 0 || rawSpec.rfind("http://", 0) == 0 ||
        rawSpec.rfind("git@", 0) == 0 || rawSpec.rfind("ssh://", 0) == 0 ||
        rawSpec.rfind("file://", 0) == 0) {
        rawSpec = "git+" + rawSpec;
    }
    SourceSpec spec = SourceSpec::parse(rawSpec, "add <spec>");

    Workspace ws = Workspace::discover(cwd);

    Manifest* targetManifest = &ws.rootManifest;
    std::filesystem::path targetManifestPath = ws.rootDir / "pedigree.json";

    if (workspaceMember.has_value()) {
        const auto* m = ws.findMember(*workspaceMember);
        if (!m) {
            throw PkgError(
                ErrorCode::WorkspaceMemberMissing,
                "workspace member '" + *workspaceMember + "' not found in workspace"
            );
        }
        for (auto& member : ws.members) {
            if (member.name == *workspaceMember) {
                targetManifest = &member.manifest;
                targetManifestPath = member.directory / "pedigree.json";
                break;
            }
        }
    }

    // Transactional: record in memory first
    if (isDev) {
        targetManifest->devDependencies[name] = spec;
        targetManifest->dependencies.erase(name);
    } else {
        targetManifest->dependencies[name] = spec;
        targetManifest->devDependencies.erase(name);
    }

    // Install and verify dependencies
    GitClient gitClient;
    Installer installer(&gitClient);
    installer.install(ws);

    // Save manifest on success
    targetManifest->save(targetManifestPath);

    out << "Added " << (isDev ? "dev dependency " : "dependency ") << name
        << " (" << spec.toString() << ")\n";
    return 0;
}

int handleRemove(
    const std::filesystem::path& cwd,
    const std::vector<std::string>& positional,
    const std::optional<std::string>& workspaceMember,
    std::ostream& out
) {
    if (positional.empty()) {
        throw PkgError(
            ErrorCode::MissingArgument,
            "missing package name to remove",
            "Usage: kattery remove <name> [-w <member>]"
        );
    }

    const std::string& name = positional[0];
    Workspace ws = Workspace::discover(cwd);

    Manifest* targetManifest = &ws.rootManifest;
    std::filesystem::path targetManifestPath = ws.rootDir / "pedigree.json";

    if (workspaceMember.has_value()) {
        bool found = false;
        for (auto& member : ws.members) {
            if (member.name == *workspaceMember) {
                targetManifest = &member.manifest;
                targetManifestPath = member.directory / "pedigree.json";
                found = true;
                break;
            }
        }
        if (!found) {
            throw PkgError(
                ErrorCode::WorkspaceMemberMissing,
                "workspace member '" + *workspaceMember + "' not found in workspace"
            );
        }
    }

    if (!targetManifest->hasDependency(name)) {
        throw PkgError(
            ErrorCode::PackageNotFound,
            "package '" + name + "' is not declared in " + targetManifestPath.filename().string()
        );
    }

    targetManifest->dependencies.erase(name);
    targetManifest->devDependencies.erase(name);

    GitClient gitClient;
    Installer installer(&gitClient);
    installer.install(ws);

    targetManifest->save(targetManifestPath);
    out << "Removed dependency " << name << "\n";
    return 0;
}

int handleInstall(
    const std::filesystem::path& cwd,
    bool frozen,
    bool production,
    bool verbose,
    std::ostream& out
) {
    Workspace ws = Workspace::discover(cwd);

    GitClient gitClient;
    Installer installer(&gitClient);

    InstallOptions opt;
    opt.frozen = frozen;
    opt.production = production;
    opt.verbose = verbose;

    InstallResult res = installer.install(ws, opt);

    out << "Installed " << res.installedPackages.size() << " package(s)";
    if (!res.linkedWorkspaceMembers.empty()) {
        out << ", linked " << res.linkedWorkspaceMembers.size() << " workspace member(s)";
    }
    if (!res.prunedPackages.empty()) {
        out << ", pruned " << res.prunedPackages.size() << " package(s)";
    }
    out << "\n";
    return 0;
}

int handleUpdate(
    const std::filesystem::path& cwd,
    const std::vector<std::string>& positional,
    std::ostream& out
) {
    Workspace ws = Workspace::discover(cwd);

    std::filesystem::path lockPath = ws.rootDir / "pedigree.lock";
    std::error_code ec;

    if (std::filesystem::is_regular_file(lockPath, ec)) {
        try {
            Lockfile lock = Lockfile::load(lockPath);
            if (!positional.empty()) {
                // Update only specified package
                const std::string& targetName = positional[0];
                lock.packages.erase(targetName);
            } else {
                // Clear all git packages to force re-resolving latest commits
                for (auto it = lock.packages.begin(); it != lock.packages.end(); ) {
                    if (it->second.isGit()) {
                        it = lock.packages.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            lock.save(lockPath);
        } catch (...) {
            std::filesystem::remove(lockPath, ec);
        }
    }

    GitClient gitClient;
    Installer installer(&gitClient);
    InstallResult res = installer.install(ws);

    out << "Updated " << res.installedPackages.size() << " package(s)\n";
    return 0;
}

int handleList(
    const std::filesystem::path& cwd,
    bool tree,
    std::ostream& out
) {
    (void)tree;
    Workspace ws = Workspace::discover(cwd);

    out << ws.rootManifest.name << "@" << ws.rootManifest.version << "\n";

    std::filesystem::path lockPath = ws.rootDir / "pedigree.lock";
    std::error_code ec;
    if (std::filesystem::is_regular_file(lockPath, ec)) {
        try {
            Lockfile lock = Lockfile::load(lockPath);
            if (!lock.packages.empty()) {
                out << "packages:\n";
                for (const auto& [name, pkg] : lock.packages) {
                    out << "  - " << name;
                    if (pkg.isGit()) {
                        out << " (git: " << pkg.commit.substr(0, 7) << ", " << pkg.url << ")";
                    } else {
                        out << " (path: " << pkg.path << ")";
                    }
                    out << "\n";
                }
            }
        } catch (...) {
            // Non-fatal if lockfile cannot be parsed
        }
    }

    if (ws.hasMembers()) {
        out << "workspace members:\n";
        for (const auto& m : ws.members) {
            out << "  - " << m.name << "@" << m.manifest.version << " (" << m.relativePath.string() << ")\n";
        }
    }
    return 0;
}

int handleRun(
    const std::filesystem::path& cwd,
    const std::vector<std::string>& positional,
    const std::vector<std::string>& forwardedArgs,
    const std::optional<std::string>& workspaceMember,
    bool runWorkspaces,
    bool keepGoing,
    bool captureOutput,
    std::ostream& out,
    std::ostream& err
) {
    Workspace ws = Workspace::discover(cwd);

    const Manifest* targetManifest = &ws.rootManifest;
    std::filesystem::path targetDir = ws.rootDir;

    if (workspaceMember.has_value()) {
        const auto* m = ws.findMember(*workspaceMember);
        if (!m) {
            throw PkgError(
                ErrorCode::WorkspaceMemberMissing,
                "workspace member '" + *workspaceMember + "' not found in workspace"
            );
        }
        targetManifest = &m->manifest;
        targetDir = m->directory;
    } else if (const auto* m = ws.findMemberByPath(cwd)) {
        targetManifest = &m->manifest;
        targetDir = m->directory;
    }

    if (positional.empty()) {
        if (targetManifest->scripts.empty()) {
            out << "No scripts defined in pedigree.json\n";
        } else {
            out << "Available scripts:\n";
            for (const auto& [name, cmd] : targetManifest->scripts) {
                out << "  - " << name << ": " << cmd << "\n";
            }
        }
        return 0;
    }

    const std::string& scriptName = positional[0];
    std::vector<std::string> extraArgs;
    for (size_t i = 1; i < positional.size(); ++i) {
        extraArgs.push_back(positional[i]);
    }
    for (const auto& a : forwardedArgs) {
        extraArgs.push_back(a);
    }

    if (runWorkspaces) {
        if (!ws.hasMembers()) {
            return ScriptRunner::runScript(
                ws.rootManifest, ws.rootDir, scriptName, extraArgs, captureOutput, out, err
            );
        }

        int lastFailure = 0;
        bool foundAny = false;
        for (const auto& m : ws.members) {
            auto it = m.manifest.scripts.find(scriptName);
            if (it != m.manifest.scripts.end()) {
                foundAny = true;
                out << "> " << m.name << ": " << scriptName << "\n";
                int code = ScriptRunner::runScript(
                    m.manifest, m.directory, scriptName, extraArgs, captureOutput, out, err
                );
                if (code != 0) {
                    lastFailure = code;
                    if (!keepGoing) {
                        return code;
                    }
                }
            }
        }

        if (!foundAny) {
            err << "error: script '" << scriptName << "' not found in any workspace member\n";
            return 127;
        }
        return lastFailure;
    }

    return ScriptRunner::runScript(
        *targetManifest, targetDir, scriptName, extraArgs, captureOutput, out, err
    );
}

int handleBuild(
    const std::filesystem::path& cwd,
    const std::vector<std::string>& positional,
    const std::vector<std::string>& forwardedArgs,
    const std::optional<std::string>& workspaceMember,
    bool captureOutput,
    std::ostream& out,
    std::ostream& err
) {
    auto keetyExe = ScriptRunner::locateKeetyExecutable();
    if (!keetyExe.has_value()) {
        throw PkgError(
            ErrorCode::ScriptExecutionFailed,
            "keety executable not found",
            "Ensure 'keety' is installed and in your PATH"
        );
    }

    Workspace ws = Workspace::discover(cwd);
    const Manifest* targetManifest = &ws.rootManifest;
    std::filesystem::path targetDir = ws.rootDir;

    if (workspaceMember.has_value()) {
        const auto* m = ws.findMember(*workspaceMember);
        if (!m) {
            throw PkgError(
                ErrorCode::WorkspaceMemberMissing,
                "workspace member '" + *workspaceMember + "' not found in workspace"
            );
        }
        targetManifest = &m->manifest;
        targetDir = m->directory;
    } else if (const auto* m = ws.findMemberByPath(cwd)) {
        targetManifest = &m->manifest;
        targetDir = m->directory;
    }

    ScriptRunner::checkKeetyVersionConstraint(*targetManifest, err);

    std::string entryFile;
    if (!positional.empty()) {
        entryFile = positional[0];
    } else if (!targetManifest->entry.empty()) {
        entryFile = targetManifest->entry;
    } else {
        entryFile = "src/main.keet";
    }

    std::vector<std::string> argv = {keetyExe->string(), "build", entryFile};
    for (size_t i = 1; i < positional.size(); ++i) {
        argv.push_back(positional[i]);
    }
    for (const auto& a : forwardedArgs) {
        argv.push_back(a);
    }

    ProcessOptions popt;
    popt.argv = std::move(argv);
    popt.cwd = targetDir;
    popt.captureOutput = captureOutput;
    ScriptRunner::injectKeetyPath(popt.env, keetyExe->parent_path());

    ProcessResult pres = ProcessRunner::run(popt);
    if (captureOutput) {
        out << pres.stdoutText;
        err << pres.stderrText;
    }
    return pres.exitCode;
}

int handleTest(
    const std::filesystem::path& cwd,
    const std::vector<std::string>& positional,
    const std::vector<std::string>& forwardedArgs,
    const std::optional<std::string>& workspaceMember,
    bool runWorkspaces,
    bool keepGoing,
    bool captureOutput,
    std::ostream& out,
    std::ostream& err
) {
    Workspace ws = Workspace::discover(cwd);

    if (runWorkspaces) {
        if (!ws.hasMembers()) {
            if (ws.rootManifest.scripts.find("test") == ws.rootManifest.scripts.end()) {
                throw PkgError(
                    ErrorCode::TestScriptNotDefined,
                    "no 'test' script defined in pedigree.json",
                    "Add a \"test\" script to the \"scripts\" section of pedigree.json"
                );
            }
            std::vector<std::string> pos = {"test"};
            pos.insert(pos.end(), positional.begin(), positional.end());
            return handleRun(cwd, pos, forwardedArgs, workspaceMember, false, keepGoing, captureOutput, out, err);
        }

        std::vector<std::string> pos = {"test"};
        pos.insert(pos.end(), positional.begin(), positional.end());
        return handleRun(cwd, pos, forwardedArgs, workspaceMember, true, keepGoing, captureOutput, out, err);
    }

    const Manifest* targetManifest = &ws.rootManifest;
    std::filesystem::path targetDir = ws.rootDir;

    if (workspaceMember.has_value()) {
        const auto* m = ws.findMember(*workspaceMember);
        if (!m) {
            throw PkgError(
                ErrorCode::WorkspaceMemberMissing,
                "workspace member '" + *workspaceMember + "' not found in workspace"
            );
        }
        targetManifest = &m->manifest;
        targetDir = m->directory;
    } else if (const auto* m = ws.findMemberByPath(cwd)) {
        targetManifest = &m->manifest;
        targetDir = m->directory;
    }

    if (targetManifest->scripts.find("test") == targetManifest->scripts.end()) {
        throw PkgError(
            ErrorCode::TestScriptNotDefined,
            "no 'test' script defined in pedigree.json",
            "Add a \"test\" script to the \"scripts\" section of pedigree.json, e.g. \"test\": \"keety test\""
        );
    }

    std::vector<std::string> pos = {"test"};
    pos.insert(pos.end(), positional.begin(), positional.end());
    return handleRun(cwd, pos, forwardedArgs, workspaceMember, false, keepGoing, captureOutput, out, err);
}

} // namespace

int Cli::run(int argc, char* argv[], std::ostream& out, std::ostream& err) {
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc > 1 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return run(args, out, err);
}

int Cli::run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
    std::filesystem::path cwd = std::filesystem::current_path();
    std::optional<std::string> workspaceMember;
    std::optional<std::string> explicitName;
    bool frozen = false;
    bool production = false;
    bool verbose = false;
    bool isDev = false;
    bool tree = false;
    bool runWorkspaces = false;
    bool keepGoing = false;

    std::string command;
    std::vector<std::string> positional;
    std::vector<std::string> forwardedArgs;

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];

        if (arg == "--") {
            for (size_t j = i + 1; j < args.size(); ++j) {
                forwardedArgs.push_back(args[j]);
            }
            break;
        }

        if (arg == "-h" || arg == "--help" || arg == "help") {
            printHelp(out);
            return 0;
        }
        if (arg == "-V" || arg == "--version" || arg == "version") {
            printVersion(out);
            return 0;
        }

        if (arg == "--cwd") {
            if (i + 1 >= args.size()) {
                err << "error: --cwd requires a directory path\n";
                return 1;
            }
            cwd = std::filesystem::absolute(args[++i]);
            continue;
        }
        if (arg == "-w" || arg == "--workspace") {
            if (i + 1 >= args.size()) {
                err << "error: --workspace requires a member name\n";
                return 1;
            }
            workspaceMember = args[++i];
            continue;
        }
        if (arg == "--workspaces") {
            runWorkspaces = true;
            continue;
        }
        if (arg == "--keep-going") {
            keepGoing = true;
            continue;
        }
        if (arg == "--name") {
            if (i + 1 >= args.size()) {
                err << "error: --name requires a package name\n";
                return 1;
            }
            explicitName = args[++i];
            continue;
        }
        if (arg == "-D" || arg == "--dev") {
            isDev = true;
            continue;
        }
        if (arg == "--frozen") {
            frozen = true;
            continue;
        }
        if (arg == "--production") {
            production = true;
            continue;
        }
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
            continue;
        }
        if (arg == "--tree") {
            tree = true;
            continue;
        }
        if (arg == "-y" || arg == "--yes") {
            continue;
        }

        if (command.empty()) {
            command = arg;
        } else {
            positional.push_back(arg);
        }
    }

    if (command.empty()) {
        printHelp(out);
        return 0;
    }

    bool captureOutput = (&out != &std::cout || &err != &std::cerr);

    try {
        if (command == "init") {
            return handleInit(cwd, positional, explicitName, out);
        }
        if (command == "add" || command == "adopt") {
            return handleAdd(cwd, positional, workspaceMember, isDev, out);
        }
        if (command == "remove" || command == "release" || command == "rm") {
            return handleRemove(cwd, positional, workspaceMember, out);
        }
        if (command == "install" || command == "i") {
            return handleInstall(cwd, frozen, production, verbose, out);
        }
        if (command == "update" || command == "up") {
            return handleUpdate(cwd, positional, out);
        }
        if (command == "list" || command == "ls") {
            return handleList(cwd, tree, out);
        }
        if (command == "run" || command == "r") {
            return handleRun(cwd, positional, forwardedArgs, workspaceMember, runWorkspaces, keepGoing, captureOutput, out, err);
        }
        if (command == "build") {
            return handleBuild(cwd, positional, forwardedArgs, workspaceMember, captureOutput, out, err);
        }
        if (command == "test" || command == "t") {
            return handleTest(cwd, positional, forwardedArgs, workspaceMember, runWorkspaces, keepGoing, captureOutput, out, err);
        }

        throw PkgError(
            ErrorCode::UnknownCommand,
            "unrecognized command '" + command + "'",
            "Run 'kattery --help' to see available commands"
        );
    } catch (const PkgError& errObj) {
        err << errObj.diagnostic().format() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        err << "error: " << ex.what() << "\n";
        return 1;
    }
}


} // namespace keety::pkg
