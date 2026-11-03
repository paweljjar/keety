# Keety Project Roadmap

This document outlines the strategic engineering roadmap and feature backlog for **KeetScript**, the **`keety`** ahead-of-time (AOT) compiler toolchain, the **`kattery`** package manager, and the developer tooling ecosystem.

The roadmap is structured into three main categories:
1. **[Shipped (A)](#a-shipped)** — Production-ready features implemented, tested, and released.
2. **[To Do (B)](#b-to-do)** — Active backlog organized by implementation horizon:
   - **[Near-Term (B1)](#b1-near-term)**
   - **[Mid-Term (B2)](#b2-mid-term)**
   - **[Long-Term (B3)](#b3-long-term)**
3. **[Under Consideration (C)](#c-under-consideration)** — Exploratory features and long-range architectural proposals.

---

## 🗺️ Roadmap Architecture

```mermaid
flowchart TD
    subgraph A["Category A: Shipped"]
        A1["6-Stage AOT Compiler Pipeline"]
        A2["Soundness & Mut Borrow Checker"]
        A3["Standard Library (io, fs, sys, math, time, json, net)"]
        A4["Package Manager (kattery)"]
        A5["Language Server Protocol (keet-lsp)"]
        A6["VS Code Extension"]
    end

    subgraph B["Category B: To Do"]
        subgraph B1["B1: Near-Term"]
            B1_1["Testing Framework (keety test)"]
            B1_2["HTTP Server (std::net::http)"]
            B1_3["Cryptography (std::crypto)"]
            B1_4["Source-to-Source Line Mapping"]
        end
        subgraph B2["B2: Mid-Term"]
            B2_1["Code Formatter (keety fmt)"]
            B2_2["Static Linter (keety lint)"]
            B2_3["Advanced LSP Refactoring"]
            B2_4["Debug Adapter Protocol (keet-dap)"]
        end
        subgraph B3["B3: Long-Term"]
            B3_1["CSP Concurrency (chan & select)"]
            B3_2["Memory Alloc Tracing"]
            B3_3["Foreign Function Interface (FFI)"]
            B3_4["Centralized Package Registry"]
        end
    end

    subgraph C["Category C: Under Consideration"]
        C1["Host Embedding API (libkeety)"]
        C2["Direct LLVM IR Backend"]
        C3["Spec Freeze & LTS Policy"]
        C4["WASM In-Browser Playground"]
    end

    A --> B1 --> B2 --> B3 -.-> C
```

---

## A. Shipped

Production-ready components fully implemented, tested with 100% test coverage, and merged into `main`:

### 1. AOT Compilation Pipeline
- **6-Stage Feed-Forward Architecture:**
  - `keety::lexer`: UTF-8 maximal-munch tokenizer with context-sensitive disambiguation for blank identifiers (`_`), generic `<TypeArgs>`, template literals with recursive `${...}` interpolation, and closing brackets (`>>`).
  - `keety::parser`: Top-down Recursive Descent + Pratt expression parser producing a strongly typed AST with exact `SourceSpan` tracking.
  - `keety::resolver`: Module dependency DAG, Tarjan's SCC circular dependency detection, and post-order topological sort for deterministic module initialization.
  - `keety::sema` & `keety::typechecker`: Mandatory explicit typing, zero implicit coercion, union algebra canonical normalization, control-flow type narrowing with mutation watching, and the 5 strict `mut` borrow-checking rules.
  - `keety::ir`: Control Flow Graph (CFG) lowering with static liveness analysis, automatic last-use `std::move` insertion, const-reference parameter promotion, and self-referential recursive variant boxing (`keet::deep_copy_ptr<T>`).
  - `keety::emitter`: Clean C++20 emission (`.hpp` and `.cpp`) and native toolchain coordination (Clang, GCC, MSVC).

### 2. Language Soundness & Safety Guarantees
- Value semantics by default for collections (`array<T>`, `map<K, V>`) and classes.
- Compile-time prevention of duplicate `mut` aliasing ($f(\text{mut } x, \text{mut } x)$ rejected).
- Deterministic 64-bit two's-complement arithmetic overflow semantics (`--wrapv`).
- Zero-cost `zonk` error recovery expressions and scoped recovery blocks.

### 3. Core Standard Library (`runtime/keet/`)
- **`io`**: Console output (`print`, `println`, `eprint`, `eprintln`) and input stream readers (`readLine`, `readAll`).
- **`fs`**: File I/O (`readText`, `readLines`, `writeText`, `appendText`), directory traversal, and path operations.
- **`sys`**: CLI arguments (`args`), environment variables (`getEnv`, `setEnv`), process exit, and platform metadata.
- **`math`**: Floating-point operations, trigonometry, constants (`PI`, `E`, `TAU`), and clamping functions.
- **`time`**: High-resolution monotonic timers (`nowNs`, `nowMs`), sleep (`sleepMs`), and wall-clock timestamps.
- **`json`**: RFC 8259 parser, dot-path queries, immutable builders, and polymorphic stringification.
- **`net`**: RFC 3986 URL parser, DNS lookups, TCP/UDP sockets, and HTTP/1.1 client (`get`, `postJson`, `download`).
- **`crypto`**: Hashing (SHA-256, SHA-512, SHA3-256, SHA3-512, BLAKE3, streaming `Hasher`), message authentication (HMAC-SHA256, HMAC-SHA512, streaming `Hmac`), authenticated symmetric encryption (AES-256-GCM, ChaCha20-Poly1305), CSPRNG (`randomBytes`, `randomHex`, `randomInt`, `randomUuid`, `timingSafeEqual`), encodings (Hex, RFC 4648 Base64, URL-safe Base64), and symbology (Code 128 1D barcode and ISO/IEC 18004 2D QR Code encoders, decoders, SVG & ASCII block renderers).

### 4. Package Manager (`kattery`)
- Project manifest (`pedigree.json`) and reproducible lockfile (`pedigree.lock`).
- Remote Git (`git+https://...`) and local path (`path:...`) dependency management with hoisting.
- Monorepo workspace discovery and package link management in `keet_modules/`.
- Cross-platform custom script execution (`kattery run <script>`).
- Starter scaffolding (`kattery init`) with starter code and automated `.gitignore`.

### 5. Language Server Protocol (`keet-lsp`)
- Native JSON-RPC stdio transport with in-memory document synchronization (`textDocument/didOpen`, `didChange`, `didClose`).
- Live background diagnostics (`textDocument/publishDiagnostics`) with line/column squiggles and error codes.
- Go-to-Definition (`textDocument/definition`) for local symbols, declarations, and imported modules.
- Rich Markdown Hover (`textDocument/hover`) with function signatures, types, and stdlib documentation.
- Context-Aware Autocompletion (`textDocument/completion`) for keywords, stdlib modules, members, and import paths.
- Hierarchical Document Outline (`textDocument/documentSymbol`) for VS Code breadcrumbs and Outline view.

### 6. Visual Studio Code Extension
- Native TextMate syntax highlighting for `.keet` files and `pedigree.json` validation schema.
- Native LSP client integration (`vscode-languageclient`) with automatic `keet-lsp` discovery.
- Graceful automatic fallback to CLI check-on-save mode when `keet-lsp` is unavailable.

### 7. Native Testing Framework (`keety test` & `std::test`)
- **First-Class Test Execution Engine:**
  - Built-in `keety test` CLI sub-command supporting discovery, filter matching (`--filter`), parallel execution, and test suite isolation.
  - Standard library `test` module (`import test;`) providing suites (`suite`), individual cases (`case`), and execution primitives.
- **Assertion Primitives:**
  - `assertEq(actual, expected)`: Deep value equality comparison with error formatting.
  - `assertNe(actual, expected)`, `assertTrue(cond)`, `assertFalse(cond)`.
  - `assertZonks(fn, expectedError)`: Exception assertion verifying runtime error names.
  - `fail(message)`: Explicit test failure.
- **Reporting & Tooling:**
  - Formatted terminal reporters with pass/fail counts, execution timings, and error traces.
  - Integration with package manager runner (`kattery test`).

---

## B. To Do

Active development backlog organized by implementation priority and time horizon.

### B1: Near-Term

High-priority deliverables focusing on immediate networking capabilities, cryptography, and debugging precision.

#### 1. HTTP Server (`std::net::http`)
- **Event-Driven Server Engine:**
  - Non-blocking HTTP/1.1 server built on top of `net::Socket`.
  - Intuitive route registry:
    ```keet
    let server: http.Server = http.createServer();
    server.get("/api/users/:id", fn(req: http.Request): http.Response {
        return http.json(200, { "id": req.params.get("id") });
    });
    server.listen(8080);
    ```
- **Routing & Handlers:**
  - Named parameter extraction (`/posts/:id`), query string parsing, and header manipulation.
- **Middleware Chain:**
  - Composable middleware support (request logging, CORS headers, gzip compression).
- **Static File Serving:**
  - `http.serveDir("/static", "./public")` with automatic MIME-type detection.

#### 2. Source-to-Source Line Mapping
- **Emitted C++ Line Directives (`#line`):**
  - Emit `#line <lineno> "<source>.keet"` directives in generated `.hpp` and `.cpp` files.
  - Ensure compiler errors from host toolchains (Clang, GCC, MSVC) point directly to the originating `.keet` source lines.
- **Debugger Line Tables:**
  - Ensure native debugger call stacks (LLDB/GDB) display original KeetScript line numbers and filenames rather than intermediate C++ lines.

---

### B2: Mid-Term

Tooling enhancements, developer ergonomics, automated code formatting, and interactive visual debugging.

#### 1. Code Formatter (`keety fmt`)
- **Deterministic Canonical Style:**
  - Opinionated AST/CST pretty-printer (inspired by `gofmt` and `rustfmt`).
  - Standardized 4-space indentation, operator padding, brace placement, and trailing commas in multi-line literals.
  - Deterministic import sorting (Standard library $\to$ third-party packages $\to$ local relative imports).
- **LSP Integration:**
  - Wire `textDocument/formatting` and `textDocument/rangeFormatting` into `keet-lsp`.
  - Provide instant "Format on Save" in VS Code without external tooling.
- **Verification Mode:**
  - `keety fmt --check` non-zero exit code flag for CI linting gates.

#### 2. Static Linter (`keety lint`)
- **Hygiene & Dead Code Analysis:**
  - Detect unused imports, unused local variables/parameters (suggesting `_`), and unreachable branches.
  - Flag redundant type annotations where the type is trivially evident.
  - Detect unmutated `mut` parameters or `let` variables and recommend `const`.
- **Configurable Rules:**
  - Project-level `.keetlint.json` configuration for team rule sets and warning suppressions.

#### 3. Advanced LSP Refactoring
- **Symbol Rename (`textDocument/rename` & `textDocument/prepareRename`):**
  - Safe, project-wide identifier renaming respecting lexical scopes and module boundaries.
- **Find All References (`textDocument/references`):**
  - Project-wide search for all read/write usages and call sites of a symbol.
- **Code Actions & Quick Fixes (`textDocument/codeAction`):**
  - 1-click Quick Fixes: "Import missing module", "Remove unused variable", "Make variable mutable".
- **Signature Help (`textDocument/signatureHelp`):**
  - Parameter tooltips and active parameter index highlighting while typing function/method calls.
- **Semantic Tokens (`textDocument/semanticTokens/full`):**
  - Accurate semantic token categorization for types, functions, variables, constants, and keywords.

#### 4. Debug Adapter Protocol (`keet-dap`)
- **Native DAP Server Binary:**
  - Implement a Debug Adapter Protocol server connecting VS Code Debug UI to LLDB/GDB.
- **Interactive Visual Debugging:**
  - Line breakpoints, conditional breakpoints, and exception hit breakpoints in `.keet` files.
  - Step Over, Step Into, Step Out, and Continue controls.
  - Variable inspector: inspect KeetScript scalar values, unfold union variants, explore maps/arrays, and evaluate watch expressions.

---

### B3: Long-Term

Advanced language systems, memory profiling, foreign interoperability, and package ecosystem scaling.

#### 1. CSP Concurrency Primitives
- **Typed Channels (`chan<T>`):**
  - Value-semantic, thread-safe message passing channels (unbuffered and buffered).
  - Channel operations: `ch.send(val)`, `val = ch.recv()`, `ch.close()`.
  - Non-blocking variants: `ch.trySend(val)`, `ch.tryRecv()`.
- **Channel Multiplexing (`select`):**
  - `select` statement for coordinating multiple channel operations with optional `default` timeout branches.
- **Task Spawning:**
  - `spawn fn(...)` construct scheduling lightweight tasks onto a thread-pool work-stealing scheduler.

#### 2. Memory Allocation Tracing
- **Allocation Profiler (`--trace-alloc`):**
  - Compiler flag emitting lightweight instrumentation for tracking dynamic allocations.
  - Detects accidental deep copies, vector geometric resizes, and string reallocations.
  - Emits heap allocation summaries at program exit or outputs trace data for flamegraph analysis.

#### 3. Foreign Function Interface (FFI)
- **`extern "C"` Bindings:**
  - Direct foreign function declarations in KeetScript:
    ```keet
    extern "C" fn sqlite3_open(filename: string, mut db: rawptr): int;
    ```
- **Type Marshaling:**
  - Zero-overhead marshaling between KeetScript strings/arrays and C-compatible raw buffers and null-terminated strings.
- **Dynamic Library Loading:**
  - Runtime dynamic library loader (`sys.loadLibrary("libfoo.so")`) for invoking dynamic symbols without compile-time linkage.

#### 4. Centralized Package Registry
- **Hosted Package Registry for `kattery`:**
  - Centralized index for discovering, versioning, and distributing KeetScript packages.
  - `kattery publish`: CLI command to authenticate, package, and upload packages with cryptographic integrity checks.
  - SemVer resolution and dependency security audit scans.

---

## C. Under Consideration

Exploratory architectural proposals and long-range research topics under evaluation:

#### 1. Host Embedding API (`libkeety`)
- Packaging the compiler and runtime as an embeddable static/shared library (`libkeety`).
- C/C++ host API allowing host applications (e.g. game engines, web servers) to instantiate KeetScript runtimes, bind native functions, and execute `.keet` scripts.

#### 2. Direct LLVM IR Backend
- An optional, direct-to-LLVM IR code generation backend (`keety build --backend=llvm`) bypassing C++ code emission and host toolchain invocation.
- Goal: Sub-millisecond incremental debug compilation speeds while retaining optimal release performance.

#### 3. Specification Freeze & LTS Policy
- Formal language specification freeze for KeetScript 1.0.
- Strict backward compatibility guarantees, semantic versioning policies, and Long Term Support (LTS) release cadence.

#### 4. WebAssembly (WASM) Playground
- Compiling `keety` and the standard library to WebAssembly / WASI.
- Interactive, zero-installation in-browser KeetScript playground with live syntax highlighting, compilation, and execution.

---

## 💡 Submitting Feedback & Proposals

Have feedback or want to champion a feature in Category B or C?
- Review [The KeetScript Language Specification](documentation/The%20KeetScript%20Language%20Specification.md) for architectural guidelines.
- Open an RFC Discussion following the KeetScript Enhancement Proposal (KEP) format.
