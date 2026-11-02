# KeetScript for Visual Studio Code

Official Visual Studio Code extension providing first-class language support for **KeetScript (`.keet`)**.

---

## Features

- **🎨 Syntax Highlighting:** Full TextMate grammar covering all KeetScript keywords (`fn`, `zonk`, `mut`, `let`, `class`, `enum`), types, literals, template expressions, and standard library namespaces.
- **⚡ Live Diagnostics:** Background compile-check on typing (debounced) and saving via `keety check --diagnostic-format=json`, highlighting syntax and type errors directly in the editor and Problems view.
- **💡 1-Click Quick Fixes:** Instant lightbulb fixes for typo suggestions (`did you mean '...'?`), missing imports (`add 'import <module>;'`), missing semicolons, and equality comparisons.
- **🔍 Rich IntelliSense & Autocompletion:** Dot-completions for standard library modules (`io.`, `fs.`, `sys.`, `math.`, `time.`, `json.`), built-in types, keywords, and container methods.
- **📖 Hover Documentation:** Inline type signatures, parameter hints, and Markdown docstrings for built-in functions, constants, and language constructs.
- **📦 Snippets:** Instant templates for `main`, `fn`, `zonk`, `zonkblock`, `ifnull`, `forin`, `class`, and `import`.
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
| `keetscript.compilerPath` | `"keety"` | Path to the `keety` compiler binary. Automatically falls back to `<workspace>/build/src/keety` if available. |
| `keetscript.checkOnSave` | `true` | Automatically run compiler diagnostics on file save. |
| `keetscript.checkOnType` | `true` | Automatically run compiler diagnostics while typing (debounced). |
| `keetscript.checkDelayMs` | `300` | Debounce delay in milliseconds for typing checks. |
| `keetscript.extraCheckArgs` | `[]` | Extra CLI arguments to pass to `keety check`. |

---

## Commands

- `KeetScript: Run Current File` (`keetscript.run`) — Runs `keety run <file>` in the integrated terminal.
- `KeetScript: Build Current File` (`keetscript.build`) — Compiles `<file>` into a native binary via `keety build`.
- `KeetScript: Check Current File` (`keetscript.check`) — Forces an immediate diagnostic pass on the active file.
