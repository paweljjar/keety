# KeetScript Language Server Protocol (`keet-lsp`) Implementation Plan

## 1. Overview & Architectural Vision

The goal of `keet-lsp` is to provide high-performance, IDE-grade developer experience for KeetScript in VS Code, Neovim, Helix, Sublime Text, and any LSP-compatible editor.

Currently, the VS Code extension runs `keety check --format json` on file saves and uses static tables for hover/completions. This has key limitations:
1. Diagnostics only update after writing files to disk (no in-memory live feedback as you type).
2. Autocompletion only knows about stdlib keywords, not symbols defined in local files (`./aaaa.keet`), imported packages (`keet_modules/`), or custom types/classes/methods.
3. No Go-to-Definition (`textDocument/definition`) or Document Symbols (`textDocument/documentSymbol`).
4. No Hover documentation on user functions, variables, classes, or inferred types (`textDocument/hover`).

`keet-lsp` will be built directly in C++ as a high-speed executable (`keet-lsp`), leveraging existing compiler frontend modules (`lexer`, `parser`, `resolver`, `sema`, `typechecker`, `diag`) over standard JSON-RPC 2.0 via `stdin`/`stdout`.

---

## 2. Core Features & Supported Capabilities

### Phase 1: Protocol Transport & In-Memory Document Model
- **JSON-RPC 2.0 Transport:** Asynchronous/streamed message parser over `Content-Length: ...\r\n\r\n{...}` via stdin/stdout.
- **Document Synchronization (`textDocument/didOpen`, `textDocument/didChange`, `textDocument/didClose`):**
  - Full document syncing (`TextDocumentSyncKind.Full`).
  - In-memory document buffer map (`uri -> content`, `version`).
- **LSP Lifecycle:** `initialize`, `initialized`, `shutdown`, `exit`.
- **Capability Negotiation:** Advertise `definitionProvider`, `documentSymbolProvider`, `completionProvider`, `hoverProvider`, `textDocumentSync`.

### Phase 2: Live In-Memory Diagnostics (`textDocument/publishDiagnostics`)
- Re-parse and typecheck in-memory source buffers on document change (debounced or per-change).
- Convert KeetScript `Diagnostic` objects (with `PrimarySpan`, `SecondarySpan`, `DiagnosticFooter`, `ErrorCode`) into LSP `Diagnostic` structures with 0-indexed line/character ranges.
- Translate KeetScript error codes (`E1000`–`E4000`) into LSP diagnostic codes with error/warning/info severity.
- Clear diagnostics on file close or fix.

### Phase 3: Go-to-Definition (`textDocument/definition`)
- Find definition for:
  - **Local variables & parameters:** Trace variable references back to their AST declaration (`LetDeclaration`, `ConstDeclaration`, `Param`).
  - **Functions & Methods:** Jump to function definition within the file or imported file.
  - **Classes & Structs & Types:** Jump to `class Foo` or `struct Bar` definition.
  - **Quoted import paths:** Clicking on `"./aaaa"` or `"./aaaa.keet"` opens and navigates to the target file.
  - **Installed packages:** Clicking on `import mathx;` jumps to `keet_modules/mathx/index.keet`.

### Phase 4: Hover Information (`textDocument/hover`)
- Rich Markdown hover on cursor:
  - Variables: `let x: int` with docstrings if present.
  - Functions: `fn add(a: int, b: int): int` showing signature and return type.
  - Classes/Types: `class User` showing member count and inheritance.
  - Built-in stdlib: signature + markdown documentation from stdlib registry.

### Phase 5: Smart Autocompletion (`textDocument/completion`)
- **Scope-aware Completions:**
  - In function scope: local variables, parameters, outer globals.
  - Dot completion (`obj.`): inspect inferred type of `obj` and list fields, methods, or module exports.
  - Module completion (`io.`): list exported functions (`println`, `print`, `Color`).
  - Relative import completion: suggest matching `.keet` files when typing inside `"./"`.
- **Keywords & Snippets:** Language keywords (`fn`, `class`, `struct`, `interface`, `match`, `for`, `spawn`, `channel`).

### Phase 6: Document Outline & Symbols (`textDocument/documentSymbol`)
- Hierarchical document outline:
  - Classes with child methods and fields.
  - Top-level functions.
  - Structs and Interfaces.
  - Enums and Constants.

### Phase 7: VS Code Extension Client Integration
- Update `tools/vscode-keetscript`:
  - Add `vscode-languageclient` npm dependency.
  - Spawn `keet-lsp` binary (locating from `keety` PATH or workspace build output).
  - Fall back gracefully to check-on-save if `keet-lsp` is not installed.
  - Package and build `.vsix` ready for installation.

---

## 3. CMake Target & Architecture

```
                 +----------------------------------------+
                 |          VS Code / Neovim / Helix      |
                 +----------------------------------------+
                                     ^
                                     | JSON-RPC 2.0 (stdin/stdout)
                                     v
                 +----------------------------------------+
                 |            keet-lsp Binary             |
                 +----------------------------------------+
                   |                 |                  |
                   v                 v                  v
            [Transport]       [LSP Server]       [AST Query Engine]
          (JSON-RPC 2.0)      (Dispatcher)    (Symbol Index & Locater)
                   |                 |                  |
                   +-----------------+------------------+
                                     |
                                     v
                        +-------------------------+
                        |       keety_core        |
                        | (Lexer, Parser, Sema,   |
                        |  Typechecker, Diag,     |
                        |  Project Root Resolver) |
                        +-------------------------+
```

- **Target `keet_lsp_core`:** Library containing JSON-RPC protocol, LSP models, and LanguageServer engine.
- **Target `keet-lsp`:** Executable binary (`src/lsp/main.cpp`).
- **Tests:** `tests/unit/test_lsp_*.cpp` covering JSON-RPC wire format, document syncing, definition resolution, hover markdown generation, and completion items.
