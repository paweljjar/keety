# Keety — The KeetScript AOT Compiler & Toolchain

[![Version](https://img.shields.io/badge/version-0.2.0-blue.svg)](#)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C.svg?logo=c%2B%2B&logoColor=white)](#prerequisites)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](#license)
[![CI](https://github.com/paweljjar/keety/actions/workflows/ci.yml/badge.svg)](https://github.com/paweljjar/keety/actions/workflows/ci.yml)

**Keety** (`keety` v0.2.0) is the official ahead-of-time (AOT) compiler and toolchain driver for the **KeetScript** programming language (`.keet`).

KeetScript bridges the gap between high-level scripting ergonomics and raw system-level performance. Featuring a clean, expressive syntax inspired by Python and TypeScript, `keety` compiles `.keet` source files into idiomatic, self-contained standard C++20 (`.cpp` / `.hpp`) and invokes your native platform toolchain (Clang, GCC, or MSVC) to produce standalone native binaries.

---

## Key Features

- **Zero-Overhead Native Compilation:** Compiles directly to modern C++20 primitives (`std::variant`, `std::vector`, RAII stack values, and thread-pool futures) without garbage collection pauses or virtual dispatch overhead.
- **Predictable Value Semantics & Safe `mut` Borrowing:** Collections (`array<T>`, `map<K, V>`) and objects use deterministic deep-copy value semantics by default, paired with a static `mut` borrow checker that prevents aliasing hazards at compile time and promotes read-only parameters to `const T&` in KeetIR.
- **First-Class Union Types & Flow Narrowing:** Algebraic union types (`int | string | null`) with canonical normalization and control-flow type narrowing (`typeof(x) == "int"`, `x != null`) guarded by a mutation watcher.
- **Ergonomic Error Recovery (`zonk`):** Handle fallible operations inline with fallback expressions (`expr zonk fallback`) or scoped recovery blocks (`expr zonk (err) { ... }`).
- **Strict Soundness Guarantees:** Mandatory explicit type annotations on declarations, zero implicit numeric/truthy coercion, strict no-shadowing enforcement, and deterministic 64-bit two's-complement arithmetic.
- **6-Stage Optimizing Pipeline:** Includes Tarjan SCC circular dependency detection, topological module initialization, liveness analysis, automatic last-use `std::move` insertion, and recursive union boxing.
- **IDE-Grade Diagnostics & Tooling:** Rich, colored, multi-line terminal diagnostics in the style of Rust and Elm, with source spans, Levenshtein "Did you mean?" suggestions and Quick Fix hints; machine-readable JSON output (`--json-diagnostics`); and an official Visual Studio Code extension.

---

## Getting Started

### Prerequisites

- **CMake** 3.20 or newer
- **C++20 Compiler** on `PATH`, required both for building the `keety` compiler itself and for `keety build` / `keety run` (the generated code and runtime target C++20):
    - Clang 14+, GCC 11+, or MSVC 19.30+ (`clang++`, `g++`, or `cl.exe`)

### Building from Source

```bash
# Configure the build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the compiler, unit/conformance tests, and benchmarks
cmake --build build -j
```

The compiled binary will be located at `./build/src/keety`.

#### CMake Configuration Options

| Option | Default | Description |
| :--- | :--- | :--- |
| `KEETY_BUILD_TESTS` | `ON` | Build unit and conformance test suites (`keety_unit_tests`) |
| `KEETY_BUILD_BENCHMARKS` | `ON` | Build reference C++ vs. KeetScript benchmarks (`keety_benchmarks`) |
| `KEETY_ENABLE_WERROR` | `OFF` | Treat compiler warnings as errors (`-Werror` / `/WX`) |
| `KEETY_ENABLE_CLANG_TIDY` | `OFF` | Run `clang-tidy` static analysis during compilation |

---

## Quick Start

Create a file named `hello.keet`:

```keet
import io;

fn safeDivide(a: int, b: int): int {
    return (a / b) zonk -1;
}

fn describe(val: int | string | null): string {
    if (typeof(val) == "int") {
        return `number: ${val.toString()}`;
    } elif (typeof(val) == "string") {
        return `text: "${val}"`;
    } else {
        return "empty";
    }
}

fn swap<T>(mut x: T, mut y: T): void {
    let temp: T = x;
    x = y;
    y = temp;
}

fn main(): int {
    let x: int = 10;
    let y: int = 20;
    swap(mut x, mut y);

    io.println(`x = ${x.toString()}, y = ${y.toString()}`);
    io.println(`10 / 0 fallback = ${safeDivide(10, 0).toString()}`);
    io.println(describe("KeetScript"));
    return 0;
}
```

Validate, run, or compile it to a standalone binary:

```bash
# Type-check and validate semantics without code generation
./build/src/keety check hello.keet

# Compile and immediately execute
./build/src/keety run hello.keet

# Compile to a native executable
./build/src/keety build hello.keet -o ./bin

# Inspect the generated C++20 source or KeetIR
./build/src/keety emit hello.keet --cpp
./build/src/keety emit hello.keet --ir
```

---

## CLI Usage

```text
Usage: keety <command> [options] <file.keet>

Commands:
  check <file>                         Fast IDE-grade semantic and type validation
  emit <file> (--cpp|--hpp|--ast|--ir) Emit generated C++ or intermediate representations
  build <file>                         Compile KeetScript source into a native binary
  run <file> [-- args]                 Compile and immediately execute the target binary
  test [--benchmarks|--conformance]    Run performance benchmarks or conformance test suite
  version                              Display keety version information
  help                                 Display this help message

Options:
  -o, --output-dir <dir>               Target output directory for emitted sources or binaries
  --cxx <compiler>                     Explicit native C++ compiler binary (clang++, g++, cl.exe)
  -O, --opt <0|1|2|3|s|z>              Optimization level (default: 3)
  --wrapv                              Enforce two's-complement overflow semantics
  -v, --verbose                        Enable verbose diagnostics and phase timings
  --no-color                           Disable ANSI color escape sequences in output
  --diagnostic-format <human|json>     Diagnostics as rich, colored, multi-line Rust/Elm-style
                                       messages with Quick Fix suggestions (human, default),
                                       or as a machine-readable JSON array (json)
  --json-diagnostics                   Shorthand for --diagnostic-format=json
  -h, --help                           Display usage documentation
  -V, --version                        Display version number
```

You can also override the runtime headers location via the `KEETY_RUNTIME_DIR` environment variable if running `keety` outside the build tree.

---

## Compiler Architecture

`keety` is structured as a deterministic, 6-stage feed-forward compilation pipeline:

```mermaid
flowchart TD
    A["Source Files (.keet)"] --> B["1. Lexical Analysis (keety::lexer)"]
    B --> C["2. Syntactic Analysis / AST (keety::parser)"]
    C --> D["3. Module DAG & Cycle Detection (keety::resolver)"]
    D --> E["4. Semantic Analysis & Type Checker (keety::sema / keety::typechecker)"]
    E --> F["5. KeetIR Lowering & Optimization Passes (keety::ir)"]
    F --> G["6. C++20 Code Emitter (keety::emitter)"]
    G --> H["Native Platform Compiler (Clang / GCC / MSVC)"]
    H --> I["Native Executable"]
```

1. **Lexer (`keety::lexer`):** UTF-8 maximal-munch tokenizer with context-sensitive disambiguation for blank identifiers (`_`), generic `<TypeArgs>`, nested closing brackets (`>>`), and recursive `${...}` template string interpolation.
2. **Parser (`keety::parser`):** Recursive descent + Pratt expression parser producing a strongly typed AST with exact `SourceSpan` tracking.
3. **Module Resolver (`keety::resolver`):** Resolves standard library (`io`, `fs`, `sys`, `math`, `time`), `keet_modules/`, and relative imports into a dependency graph, detects cycles via Tarjan's SCC algorithm, and computes topological initialization order.
4. **Semantic Analyzer & Type Checker (`keety::sema`, `keety::typechecker`):** Enforces strict no-shadowing, exported `const`-view rules, union algebra normalization, flow-sensitive type narrowing with mutation invalidation, and the 5 `mut` borrow-checking rules.
5. **KeetIR & Optimization Passes (`keety::ir`):** Lowers the AST into basic-block intermediate representation and runs static optimization passes:
    - *Const-Ref Promotion:* Promotes unmutated by-value parameters to `const T&`.
    - *Liveness Analysis & Move Insertion:* Automatically wraps dead-after-use local variables in `std::move(...)`.
    - *Recursive Boxing:* Wraps self-referential union variants in value-semantic `keet::deep_copy_ptr<T>`.
    - *Pipeline Reserve:* Pre-allocates vector capacity for chained `.map()` / `.filter()` functional pipelines.
6. **C++20 Emitter & Toolchain Driver (`keety::emitter`, `keety::driver`):** Emits clean `.hpp` and `.cpp` files and invokes the host C++ compiler.

---

## Standard Library

KeetScript ships with a header-only C++20 runtime (`runtime/keet/`) and five built-in standard library modules:

| Module | Import | Capabilities |
| :--- | :--- | :--- |
| **`io`** | `import io;` | Console output (`print`, `println`, `eprint`, `eprintln`) and standard input (`readLine`, `readAll`) |
| **`fs`** | `import fs;` | File I/O (`readText`, `readLines`, `writeText`, `appendText`), path manipulation, and directory traversal |
| **`sys`** | `import sys;` | Command-line arguments (`args`), environment variables (`getEnv`, `getEnvOr`, `setEnv`, `unsetEnv`, `hasEnv`), process exit, and platform metadata |
| **`math`** | `import math;` | Constants (`PI`, `E`, `TAU`), trigonometry, logarithms, rounding, and clamping utilities |
| **`time`** | `import time;` | Monotonic high-resolution timers (`nowNs`, `nowMs`), wall-clock timestamps, and thread sleep (`sleepMs`) |

---

## Testing & Benchmarks

### Running Unit & Conformance Tests

The test suite uses Catch2 and covers all compiler stages, runtime primitives, positive semantic conformance (`tests/conformance/positive/`), and negative diagnostic verification (`E1000`–`E4000` in `tests/conformance/negative/`).

```bash
# Run via CTest
ctest --test-dir build --output-on-failure

# Or run directly via the keety CLI driver
./build/src/keety test --conformance
```

### Running Performance Benchmarks

The benchmark suite compares generated KeetScript against idiomatic handwritten C++20 across four workloads (numeric vector pipeline, hash map / union variant lookup, string ingestion, and async task concurrency):

```bash
./build/src/keety test --benchmarks
# Or invoke the benchmark runner binary directly:
./build/benchmarks/keety_benchmarks
```

---

## Editor Support (VS Code)

An official Visual Studio Code extension is included in [`tools/vscode-keetscript/`](tools/vscode-keetscript/README.md), providing:
- Full TextMate syntax highlighting for `.keet` files
- Live background diagnostics (`keety check --json-diagnostics`)
- 1-click Quick Fixes for typos, missing imports, and missing semicolons
- Autocompletion and hover documentation for standard library modules
- Editor title bar **Run** and **Build** buttons

To install locally into VS Code:

```bash
./tools/vscode-keetscript/scripts/install-local.sh
```

---

## Repository Structure

```text
keety/
├── CMakeLists.txt              # Top-level CMake build configuration
├── include/keety/              # Public compiler headers (lexer, parser, sema, ir, emitter, driver)
├── src/                        # Compiler implementation and CLI entrypoint
├── runtime/keet/               # Header-only C++20 runtime & standard library implementation
├── tests/
│   ├── unit/                   # Component unit tests (Lexer, Parser, TypeChecker, IR, Emitter)
│   └── conformance/            # Language conformance suites (positive & negative E1000-E4000)
├── benchmarks/                 # Reference performance benchmarks (.keet vs. handwritten C++20)
├── tools/vscode-keetscript/    # Official Visual Studio Code language extension
└── documentation/              # Formal language, compiler, stdlib, and test specifications
```

---

## Documentation

Detailed specifications are available in the [`documentation/`](documentation/) directory:

- [The KeetScript Language Specification](documentation/The%20KeetScript%20Language%20Specification.md) — Complete EBNF grammar, type system, memory model, and execution semantics.
- [Keety Compiler Architecture Specification](documentation/Keety%20Compiler%20Architecture%20Specification.md) — Deep dive into the 6-stage pipeline, KeetIR optimization passes, and C++20 emission templates.
- [KeetScript Conformance Test Suite and Benchmark Specification](documentation/KeetScript%20Conformance%20Test%20Suite%20and%20Benchmark%20Specification.md) — Validation test cases and benchmark methodology.
- **Standard Library Reference:**
    - [`io` — Input / Output](documentation/stdlib/io.md)
    - [`fs` — File System](documentation/stdlib/fs.md)
    - [`sys` — System & Process](documentation/stdlib/sys.md)
    - [`math` — Mathematical Functions](documentation/stdlib/math.md)
    - [`time` — Clocks & Durations](documentation/stdlib/time.md)

---

## License

Distributed under the MIT License. See [`LICENSE`](LICENSE.md) for details.