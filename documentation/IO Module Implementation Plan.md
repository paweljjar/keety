# KeetScript Standard Library: `io` Module Implementation Plan

This document outlines the hierarchical implementation plan (`Phase -> Task`) for the built-in `io` module in **KeetScript** and the **`keety`** AOT compiler. The `io` module provides type-safe console output (`print`, `println`), standard input reading (`readln`, `input`), and ANSI terminal color/style formatting (`io.Color`, inline color helpers).

---

## Phase 1: C++20 Runtime Layer Implementation (`runtime/keet/io.hpp`)
*Objective: Implement a zero-overhead, header-only C++20 runtime module handling standard I/O streams and ANSI escape sequences with automatic terminal (TTY) detection.*

- [x] **Task 1.1: Define `keet::io::Color` Enum and ANSI Escape Utilities**
  - Create `runtime/keet/io.hpp` and include it in `runtime/keet/runtime.hpp`.
  - Define `enum class Color : int64_t { Default = 0, Red, Green, Yellow, Blue, Magenta, Cyan, White, Gray }`.
  - Implement TTY detection (`isatty(fileno(stdout))` on POSIX / `_isatty` on Windows) and respect the `NO_COLOR` environment variable.
  - Provide a configurable override `keet::io::set_color_enabled(bool)` to allow deterministic unit testing of ANSI sequences regardless of test runner output redirection.

- [x] **Task 1.2: Implement Inline String Styling Helpers (`red`, `green`, `bold`, etc.)**
  - Implement `keet::io::colorize(std::string_view text, Color color)` returning a `std::string` wrapped in the corresponding ANSI escape code (`\033[...m...\033[0m`) when colors are active, or unmodified text otherwise.
  - Expose inline helper functions in `namespace keet::io`: `red(s)`, `green(s)`, `yellow(s)`, `blue(s)`, `magenta(s)`, `cyan(s)`, `white(s)`, `gray(s)`, and `bold(s)`.

- [x] **Task 1.3: Implement Variadic `print` and `println` Functions**
  - Implement variadic templates `keet::io::print(Args&&... args)` and `keet::io::println(Args&&... args)` accepting arbitrary KeetScript primitive and composite types (`int64_t`, `double`, `BoolByte`/`bool`, `char`, `std::string`).
  - Support optional whole-line color formatting when `keet::io::Color` is passed as the trailing argument (or via dedicated `print_colored` / `println_colored` overloads).
  - Ensure `bool` / `BoolByte` formats deterministically as `"true"` / `"false"` and floating-point values follow KeetScript formatting conventions.

- [x] **Task 1.4: Implement Console Input Functions (`readln` and `input`)**
  - Implement `keet::io::readln(std::istream& in = std::cin)` using `std::getline`, automatically stripping trailing `\r` characters for cross-platform CRLF/LF compatibility.
  - Implement `keet::io::input(std::string_view prompt = "", std::istream& in = std::cin, std::ostream& out = std::cout)` that writes `prompt` to `out`, flushes the stream immediately (`out.flush()`), and returns `readln(in)`.

---

## Phase 2: Compiler Symbol Registration & Semantic Analysis (`sema` & `typechecker`)
*Objective: Register the built-in `io` module, its functions, and the `io.Color` enum in the `keety` compiler front-end.*

- [x] **Task 2.1: Register Built-in `io` Module in `ModulePathResolver` and `Universe`**
  - Update `src/resolver/module_path_resolver.cpp` and `src/sema/universe.cpp` so that `import io;` resolves as a built-in standard library module without searching the filesystem for an `io.keet` file.
  - Populate the module scope with `io` symbols when `import io;` is declared.

- [x] **Task 2.2: Define `io` Function Signatures and `io.Color` in `TypeChecker`**
  - Update `src/typechecker/type_checker.cpp` to type-check member calls and enum accesses on the `io` module:
    - `io.print(...args): void` (accepts $\ge 0$ printable arguments, plus an optional trailing `io.Color`).
    - `io.println(...args): void` (accepts $\ge 0$ printable arguments, plus an optional trailing `io.Color`).
    - `io.readln(): string` (accepts 0 arguments).
    - `io.input(prompt?: string): string` (accepts 0 or 1 `string` argument).
    - `io.red(s: string): string`, `io.green(s: string): string`, `io.yellow(s: string): string`, `io.blue(s: string): string`, `io.magenta(s: string): string`, `io.cyan(s: string): string`, `io.white(s: string): string`, `io.gray(s: string): string`, `io.bold(s: string): string`.
    - `io.Color` enum members: `Default`, `Red`, `Green`, `Yellow`, `Blue`, `Magenta`, `Cyan`, `White`, `Gray`.

- [x] **Task 2.3: Semantic Validation & Diagnostics (`diag`)**
  - Emit clear diagnostic errors when invalid argument types or arities are passed to `io.*` functions (e.g., passing an `int` to `io.input(42)` or `io.red(100)`, or passing arguments to `io.readln("x")`).

---

## Phase 3: IR Lowering & C++20 Code Generation (`ir` & `emitter`)
*Objective: Lower `io.*` AST nodes into Keet IR (K-IR) instructions and emit clean C++20 runtime calls.*

- [x] **Task 3.1: Lower `io` Calls and `io.Color` in `ASTLowerer` (`src/ir/ast_lowerer.cpp`)**
  - Lower `io.*` member call expressions into intrinsic/runtime calls (`@keet.io.print`, `@keet.io.println`, `@keet.io.readln`, `@keet.io.input`, `@keet.io.red`, etc.).
  - Lower `io.Color.<Member>` accesses into typed constant/enum operands in K-IR.

- [x] **Task 3.2: Emit C++20 `::keet::io::*` Calls in `CppEmitter` & `HeaderEmitter`**
  - Update `src/emitter/cpp_emitter.cpp` to translate K-IR `@keet.io.*` instructions into `::keet::io::print(...)`, `::keet::io::println(...)`, `::keet::io::readln()`, `::keet::io::input(...)`, `::keet::io::red(...)`, and `::keet::io::Color::*`.
  - Ensure generated C++ files include `<keet/runtime.hpp>` (which includes `<keet/io.hpp>`).

---

## Phase 4: Unit & Conformance Test Suite (`tests/`)
*Objective: Achieve 100% test coverage across C++ runtime unit tests, compiler pipeline unit tests, and end-to-end KeetScript conformance tests.*

- [x] **Task 4.1: C++ Runtime Unit Tests (`tests/unit/test_runtime_io.cpp`)**
  - Test `keet::io::print` and `keet::io::println` using `std::ostringstream` redirection across `int`, `float`, `BoolByte`, and `string` arguments.
  - Test `keet::io::red`, `green`, `yellow`, `bold`, and `Color` with `set_color_enabled(true)` (verifying exact ANSI escape sequences) and `set_color_enabled(false)` (verifying plain text output).
  - Test `keet::io::readln` and `keet::io::input` using mocked `std::istringstream` inputs (including LF `\n` and CRLF `\r\n` line endings).

- [x] **Task 4.2: Compiler Unit Tests (`tests/unit/test_type_checker.cpp`, `tests/unit/test_cpp_emitter.cpp`)**
  - Add unit tests verifying valid `import io;` resolution, valid `io.*` calls, and rejection of invalid argument types/counts.
  - Add emitter tests verifying that `io.println("Status:", io.green("OK"))` and `io.input("Name: ")` emit the expected C++20 code.

- [x] **Task 4.3: End-to-End Conformance Tests (`tests/conformance/`)**
  - Create positive conformance test `tests/conformance/positive/06_io/io_console.keet` and its test runner in `tests/conformance/test_conformance_io.cpp` to compile and execute a full KeetScript program using `io.print`, `io.println`, `io.Color`, `io.green`, `io.readln`, and `io.input`.
  - Create negative conformance test `tests/conformance/negative/neg_io_type_mismatch.keet` verifying compile-time rejection of invalid `io` calls.

- [x] **Task 4.4: CMake & CTest Registration (`tests/CMakeLists.txt`)**
  - Register `test_runtime_io.cpp` and `test_conformance_io.cpp` in `tests/CMakeLists.txt`.
  - Run `ctest --test-dir build --output-on-failure` and ensure 100% of tests pass with zero warnings.

---

## Phase 5: Documentation Updates (`documentation/`)
*Objective: Keep the language and test specifications synchronized with the implementation.*

- [ ] **Task 5.1: Update `The KeetScript Language Specification.md` (and/or `documentation/stdlib/io.md`)**
  - Document the standard `io` module API (`print`, `println`, `readln`, `input`, `Color`, and inline styling helpers).

- [ ] **Task 5.2: Update `KeetScript Conformance Test Suite and Benchmark Specification.md`**
  - Register the positive and negative `io` conformance test cases in the specification document.
