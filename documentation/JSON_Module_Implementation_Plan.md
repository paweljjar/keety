# KeetScript Standard Library: `json` Module Implementation Plan

## 1. Executive Summary & Architectural Decisions

The `json` standard library module (`import json;`, `import json as j;`, `from "json" import ...;`) brings RFC 8259-compliant JSON parsing, dot-path querying, typed collection extraction, immutable object/array mutation builders, and polymorphic serialization to KeetScript.

### Core Design Principles
1. **Option A — Native Type Representation & Dot-Path Extraction (No `json.Value` Compiler Type):**
   - Operates purely on KeetScript's built-in types (`string`, `int`, `float`, `bool`, `array<T>`, `map<string, T>`).
   - Provides dot-path extraction (`getString`, `getInt`, `getFloat`, `getBool`, `getArray`, `getObject`, `hasKey`, `isNull`) and non-throwing fallback extractors (`getStringOr`, `getIntOr`, `getFloatOr`, `getBoolOr`) supporting arbitrarily nested object keys and array indices (e.g., `"server.host"`, `"users.0.name"`, `"matrix.1.2"`).
   - Provides typed collection parsers (`parseMap`, `parseArray`, `parseIntArray`, `parseFloatArray`, `parseBoolArray`, `keys`, `size`) and immutable JSON builder/mutation helpers (`emptyObject`, `emptyArray`, `setString`, `setInt`, `setFloat`, `setBool`, `setNull`, `setRaw`, `removeKey`, `readFile`, `writeFile`).
2. **Option C — Polymorphic `json.stringify`:**
   - Polymorphic compiler built-in in `TypeChecker` and C++20 `if constexpr` template serializer in `runtime/keet/json.hpp`.
   - Serializes `int`, `float`, `bool`, `char`, `string`, `null`, `array<T>`, `map<string, V>`, and union/nullable types (`std::variant<...>`) into RFC 8259 JSON strings with deterministic lexicographical map key ordering.
3. **Strict Language Guardrails:**
   - **Strictly `camelCase`** function names across all APIs (zero `snake_case` aliases).
   - **No trailing `!`** on return types in documentation, signatures, or diagnostics.
   - **Strict Zero-Coercion Policy (`E3002`):** `int` and `float` remain distinct in typed extractors and builders (`getInt` vs `getFloat`, `setInt` vs `setFloat`), while `getFloat` / `parseFloatArray` accept standard JSON numbers seamlessly.
   - **Native `zonk` Error Recovery:** All fallible operations throw `keet::error("JsonError", ...)` (or `FileSystemError` / `JsonError` in file helpers) with clear diagnostic messages including line/column positions for syntax errors.

---

## 2. End-to-End Compiler Pipeline Architecture

```mermaid
flowchart LR
    subgraph Source["KeetScript Source (.keet)"]
        A["import json;\njson.getString(raw, \"a.b\")\njson.stringify(data)"]
    end

    subgraph Frontend["Stage 1–3: Resolver, Sema & TypeChecker"]
        B["ModulePathResolver\nrecognizes 'json', 'std.json', 'keet.json'"]
        C["sema::populateJsonModuleScope\n35 exported camelCase functions"]
        D["TypeChecker\nStrict signature checks +\npolymorphic json.stringify(Any)"]
    end

    subgraph MiddleEnd["Stage 4–5: KeetIR & Lowering"]
        E["AstLowerer\nLowers json.fn(...) &\nselective imports to keet::json::fn"]
    end

    subgraph Backend["Stage 6: C++20 Emitter & Runtime"]
        F["CppEmitter\nEmits keet::json::* calls\n#include <keet/runtime.hpp>"]
        G["runtime/keet/json.hpp\nRFC 8259 DOM + Dot-Path Engine +\nC++20 constexpr/template Serializer"]
    end

    A --> B --> C --> D --> E --> F --> G
```

---

## 3. Complete `json` Module API Specification (35 Functions)

### 3.1 Validation & Type Inspection (Non-Throwing / Fallible Inspection)

| Function Signature | Return Type | Fallible (`zonk`) | Description |
|---|---|---|---|
| `json.isValid(text: string): bool` | `bool` | No | Returns `true` if `text` is strictly valid RFC 8259 JSON; `false` otherwise. |
| `json.typeOf(text: string): string` | `string` | No | Returns `"object"`, `"array"`, `"string"`, `"number"`, `"bool"`, `"null"`, or `"invalid"`. |
| `json.isObject(text: string): bool` | `bool` | No | Returns `true` if `text` is a valid JSON object (`{...}`). |
| `json.isArray(text: string): bool` | `bool` | No | Returns `true` if `text` is a valid JSON array (`[...]`). |
| `json.size(text: string): int` | `int` | Yes (`JsonError`) | Returns the number of entries in a JSON object or elements in a JSON array. Throws `JsonError` if invalid JSON or not an object/array. |
| `json.keys(text: string): array<string>` | `array<string>` | Yes (`JsonError`) | Returns a sorted array of top-level keys of a JSON object. Throws `JsonError` if invalid JSON or not an object. |

### 3.2 Dot-Path Queries & Typed Extraction

Dot-paths use `.` to traverse nested objects and arrays (e.g., `"server.host"`, `"users.0.name"`, `"matrix.1.2"`). An empty path `""` refers to the root JSON value.

| Function Signature | Return Type | Fallible (`zonk`) | Description |
|---|---|---|---|
| `json.hasKey(text: string, path: string): bool` | `bool` | No | Returns `true` if `text` is valid JSON and `path` resolves to an existing key/index (even if its value is `null`); `false` otherwise. |
| `json.isNull(text: string, path: string): bool` | `bool` | No | Returns `true` if `text` is valid JSON and `path` resolves to a JSON `null` literal. |
| `json.getString(text: string, path: string): string` | `string` | Yes (`JsonError`) | Extracts the unescaped string at `path`. Throws `JsonError` on invalid JSON, missing path, or non-string value. |
| `json.getInt(text: string, path: string): int` | `int` | Yes (`JsonError`) | Extracts an exact integer (`int64_t`) at `path`. Throws `JsonError` on missing path, non-number, or fractional/out-of-range number. |
| `json.getFloat(text: string, path: string): float` | `float` | Yes (`JsonError`) | Extracts a `double` at `path` (accepts both integer and floating-point JSON numbers). Throws `JsonError` on missing path or non-number. |
| `json.getBool(text: string, path: string): bool` | `bool` | Yes (`JsonError`) | Extracts a boolean at `path`. Throws `JsonError` on missing path or non-boolean value. |
| `json.getArray(text: string, path: string): array<string>` | `array<string>` | Yes (`JsonError`) | Extracts a JSON array at `path`, returning each element serialized as a canonical JSON string (or unquoted string if string element? *Note:* returns raw JSON representation for nested objects/arrays/primitives, and unescaped string or raw JSON—see §4.2 design rules: `getArray` returns serialized JSON strings for each element so nested objects/arrays can be further queried with `json.get*`, while `parseArray` extracts string elements). |
| `json.getObject(text: string, path: string): string` | `string` | Yes (`JsonError`) | Extracts a nested JSON object or sub-document at `path` as a serialized JSON string. Throws `JsonError` if missing or not an object. |

### 3.3 Non-Throwing Fallback Extractors (`*Or`)

| Function Signature | Return Type | Fallible (`zonk`) | Description |
|---|---|---|---|
| `json.getStringOr(text: string, path: string, defaultVal: string): string` | `string` | No | Returns `json.getString(text, path)` on success, or `defaultVal` if invalid JSON, missing path, or type mismatch. |
| `json.getIntOr(text: string, path: string, defaultVal: int): int` | `int` | No | Returns `json.getInt(text, path)` on success, or `defaultVal` on error. |
| `json.getFloatOr(text: string, path: string, defaultVal: float): float` | `float` | No | Returns `json.getFloat(text, path)` on success, or `defaultVal` on error. |
| `json.getBoolOr(text: string, path: string, defaultVal: bool): bool` | `bool` | No | Returns `json.getBool(text, path)` on success, or `defaultVal` on error. |

### 3.4 Typed Collection Parsers

| Function Signature | Return Type | Fallible (`zonk`) | Description |
|---|---|---|---|
| `json.parseMap(text: string): map<string, string>` | `map<string, string>` | Yes (`JsonError`) | Parses a JSON object into a `map<string, string>`. String values are unescaped directly; non-string values (numbers, booleans, nested objects/arrays, `null`) are converted to their canonical JSON string representation. Throws `JsonError` if `text` is not a valid JSON object. |
| `json.parseArray(text: string): array<string>` | `array<string>` | Yes (`JsonError`) | Parses a JSON array into an `array<string>`. String elements are unescaped directly; non-string elements are converted to their canonical JSON string representation. Throws `JsonError` if `text` is not a valid JSON array. |
| `json.parseIntArray(text: string): array<int>` | `array<int>` | Yes (`JsonError`) | Parses a homogeneous JSON array of integers into `array<int>`. Throws `JsonError` if not an array or if any element is not an exact integer. |
| `json.parseFloatArray(text: string): array<float>` | `array<float>` | Yes (`JsonError`) | Parses a homogeneous JSON array of numbers into `array<float>`. Throws `JsonError` if not an array or if any element is not a number. |
| `json.parseBoolArray(text: string): array<bool>` | `array<bool>` | Yes (`JsonError`) | Parses a homogeneous JSON array of booleans into `array<bool>`. Throws `JsonError` if not an array or if any element is not a boolean. |

### 3.5 Immutable Object & Array Builders / Mutation Helpers

All builder functions take an existing JSON document (`text`), validate it, apply the modification at `path` (creating intermediate objects along a dot-path if needed), and return a **new** serialized JSON string, preserving KeetScript's deep-value semantics.

| Function Signature | Return Type | Fallible (`zonk`) | Description |
|---|---|---|---|
| `json.emptyObject(): string` | `string` | No | Returns `"{}"`. |
| `json.emptyArray(): string` | `string` | No | Returns `"[]"`. |
| `json.setString(text: string, path: string, value: string): string` | `string` | Yes (`JsonError`) | Sets `path` to the JSON string `value` in `text` and returns the updated JSON string. |
| `json.setInt(text: string, path: string, value: int): string` | `string` | Yes (`JsonError`) | Sets `path` to the integer `value` in `text` and returns the updated JSON string. |
| `json.setFloat(text: string, path: string, value: float): string` | `string` | Yes (`JsonError`) | Sets `path` to the finite float `value` in `text` and returns the updated JSON string. Throws `JsonError` if `value` is `NaN` or `Infinity`. |
| `json.setBool(text: string, path: string, value: bool): string` | `string` | Yes (`JsonError`) | Sets `path` to the boolean `value` in `text` and returns the updated JSON string. |
| `json.setNull(text: string, path: string): string` | `string` | Yes (`JsonError`) | Sets `path` to JSON `null` in `text` and returns the updated JSON string. |
| `json.setRaw(text: string, path: string, rawJson: string): string` | `string` | Yes (`JsonError`) | Parses `rawJson` as a JSON value and embeds it at `path` inside `text`. Throws `JsonError` if either `text` or `rawJson` is invalid JSON. |
| `json.removeKey(text: string, path: string): string` | `string` | Yes (`JsonError`) | Removes the object key or array index at `path` (no-op if key does not exist) and returns the updated JSON string. Throws `JsonError` if `text` is invalid JSON or `path` is empty. |

### 3.6 Serialization, Formatting, Escaping & File I/O

| Function Signature | Return Type | Fallible (`zonk`) | Description |
|---|---|---|---|
| `json.stringify(value: Any): string` | `string` | Yes (`JsonError`) | Polymorphic serializer accepting `int`, `float`, `bool`, `char`, `string`, `null`, `array<T>`, `map<string, V>`, and unions. Throws `JsonError` if a `float` is `NaN` or `Infinity`. |
| `json.format(text: string, indent: int): string` | `string` | Yes (`JsonError`) | Validates `text` and pretty-prints it with `indent` spaces per nesting level (`0 <= indent <= 16`). Throws `JsonError` on invalid JSON or negative/`>16` indent. |
| `json.minify(text: string): string` | `string` | Yes (`JsonError`) | Validates `text` and removes all insignificant whitespace. Throws `JsonError` on invalid JSON. |
| `json.escape(str: string): string` | `string` | No | Escapes a raw string into a double-quoted JSON string literal (`"..."`), escaping `"`, `\`, `\b`, `\f`, `\n`, `\r`, `\t`, and control characters `< 0x20`. |
| `json.unescape(str: string): string` | `string` | Yes (`JsonError`) | Unescapes a JSON string literal (with or without surrounding double quotes), resolving `\uXXXX` and UTF-16 surrogate pairs to UTF-8. Throws `JsonError` on invalid escape sequences. |
| `json.readFile(path: string): string` | `string` | Yes (`JsonError`) | Reads UTF-8 file at `path`, validates that its contents are valid RFC 8259 JSON, and returns the trimmed/validated JSON text. Throws `JsonError` if file cannot be read or contains invalid JSON. |
| `json.writeFile(path: string, text: string, indent: int): bool` | `bool` | Yes (`JsonError`) | Validates `text` as JSON, formats it (`indent == 0` for minified, `indent > 0` for pretty-printed with trailing newline), and writes it to `path`. Returns `true` on success; throws `JsonError` on invalid JSON, invalid indent, or file write error. |

---

## 4. Phased Implementation Plan

### Phase 1: C++20 Runtime Implementation (`runtime/keet/json.hpp`)

**Objective:** Implement a self-contained, zero-dependency, RFC 8259-compliant JSON parser, DOM node representation, dot-path engine, and polymorphic C++20 serializer in `runtime/keet/json.hpp`.

#### Tasks
1. **Create `runtime/keet/json.hpp` (`namespace keet::json`):**
   - **Internal DOM (`detail::JsonNode`):**
     - Represent JSON values internally using a `std::variant`:
       - `std::monostate` (`null`)
       - `bool`
       - `int64_t` (exact 64-bit signed integers)
       - `double` (floating-point numbers)
       - `std::string`
       - `std::vector<JsonNode>` (arrays)
       - `std::map<std::string, JsonNode>` (objects — `std::map` guarantees deterministic lexicographical key ordering on serialization, or preserve insertion order / sorted keys consistently).
   - **RFC 8259 Recursive-Descent Parser (`detail::Parser`):**
     - Track `line` and `column` numbers for accurate `keet::error("JsonError", ...)` messages (e.g., `"JsonError: unexpected character '}' at line 3, column 12"`).
     - Enforce maximum nesting depth (e.g., `512`) to prevent stack overflow on malicious inputs.
     - Parse strings with full escape handling (`\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`, and `\uXXXX` including UTF-16 high/low surrogate pairs `0xD800..0xDFFF` encoded into valid 4-byte UTF-8). Reject unescaped control characters (`0x00..0x1F`).
     - Parse numbers strictly per RFC 8259 (no leading zeros like `01`, no trailing decimal point `1.`, no leading `+`). Distinguish integer tokens that fit in `int64_t` from floating-point tokens (`double`).
     - Reject trailing non-whitespace tokens after the root value.
   - **Dot-Path Navigation & Mutation Engine (`detail::resolve_path`, `detail::mutate_path`):**
     - Split `path` on `.` segments.
     - If segment navigates into an Object, look up key; if navigating into an Array, parse non-negative integer index `0 <= idx < arr.size()`.
     - In `set*` mutation helpers:
       - Support updating existing keys or inserting new keys into objects, automatically creating intermediate nested objects if a missing key is traversed along the dot-path.
       - Support updating existing array indices `0 <= idx < arr.size()` or appending at `idx == arr.size()`.
   - **Polymorphic `keet::json::stringify(const T& value)`:**
     - Use C++20 `if constexpr` type traits (`detail::is_vector_v`, `detail::is_unordered_map_v`, `detail::is_variant_v`, `std::is_same_v<T, keet::bool_byte>`, `std::is_same_v<T, std::monostate>`, `keet::deep_copy_ptr<U>`):
       - `std::monostate` / ` std::nullptr_t` $\rightarrow$ `"null"`
       - `bool` / `keet::bool_byte` $\rightarrow$ `"true"` or `"false"`
       - `int64_t` / integral types (excluding `char` and `bool`) $\rightarrow$ `std::to_string(val)`
       - `char` $\rightarrow$ single-character escaped JSON string
       - `double` / floating-point $\rightarrow$ check `std::isfinite(val)` (throw `keet::error("JsonError", "stringify: cannot serialize NaN or Infinity to JSON")` if non-finite); format cleanly without trailing zeros while preserving precision (`std::snprintf` with `%.15g` or `std::to_chars`).
       - `std::string` / `std::string_view` / `const char*` $\rightarrow$ `escape(val)`
       - `std::vector<E>` $\rightarrow$ `"[" + stringify(e0) + "," + ... + "]"`
       - `std::unordered_map<std::string, V>` / `std::map<std::string, V>` $\rightarrow$ sort keys lexicographically for deterministic output and emit `"{\"k1\":" + stringify(v1) + ... + "}"`
       - `std::variant<Ts...>` $\rightarrow$ `std::visit([](const auto& inner) { return stringify(inner); }, val)`
       - `keet::deep_copy_ptr<U>` $\rightarrow$ dereference if non-null, or `"null"`.
2. **Create `runtime/keet/std/json.hpp`:**
   - Forwarding header `#pragma once` + `#include <keet/json.hpp>`.
3. **Update `runtime/keet/runtime.hpp`:**
   - Add `#include <keet/json.hpp>` alongside `<keet/io.hpp>`, `<keet/fs.hpp>`, `<keet/sys.hpp>`, `<keet/math.hpp>`, and `<keet/time.hpp>`.

---

### Phase 2: Module Resolution, Universe & Semantic Analysis

**Objective:** Register `json` (`"json"`, `"std.json"`, `"keet.json"`) in the module resolver and universe symbol tables.

#### Tasks
1. **Update `src/resolver/module_path_resolver.cpp`:**
   - Add `"json"` to `stdlibModules` in `ModulePathResolver::isStandardLibraryModule`.
2. **Update `include/keety/sema/universe.hpp` and `src/sema/universe.cpp`:**
   - Update `isStdlibModule(std::string_view name)` to return `true` for `"json"`, `"std.json"`, and `"keet.json"`.
   - Declare and implement `void populateJsonModuleScope(Scope& jsonScope)` registering all 37 `camelCase` functions as exported `SymbolKind::Function` symbols:
     - `isValid`, `typeOf`, `isObject`, `isArray`, `size`, `keys`
     - `hasKey`, `isNull`, `getString`, `getInt`, `getFloat`, `getBool`, `getArray`, `getObject`
     - `getStringOr`, `getIntOr`, `getFloatOr`, `getBoolOr`
     - `parseMap`, `parseArray`, `parseIntArray`, `parseFloatArray`, `parseBoolArray`
     - `emptyObject`, `emptyArray`, `setString`, `setInt`, `setFloat`, `setBool`, `setNull`, `setRaw`, `removeKey`
     - `stringify`, `format`, `minify`, `escape`, `unescape`, `readFile`, `writeFile`
   - Update `createStdlibModuleScope` to dispatch `"json"`, `"std.json"`, and `"keet.json"` to `populateJsonModuleScope(*scope)`.

---

### Phase 3: Static Type Checking (`TypeChecker`)

**Objective:** Enforce strict arity, parameter types, return types, polymorphic `json.stringify` validation, and helpful diagnostics (`E2001`, `E2005`, `E3002`, `E3004`) in `src/typechecker/type_checker.cpp`.

#### Tasks
1. **Selective Import Resolution (`from "json" import ...`):**
   - In `TypeChecker::checkImportDecl`, handle `modPath == "json" || modPath == "std.json" || modPath == "keet.json"`:
     - Create `auto jsonScope = sema::createStdlibModuleScope("json");`
     - Bind each selectively imported symbol with its exact `Type::function(...)` signature.
     - For `stringify`, bind a sentinel function type `Type::function({Type::unknownType()}, Type::stringType())` and handle direct calls to selectively imported `stringify(...)` with the same polymorphic validation as `json.stringify(...)`.
     - Emit `ErrorCode::UnexportedSymbol` with Levenshtein `"did you mean ...?"` suggestion if a non-existent symbol is imported from `"json"`.
2. **Unimported `json` Usage Diagnostics:**
   - In `TypeChecker::checkCallExpr` and `TypeChecker::checkMemberExpr`, detect when `json.fn(...)` or `json.member` is used without importing `json`, emitting `ErrorCode::UndeclaredIdentifier` with help `"add 'import json;' at the top of the file"`.
3. **Member Call Type Checking (`json.fn(...)`):**
   - Validate all 37 `json` functions in `TypeChecker::checkCallExpr`:
     - Verify exact argument count (`ErrorCode::ArityMismatch`).
     - Verify argument types strictly without implicit coercion (`ErrorCode::TypeMismatch`).
     - For `json.stringify(arg)` (and selectively imported `stringify(arg)`):
       - Require exactly 1 argument.
       - Validate that the argument type is serializable (`int`, `float`, `bool`, `char`, `string`, `null`, `array<T>`, `map<string, V>`, or a `UnionType` of serializable variants; reject `void`, `function`, or `map<K, V>` where `K` is not `string`).
       - Return `Type::stringType()`.
4. **Member Expression Lookup (`json.fn` as value / non-call member access):**
   - In `TypeChecker::checkMemberExpr`, validate member access on `json` module symbols and report `ErrorCode::UnexportedSymbol` with Levenshtein suggestions for unknown members.

---

### Phase 4: IR Lowering & C++20 Code Emission (`AstLowerer` & `CppEmitter`)

**Objective:** Lower `json.*` module calls and selectively imported `json` functions to `keet::json::*` in both the KeetIR pipeline and direct C++20 code emitter.

#### Tasks
1. **Update `include/keety/ir/ast_lowerer.hpp` and `src/ir/ast_lowerer.cpp`:**
   - Add `bool has_json_import_{false};` and `std::unordered_set<std::string> imported_json_symbols_;`.
   - In `AstLowerer::lower_module`, detect `import json;` and `from "json" import ...;`, populating `has_json_import_` and `imported_json_symbols_`.
   - In `AstLowerer::lower_call`:
     - Rewrite `json.<fn>(...)` (or aliased module calls) to `keet::json::<fn>(...)`.
     - Rewrite selectively imported `<fn>(...)` in `imported_json_symbols_` to `keet::json::<fn>(...)`.
2. **Update `include/keety/emitter/cpp_emitter.hpp` and `src/emitter/cpp_emitter.cpp`:**
   - Add `bool has_json_import_{false};` and `std::unordered_set<std::string> imported_json_symbols_{};`.
   - Track `json` imports in `CppEmitter::emit_From_ast` (and IR emission path).
   - Map `json.<fn>` and selectively imported `json` symbols to `keet::json::<fn>`.

---

### Phase 5: Unit Tests, Conformance Tests & Documentation

**Objective:** Comprehensively verify the C++20 runtime, compiler diagnostics, AOT compilation, and `zonk` error recovery, and document the `json` module.

#### Tasks
1. **Runtime Unit Tests (`tests/unit/test_runtime_json.cpp`):**
   - Test RFC 8259 parsing edge cases: empty/whitespace strings, trailing tokens, deeply nested structures, Unicode escapes `\uXXXX` and UTF-16 surrogate pairs (`\uD83D\uDE80` $\rightarrow$ `🚀`), number precision (`int64_t` boundaries, scientific notation), escaped control characters.
   - Test all 37 `keet::json::*` functions: validation (`isValid`, `typeOf`, `isObject`, `isArray`, `size`, `keys`), dot-path extraction (`getString`, `getInt`, `getFloat`, `getBool`, `getArray`, `getObject`, `hasKey`, `isNull`, `*Or` helpers), typed array/map parsers (`parseMap`, `parseArray`, `parseIntArray`, `parseFloatArray`, `parseBoolArray`), immutable builders (`emptyObject`, `emptyArray`, `set*`, `removeKey`), polymorphic `stringify` (primitives, nested vectors, string maps, variants), `format`, `minify`, `escape`, `unescape`, `readFile`, and `writeFile`.
   - Verify that all invalid inputs throw `keet::error` with `err.name == "JsonError"`.
2. **Compiler & Sema Unit Tests (`tests/unit/test_universe_symbols.cpp`, `tests/unit/test_type_checker.cpp`):**
   - Verify `isStdlibModule("json")`, `createStdlibModuleScope("json")`, type checking of `json.*` calls, polymorphic `json.stringify`, and negative diagnostics (unimported `json`, unknown `json` member, wrong argument types/arity).
3. **End-to-End Conformance Tests (`tests/conformance/test_conformance_json.cpp`):**
   - Compile and execute full `.keet` programs testing:
     - Qualified import (`import json;`), aliased import (`import json as j;`), and selective import (`from "json" import ...;`).
     - Nested dot-path extraction and array indexing (`"users.0.name"`, `"config.server.port"`).
     - `zonk` inline fallbacks and scoped `zonk (err) { ... }` recovery on malformed JSON, missing keys, and type mismatches.
     - Building complex JSON objects with `emptyObject`, `setString`, `setInt`, `setRaw`, `removeKey`.
     - Polymorphic `json.stringify` on `int`, `float`, `bool`, `string`, `array<int>`, `map<string, int>`, and nested collections.
     - File round-trip with `json.writeFile` and `json.readFile`.
4. **Update `tests/CMakeLists.txt`:**
   - Register `unit/test_runtime_json.cpp` and `conformance/test_conformance_json.cpp`.
5. **Documentation (`documentation/stdlib/json.md` & `README.md`):**
   - Create `documentation/stdlib/json.md` following the exact structure and style of `documentation/stdlib/fs.md` and `documentation/stdlib/time.md`.
   - Update `README.md` stdlib summary table to include `json`.

---

## 5. File Impact Matrix

| File Path | Action | Responsibility |
|---|---|---|
| `runtime/keet/json.hpp` | **Create** | Self-contained C++20 RFC 8259 JSON parser, dot-path query/mutation engine, and polymorphic `stringify` implementation. |
| `runtime/keet/std/json.hpp` | **Create** | Standard library forwarding header (`#include <keet/json.hpp>`). |
| `runtime/keet/runtime.hpp` | **Modify** | Include `<keet/json.hpp>` in canonical runtime umbrella header. |
| `src/resolver/module_path_resolver.cpp` | **Modify** | Add `"json"` to recognized standard library module names. |
| `include/keety/sema/universe.hpp` | **Modify** | Declare `populateJsonModuleScope(Scope& jsonScope)`. |
| `src/sema/universe.cpp` | **Modify** | Update `isStdlibModule`, `createStdlibModuleScope`, and implement `populateJsonModuleScope`. |
| `src/typechecker/type_checker.cpp` | **Modify** | Type-check `import json;`, `from "json" import ...;`, `json.*` calls, and polymorphic `json.stringify`. |
| `include/keety/ir/ast_lowerer.hpp` | **Modify** | Track `has_json_import_` and `imported_json_symbols_`. |
| `src/ir/ast_lowerer.cpp` | **Modify** | Lower `json.*` and selectively imported calls to `keet::json::*`. |
| `include/keety/emitter/cpp_emitter.hpp` | **Modify** | Track `has_json_import_` and `imported_json_symbols_`. |
| `src/emitter/cpp_emitter.cpp` | **Modify** | Emit `keet::json::*` calls in generated C++20 code. |
| `tests/unit/test_runtime_json.cpp` | **Create** | Unit tests for `keet::json` C++20 runtime functions and error cases. |
| `tests/conformance/test_conformance_json.cpp` | **Create** | End-to-end KeetScript compilation and execution conformance tests for `json`. |
| `tests/CMakeLists.txt` | **Modify** | Add `test_runtime_json.cpp` and `test_conformance_json.cpp` to `keety_unit_tests`. |
| `documentation/stdlib/json.md` | **Create** | Complete reference documentation and `zonk` usage examples for the `json` module. |
| `README.md` | **Modify** | List `json` in the Standard Library overview section. |
