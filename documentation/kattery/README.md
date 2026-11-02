# Kattery — The KeetScript Package & Workspace Manager

**Kattery** (`kattery` v0.1.0) is the official package manager and workspace orchestrator for the **KeetScript** programming language.

In the KeetScript toolchain, responsibilities are cleanly divided:
- **`keety`** is the ahead-of-time (AOT) compiler and toolchain driver that translates `.keet` files into C++20 and builds native executables.
- **`kattery`** manages dependencies, workspaces, project manifests (`pedigree.json`), reproducible lockfiles (`pedigree.lock`), and script execution.

---

## Table of Contents

- [Quick Start](#quick-start)
- [CLI Command Reference](#cli-command-reference)
  - [`kattery init`](#kattery-init)
  - [`kattery add` / `adopt`](#kattery-add--adopt)
  - [`kattery remove` / `release` / `rm`](#kattery-remove--release--rm)
  - [`kattery install` / `i`](#kattery-install--i)
  - [`kattery update` / `up`](#kattery-update--up)
  - [`kattery list` / `ls`](#kattery-list--ls)
  - [`kattery run` / `r`](#kattery-run--r)
  - [`kattery build`](#kattery-build)
  - [`kattery test` / `t`](#kattery-test--t)
- [Global Options](#global-options)
- [`pedigree.json` Manifest Reference](#pedigreejson-manifest-reference)
- [`pedigree.lock` & Reproducibility](#pedigreelock--reproducibility)
- [Monorepo Workspaces](#monorepo-workspaces)
- [Authoring a KeetScript Package](#authoring-a-keetscript-package)
- [Security & Offline Execution](#security--offline-execution)

---

## Quick Start

Initialize a new project:

```bash
mkdir my-app && cd my-app
kattery init
```

Add a Git or local path dependency:

```bash
# Add a remote Git package
kattery add mathx git+https://github.com/example/mathx.git#v1.0.0

# Add a local path dependency
kattery add utils path:../shared-utils
```

Install locked dependencies:

```bash
kattery install
```

Run defined scripts or build:

```bash
kattery run start
kattery build
kattery test
```

---

## CLI Command Reference

### `kattery init`
Scaffolds a new `pedigree.json` manifest and `.gitignore` in the target directory.

```bash
kattery init [--name <name>] [--yes]
```

- `--name <name>`: Explicit package name (defaults to sanitized directory name).
- `--yes`, `-y`: Skip interactive defaults.
- Refuses to overwrite an existing `pedigree.json`.

### `kattery add` / `adopt`
Resolves, downloads, installs, and records a new runtime or development dependency.

```bash
kattery add <name> <spec> [-D] [-w <member>]
```

- `<name>`: Package identifier matching `^[a-z][a-z0-9_-]{0,63}$`.
- `<spec>`: Source specification:
  - `git+https://github.com/user/repo.git[#ref]`
  - `git+ssh://git@github.com/user/repo.git[#ref]`
  - `git+file:///path/to/repo.git[#ref]`
  - `path:../relative/directory`
  - Bare URLs (`https://...`, `git@...`) are automatically normalized to `git+...`.
- `-D`, `--dev`: Adds to `devDependencies` instead of `dependencies`.
- `-w`, `--workspace <member>`: Targets a specific workspace member manifest.

### `kattery remove` / `release` / `rm`
Removes a declared dependency from `pedigree.json`, prunes unreferenced packages from `keet_modules/`, and updates `pedigree.lock`.

```bash
kattery remove <name> [-w <member>]
```

### `kattery install` / `i`
Reconciles `keet_modules/` with `pedigree.lock` and `pedigree.json`. Generates `pedigree.lock` if absent or stale.

```bash
kattery install [--frozen] [--production]
```

- `--frozen`: Strict CI mode. Fails with `KAT3003` if `pedigree.lock` is out of date or would be modified.
- `--production`: Skips installing packages listed in `devDependencies`.

### `kattery update` / `up`
Re-resolves Git references (branch tips / tags) to the latest commits and rewrites `pedigree.lock`.

```bash
kattery update [<name>]
```

- With `<name>`: Updates only the designated package.
- Without arguments: Updates all external Git dependencies.

### `kattery list` / `ls`
Displays all installed packages and resolved workspace members.

```bash
kattery list [--tree]
```

- `--tree`: Displays dependencies hierarchically.

### `kattery run` / `r`
Executes custom scripts defined in `pedigree.json` using the host system shell (`/bin/sh -c` on POSIX, `cmd /C` on Windows).

```bash
kattery run [<script>] [-- <forwarded_args...>]
```

- Without `<script>`: Lists all available scripts defined in `pedigree.json`.
- Prepends the directory containing the `keety` executable to `PATH` so scripts can invoke `keety` directly without absolute paths.
- Propagates exit codes directly (`127` if the script name is not found).
- `--workspaces`: Executes the script across all workspace members declaring it.
- `--keep-going`: When combined with `--workspaces`, continues executing remaining packages even if one exits with an error.

### `kattery build`
Compiles the application entry point using the underlying `keety build` compiler pipeline.

```bash
kattery build [<file>] [-- <keety_args...>]
```

- Uses `<file>` if provided; otherwise falls back to `entry` in `pedigree.json` (defaults to `src/main.keet`).
- Forwards extra arguments directly to `keety build`.

### `kattery test` / `t`
Runs the project test suite.

```bash
kattery test [-- <args...>]
```

- Executes `scripts.test` from `pedigree.json`.
- Throws error code `KAT4003` with a helpful hint if no `test` script is defined.
- Supports `--workspaces` and `--keep-going`.

---

## Global Options

| Flag | Description |
| :--- | :--- |
| `--cwd <dir>` | Runs the command as if launched from `<dir>` |
| `-w, --workspace <name>` | Targets a specific member package in a workspace |
| `--workspaces` | Runs the command across all workspace member packages |
| `--keep-going` | Continues executing remaining workspace members on error |
| `-D, --dev` | Flags dependency as development-only |
| `--frozen` | Fails if `pedigree.lock` requires updates (CI mode) |
| `--production` | Excludes `devDependencies` from installation |
| `-v, --verbose` | Prints verbose operational diagnostics |
| `-V, --version` | Displays `kattery` version |
| `-h, --help` | Displays help information |

---

## `pedigree.json` Manifest Reference

The `pedigree.json` file defines project metadata, dependencies, and build scripts:

```json
{
  "name": "my-service",
  "version": "1.0.0",
  "description": "High-performance microservice written in KeetScript",
  "entry": "src/main.keet",
  "keety": ">=0.1.0",
  "dependencies": {
    "mathx": "git+https://github.com/keetscript/mathx.git#v1.2.0",
    "shared-utils": "path:../shared-utils"
  },
  "devDependencies": {
    "test-runner": "git+https://github.com/keetscript/test-runner.git#main"
  },
  "scripts": {
    "start": "keety run src/main.keet",
    "test": "keety test",
    "build": "keety build src/main.keet -O3"
  },
  "workspaces": [
    "packages/*"
  ]
}
```

### Fields

| Field | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| `name` | string | Yes | Unique package name (1-64 lowercase alphanumeric, `-`, `_`). Cannot collide with stdlib (`io`, `fs`, `sys`, `math`, `time`, `json`, `net`). |
| `version` | string | Yes | Semver version string (`MAJOR.MINOR.PATCH`). |
| `description` | string | No | Descriptive summary. |
| `entry` | string | No | Application entry `.keet` file (defaults to `src/main.keet`). |
| `keety` | string | No | Compiler version constraint in `^>=X.Y.Z$` format. |
| `dependencies` | object | No | Map of package name to source specification. |
| `devDependencies` | object | No | Development-only dependencies (skipped with `--production`). |
| `scripts` | object | No | Map of script names to shell commands. |
| `workspaces` | array | No | Glob patterns or directories for monorepo member discovery (root only). |

---

## `pedigree.lock` & Reproducibility

`pedigree.lock` guarantees fully reproducible, byte-for-byte identical installations across platforms:

```json
{
  "lockVersion": 1,
  "packages": {
    "mathx": {
      "source": "git",
      "url": "https://github.com/keetscript/mathx.git",
      "ref": "v1.2.0",
      "commit": "9fceb02d0ae598e95dc970b74767f19372d61af8",
      "tree": "4b825dc642cb6eb9a060e54bf8d69288fbee4904",
      "dependencies": []
    },
    "shared-utils": {
      "source": "path",
      "path": "../shared-utils",
      "dependencies": []
    }
  }
}
```

- **Canonical Sorting:** Packages and keys are sorted deterministically.
- **Tree Hash Integrity:** After Git checkout, `kattery` computes the Git tree hash and validates it against the locked `tree` hash. Any tampering or corrupted checkout fails with error `KAT2005`.
- **State Tracking:** Installed packages are tracked in `keet_modules/.kattery.json` so unmanaged files in `keet_modules/` are never deleted.

---

## Monorepo Workspaces

Kattery natively supports monorepo workspaces:

1. **Root Declaration:** The workspace root manifest defines `"workspaces": ["packages/*", "apps/cli"]`.
2. **Hoisting:** Dependencies declared across member packages are hoisted and installed once at `<workspace-root>/keet_modules/`.
3. **Automatic Linking:** Each workspace member is linked at `<workspace-root>/keet_modules/<member-name>`.
4. **Resolution by `keety`:** The compiler's `findProjectRoot` walks upward to locate `<workspace-root>/keet_modules/`, allowing members to import sibling members seamlessly (`import math_utils;`).
5. **Batch Commands:** `kattery run <script> --workspaces` executes scripts across all members.

---

## Authoring a KeetScript Package

1. **Entry Convention:** Library packages expose their public API in `index.keet` at the package root.
2. **Explicit Exports:** Functions, types, and constants must be marked with `export`:

```keet
export fn add(a: int, b: int): int {
    return a + b;
}

export class Calculator {
    pub fn compute(): int {
        return 42;
    }
}
```

3. **No Unsound Coercion:** All declarations require explicit type annotations.

---

## Security & Offline Execution

- **Non-Interactive Git:** All Git invocations run with `GIT_TERMINAL_PROMPT=0` to ensure CI jobs never hang on credential prompts.
- **Shared Cache:** Git clones are mirrored in `$KATTERY_HOME/cache/git` (defaults to `~/.kattery/cache/git`). Materializations create clean source snapshots without `.git` metadata.
- **Offline Restores:** Running `kattery install --frozen` with pre-cached mirrors allows air-gapped and offline builds.
