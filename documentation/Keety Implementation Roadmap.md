# Keety Compiler Implementation Roadmap & Progress Tracker

This document specifies the operational development plan for the `keety` compiler organized in the hierarchy: **Stage $\rightarrow$ Phase $\rightarrow$ Task**. It serves as an active progress tracker and development navigation guide.

---

## Progress Summary

- [x] **Stage I: Foundations, Diagnostic Infrastructure & Runtime** (100% Completed)
- [ ] **Stage II: Front-End (Lexer, Parser & AST)** (In Progress)
- [ ] **Stage III: Module Dependency Graph & Semantic Analysis**
- [ ] **Stage IV: KeetIR Intermediate Representation & Optimizations**
- [ ] **Stage V: Back-End (C++17 Code Emission & Toolchain Linking)**
- [ ] **Stage VI: CLI Driver, Conformance Test Suite & Benchmarks**

---

## Detailed Task Registry

### **Stage I: Foundations, Diagnostic Infrastructure & Runtime** `[COMPLETED]`

#### **Phase 1.1: Project Setup & Build Environment**
- [x] **Task 1.1.1:** Root `CMakeLists.txt` configuration for C++20 standard, strict warnings (`-Wall -Wextra -Wpedantic` / MSVC flags), `.clang-format`, and `.clang-tidy` integration.
- [x] **Task 1.1.2:** Directory hierarchy configuration (`include/keety/`, `src/`, `runtime/`, `tests/`, `benchmarks/`).
- [x] **Task 1.1.3:** Unit test runner integration (Catch2 v3.5.2 via `FetchContent`) in CMake with CTest discovery.

#### **Phase 1.2: Source Location & Diagnostic Reporting Engine (`keety::diag`)**
- [x] **Task 1.2.1:** Implement source location primitives: `SourceLocation` (offset, line, column) and `SourceSpan` (half-open range [start, end)).
- [x] **Task 1.2.2:** Implement `SourceManager` and `SourceBuffer` with hermetic in-memory file management and UTF-8 line/column decoding.
- [x] **Task 1.2.3:** Implement Rustc/Clang-style diagnostic emitter (`DiagnosticEmitter`: ANSI color underlines `^^^^`, inline labels, line context, `= help:` and `= note:` footers).
- [x] **Task 1.2.4:** Define error code registry and enum (`E0001`–`E6999`) with title, description, and canonical category ranges.

#### **Phase 1.3: C++ Runtime Headers & Primitives (`runtime/keet/`)**
- [x] **Task 1.3.1:** Implement `keet::error` (base runtime exception struct inheriting from `std::exception` with `name`, `message`, `toString()`, and `what()`).
- [x] **Task 1.3.2:** Implement `keet::BoolByte` (1-byte boolean wrapper eliminating the `std::vector<bool>` proxy reference specialization issues).
- [x] **Task 1.3.3:** Implement `keet::deep_copy_ptr<T>` (transparent value-copying smart pointer for self-referential classes and recursive nodes).
- [x] **Task 1.3.4:** Implement deterministic 64-bit two's-complement overflow helpers (`add_wrap`, `sub_wrap`, `mul_wrap`, `neg_wrap`, `div_safe`, `mod_safe`).

---

### **Stage II: Front-End (Lexer, Parser & AST)**

#### **Phase 2.1: Lexical Analysis (`keety::lexer`)**
- [x] **Task 2.1.1:** Define `TokenKind` (keywords, literals, operators, delimiters, special tokens) and the `Token` struct.
- [x] **Task 2.1.2:** Implement UTF-8 scanner with *Maximal Munch* rule and comment handling (`//`, `/* ... */`).
- [x] **Task 2.1.3:** Implement context-sensitive disambiguation for Blank Identifier: standalone `_` as `Token::BlankIdent`, and `_count` as a regular identifier.
- [x] **Task 2.1.4:** Implement angle bracket disambiguation: `<` immediately following an identifier with no whitespace triggers generic type arguments, and type-context `>` does not combine into `>>`.
- [x] **Task 2.1.5:** Implement multiline template literal scanner with recursive `${expr}` interpolation state stack.

#### **Phase 2.2: Abstract Syntax Tree Representation (`keety::ast`)**
- [x] **Task 2.2.1:** Define declaration AST node hierarchy (`VarDecl`, `ConstDecl`, `DefineDecl`, `FnDecl`, `ClassDecl`, `EnumDecl`, `ErrTypeDecl`, `ImportDecl`, `ExportDecl`).
- [x] **Task 2.2.2:** Define statement AST node hierarchy (`IfStmt`, `WhileStmt`, `ForStmt`, `SwitchStmt`, `ReturnStmt`, `YieldStmt`, `BreakStmt`, `ContinueStmt`, `ZonkStmt`).
- [x] **Task 2.2.3:** Define expression and operator AST nodes (`BinaryExpr`, `UnaryExpr`, `CallExpr`, `IndexExpr`, `MemberExpr`, `ZonkExpr`, `TernaryExpr`, `TypeCastExpr`, `ClosureExpr`).
- [x] **Task 2.2.4:** Define type annotation AST nodes (`PrimitiveType`, `ArrayType`, `MapType`, `UnionType`, `FunctionType`, `CustomType`).

#### **Phase 2.3: Syntactic Analysis (`keety::parser`)**
- [x] **Task 2.3.1:** Implement top-down Recursive Descent parser with bounded LL(k) lookahead.
- [x] **Task 2.3.2:** Implement Pratt / Precedence Climbing parser for binary and unary expressions according to formal operator precedence tables.
- [x] **Task 2.3.3:** Implement header disambiguation for `if`, `while`, and `switch` statements (`IfHeaderInit` with semicolon vs simple conditional expression).
- [x] **Task 2.3.4:** Implement `zonk` suffix disambiguation: fallback expression `expr zonk (fallback)` vs recovery block `expr zonk (err) { ... }`.
- [x] **Task 2.3.5:** Parse loops and switch statements with labels (`as labelName`) and enforce mandatory statement-terminating semicolons (`;`).

---

### **Stage III: Module Dependency Graph & Semantic Analysis** `[CURRENT]`

#### **Phase 3.1: Module Resolver & Cyclic Dependency Detection (`keety::resolver`)**
- [x] **Task 3.1.1:** Implement module path resolution (standard library modules, third-party `keet_modules/`, local `./relative` paths).
- [x] **Task 3.1.2:** Construct directed dependency graph $G = (V, E)$.
- [x] **Task 3.1.3:** Implement Tarjan's Strongly Connected Components (SCC) algorithm to detect cycles and emit `E1001`.
- [x] **Task 3.1.4:** Compute deterministic topological initialization order (post-order DFS) and single-instantiation deduplication for diamond imports.

#### **Phase 3.2: Symbol Scoping & Strict No-Shadowing Enforcement**
- [x] **Task 3.2.1:** Implement hierarchical symbol table (`Scope`: Universe $\rightarrow$ Module $\rightarrow$ Function/Method $\rightarrow$ Block).
- [x] **Task 3.2.2:** Register predeclared Universe symbols (`int`, `float`, `string`, `bool`, `error`, `typeof`, etc.).
- [x] **Task 3.2.3:** Enforce strict no-shadowing rule: emit `E2001` if a local variable or parameter shadows any outer declaration.
- [x] **Task 3.2.4:** Enforce `const`-view visibility semantics for imported `export let` variables (`E2002`).

#### **Phase 3.3: Static Type Checking & Union Algebra Normalization (`keety::typechecker`)**
- [x] **Task 3.3.1:** Enforce explicit type annotations on declarations (`let`, `const`, parameters) and emit `E3001` for omitted types.
- [x] **Task 3.3.2:** Enforce strict zero-coercion policy: emit `E3002` on implicit int $\rightarrow$ float conversion.
- [x] **Task 3.3.3:** Contextually type empty collection literals (`[]` as `array<T>`, `{}` as `map<K, V>`).
- [x] **Task 3.3.4:** Canonical union algebra normalization (flattening, deduplication, subtype absorption).

#### **Phase 3.4: Control-Flow Analysis (CFA) & Type Narrowing**
- [x] **Task 3.4.1:** Track flow-based type narrowing within `if (typeof(x) == "T")` and `if (x != null)`.
- [x] **Task 3.4.2:** Implement *Mutation Watcher* dirty-bit tracking: invalidate narrowed types upon reassignment or passing to `mut`.
- [x] **Task 3.4.3:** Exhaustiveness and reachability analysis for non-void functions, flagging dead code after terminating statements.

#### **Phase 3.5: Mutable Reference Borrow Checker (`mut`)**
- [x] **Task 3.5.1:** Validate l-values: reject r-values and literals passed to `mut` parameters (`E4005`).
- [x] **Task 3.5.2:** Enforce exclusive mutable borrows: reject duplicate borrows `f(mut x, mut x)` (`E4001`).
- [x] **Task 3.5.3:** Reject simultaneous borrowing of container and its element: `f(mut arr[i], mut arr)` (`E4002`).
- [x] **Task 3.5.4:** Forbid taking mutable references to map elements: `f(mut m[k])` (`E4003`).
- [x] **Task 3.5.5:** Reject `mut` parameters in `async` functions (`E4004`) or default arguments (`E4006`).

---

### **Stage IV: KeetIR Intermediate Representation & Optimizations**

#### **Phase 4.1: AST Lowering to KeetIR**
- [ ] **Task 4.1.1:** Define KeetIR instruction set and Control Flow Graph (CFG with Basic Blocks).
- [ ] **Task 4.1.2:** Lower expressions, loops, and pattern-matching switches to basic blocks and conditional branches.
- [ ] **Task 4.1.3:** Lower `zonk` error recovery to exception landing pads and handlers.

#### **Phase 4.2: KeetIR Optimization Passes**
- [ ] **Task 4.2.1:** *Pass 1 (Static Liveness Analysis):* Track variable live ranges and identify last-use points.
- [ ] **Task 4.2.2:** *Pass 2 (Last-Use Move Insertion):* Replace last reads of r-value eligible variables with `std::move`.
- [ ] **Task 4.2.3:** *Pass 3 (Const-Reference Parameter Promotion):* Promote unmutated composite parameters (`string`, `array`, `map`, `class`) to `const T&`.
- [ ] **Task 4.2.4:** *Pass 4 (Pipeline Capacity Pre-Allocation):* Detect functional chains (`.map()`, `.filter()`) and insert `.reserve()`.
- [ ] **Task 4.2.5:** *Pass 5 (Recursive Smart-Pointer Boxing):* Detect self-referential class fields and insert `keet::deep_copy_ptr<T>`.

---

### **Stage V: Back-End (C++17 Code Emission & Toolchain Linking)**

#### **Phase 5.1: C++ Header Emitter (`<module>.hpp`)**
- [ ] **Task 5.1.1:** Emit `#pragma once`, required standard headers, and imported module includes.
- [ ] **Task 5.1.2:** Emit module namespace encapsulation `namespace keet_module_<name> { ... }`.
- [ ] **Task 5.1.3:** Emit forward declarations, `constexpr` definitions, and class/struct/enum definitions.
- [ ] **Task 5.1.4:** Emit generic function templates, exported function prototypes, and `inline const T&` const-views for module variables.

#### **Phase 5.2: C++ Implementation Emitter (`<module>.cpp`)**
- [ ] **Task 5.2.1:** Emit storage allocations for module-level variables (`_internal_<name>`).
- [ ] **Task 5.2.2:** Translate function bodies to C++17 using `std::variant`, `std::vector`, `std::unordered_map`.
- [ ] **Task 5.2.3:** Emit root `main(int argc, char* argv[])` with uncaught `keet::error` and `std::exception` handlers.

#### **Phase 5.3: Native Toolchain Driver**
- [ ] **Task 5.3.1:** Detect platform C++ toolchain (`clang++`, `g++`, `cl.exe`).
- [ ] **Task 5.3.2:** Execute native compiler invocations with `-std=c++17`, optimization level (`-O3`), and `-fwrapv`.
- [ ] **Task 5.3.3:** Link object files into final executable binaries or shared libraries.

---

### **Stage VI: CLI Driver, Conformance Test Suite & Benchmarks**

#### **Phase 6.1: CLI Interface (`keety`)**
- [ ] **Task 6.1.1:** Implement `keety check <file>` command (instant IDE-grade validation without C++ emission).
- [ ] **Task 6.1.2:** Implement `keety emit <file> (--cpp | --hpp | --ast | --ir)`.
- [ ] **Task 6.1.3:** Implement `keety build <file>` and `keety run <file> [-- args]`.
- [ ] **Task 6.1.4:** Support CLI configuration flags (`--output-dir`, `--cxx`, `--opt`, `--verbose`).

#### **Phase 6.2: Conformance Test Suite (`keety test --conformance`)**
- [ ] **Task 6.2.1:** Positive conformance tests: Scalars and Zero-UB (`scalars_wraparound`, `scalars_divzero_recover`, `scalars_float_clamp_bounds`).
- [ ] **Task 6.2.2:** Positive conformance tests: Value semantics and mutable borrowing (`value_semantics_collections`, `mut_parameter_swap`).
- [ ] **Task 6.2.3:** Positive conformance tests: Unions and flow narrowing (`union_flow_narrowing`, `union_narrowing_invalidation`).
- [ ] **Task 6.2.4:** Positive conformance tests: OOP, inheritance, and constructors (`class_inheritance_constructor`).
- [ ] **Task 6.2.5:** Positive conformance tests: Control flow and error recovery (`switch_named_loop_break`, `zonk_block_yield`).
- [ ] **Task 6.2.6:** Negative compiler rejection tests: Verify exact diagnostic error codes (`neg_shadowing` $\rightarrow$ `E2001`, `neg_mut_alias` $\rightarrow$ `E4001`, `neg_circular_module` $\rightarrow$ `E1001`, etc.).

#### **Phase 6.3: Reference Performance Benchmarks (`keety test --benchmarks`)**
- [ ] **Task 6.3.1:** Benchmark 1: *Heavy Numeric Vector Processing* (target: ratio $\le 1.02\times$ C++17).
- [ ] **Task 6.3.2:** Benchmark 2: *Hash Map & Union Variant Dispatch* (target: ratio $\le 1.04\times$ C++17).
- [ ] **Task 6.3.3:** Benchmark 3: *High-Throughput String Ingestion* (target: ratio $\le 1.01\times$ C++17).
- [ ] **Task 6.3.4:** Benchmark 4: *Asynchronous Task Concurrency* (target: ratio $\le 1.05\times$ C++17).
