# KeetScript for Visual Studio Code

Official Visual Studio Code and Antigravity IDE extension providing first-class language support for **KeetScript (`.keet`)** and **Keet Markup Language (`.kml`)** Single-File Components.

---

## Features

- **🚀 Native Language Server (`keet-lsp`):** Sub-millisecond, in-memory language intelligence powered by the compiler's native pipeline:
  - **Go to Definition (`F12`):** Instantly jump to local variables, parameters, functions, classes, imported modules, and `.kml` custom components.
  - **Find All References (`Shift+F12`):** Workspace-wide symbol reference search respecting lexical scopes and module boundaries.
  - **Rename Symbol (`F2`):** Safe, scope-aware identifier renaming.
  - **Signature Help (`Cmd+Shift+Space`):** Active parameter highlighting for functions, methods, constructors, and stdlib builtins.
  - **Outline & Document Symbols:** Hierarchical symbol trees for classes, methods, functions, and KML component runes/props.
  - **Document Formatting:** AST-driven canonical code formatting and selection range formatting.
  - **Semantic Tokens:** Fine-grained token highlighting.
- **🎨 Syntax Highlighting:** Full TextMate grammars covering all KeetScript keywords (`fn`, `zonk`, `mut`, `let`, `class`, `enum`), types, template strings, and KML tags, runes, and directives.
- **⚡ Live Diagnostics:** Background compile-check on typing (debounced) and saving via `keety check --diagnostic-format=json`, highlighting syntax and type errors directly in the editor and Problems view.
- **💡 1-Click Quick Fixes:** Instant lightbulb fixes for typo suggestions (`did you mean '...'?`), missing imports (`add 'import <module>;'`), missing semicolons, and equality comparisons.
- **🔍 Rich IntelliSense & Autocompletion:** Trigger-assisted completions for standard library modules (`io.`, `fs.`, `sys.`, `math.`, `time.`, `json.`, `net.`, `ui.`), KML tags (`<vstack>`, `<button>`), event directives (`@click`, `@keydown`), runes (`$state`), and import paths.
- **📖 Hover Documentation:** Inline type signatures, parameter hints, and Markdown docstrings for built-in functions, constants, KML layout primitives, and user declarations.
- **📦 Snippets:** Instant templates for `main`, `fn`, `zonk`, `zonkblock`, `ifnull`, `forin`, `class`, and `import`.
- **📋 Kattery Manifest Support:** Built-in JSON schema validation and autocomplete for `pedigree.json` package manifests, plus JSON syntax highlighting for `pedigree.lock` files.
- **▶ Run & Build Buttons:** One-click execution button (▶) in the editor title bar to run `keety run <file>` in the Integrated Terminal.

---

## Installation (Local Development)

To link and install this extension into your local VS Code environment:

```bash
cd tools/vscode-keetscript
./scripts/install-local.sh
```

Then reload your VS Code window (`Cmd+Shift+P` -> `Developer: Reload Window`).

---

## Configuration Settings

| Setting | Default | Description |
| :--- | :--- | :--- |
| `keetscript.enableLsp` | `true` | Enable Language Server Protocol (`keet-lsp`) for live in-memory diagnostics, definitions, hover, completion, and outline. |
| `keetscript.lspServerPath` | `"keet-lsp"` | Path to the `keet-lsp` Language Server binary. Automatically falls back to workspace `build/src/keet-lsp`. |
| `keetscript.compilerPath` | `"keety"` | Path to the `keety` compiler binary. Automatically falls back to workspace `build/src/keety`. |
| `keetscript.checkOnSave` | `true` | Automatically run compiler diagnostics on file save. |
| `keetscript.checkOnType` | `true` | Automatically run compiler diagnostics while typing (debounced). |
| `keetscript.checkDelayMs` | `300` | Debounce delay in milliseconds for typing checks. |
| `keetscript.extraCheckArgs` | `[]` | Extra CLI arguments to pass to `keety check`. |

---

## Commands

- `KeetScript: Run Current File` (`keetscript.run`) — Runs `keety run <file>` in the integrated terminal.
- `KeetScript: Build Current File` (`keetscript.build`) — Compiles `<file>` into a native binary via `keety build`.
- `KeetScript: Check Current File` (`keetscript.check`) — Forces an immediate diagnostic pass on the active file.
