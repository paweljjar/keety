# Introduction

KeetScript is an open-source, general-purpose programming language designed to bridge the gap between high-level developer ergonomics and raw system-level performance. 

Modern software development often presents a frustrating compromise: developers are forced to choose between languages that are expressive and easy to write—such as Python or JavaScript—but suffer from runtime overhead and garbage collection pauses, or languages like C++ and Java that deliver uncompromising performance at the cost of verbose syntax, complex memory management, and steep learning curves. 

KeetScript was created to solve this dilemma. Rather than treating high performance and elegant simplicity as mutually exclusive, KeetScript provides a clean, Python- and JavaScript-inspired scripting syntax that compiles ahead-of-time (AOT) directly into clean C++ source and header files. By acting as a transparent, lightweight layer between the developer and the power of C++, KeetScript eliminates boilerplate while delivering native execution speed.

Key objectives and features of KeetScript include:

* **Expressive & Accessible Syntax:** A clean, easy-to-learn syntax that drastically reduces cognitive load without sacrificing low-level control.
* **Ahead-of-Time (AOT) Compilation:** Transpiles efficiently to C++/header files, allowing developers to leverage native compiler optimizations (GCC, Clang, MSVC).
* **Modern Developer Tooling:** Simple, out-of-the-box project initialization and management inspired by modern package ecosystems.
* **General-Purpose Flexibility:** Designed from the ground up to be a versatile language suitable for everything from utility scripts to high-performance application backends.

KeetScript gives developers the speed and power of C++ with the speed and ease of modern scripting languages.

## Companion Specifications

* **Compiler Architecture & Toolchain Design:** [Keety Compiler Architecture Specification.md](file:///Users/pawel/Documents/Keet%20Documentation/Keety%20Compiler%20Architecture%20Specification.md) details the 6-stage AOT compiler pipeline, KeetIR optimization passes, diagnostic reporting standards, and canonical C++17 emission templates.
* **Test Suite & Benchmarks:** [KeetScript Conformance Test Suite and Benchmark Specification.md](file:///Users/pawel/Documents/Keet%20Documentation/KeetScript%20Conformance%20Test%20Suite%20and%20Benchmark%20Specification.md) specifies test harness suites (positive semantics, negative diagnostics `E1000`–`E4000`) and the 4 canonical performance benchmarks against idiomatic C++17.


# Notation

The syntax of KeetScript is specified using Extended Backus-Naur Form (EBNF):

```ebnf
Syntax        = { Production } .
Production    = production_name "=" [ Expression ] "." .
Expression    = Term { "|" Term } .
Term          = Factor { Factor } .
Factor        = production_name | token [ "…" token ] | Group | Option | Repetition .
Group         = "(" Expression ")" .
Option        = "[" Expression "]" .
Repetition    = "{" Expression "}" .
```

Productions are expressions constructed from terms and the following operators, in increasing precedence:

* `|` : alternation
* `( )` : grouping
* `[ ]` : option (0 or 1 times)
* `{ }` : repetition (0 to n times)

Lowercase production names are used to identify lexical (terminal) tokens. Non-terminals are written in `CamelCase`. Lexical terminal tokens are enclosed in double quotes `""` or backticks `` ``.

The form `a … b` represents the set of characters from `a` through `b` as alternatives. Semicolons `;` are used as explicit statement terminators to ensure precise and efficient ahead-of-time (AOT) translation to C++.


# Source code representation

Source code is Unicode text encoded in UTF-8. This permits string literals, documentation, and comments to contain arbitrary Unicode text, while core syntax and identifiers remain strictly bounded.

Implementation restriction: A compiler may disallow the NUL character (U+0000) in the source text.

# Characters

The following terms are used to denote basic character categories:

```ebnf
newline      = /* the Unicode code point U+000A */ .
source_char  = /* an arbitrary Unicode code point except newline */ .
ascii_letter = "a" … "z" | "A" … "Z" .
ascii_digit  = "0" … "9" .
```

# Letters and digits

To ensure predictable identifier naming and clean ahead-of-time (AOT) translation to C++, KeetScript restricts identifier characters to basic ASCII letters, underscores, and digits.

```ebnf
letter        = ascii_letter | "_" .
decimal_digit = "0" … "9" .
binary_digit  = "0" | "1" .
octal_digit   = "0" … "7" .
hex_digit     = "0" … "9" | "A" … "F" | "a" … "f" .
```


# Lexical elements

## Comments

Comments serve as program documentation and are ignored by the compiler. There are two standard forms:

* **Line comments:** Start with the character sequence `//` and continue until the end of the line.
* **Block comments:** Start with the character sequence `/*` and stop with the first subsequent character sequence `*/`.

Comments cannot start inside a string or template literal.

## Tokens

Tokens form the vocabulary of the KeetScript language. There are four primary classes: identifiers, keywords, operators and punctuation, and literals. 

White space—formed from spaces, horizontal tabs, carriage returns, and newlines—is ignored except as it separates tokens that would otherwise combine into a single token (and to disambiguate generic `<TypeArgs>` in expressions, where `<` must immediately follow an identifier without intervening whitespace). While breaking input into tokens, the lexer always consumes the longest sequence of characters that form a valid token (maximal munch), subject to two context-sensitive disambiguation rules:

1. **Blank Identifier (`_`):** Under maximal munch, a standalone underscore character `_` (not immediately followed by a `letter` or `ascii_digit`) is tokenized as the reserved `BlankIdent` token (`_`), whereas an underscore immediately followed by one or more letters or digits (such as `_count` or `__init`) is tokenized as a regular `Identifier`.
2. **Closing Angle Brackets in Type Contexts (`>` vs. `>=` / `>>`):** Because KeetScript omits bitwise shift operators (`>>`) from the core grammar and supports nested generic types initialized with `=` (such as `let matrix: array<array<int>> = [];` or `let a: array<int>= [];`), when parsing inside a `TypeAnnotation`, `TypeArguments`, or `TypeParameters` context, a `>` character closes the innermost open `<` type bracket rather than combining with an adjacent `>` or `=` into a multi-character operator token.

## Semicolons

To ensure precise, predictable, and highly efficient ahead-of-time (AOT) translation to C++, KeetScript strictly requires explicit semicolons (`;`) as statement terminators. 

## Identifiers

Identifiers name program entities such as variables, constants, types, and functions. An identifier is a sequence of one or more letters and digits, where the first character must be a letter or an underscore (`_`), excluding the standalone single underscore `_` which is reserved as the [blank identifier](#blank-identifier) (`BlankIdent`).

```ebnf
Identifier = ascii_letter { letter | ascii_digit } |
             "_" ( letter | ascii_digit ) { letter | ascii_digit } .
BlankIdent = "_" .
```

Examples of valid identifiers:
`x`, `_count`, `total_sum`, `UserValue2`.

## Keywords

The following keywords are reserved and may not be used as identifiers:

### Declarations & Bindings
* `define`, `const`, `let`, `mut`, `fn`, `class`, `inherits`, `inherited`, `this`, `import`, `export`, `extern`, `type`, `pub`, `priv`, `enum`

### Control Flow & Type Inspection
* `if`, `else`, `elif`, `for`, `in`, `return`, `yield`, `while`, `break`, `continue`, `switch`, `case`, `default`, `typeof`, `as`, `from`

### Error Handling & Recovery
* `errtype`, `zonk`

### Built-in Types
* `bool`, `int`, `float`, `char`, `string`, `void`, `error`, `array`, `map`

### Literals & Constants
* `true`, `false`, `null`

### Concurrency
* `await`, `async`

## Operators and punctuation

The following character sequences represent operators, type/function tokens, and punctuation:

```
+    -    *    /    %
++   --   ?    ==   !=
<    <=   >    >=
&&   ||   !    |
=    +=   -=   *=   /=   %=
=>   .    :    ,    ;
(    )    [    ]    {    }
${
```

* `.` is used natively for member and child access across objects and classes (JavaScript-style).
* `:` and `=>` are used for type annotations, map key-value separators, ternary conditionals (`? :`), and function types (e.g., `let cb: (x: int) => int;`).
* `|` is used as the type union operator in type annotations (e.g., `let x: string | null;`).
* `++` and `--` are postfix increment and decrement statement tokens (not value-yielding sub-expressions).
* Low-level bitwise, pointer, and manual memory operators are intentionally omitted from the core syntax and deferred to specialized libraries.

## Integer literals

An integer literal is a sequence of digits representing an integer constant. An optional prefix sets a non-decimal base: `0b` or `0B` for binary, `0o` or `0O` for octal, and `0x` or `0X` for hexadecimal. A single `0` is considered a decimal zero. Leading zeroes on decimal literals (e.g., `0600`) and digit separators (underscores) are not permitted.

```ebnf
IntLit        = DecimalLit | BinaryLit | OctalLit | HexLit .
DecimalLit    = "0" | ( "1" … "9" ) { decimal_digit } .
BinaryLit     = "0" ( "b" | "B" ) binary_digit { binary_digit } .
OctalLit      = "0" ( "o" | "O" ) octal_digit { octal_digit } .
HexLit        = "0" ( "x" | "X" ) hex_digit { hex_digit } .
```

Examples of valid integer literals:
`0`, `42`, `0b1010`, `0o600`, `0xBadFace`.

## Floating-point literals

A floating-point literal is a decimal representation of a floating-point constant. It consists of an integer part, a decimal point, a fractional part, and an optional exponent part (`e` or `E` followed by an optional sign and decimal digits). Either the integer part or the fractional part may be elided, and either the decimal point or the exponent part may be elided.

```ebnf
FloatLit        = DecimalDigits "." [ DecimalDigits ] [ DecimalExponent ] |
                  DecimalDigits DecimalExponent |
                  "." DecimalDigits [ DecimalExponent ] .
DecimalDigits   = decimal_digit { decimal_digit } .
DecimalExponent = ( "e" | "E" ) [ "+" | "-" ] DecimalDigits .
```

Examples of valid floating-point literals:
`0.`, `3.14159`, `.5`, `1e-5`, `2.5E+10`.

*(Note: Imaginary literals and complex numbers are omitted from the core lexical grammar and provided via standard math libraries.)*

## String and character literals

KeetScript unifies the lexical representation of text literals using three delimiter styles: single quotes (`'...'`), double quotes (`"..."`), and backticks (`` `...` ``).

Whether a quoted literal represents a `string` or a single `char` is determined semantically by its target type annotation and length rather than the quote style:
* Both `'...'` and `"..."` are valid for `string` bindings of any length, and valid for `char` bindings if and only if the literal evaluates to a single character (1 byte/code unit). Multi-character literals assigned to a `char` type result in a compile-time error.
* Single-quoted and double-quoted literals are single-line and cannot contain unescaped newlines.
* Backtick literals (`` `...` ``) represent multi-line template strings and support JavaScript-style expression interpolation via `${...}`. Inside a `TemplateLit`, a standalone `$` character not immediately followed by `{` is treated as a normal `TemplateChar`.

```ebnf
StringLit             = SingleQuotedLit | DoubleQuotedLit | TemplateLit .
SingleQuotedLit       = "'" { SingleQuotedChar | EscapeSeq } "'" .
DoubleQuotedLit       = '"' { DoubleQuotedChar | EscapeSeq } '"' .
TemplateLit           = "`" { TemplateChar | EscapeSeq | Interpolation } "`" .

Interpolation         = "${" Expression "}" .
SingleQuotedChar      = /* any source_char except "'" and "\" */ .
DoubleQuotedChar      = /* any source_char except '"' and "\" */ .
TemplateChar          = /* any source_char or newline except "`", "\", and "${" */ .
EscapeSeq             = "\" ( "n" | "r" | "t" | "0" | "\" | "'" | '"' | "`" | "$" | ( "x" hex_digit hex_digit ) ) .
```

Examples:
```keet
let a: string = 'Hello, World!'; // valid string
let b: string = "Hello, World!"; // valid string
let c: char = "H";               // valid char (single character in double quotes)
let d: char = 'H';               // valid char (single character in single quotes)
let e: char = "Hello, World!";   // compile-time error: cannot assign multi-character literal to char
let f: char = 'Hello, World!';   // compile-time error: cannot assign multi-character literal to char

let greeting: string = `Hello, ${a}
Welcome to KeetScript!`;         // valid multi-line interpolated template literal
```


# Constants

KeetScript distinguishes between two tiers of non-reassignable bindings—compile-time constants (`define`) and immutable runtime constants (`const`)—alongside fixed literal values.

## Binding Modifiers and Mutability Matrix (`define`, `const`, `let`, `mut`)

To avoid confusion with dynamically typed scripting languages (where `const` often implies only shallow immutability of the reference), KeetScript strictly enforces **deep immutability** and deterministic value semantics across its declaration modifiers:

| Modifier | Syntactic Role | Lifetime & Storage | Reassignable (`=`) | Internal Mutation (`.push()`, `.field =`) | C++17 Lowering |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `define` | Declaration keyword | Compile-time constant (no runtime l-value address) | No | No (deeply immutable) | `constexpr T` |
| `const` | Declaration keyword | Runtime immutable binding (stack or module unit) | No | No (deeply immutable) | `const T` |
| `let` | Declaration keyword | Runtime mutable storage (stack or module unit) | Yes | Yes (if receiver is mutable) | `T` |
| `mut` | Parameter / capture modifier | Caller's mutable storage location (borrowed l-value) | Yes (mutates caller's storage) | Yes (mutates caller's storage) | `T&` |

* **Deep Immutability vs. Shallow Reference Immutability:** In languages like JavaScript or Python, declaring a collection with `const` prevents reassigning the variable but still allows modifying its contents (e.g., `arr.push(...)`). In KeetScript, **`const` enforces deep immutability**: calling a mutating method (`fn`, such as `arr.push(...)`, `arr.clear()`, `map.remove(...)`) or assigning to a field of a `const` composite value is a **compile-time error**.
* **`mut` Is Not a Declaration Keyword:** `mut` cannot be used to declare standalone local or global variables (e.g., `mut x: int = 5;` is a **compile-time error**). `mut` is strictly an explicit modifier for synchronous function/method parameters (`mut param: T`) and closure capture lists (`[mut x]`), denoting pass-by-mutable-reference.

## Strict Typing and Initialization

Unlike languages with untyped constants or implicit type inference, every constant binding in KeetScript **must** be explicitly typed at declaration. A constant's type is strictly fixed from the moment of initialization. 

Constant bindings may also utilize TypeScript-style **union types** and **literal types** (e.g., `int | 'y'` or `string | null`), provided the initial value satisfies the declared type constraint.

## Compile-Time Constants (`define`)

Declarations using the `define` keyword represent strict compile-time constants, mapping directly to C++ `constexpr`. Their values must be determinable at compile time (such as numeric literals, boolean literals, characters, or compile-time constant expressions). *(See [Constant declarations](#constant-declarations) for the formal `DefineDecl` and `ConstDecl` grammar.)*

## Immutable Runtime Constants (`const`)

Declarations using the `const` keyword represent read-only bindings initialized at runtime or compile time, mapping to C++ `const`. Once initialized, a `const` binding cannot be reassigned. Unlike `define`, a `const` binding may hold primitive values, strings, functions, or composite values (such as arrays and class instances).

Examples of constant declarations:

```keet
define pi: float = 3.14;                          // Compile-time constant (maps to C++ constexpr)
const foo: string = "lorem ipsum";                // Immutable binding (maps to C++ const)
let howManyApplesADayKeepADoctorAway: int = 1;    // Mutable variable (for comparison)
```

## Built-in Constant Values (`true`, `false`, `null`)

KeetScript predeclares three fundamental constant values:
* **Boolean constants:** `true` and `false`.
* **Nullability & Uninitialized constant:** `null` represents the absence of an object or value, as well as the default state of an uninitialized variable (commonly used in union types such as `string | null`).

## Constant Expressions

Constant expressions may combine literals and other constants using standard arithmetic, comparison, and logical operators (e.g., `define bufferSize: int = 1024 * 64;`). 

To keep the KeetScript ahead-of-time (AOT) transpilation step fast and lightweight, the KeetScript compiler (`keety`) validates types and syntax but passes constant expressions directly through to the generated C++ source code, relying on the underlying C++ compiler (GCC, Clang, MSVC) to perform constant folding and optimization.


# Variables

Variables introduce mutable storage bindings in KeetScript using the `let` keyword. Unlike constants (`define` and `const`), variables can be reassigned multiple times throughout their lifetime, provided the assigned values conform to their declared types.

## Declaration, Initialization, and Uninitialized State

Every variable must be explicitly typed at declaration. A variable may be declared with or without an initial expression:

* **Initialized Declaration:** Binds an initial value to the variable.
* **Default Collection Initialization:** Variables of type `array<T>` or `map<K, V>` declared without an `= ...` initializer automatically default-initialize to an empty collection at runtime without requiring `| null` in their type annotation.
* **Uninitialized Declaration (`| null`):** Any other variable may be declared without an initial value *only if* its declared type explicitly includes the `null` type (e.g., `let x: string | null;`). If a non-collection variable does not allow `null`, omitting an initialization value results in a compile-time error.
* **Default `null` Value:** Any nullable variable declared without an initial value automatically holds the `null` value at runtime until explicitly assigned. Reading an uninitialized variable that holds `null` is legally permitted. *(See [Variable declarations](#variable-declarations) for the formal `VarDecl` grammar.)*

Examples of variable declarations:
```keet
let count: int = 0;                     // Initialized variable
let items: array<int>;                  // Default-initialized to an empty array
let activeUser: string | null;          // Uninitialized variable (automatically holds `null`)
let score: int = 10, maxScore: int = 100; // Comma-separated declarations
```

## Union Types and C++ Translation (`std::variant`)

KeetScript supports TypeScript-style union types, allowing a variable to hold values of multiple distinct types over its lifecycle (e.g., `let x: float | int | string | char | null;`).

To ensure high-performance execution when transpiling to C++, the `keety` compiler translates union-typed variables into C++17 `std::variant`. For example, a union of `float | int | string` maps cleanly to:
```cpp
std::variant<double, int64_t, std::string> x;
```
This guarantees zero heap allocation overhead for primitive union types while retaining complete type safety.

## Type Inspection via `typeof`

KeetScript provides a built-in `typeof` operator to inspect and check the active type of a variable or expression at runtime before performing type-specific operations or method calls.

Evaluating `typeof(x)` returns a runtime `string` corresponding to the active value's type:
* **Primitive, String, and Special Types:** `"bool"`, `"int"`, `"float"`, `"char"`, `"string"`, or `"null"`.
* **Collection and Callable Types:** `"array"`, `"map"`, or `"function"`.
* **Nominal User-Defined Types (`class` and `enum`):** The exact declared identifier of the `class` or `enum` (e.g., `"Node"` or `"Status"`).

```keet
let val: int | string = 42;

if (typeof(val) == "int") {
    io.println("It's an integer");
} else {
    io.println("It's a string");
}
```

## Scoping and Shadowing Rules

* **Comma-Separated Declarations:** Multiple variables of the same or different types may be declared in a single `let` statement separated by commas (e.g., `let a: int = 1, b: string = "hello";`).
* **Strict No-Shadowing Rule:** To prevent bugs and maintain clean readability during AOT translation to C++, local `let` variables **cannot** shadow a variable of the same name declared in an outer scope. Declaring a variable whose identifier already exists in an enclosing scope results in a compile-time error.


# Types

A type determines a set of values together with operations and methods specific to those values. Every variable, constant, parameter, and function return in KeetScript has an explicit static type. A type may be denoted by a predeclared or user-defined *type name* (optionally instantiated with generic `TypeArguments`), a *composite or generic type* (`array<T>`, `map<K, V>`), a *function type*, a *literal type*, or a *union type* combining multiple types with `|`.

## Explicit Typing, Zero Implicit Inference, and Coercion Policy

To guarantee maximum clarity, prevent subtle cross-platform widening bugs, and ensure seamless AOT compilation to C++, KeetScript enforces an absolute explicit typing model:

1. **Mandatory Explicit Annotations:** Every variable (`let`), constant (`const`, `define`), function/method parameter, return type, and class field declaration **must** specify an explicit `: TypeAnnotation`. Implicit type inference for bindings (such as `let x = 10;` or C++ `auto`) is disallowed and results in a **compile-time error**.
2. **Contextual Typing Restriction:** The only expressions that participate in type inference are empty collection literals (`[]` and `{}`). Because an empty literal lacks an inherent element type, it derives its concrete type contextually from the explicit type annotation of the binding or parameter to which it is assigned.
3. **Strict Zero-Coercion Policy:** KeetScript forbids all forms of implicit type coercion:
   * **No Implicit Numeric Widening or Narrowing:** An `int` value cannot be implicitly passed or assigned to a `float`, nor can a `float` be implicitly assigned to an `int`. Explicit conversions (`float(n)` / `n.toFloat()`, `int(f)` / `f.toInt()`) are strictly required.
   * **No Implicit Boolean Coercions ("Truthy" / "Falsy"):** Control-flow conditions (`if`, `while`, ternary `? :`, logical `!`, `&&`, `||`) must evaluate strictly to the boolean type `bool`. Evaluating integers, pointers, strings, or collections as boolean conditions (e.g., `if (count)` or `if (str)`) is a **compile-time error**.

```ebnf
TypeAnnotation = UnionType .
UnionType      = PrimaryType { "|" PrimaryType } .
PrimaryType    = TypeName | ArrayType | MapType | FunctionType | LiteralType | "(" TypeAnnotation ")" .
TypeName       = "bool" | "int" | "float" | "char" | "string" | "void" | "null" | "error" | Identifier [ TypeArguments ] .
TypeArguments  = "<" TypeArgList ">" .
TypeArgList    = TypeAnnotation { "," TypeAnnotation } .
LiteralType    = [ "+" | "-" ] ( IntLit | FloatLit ) | StringLit | "true" | "false" .
```

## Boolean types

A *boolean type* represents the set of Boolean truth values denoted by the predeclared constants `true` and `false`. The predeclared boolean type is `bool` (mapping to C++ `bool`).

## Numeric types

To keep the core language simple and predictable while avoiding architecture-dependent sizing pitfalls, KeetScript provides two built-in 64-bit numeric types:

```
int      the set of all signed 64-bit integers (-9223372036854775808 to 9223372036854775807), mapping to C++ int64_t
float    the set of all IEEE 754 64-bit floating-point numbers, mapping to C++ double
```

*(Note: Smaller fixed-width integer and floating-point types are omitted from the core grammar and deferred to specialized standard libraries.)*

All numeric types are distinct. **Explicit type casting** is required when mixing `int` and `float` in expressions or assignments; implicit numeric widening or narrowing is not permitted.

* **Zero Undefined Behavior (Zero-UB) Numeric Guarantees:**
  * **Signed Integer Overflow (Two's-Complement Wrap-Around):** Unlike C and C++, where signed integer overflow is undefined behavior, 64-bit `int` arithmetic in KeetScript (`+`, `-`, `*`, unary `-`, `++`, `--`) is strictly defined to use **two's-complement wrap-around semantics** modulo $2^{64}$ (matching Go and Java). In generated C++, `keety` enforces wrap-around via unsigned arithmetic casting or `-fwrapv`.
  * **Division and Remainder by Zero:** Integer division or remainder by zero (`a / 0` or `a % 0`) and signed minimum division overflow (`INT64_MIN / -1`) are strictly defined: `a / 0` and `a % 0` deterministically raise a runtime error recoverable via `zonk`, and `INT64_MIN / -1` wraps to `INT64_MIN` (with `INT64_MIN % -1` yielding `0`).
  * **Float-to-Int Conversion Bounds:** Converting a `float` to `int` (`int(f)`, `f.toInt()`, `f.round()`, `f.floor()`, `f.ceil()`) when `f` is `NaN`, `±Infinity`, or outside the representable 64-bit signed integer range (`[-9223372036854775808.0, 9223372036854775807.0]`) deterministically raises a runtime error recoverable via `zonk`, completely eliminating C++ `static_cast<int64_t>` undefined behavior.

## Character and string types

* **`char` (UTF-8 Code Unit):** Represents a single 8-bit byte/code unit (`0` through `255`), mapping directly to C++ `char` (and `uint8_t` in numeric conversions). A `char` holds exactly one byte of text; multi-byte Unicode characters cannot be stored in a single `char` and must be represented as a `string`.
* **`string` (UTF-8 Byte Sequence):** Represents a sequence of valid UTF-8 code units, mapping directly to C++ `std::string`. Unlike languages where strings are strictly immutable, a `string` in KeetScript is **mutable** when bound via `let`, and **immutable** when bound via `const` or `define`.
* **Byte-Level Operations vs. Unicode Code Points:**
  * **Built-in Byte Operations:** The length of a string `s.length()`, 0-based indexing `s[i]` (returning `char`), and byte slicing `s.slice(start, end)` operate strictly on **raw UTF-8 code units (bytes)**, matching C++ `std::string::size()` and `operator[]`. This guarantees deterministic $O(1)$ random access and eliminates hidden linear-time Unicode scanning overhead in core operations.
  * **Unicode Scalar Values & Grapheme Clusters:** Iterating or indexing by full Unicode code points (runes) or user-perceived grapheme clusters is provided via the standard library module `unicode` (or string helper methods `.codePoints(): array<int>`, `.runes(): array<string>`).
* **UTF-8 Validation & Encoding Guarantees:**
  * KeetScript source code and string literals are strictly validated as compliant UTF-8 at compile time.
  * Ingesting external data from native C++ FFI, files, or network streams assumes valid UTF-8. Functions performing decoding checks deterministically raise a recoverable `UnicodeDecodeError` via `zonk` if invalid byte sequences are encountered.

## Special types (`void`, `null`)

* **`void`:** Used exclusively as a function return type to indicate that the function does not return a value.
* **`null`:** Represents the absence of a value or object, as well as an uninitialized state. It is required in the type annotation of any non-collection `let` variable declared without an initial value (e.g., `string | null`).

## Array types

An array is a dynamically sized, numbered sequence of elements of a single type, called the element type, mapping to C++ `std::vector<T>`. Fixed-size arrays are omitted from the core syntax and provided via libraries.

* **`array<bool>` Specialization Guarantee:** To avoid the broken proxy-reference semantics and non-contiguous bit-packing pitfalls of standard C++ `std::vector<bool>`, `array<bool>` in KeetScript transpiles to a byte-backed contiguous container (`keet::array<bool>` backed by a vector of byte-sized booleans, such as `std::vector<char>` or a wrapper struct `struct BoolByte { bool val; }` providing genuine `bool&` element l-value references). This strictly complies with C++ strict-aliasing rules while ensuring that `array<bool>` behaves identically to all other `array<T>` types, including when indexing, mutating in place, or passing elements to `mut bool` parameters.

```ebnf
ArrayType   = "array" "<" ElementType ">" .
ElementType = TypeAnnotation .
```

Examples of array types:
```keet
let numbers: array<int> = [1, 2, 3];
let matrix: array<array<float>> = [[1.0, 2.0], [3.0, 4.0]];
let mixed: array<int | string> = [1, "two", 3];
```

## Map types

A map is an unordered collection of key-value pairs where all keys are unique and of a single `KeyType`, and all values are of a single `ValueType`, mapping to C++ `std::unordered_map<K, V>`. In type annotations, the key type and value type are separated by a comma (`,`).

`KeyType` must be a deterministically hashable and equality-comparable type: predeclared discrete primitive types (`bool`, `int`, `char`), `string`, `enum` types, discrete literal types, or union types composed exclusively of these types (matching C++ `std::unordered_map` requirements for `std::hash` and `operator==`).

* **`float` Disallowed as `KeyType`:** `float` (and any union containing `float`) is **not** permitted as a map `KeyType`. Under IEEE 754 floating-point semantics, `NaN != NaN` violates the reflexive equivalence relation (`k == k`) required by hash tables, and signed zeroes (`-0.0 == +0.0`) can produce inconsistent hash buckets in `std::unordered_map`.

```ebnf
MapType   = "map" "<" KeyType "," ValueType ">" .
KeyType   = TypeAnnotation .
ValueType = TypeAnnotation .
```

Examples of map types:
```keet
let ages: map<string, int>;
let lookup: map<int, array<string>>;
```

## Class types

A `class` defines an object-oriented composite type encapsulating state (fields), behavior (methods), and a constructor, transpiling to a C++ `class`.

* **Default Visibility:** Class members are **public (`pub`) by default** unless explicitly marked private (`priv`).
* **Inheritance (Implementation Reuse Without Object Slicing):** Single inheritance is supported using the `inherits` keyword in the class header. Methods that override or derive from a parent class must be prefixed with the `inherited` keyword, followed by an optional `pub` or `priv` visibility modifier (defaulting to `pub` if omitted). Fields from the parent class are inherited directly into the derived class's memory layout and cannot be redeclared. Inside derived class methods, overridden parent methods can be statically dispatched via `inherited.method(...)`.
  * In KeetScript, `inherits` exists strictly for **implementation reuse and code sharing** across concrete value-type classes, **not** for pointer/reference-based dynamic subtype polymorphism (`virtual` vtable dispatch).
  * Every class strictly retains stack/value semantics. An instance of a derived class (`class Child inherits Parent`) is **not** implicitly assignable to `Parent`, preventing C++ object slicing by design. Dynamic heterogeneous polymorphism across distinct class types is expressed explicitly using **union types** (e.g., `type Shape = Circle | Rectangle;`), which compile to zero-heap-indirection `std::variant<Circle, Rectangle>` values inspected via `typeof(s)`.
* **Receiver Mutability (`const fn` vs. `fn` Methods):**
  * Methods declared with `const fn` have an immutable receiver (`const this`, transpiling to C++ `const`-qualified member functions, e.g., `int64_t getCount() const`) and cannot mutate instance fields (`this.field`) or call non-`const` (`fn`) methods on `this`.
  * Methods declared with `fn` (without `const`) have a mutable receiver and may mutate instance fields, but cannot be called on `const` class instances.
* **Constructors and `const` Fields:** A constructor is declared as a block bearing the exact name of the class (`ClassName(...) [ : inherited(...) ] { ... }`) with no return type annotation.
  * If the class derives from a parent class via `inherits`, its constructor may (and must, if the parent class lacks a parameterless default constructor) explicitly invoke the parent constructor using `: inherited(args...)` before the body block `{ ... }`.
  * Instance fields declared with `const` may be initialized either inline at their field declaration or inside the class constructor body, after which they cannot be mutated (`this.constField = ...` inside a regular method is a compile-time error).
  *(C++ AOT note: In standard C++, a non-static `const` data member implicitly deletes the class's copy-assignment operator `operator=`. To preserve KeetScript's value-copy assignment semantics (`obj1 = obj2`) on mutable `let` variables holding class instances while enforcing `const` field immutability against direct member mutation, `keety` emits non-`const` underlying storage fields in the generated C++ class and enforces `const` field read-only guarantees strictly at compile time.)*
* **Class Value Equality (`const fn equals`):** By default, `class` instances cannot be compared with `==` or `!=`. A class may opt into equality comparison by declaring a public method `pub const fn equals(other: ClassName): bool`, which `keety` lowers to C++ `bool operator==(const ClassName& other) const` (and synthesizes `operator!=` as `!equals(other)`).

```ebnf
ClassDecl       = "class" Identifier [ TypeParameters ] [ "inherits" Identifier [ TypeArguments ] ] "{" { ClassMember } "}" .
ClassMember     = ConstructorDecl | MemberDecl .
ConstructorDecl = Identifier Parameters [ ":" "inherited" "(" [ ArgumentList ] ")" ] Block .
MemberDecl      = MethodDecl | [ "pub" | "priv" ] FieldDecl .
MethodDecl      = [ "inherited" ] [ "pub" | "priv" ] [ "async" ] [ "const" ] "fn"
                  Identifier [ TypeParameters ] Parameters ":" TypeAnnotation Block .
FieldDecl       = ( "let" | "const" | "define" ) Identifier ":" TypeAnnotation [ "=" Expression ] ";" .
```

Example of a class hierarchy with inheritance, base constructor invocation, and `inherited` method dispatch:
```keet
class Animal {
    pub let name: string;

    Animal(name: string) {
        this.name = name;
    }

    pub fn speak(): string {
        return "...";
    }
}

class Dog inherits Animal {
    pub let breed: string;

    // Derived constructor calling base constructor via `: inherited(...)`
    Dog(name: string, breed: string) : inherited(name) {
        this.breed = breed;
    }

    // Overriding method with `inherited` prefix and dispatching to base implementation
    inherited pub fn speak(): string {
        return `${inherited.speak()} Woof!`;
    }
}
```

## Enum types

An `enum` defines a distinct named type consisting of a set of named integer constants, mapping to a C++ scoped enumeration (`enum class`). Each enumerator may be assigned an explicit integer value; if omitted, the first enumerator receives the value `0`, and any subsequent enumerator without an explicit value receives the value of the preceding enumerator plus `1`.

* **Enumerator Access:** Scoped enumerators are accessed via member selector notation on the enum type name (`EnumName.Member`, e.g., `Status.OK`).
* **Conversion to `int`:** An enumerator is converted to its underlying 64-bit signed `int` value using `int(EnumName.Member)` or `EnumName.Member.toInt()`.
* **Checked Conversion from `int` (`EnumName(intValue)`):** An integer value can be converted to an enum value using constructor syntax `EnumName(intValue)`. At runtime, the conversion verifies that `intValue` matches the integer value of one of the declared enumerators of `EnumName`. If a match is found, the corresponding enum value is returned; if `intValue` does not match any declared enumerator, a runtime error is deterministically raised (recoverable via `zonk`).

```ebnf
EnumDecl   = "enum" Identifier "{" { EnumMember } "}" .
EnumMember = Identifier [ "=" IntLit ] [ "," | ";" ] .
```

Example of an enum declaration and conversions:
```keet
enum Status {
    OK = 200,
    NotFound = 404,
    InternalError = 500,
}

let code: int = int(Status.OK);                         // 200 (or Status.OK.toInt())
let okStatus: Status = Status(200);                     // Status.OK
let fallback: Status = Status(999) zonk Status.InternalError; // invalid code safely recovers via zonk
```

## Function types

A function type denotes the set of all functions and callbacks sharing the same parameter types (including any `mut` pass-by-reference modifiers), return type, and synchronous/asynchronous (`async`) execution mode. In type annotations, function types are written using `=>` notation (`[ "async" ] (param: Type) => ReturnType`), whereas function declarations and inline anonymous callbacks use the `fn` keyword (`fn (x: int): int { ... }`).

```ebnf
FunctionType  = [ "async" ] Parameters "=>" TypeAnnotation .
Parameters    = "(" [ ParameterList ] ")" .
ParameterList = ParameterDecl { "," ParameterDecl } .
ParameterDecl = [ "mut" ] ( Identifier | BlankIdent ) ":" TypeAnnotation [ "=" Expression ] .
```

Examples of function types:
```keet
let validator: (x: int) => bool;
let logger: (msg: string) => void;
let inPlaceTransform: (mut buf: array<int>) => void;
let comparator: (a: float, b: float) => int;
let asyncFetcher: async (url: string) => string;
```

## Union and literal types

* **Union Types (`|`):** Combine multiple types into a single type constraint (e.g., `int | string | null`), allowing a binding to accept any value belonging to one of the constituent types.
  * **Formal Union Algebra & Canonical Normalization:** Union types are structurally typed, associative, commutative, and idempotent:
    * **Associativity & Flattening:** Nested unions are automatically flattened: `A | (B | C)` is identical to `A | B | C`.
    * **Commutativity & Deduplication:** The order of constituent types does not affect type identity: `int | string` is identical to `string | int`. Duplicate types are automatically deduplicated (`int | int | string` normalizes to `int | string`).
    * **Subtype & Literal Absorption:** If a union contains both a base primitive type and one or more literal types of that same base type, the literal types are absorbed into the base type: `200 | 404 | int` normalizes canonically to `int`.
    * **Nullable Representation:** The nullable union `T | null` is the standard representation for optional or uninitialized values across the language.
  * **Flow-Based Type Narrowing & Strict Invalidation Rules:**
    * In conditional branches guarded by type inspection (`if (typeof(x) == "T")`) or non-null checks (`if (x != null)`), the compiler statically narrows the type of `x` to `T` throughout that branch's lexical scope.
    * **Narrowing Invalidation:** Type narrowing is ephemeral and strictly conditional on the stability of `x`. Any mutation of `x`—including direct reassignment (`x = expr;`), compound assignment (`x += expr;`), passing `x` as an argument to a `mut` parameter (`f(mut x)`), or mutating `x` via a captured `[mut x]` closure—**immediately invalidates** the narrowed type and resets `x` to its full declared union type for all subsequent statements in that scope.
* **Literal Types:** Any literal of a basic datatype (`int`, `float`, `char`, `string`, `bool`, including signed numeric literals such as `-1` or `+3.14`) may be used directly in a type annotation to restrict a binding to that exact value or set of values. Literal types are strictly a compile-time `keety` type-checker constraint that erases to the underlying base type in the generated C++ code (for example, `200 | 404` compiles to a plain `int64_t`, and `200 | "ok"` compiles to `std::variant<int64_t, std::string>`).
  * **Quote Delimiter Rule for Character vs. String Literal Types:** When text literals appear as literal types without an explicit surrounding base-type annotation:
    * A 1-character literal enclosed in **single quotes** (e.g., `'y'`, `'n'`) has underlying base type **`char`** (transpiling to C++ `char`).
    * A literal enclosed in **double quotes** (e.g., `"y"`, `"ok"`) has underlying base type **`string`** (transpiling to C++ `std::string`).
    * Multi-character single-quoted literals (e.g., `'hello'`) remain `string` literals as defined in [String and character literals](#string-and-character-literals).

Examples of union and literal types:
```keet
let confirmation: 'y' | 'n' = 'y';
let httpStatus: 200 | 404 | 500 = 200;
let direction: -1 | 0 | 1 = 0;
let mode: "read" | "write" | bool = false;
```

## Type aliases (`type`)

A type alias declaration binds an identifier (optionally parameterized with generic `TypeParameters`) to an existing type, union, or composite type (TypeScript-style). A type alias does not create a distinct runtime type; it is completely interchangeable with the type it aliases. *(See [Type declarations](#type-declarations) for the formal `TypeAliasDecl` grammar.)*

Examples:
```keet
type UserID = int | string;
type Handler = (req: string) => bool;
type StringMatrix = array<array<string>>;
type Nullable<T> = T | null;
```

## Error types (`errtype`)

KeetScript includes a dedicated `errtype` construct for defining typed errors that integrate with the `zonk` error-handling and recovery mechanism. An `errtype` can be referenced in type annotations and function signatures to represent recoverable failure states. *(Full semantics and grammar for `errtype` and `zonk` are detailed in the Error Handling section.)*


# Properties of types and values

## Representation of values

All values in KeetScript—including predeclared primitive types (`bool`, `int`, `float`, `char`), strings (`string`), collections (`array<T>`, `map<K, V>`), `enum` values, and `class` instances—are self-contained and adhere strictly to **value semantics** (deep copy on assignment).

Each value contains a complete, independent copy of its data, and variables store the entire value directly (mirroring C++ stack and value-type semantics such as `int64_t`, `double`, `std::string`, `std::vector<T>`, `std::unordered_map<K, V>`, and stack-allocated C++ classes):

* Assigning one variable to another (`let b: array<int> = a;`) or passing a value to a standard (non-`mut`) function parameter creates an independent copy of the underlying data. Mutating `b` (or mutating a non-`mut` function parameter) never mutates `a` (or the caller's variable).
  * **Explicit Mutable Reference Parameters (`mut`):** When in-place mutation of a caller's variable is explicitly desired (such as `fn swap<T>(mut a: T, mut b: T): void`), a parameter may be declared with the `mut` modifier (`mut param: T`), which passes a mutable reference (`T&` in C++) to a caller's mutable `let` l-value.
  * *(Implementation note: For standard non-`mut` parameters, while preserving strict value-semantics guarantees at the language level, the `keety` compiler transparently emits `const T&` in the generated C++ code whenever the parameter is not mutated within the callable body to avoid unnecessary deep copies.)*
* Class instances are copied by value on assignment rather than sharing a heap reference.
* **Recursive / Self-Referential Class Fields:** To support recursive data structures (such as `class Node { let next: Node | null; }`) while preserving value semantics and avoiding incomplete-type infinite sizing in C++ `std::variant`, the `keety` compiler automatically wraps self-referential class fields or recursive variant branches in a value-copying smart pointer under the hood in generated C++.
* Function values (`(x: int) => bool`) represent callable entities (mapping to C++ `std::function` or function pointers) that are likewise copied by value when assigned.
* Union values (`T1 | T2`) store the active value directly inside a contiguous `std::variant` container without implicit heap indirection (except for recursive class branches as noted above).
* The value `null` is never an implicit default for any type unless `null` is explicitly included in a union type annotation (`T | null`).
* **Last-Use Move Optimization (`std::move`):** While KeetScript guarantees deterministic deep-copy value semantics at the language level, the `keety` compiler performs static liveness analysis on local variables to eliminate redundant C++ deep copies automatically. In generated C++17 code, `keety` emits `std::move(...)` rather than copying when:
  1. An argument passed to a value parameter or container method (such as `arr.push(expr)`) is an rvalue or temporary expression.
  2. A local variable (that is not captured by reference or aliased) is passed by value to a function, assigned, or appended via `.push(localVar)` at its **final statically reachable evaluation** (last-use) before exiting its lexical scope.
  3. A `return localVar;` statement returns a local variable from a function or method (leveraging C++17 copy elision / NRVO and automatic move semantics).

## Underlying types

Each type `T` has an *underlying type*:

1. If `T` is one of the predeclared types (`bool`, `int`, `float`, `char`, `string`, `void`, `null`), a `class`, an `enum`, a composite type (`array<T>`, `map<K, V>`), a function type, or a union type, its underlying type is `T` itself (with any type aliases recursively resolved).
2. If `T` is a **type alias** declared with `type`, its underlying type is the underlying type of the type expression to which `T` refers.
3. If `T` is a **literal type** (e.g., `200`, `-1`, `'y'`, `"ok"`, `true`), its compile-time value set is restricted to that literal, while its underlying runtime C++ type is the corresponding base primitive type (`int`, `float`, `char`, `string`, or `bool`). For single-character text literal types, single-quoted `'c'` has underlying base type `char`, while double-quoted `"c"` (and multi-character text literals) has underlying base type `string`.

```keet
type ID = int;
type UserID = ID;
type IntList = array<ID>;
```

In the example above, the underlying type of `int`, `ID`, and `UserID` is `int`. The underlying type of `IntList` is `array<int>`.

## Type identity

Two types are either *identical* ("the same") or *different*. Composite types and unions are structurally typed, whereas `class` and `enum` types are **nominally typed** (matching C++ directly):

* **Type Aliases:** A type alias (`type A = B;`) is always identical to the type it aliases.
* **Basic Types:** Two basic types are identical if they are the same predeclared type (`bool`, `int`, `float`, `char`, `string`, `void`, `null`).
* **Array Types:** Two array types `array<T1>` and `array<T2>` are identical if their element types `T1` and `T2` are identical.
* **Map Types:** Two map types `map<K1, V1>` and `map<K2, V2>` are identical if `K1` is identical to `K2` and `V1` is identical to `V2`.
* **Function Types:** Two function types are identical if they have the same synchronous/asynchronous (`async`) qualifier, the same number of parameters, corresponding parameter types and `mut` modifiers are identical, and their return types are identical. Parameter names do not affect type identity (`(x: int) => bool` is identical to `(y: int) => bool`, whereas `(mut x: int) => void` is distinct from `(x: int) => void`).
* **Class Types:** Two `class` types are identical if and only if they refer to the exact same `class` declaration (and, for generic classes, are instantiated with identical `TypeArguments`). Two separately declared classes (`class Dog` and `class Cat`) are distinct types even if they have identical fields and methods.
* **Enum Types:** Two `enum` types are identical if and only if they refer to the exact same `enum` declaration.
* **Union Types:** Union types are **order-independent and deduplicated**. Two union types are identical if they contain the exact same set of constituent types after flattening nested unions and resolving aliases. For example, `int | string`, `string | int`, and `int | int | string` are all identical types.

## Assignability

A value `x` of type `V` is *assignable* to a variable or parameter of type `T` ("`x` is assignable to `T`") if one of the following conditions applies:

* **Identical Types:** `V` and `T` are identical (including resolved `type` aliases).
* **Literal to Base Type:** `V` is a literal or literal type (e.g., `200` or `'a'`) whose base type is identical to `T` (e.g., `int` or `char`), or `x` is a constant literal matchable against a literal type `T`.
* **Union Widening:** `T` is a union type (`T1 | T2 | ... | Tn`), and `V` is assignable to at least one constituent type `Ti` of `T`, or `V` is itself a union type whose every constituent member is assignable to `T` (e.g., a value of type `int` or `int | string` is assignable to `int | string | null`).
* **Flow-Based Union Narrowing:** `V` is a union type (e.g., `int | string`), and the assignment to `T` (e.g., `int`) occurs inside a conditional block (such as `if (typeof(x) == "int") { ... }`) where the `keety` compiler can statically prove that `x` currently holds a value of type `T`. Flow-based type narrowing of a mutable `let` union variable inside a conditional block is immediately invalidated if that variable is reassigned within the block.
* **Explicit `null` Assignability:** `x` is the predeclared constant `null` and `T` is either `null` or a union type that explicitly includes `null` (`... | null`). `null` is **never** assignable to a non-nullable `class`, `array<T>`, `map<K, V>`, or `string`.

**Non-Assignability of Inherited Classes:**
Because classes are nominally typed and have strict value semantics (preventing C++ object slicing), an instance of a derived class (`class Child inherits Parent`) is **not** assignable to a variable of type `Parent` unless explicitly combined in a union (`Parent | Child`).

## Representability

A literal or constant expression `x` is *representable* by a value of type `T` if one of the following conditions applies:

* **Exact Kind and Range Match:** `x` belongs to the set of values determined by `T`, and its syntax matches the required literal category of `T`:
  * **`int`:** `x` must be an integer literal (`IntLit`) whose value falls within the signed 64-bit integer range (`-9223372036854775808` to `9223372036854775807`). Integer literals exceeding 64-bit bounds result in a **compile-time error**.
  * **`float`:** `x` must be a floating-point literal (`FloatLit`, requiring a decimal point or exponent, such as `42.0` or `1e5`) that can be rounded to IEEE 754 64-bit `double` precision without overflow. An integer literal without a decimal point or exponent (e.g., `42`) is **not** representable as a `float` without an explicit cast.
  * **`char`:** `x` is a quoted literal containing exactly one 8-bit character (e.g., `'a'`).
  * **`string`:** `x` is any valid string literal (`'...'`, `"..."`, or `` `...` ``).
  * **`bool`:** `x` is `true` or `false`.

```
x                       T           Representability

42                      int         Representable: 42 is within 64-bit signed int bounds
9223372036854775808     int         Compile-time error: overflows 64-bit signed int
42.0                    float       Representable: valid FloatLit within 64-bit IEEE 754 bounds
42                      float       Compile-time error: integer literal requires decimal (42.0) or explicit cast
'a'                     char        Representable: single character literal
'a'                     string      Representable: unified string/char literal annotated as string
"hello"                 char        Compile-time error: multi-character literal cannot represent char
0                       bool        Compile-time error: 0 is not a boolean value
```

## Method sets

The *method set* of a type determines the methods that can be called on an operand of that type using member access notation (`x.method(...)`):

* **Built-in Primitive, String, and Collection Types:** Unlike languages where primitive types have no methods, KeetScript provides built-in method sets across its predeclared and composite types for type conversions, formatting, and manipulation. The `keety` compiler translates these calls directly into zero-overhead C++ operations or runtime helpers:
  * `int`: includes conversion and formatting methods such as `.toFloat()`, `.toString()`, `.toBinary()`, `.toHex()`, etc.
  * `float`: includes methods such as `.toInt()`, `.toString()`, `.round()`, `.floor()`, `.ceil()`, etc.
  * `string`: includes `.length()`, `.toInt()`, `.toFloat()`, `.upper()`, `.lower()`, etc.
  * `array<T>` and `map<K, V>`: include collection management methods such as `.length()`, `.push(...)`, `.pop()`, `.has(...)`, `.remove(...)`, etc.
* **Class Types:**
  * The external method set of a mutable (`let`) instance of a `class` type `C` consists of all `pub` (or default-public) `fn` and `const fn` methods declared directly on `C`, together with any `inherited` methods marked `pub` (or default-public) from its parent class chain.
  * The external method set of an immutable (`const`) instance of a `class` type `C` is restricted to `const fn` public methods (`fn` methods without `const` cannot be called on `const` class instances).
  * Inside the lexical body of class `C`, the internal method set additionally includes all `priv` and `inherited priv` methods of `C`.
* **Method Overloading:** KeetScript supports **method overloading**. Within a single class (or built-in type), multiple methods may share the same identifier provided their parameter lists differ in the number of parameters or in at least one parameter type. Return type alone cannot disambiguate overloaded methods. Calling an overloaded method resolves at compile time to the signature whose parameter types match the argument types.


# Blocks

A *block* is a possibly empty sequence of declarations and statements within matching brace brackets `{ ... }`. Standalone blocks may also be nested directly inside functions or other blocks to restrict variable lifetimes (while still adhering to the strict no-shadowing rule against enclosing scopes).

```ebnf
Block         = "{" StatementList "}" .
StatementList = { Statement } .
```

*(Note: Individual statements and declarations within `StatementList` require explicit terminating semicolons `;`, whereas compound block-terminated constructs such as `if`, `for`, `while`, `switch`, `class`, and `fn` blocks do not require a trailing semicolon after `}`.)*

Furthermore, `switch` clauses in KeetScript use explicit brace-delimited blocks for each `case(...) { ... }` and `default { ... }` branch rather than colon-delimited fallthrough labels:

```keet
switch (status) {
    case (200) {
        let msg: string = "OK";
        io.println(msg);
    }
    default {
        io.println("Unknown");
    }
}
```

In addition to explicit `{ ... }` blocks in the source code, there are implicit lexical blocks:

1. The *universe block* encompasses all KeetScript source text and contains all predeclared types (`int`, `float`, `string`, `bool`, `char`, `void`, `null`, `error`, `array`, `map`), constants (`true`, `false`, `null`), and built-in operators (`typeof`).
2. Each `.keet` source file acts as its own *module block* containing all KeetScript source text in that file. Top-level declarations are private to the module block unless explicitly marked with `export`.
3. Each `if` (and `elif`), `while`, `for`, and `switch` statement is considered to be in its own implicit block, allowing variables declared in the control-flow header to be scoped strictly to that statement and its branches.
4. Each function, method, or constructor parameter list `(param: Type, ...)` acts as an implicit block encompassing the body block of that callable.
5. Each `class` and `enum` body acts as a lexical block for its member declarations.

Blocks nest and directly govern [Declarations and scope](#declarations-and-scope).


# Declarations and scope

A *declaration* binds a non-[blank](#blank-identifier) identifier to a [constant](#constant-declarations) (`define` or `const`), [variable](#variable-declarations) (`let`), [type](#type-declarations) (`type`, `class`, `enum`, `errtype`), [type parameter](#type-parameter-declarations-generics), [function](#function-declarations) (`fn`), or imported module (`import`). Every identifier in a program must be declared. No non-blank identifier may be declared twice in the same block (except for overloaded functions or methods with distinct parameter type signatures).

```ebnf
Declaration  = DefineDecl | ConstDecl | VarDecl | TypeDecl | FunctionDecl .
TopLevelDecl = [ "export" ] ( Declaration | ExternDecl ) .
```

The *scope* of a declared identifier is the extent of source text in which the identifier denotes the specified constant, variable, type, function, or module.

KeetScript is lexically scoped using [blocks](#blocks):

1. **Universe scope:** The scope of a [predeclared identifier](#predeclared-identifiers) is the universe block.
2. **Module scope (Order-Independent Top-Level Types & Functions):** The scope of an identifier denoting a `type` alias, `class`, `enum`, `errtype`, or top-level function declared at module level is the entire *module block*. Top-level functions and types may reference each other regardless of their source order in the `.keet` file (the `keety` compiler automatically emits forward declarations in the generated C++ headers).
3. **Import scope:** The scope of an imported module or symbol identifier is the module block of the `.keet` file containing the `import` declaration.
4. **Declare-Before-Use for Value Bindings (`define`, `const`, `let`):** The scope of a constant or variable identifier declared via `define`, `const`, or `let` (including inside control-flow headers) begins immediately after its initializer/declarator (`VarSpec`, `ConstDecl`, or `DefineDecl`) and ends at the end of the innermost containing block. Value bindings must be declared in source order before they are referenced.
5. **Parameter scope:** The scope of an identifier denoting a function, method, or constructor parameter is the callable's parameter block and body block.
6. **Type parameter scope:** The scope of a generic type parameter `<T>` begins immediately after the `<...>` parameter list and extends to the end of the generic function, method, class, or type alias declaration.
7. **Strict No-Shadowing Across Value Bindings:** An identifier denoting a value or variable binding (`define`, `const`, `let`, function parameter, or module-level binding) declared in an outer block **cannot** be redeclared in an inner block. Attempting to shadow an enclosing binding results in a compile-time error.
8. **Class Field Access (`this`), Base Access (`inherited`), and Method Scope:** Inside a `class` method or constructor, instance fields and methods are accessed explicitly via the `this` receiver keyword (e.g., `this.field`, `this.method()`), while immediate parent class members are accessed via `inherited` (e.g., `inherited.method()`). Consequently, method parameters and local variables inside a method body **may** share the same name as a class field without violating the no-shadowing rule (e.g., `this.x = x;`).

## Blank identifier

The *blank identifier* is represented by a single underscore character `_`. It serves as an anonymous discard placeholder rather than a regular identifier. Using `_` in a declaration (such as an unused function parameter, loop binding, or discarded value) does not introduce a binding into the current scope and may be repeated multiple times in the same block or parameter list without triggering duplicate-declaration or shadowing errors.

<a id="predeclared-identifiers"></a>
## Predeclared identifiers and universe scope

The universe block encompasses all predeclared keywords, built-in types, constants, and operators that are globally available across all KeetScript modules without imports:

```
Types (Reserved Keywords):
    bool  int  float  char  string  void  null
    error  array  map

Constants (Reserved Keywords):
    true  false  null

Operators / Built-ins:
    typeof
```

Because these type and constant names are reserved keywords in the universe scope, attempting to declare variables, constants, functions, parameters, or user types with these names is rejected at compile time (consistent with both keyword reservation rules and the strict no-shadowing principle).

*(Note: Standard library modules such as `io`—e.g., `io.println`—are not predeclared in the universe block and must be brought into scope via `import io;`.)*

## Exported declarations

By default, all declarations in a `.keet` file are private to that file's module block. A top-level declaration may be *exported* to permit access from other `.keet` modules by placing the `export` keyword directly in front of the declaration.

`export` may only appear at the top level of a module block:

```keet
export define MAX_RETRIES: int = 3;
export const DEFAULT_HOST: string = "localhost";
export let activeConnections: int = 0;
export type Handler = (req: string) => bool;
export fn connect(host: string): bool { return true; }
export class Client {}
export enum Status { OK = 0, Error = 1 }
```

## Constant declarations

A constant declaration binds an identifier to a compile-time constant expression (`define`, mapping to C++ `constexpr`) or an immutable runtime value (`const`, mapping to C++ `const`). Every constant declaration requires an explicit type annotation and an initialization expression.

```ebnf
DefineDecl = "define" Identifier ":" TypeAnnotation "=" Expression ";" .
ConstDecl  = "const" Identifier ":" TypeAnnotation "=" Expression ";" .
```

Examples:
```keet
define MAX_BUFFER: int = 1024 * 16;
const appName: string = "KeetApp";
const fetchTask: async () => void = async fn (): void {
    // asynchronous anonymous function assigned to a const binding
};
```

## Variable declarations

A variable declaration (`let`) creates one or more mutable variables, binds identifiers to them, and assigns each an explicit type and optional initial value (following the initialization rules in [Variables](#variables)).

```ebnf
VarDecl     = "let" VarSpecList ";" .
VarSpecList = VarSpec { "," VarSpec } .
VarSpec     = Identifier ":" TypeAnnotation [ "=" Expression ] .
```

Examples:
```keet
let retries: int = 0;
let buffer: array<char>;
let currentError: string | null;
let x: float = 1.0, y: float = 2.0;
```

## Type declarations

KeetScript supports four forms of user-defined type declarations:

1. **Type Aliases (`type`):** Binds an identifier (optionally generic) as a transparent alias to an existing type, union, or composite type.
2. **Class Declarations (`class`):** Introduces a new, nominally typed object-oriented class (optionally generic or inheriting from a parent class via `inherits`).
3. **Enum Declarations (`enum`):** Introduces a new, nominally typed scoped integer enumeration.
4. **Error Type Declarations (`errtype`):** Introduces a typed error construct for use with `zonk` error handling.

```ebnf
TypeDecl      = TypeAliasDecl | ClassDecl | EnumDecl | ErrTypeDecl .
TypeAliasDecl = "type" Identifier [ TypeParameters ] "=" TypeAnnotation ";" .
```

Examples:
```keet
type StringMap<V> = map<string, V>;
type Nullable<T> = T | null;
```

## Type parameter declarations (Generics)

Users may declare generic functions, methods, classes, and `type` aliases by specifying a *type parameter list* enclosed in angle brackets `<...>`, which compiles directly to C++ `template <typename ...>` declarations.

Each type parameter may optionally specify a *type constraint* after `:` (such as a union of permitted types `int | float`) to restrict the permissible type arguments at compile time:

```ebnf
TypeParameters = "<" TypeParamList ">" .
TypeParamList  = TypeParamDecl { "," TypeParamDecl } .
TypeParamDecl  = Identifier [ ":" TypeConstraint ] .
TypeConstraint = TypeAnnotation .
```

All non-blank identifiers in a `TypeParamList` must be unique. Generic types and functions are instantiated by supplying explicit type arguments `<...>` at the use site:

```keet
fn identity<T>(val: T): T {
    return val;
}

fn addNumeric<T: int | float>(a: T, b: T): T {
    return a + b;
}

class Box<T> {
    let value: T;
    Box(initial: T) {
        this.value = initial;
    }
}

let intBox: Box<int> = Box<int>(42);
```

## Function declarations

A function declaration binds an identifier to a callable function using the `fn` keyword. Functions may be synchronous or marked `async` for asynchronous execution.

* **`async` Functions, Function Types, and C++17 Task Lowering:**
  * A function declaration or anonymous function expression (`FunctionLit`) is marked asynchronous with `async fn`, while a function type annotation uses `[ "async" ] (param: Type) => ReturnType`.
  * **C++17 Lowering Model:** Because KeetScript targets standard C++17, an asynchronous function `async fn f(params...): T` transpiles to a C++17 function returning an asynchronous task future (`keet::Task<T>`, backed by `std::future<T>` or the runtime's work-stealing thread-pool task handle). Evaluating `await taskExpr` inside an `async` callable yields execution of the current task back to the worker pool/scheduler and synchronizes on completion of `taskExpr` before unwrapping the result of type `T` (or propagating any raised `keet::error`).
  * **Thread-Safety and Prohibition of `mut` Parameters in `async`:** Parameters of functions, methods, or function types declared `async` **cannot** use the `mut` modifier (`async fn (mut x: int): void` is a **compile-time error**). Combined with default capture-by-value (`[=]`) on closures, this guarantees that asynchronous tasks running on background worker threads communicate strictly via isolated value copies and never hold dangling or data-racing C++ references (`T&`) to a caller's stack frame.
* **Parameter Passing, Mutable Reference Parameters (`mut`), and Aliasing Safety Rules:**
  * By default, function and method parameters are passed by value (local mutations inside the function body affect only the function's local copy, and the compiler may elide copies via `const T&` or last-use `std::move` in C++ when safe).
  * Prefixing a parameter of a synchronous function or method with `mut` (`mut param: T`) declares an explicit **pass-by-mutable-reference** parameter, mapping directly to `T&` in generated C++.
  * To prevent C++ reference aliasing bugs, iterator/reallocation invalidation, and dangling references, `keety` enforces five strict compile-time safety rules on `mut` parameters:
    1. **Mutable L-Value Requirement:** At the call site, the argument supplied to a `mut` parameter (optionally prefixed with `mut` at the call site, e.g., `swap(mut x, mut y)` or `swap(x, y)`) must be a mutable (`let`) addressable variable, a mutable array/string index (`arr[i]`), or a mutable class field (`obj.field`). Passing a literal, `const`/`define` binding, or temporary expression to a `mut` parameter is a **compile-time error**.
    2. **Map Elements Cannot Be Passed as `mut`:** Passing a map index expression (`m[k]` or `mut m[k]`) to a `mut` parameter is a **compile-time error**, both because reading `m[k]` yields `V | null` rather than an l-value of `V` and because `std::unordered_map` entries are not stable, contiguous storage locations across insertions/rehashes. Map entries must be updated via direct assignment (`m[k] = val;`, `m[k] += delta;`) or map methods.
    3. **Exclusive Borrowing (No Duplicate `mut` Aliasing):** Passing the same underlying variable or storage path more than once as a `mut` argument in a single call (or passing the same variable to both a `mut` parameter and another parameter in the same call where one aliases the other, such as `f(mut a, mut a)` or `swap(a, a)`) is a **compile-time error**.
    4. **Container-Element Invalidation Prohibition:** Passing both a collection variable (`arr`) and an indexed element of that same collection (`arr[i]`) as `mut` arguments (or where either parameter is `mut`) in the same call (e.g., `f(mut arr[i], mut arr)`) is a **compile-time error**, preventing `std::vector` reallocation (`arr.push(...)` or `arr.clear()`) from dangling the element reference `arr[i]`.
    5. **No Default Values or `async` Usage:** Default parameter values (`= Expression`) and `async` callables are not permitted with `mut` parameters.
* **Anonymous Functions (Callbacks):** Inline callbacks and function literals use `fn` without a name (`[ "async" ] "fn" [ CaptureClause ] Parameters ":" TypeAnnotation Block`) and, like named functions, strictly require an explicit `: TypeAnnotation` return type.
* **Default Parameter Values:** Function and method parameters may specify a default expression (`param: Type = DefaultExpr`). Parameters with default values must appear after all required parameters in the parameter list.
* **Function Overloading:** Like class methods, standalone functions in the same scope may be overloaded by declaring multiple functions with the same name provided their parameter lists differ in arity or parameter types.
* **Return Requirement:** If a function's return type is anything other than `void`, every execution path through the function body must end in a terminating `return` statement (or unrecovered error).

```ebnf
FunctionDecl  = [ "async" ] "fn" Identifier [ TypeParameters ] Parameters ":" TypeAnnotation Block .
FunctionLit   = [ "async" ] "fn" [ CaptureClause ] Parameters ":" TypeAnnotation Block .
CaptureClause = "[" [ CaptureList ] "]" .
CaptureList   = CaptureSpec { "," CaptureSpec } .
CaptureSpec   = [ "mut" ] Identifier .
```

*(See [Function types](#function-types) for the `FunctionType`, `Parameters`, `ParameterList`, and `ParameterDecl` productions.)*

Examples of function declarations, `mut` reference parameters, and callbacks:
```keet
fn add(a: int, b: int): int {
    return a + b;
}

fn swap<T>(mut a: T, mut b: T): void {
    let temp: T = a;
    a = b;
    b = temp;
}

fn greet(name: string = "World"): void {
    io.println(`Hello, ${name}!`);
}

fn sth1(a: (x: int) => int, b: int): int {
    return a(b);
}

sth1(fn (x: int): int {
    return x + 1;
}, 5);

async fn fetchData(url: string): string {
    // asynchronous function body
    return "response";
}
```

## Method declarations

Methods are functions declared inside a `class` block using the `fn` keyword and bound to that class type.

* **Visibility & Inheritance:** Methods are public (`pub`) by default, may be marked private (`priv`), and must be prefixed with `inherited` when overriding or deriving from a parent class method.
* **`this` Receiver & Receiver Mutability (`const fn` vs. `fn`):** Inside a method or constructor body, the current class instance is accessed via `this` (e.g., `this.fieldName` or `this.otherMethod()`):
  * Methods declared with `const fn` have an immutable receiver (`const this`, transpiling to C++ `const`-qualified member functions, e.g., `int64_t getCount() const`) and cannot mutate instance fields (`this.field`) or call non-`const` (`fn`) methods on `this`.
  * Methods declared with `fn` (without `const`) have a mutable receiver and may mutate instance fields, but cannot be called on `const` class instances.
* **Overloading & Generics:** Class methods support default parameter values, generic type parameters `<T>`, `async` execution, and compile-time method overloading.

```ebnf
MethodDecl = [ "inherited" ] [ "pub" | "priv" ] [ "async" ] [ "const" ] "fn"
             Identifier [ TypeParameters ] Parameters ":" TypeAnnotation Block .
```

Example of a class with fields, constructor, `this` access, and overloaded methods:
```keet
class Counter {
    priv let count: int = 0;

    Counter(initial: int = 0) {
        this.count = initial;
    }

    pub fn increment(): void {
        this.count += 1;
    }

    pub fn increment(step: int): void {
        this.count += step;
    }

    pub const fn getCount(): int {
        return this.count;
    }
}
```


# Expressions

An *expression* specifies the computation of a value by applying operators and functions to operands.

```ebnf
Expression         = ConditionalExpr .
ConditionalExpr    = ZonkExpr [ "?" Expression ":" ConditionalExpr ] .
ZonkExpr           = LogicalOrExpr { ZonkSuffix } .
ZonkSuffix         = "zonk" ( LogicalOrExpr | "(" [ Identifier ] ")" Block ) .
LogicalOrExpr      = LogicalAndExpr { "||" LogicalAndExpr } .
LogicalAndExpr     = EqualityExpr { "&&" EqualityExpr } .
EqualityExpr       = RelationalExpr { ( "==" | "!=" ) RelationalExpr } .
RelationalExpr     = AdditiveExpr { ( "<" | "<=" | ">" | ">=" ) AdditiveExpr } .
AdditiveExpr       = MultiplicativeExpr { ( "+" | "-" ) MultiplicativeExpr } .
MultiplicativeExpr = PostfixTypeExpr { ( "*" | "/" | "%" ) PostfixTypeExpr } .
PostfixTypeExpr    = UnaryExpr { TypeAssertion } .
TypeAssertion      = "as" TypeAnnotation .
UnaryExpr          = PrimaryExpr | ( "+" | "-" | "!" | "await" ) UnaryExpr | TypeofExpr .
TypeofExpr         = "typeof" "(" Expression ")" .
```

## Operands

Operands denote the elementary values in an expression. An operand may be a literal, a (possibly generic-instantiated) identifier denoting a [constant](#constant-declarations), [variable](#variable-declarations), [function](#function-declarations), or [type](#type-declarations) constructor, the `this` receiver keyword, the `inherited` base-class receiver keyword, a built-in functional type conversion, or a parenthesized expression.

```ebnf
Operand     = Literal | OperandName | "this" | "inherited" | Conversion | "(" Expression ")" .
Literal     = BasicLit | CompositeLit | FunctionLit .
BasicLit    = IntLit | FloatLit | StringLit | "true" | "false" | "null" .
OperandName = Identifier [ TypeArguments ] .
```

* **Generic `TypeArguments` vs. Relational `<` Disambiguation:** In an `OperandName` or `Selector` within an expression, a `<` token is parsed as the opening delimiter of a generic `TypeArguments` list (`<T1, T2>`) if and only if it **immediately follows the `Identifier` token with no intervening whitespace** (e.g., `Box<int>(42)`, `arr.map<string>(...)`) and is followed by a valid `TypeArgList` closed by `>` before `(` or `.`. Whenever `<` is preceded by whitespace (e.g., `a < b > c`), it is unambiguously parsed as the binary relational less-than operator.

The `this` operand is valid only within the body of a `class` constructor or method and denotes the current receiver instance. Similarly, the `inherited` operand is valid only within the body of a derived `class` constructor or method and denotes the current instance viewed through its immediate base class, enabling static dispatch to base methods or fields (e.g., `inherited.method()`).

## Literals

### Composite literals (`array`, `map`, and `class` instantiation)

Composite literals construct values for arrays and maps directly within expressions, while classes are constructed using direct constructor invocation. Every composite value adheres strictly to [value semantics](#representation-of-values).

```ebnf
CompositeLit = ArrayLit | MapLit .
ArrayLit     = "[" [ ExpressionList [ "," ] ] "]" .
MapLit       = "{" [ KeyValueList [ "," ] ] "}" .
KeyValueList = KeyValuePair { "," KeyValuePair } .
KeyValuePair = Expression ":" Expression .
ExpressionList = Expression { "," Expression } .
```

* **Array Literals (`ArrayLit`):** An array literal is written as a comma-separated list of element expressions enclosed in square brackets `[expr1, expr2, ...]`. All element expressions must be [assignable](#assignability) to the target `array<T>` element type `T`.
* **Map Literals (`MapLit`):** A map literal is written using JSON/Python-style braces and colons `{"key1": val1, "key2": val2}`. Each key expression must be assignable to the target `map<K, V>`'s `KeyType` `K` (which must satisfy the hashability and equality constraints defined in [Map types](#map-types)), and each value expression must be assignable to `ValueType` `V`.
  *(Syntactic disambiguation note: Because `MapLit` only appears in expression positions—such as the right-hand side of `=`, function arguments, or `return` expressions—whereas statement `Block`s only appear in statement or declaration positions, the parser unambiguously distinguishes `{}` and `{"k": v}` from statement blocks without lookahead conflicts.)*
* **Contextual Typing of Empty Collection Literals (`[]` and `{}`):** Empty collection literals (`[]` and `{}`) do not possess an inherent, self-sufficient type on their own. Evaluating an empty array literal `[]` (initializing an empty `std::vector<T>`) or an empty map literal `{}` (initializing an empty `std::unordered_map<K, V>`) is valid **only** within a typed context where the target collection type (`array<T>` or `map<K, V>`) is statically known—such as in a variable or constant declaration with an explicit type annotation (`let m: map<string, int> = {};`), on the right-hand side of an assignment to an already-typed collection variable, as an argument to a typed function parameter, or in a `return` statement inside a function with a declared collection return type. Using an untyped or unconstrained empty collection literal where context cannot supply the element or key/value types results in a **compile-time error**.
* **Class Instantiation (No `new` Keyword):** KeetScript intentionally omits the `new` keyword. A `class` is instantiated directly by calling its class name (with optional generic `TypeArguments`) as a constructor function: `ClassName(args...)` or `ClassName<TypeArgs>(args...)`. This directly reflects KeetScript's C++ stack/value construction semantics (`ClassName(args...)` in C++).

Examples of composite construction:
```keet
let primes: array<int> = [2, 3, 5, 7, 11];
let emptyList: array<string> = [];        // valid: element type `string` known from variable declaration

let ports: map<string, int> = {
    "http": 80,
    "https": 443,
};
let emptyCache: map<int, string> = {};    // valid: key/value types known from variable declaration

// typeof([]);                            // compile-time error: unconstrained empty array literal has no element type
// let ambiguous: array<int> | array<string> = []; // compile-time error: target type does not uniquely determine element type

let intBox: Box<int> = Box<int>(42);
let counter: Counter = Counter(10);
```

### Function literals (`FunctionLit`) and closure captures

A *function literal* represents an anonymous function or inline callback and evaluates to a value of [function type](#function-types). *(See [Function declarations](#function-declarations) for the formal `FunctionLit` and `CaptureClause` grammar.)*

Like named function declarations, every `FunctionLit` strictly requires explicit parameter type annotations and an explicit `: TypeAnnotation` return type.

* **Enclosing Scope Capture:** A function literal may reference constants and variables declared in enclosing lexical scopes (closures).
* **Default Capture-by-Value Semantics (`[=]` in C++):** To uphold KeetScript's strict [value semantics](#representation-of-values) and eliminate dangling-reference lifetime bugs when callbacks are returned from functions or executed asynchronously, outer bindings referenced inside a `FunctionLit` without an explicit `CaptureClause` (or listed without `mut` in `[x, y]`) are **captured by value** by default (translating directly to C++ `[=]` lambda captures). Each value-captured binding inside the closure holds an independent copy of the outer variable's value taken at the moment the `FunctionLit` expression is evaluated.
* **Explicit Mutable Reference Capture (`[mut x]`):** When a synchronous closure needs to mutate an enclosing `let` variable (such as updating an outer accumulator or counter inside a higher-order algorithm), the `FunctionLit` may specify an explicit capture clause (`fn [mut count] (x: int): void { count += x; }`, mapping to `[&count]` in C++).
  * **Strict Non-Escaping Lifetime Constraints on `[mut]` Closures:** To guarantee zero dangling C++ references (`[&]`), a `FunctionLit` with one or more `[mut ...]` captures is strictly **non-escaping**:
    1. It can **only** be passed as a direct argument to a synchronous callable (such as `arr.filter(fn [mut count] (x: int): bool { ... })`) or immediately invoked in place.
    2. It **cannot** be assigned to any variable whose lifetime exceeds the innermost block, cannot be returned from a function, cannot be stored inside a collection (`array`, `map`) or `class` field, and cannot be used with an `async fn` literal. Violating any of these escape constraints is a **compile-time error**.

```keet
let factor: int = 10;
const multiply: (x: int) => int = fn (x: int): int {
    return x * factor; // `factor` is captured by value (`[=]` in C++)
};
factor = 99;
let res: int = multiply(5); // evaluates to 50, unaffected by later mutation of `factor`

let visitedCount: int = 0;
let items: array<int> = [1, 2, 3, 4];
_ = items.filter(fn [mut visitedCount] (x: int): bool {
    visitedCount += 1;      // valid: direct non-escaping synchronous callback (`[&visitedCount]`)
    return x % 2 == 0;
});                         // `visitedCount` is now 4
```

## Primary expressions

Primary expressions are the operands for unary and binary expressions. They encompass operands, member selectors, collection and string indexing, and callable invocations.

```ebnf
PrimaryExpr  = Operand { Selector | Index | Arguments } .
Selector     = "." Identifier [ TypeArguments ] .
Index        = "[" Expression "]" .
Arguments    = "(" [ ArgumentList [ "," ] ] ")" .
ArgumentList = Argument { "," Argument } .
Argument     = [ "mut" ] Expression .
```

*(Note: Slice syntax such as `x[a:b]` is intentionally omitted from the core grammar. Sub-arrays and substrings are obtained via built-in `.slice(start, end)` methods on `array<T>` and `string`.)*

## Selectors

For a primary expression `x`, the selector expression `x.f` denotes the field, method, enumerator, or module member `f` of the value or entity `x`:

* **Module Member Selection:** If `x` denotes an imported module (e.g., `io`), `x.f` denotes the exported declaration `f` within that module (e.g., `io.println`).
* **Enum Member Selection:** If `x` denotes an `enum` type (e.g., `Status`), `x.f` denotes the scoped enumerator `Status.OK` (mapping to `Status::OK` in C++).
* **Class Field and Method Selection:** If `x` is an instance of a `class` (including `this`), `x.f` selects the field or method `f` according to the visibility and `const fn` receiver rules in [Method sets](#method-sets). Accessing a `priv` member from outside the lexical body of its declaring class is a compile-time error.
* **Base Class Member Selection (`inherited.f`):** If `x` is the `inherited` operand within a method or constructor of a derived class, `inherited.f` explicitly selects and statically invokes the method or accesses the field `f` of the immediate parent class, bypassing any derived-class override (transpiling directly to C++ `BaseClass::f(...)`).
* **Built-in Type Method Selection:** If `x` is a value of a predeclared primitive type (`int`, `float`, `bool`, `char`), `string`, `array<T>`, or `map<K, V>`, `x.f` selects the built-in method `f` (such as `x.toString()` or `x.length()`).
* **Union Receiver Constraint:** Selecting a member `x.f` on an unnarrowed union value `T1 | T2` is a compile-time error unless `x` has first been narrowed via flow-based type narrowing (`typeof(x)`) or an explicit type assertion (`x as T1`).

## Index expressions

A primary expression of the form `a[x]` denotes the element of the array, string, or map `a` indexed by `x`:

* **Array Indexing (`array<T>`):** For `a` of type `array<T>`, the index `x` must be of type `int`; the expression `a[x]` has type `T`. Valid indices are `0 <= x < a.length()`. Out-of-bounds array indexing raises a runtime bounds error. If `a` is a mutable `let` variable, `a[x]` is addressable and may appear on the left-hand side of an assignment (`a[x] = val`).
* **String Indexing (`string`):** For `a` of type `string`, the index `x` must be of type `int`; the expression `a[x]` has type `char` and yields the raw UTF-8 byte (code unit) at 0-based byte offset `x` (`0 <= x < a.length()`). If `a` is a mutable `let` string, `a[x]` may be mutated in place (`a[0] = 'J'`).
* **Safe Map Lookup and Assignment (`map<K, V>`):** For `a` of type `map<K, V>`, the key expression `x` must be assignable to `KeyType` `K`:
  * **Read Lookup (`V | null`):** Unlike C++ `std::unordered_map::operator[]`, reading `a[x]` in an expression **never** silently default-inserts a missing key. Instead, a read expression `a[x]` evaluates to the nullable union type **`V | null`**—returning the associated value of type `V` if key `x` exists, or `null` if `x` is absent.
  * **Narrowed Map Lookup:** If the `keety` compiler can statically prove in the current control-flow branch that `a.has(x)` is `true` (e.g., inside `if (a.has(k)) { ... }` without intervening mutation of `a`), or if the lookup is immediately narrowed using an explicit type assertion with `zonk` fallback (`(a[x] as V) zonk defaultVal`), the resulting expression yields `V` directly. Because a missing key evaluates normally to `null` rather than raising a runtime error, unwrapping a missing key with a fallback value is achieved via `a.get(x, defaultVal)` or `(a[x] as V) zonk defaultVal` (a bare `a[x] zonk defaultVal` does not trigger `zonk` on `null` because `null` is a valid non-error value).
  * **Map Element Assignment:** When `a[x]` appears as the left-hand target of an assignment (`a[x] = val` on a mutable `let` map), `val` must be assignable to `V` (not `V | null`, unless `V` itself includes `null`), inserting a new key-value pair or overwriting the existing value for `x`.

```keet
let scores: map<string, int> = {"Alice": 100};

let maybeBob: int | null = scores["Bob"]; // returns `null`; does NOT insert "Bob"
scores["Bob"] = 85;                       // inserts "Bob": 85

let bobScore: int = scores.get("Bob", 0); // returns 85; returns `int` directly
let charlie: int = (scores["Charlie"] as int) zonk 0; // falls back to 0 via type assertion

if (scores.has("Alice")) {
    let aliceScore: int = scores["Alice"]; // narrowed to `int` via `scores.has(...)`
}
```

## Calls

Given an expression `f` of function type, method selector, or class constructor name, `f(a1, a2, ...)` invokes `f` with arguments `a1, a2, ...`:

* **Argument Evaluation and Value vs. `mut` Reference Passing:** Arguments are evaluated from left to right before the function body executes. Each argument expression must be [assignable](#assignability) to the corresponding parameter's declared type:
  * For standard value parameters, a copy of the argument value is passed (or elided via `const T&` or last-use `std::move` in C++ when safe).
  * For parameters declared with the `mut` modifier (`mut param: T`), the argument expression at the call site must satisfy the five `mut` aliasing and addressability safety rules defined in [Function declarations](#function-declarations): it must be a mutable (`let`) addressable variable, array/string index, or class field (never a map element `m[k]`), must not alias any other argument in the same call (`swap(a, a)` is a compile-time error), and must not simultaneously pass a container and its element (`f(arr[i], arr)` is a compile-time error).
  * Omitted trailing arguments for parameters declared with default values (`param: Type = DefaultExpr`) are filled using their declared default expressions.
* **Overload Resolution:** If `f` refers to an overloaded function or method name, the compiler resolves the call at compile time to the unique declaration whose parameter count and parameter types match the argument types. If no overload matches or multiple overloads match ambiguously, a compile-time error occurs.
* **Constructor Calls:** If `f` is a `class` name (`ClassName(args...)` or `ClassName<TypeArgs>(args...)`), the call allocates and initializes a new value instance of that class by invoking the matching constructor declared in `ClassName`.

## Operators

Operators combine operands into expressions.

### Operator precedence

Unary operators and postfix primary operations have the highest precedence. Binary, error-recovery (`zonk`), and conditional operators are grouped into levels of decreasing binding power as follows:

```
Precedence    Operator / Construct                           Associativity
    11        ()  []  .  <TypeArgs>  (Postfix / Primary)     Left-to-right
    10        +  -  !  await  typeof(...)  (Unary prefix)    Right-to-left
     9        as  (Union type assertion)                     Left-to-right
     8        *  /  %  (Multiplicative)                      Left-to-right
     7        +  -  (Additive & concatenation)               Left-to-right
     6        <  <=  >  >=  (Relational)                     Left-to-right
     5        ==  !=  (Equality)                             Left-to-right
     4        &&  (Logical AND, short-circuiting)            Left-to-right
     3        ||  (Logical OR, short-circuiting)             Left-to-right
     2        zonk  (Inline error recovery)                  Left-to-right
     1        ? :  (Ternary conditional)                     Right-to-left
```

Because `zonk` binds looser than arithmetic, relational, and logical operators (Precedence 2), an expression such as `a zonk b + c` is parsed naturally as `a zonk (b + c)`, where `b + c` serves as the fallback expression if `a` fails. To apply a binary operator to the recovered result of a `zonk` expression, explicit parentheses are used: `(a zonk b) + c`.

Binary operators of the same precedence associate from left to right, whereas unary prefix operators and the ternary conditional operator (`? :`) associate from right to left. Parentheses `(...)` may be used freely to override precedence and group sub-expressions explicitly.

### Arithmetic operators

Arithmetic operators apply to numeric values (`int` and `float`), with `+` additionally supporting string concatenation. Implicit numeric conversions between `int` and `float` are forbidden; both operands of a numeric binary operator must have the same type (`int` with `int`, or `float` with `float`).

```
+    sum / unary identity          int, float (and string/char concatenation)
-    difference / unary negation   int, float
*    product                       int, float
/    quotient                      int, float
%    remainder                     int only
```

* **Zero Undefined Behavior (Two's-Complement Wrap-Around):** All 64-bit signed `int` additions (`+`), subtractions (`-`), multiplications (`*`), and unary negations (`-x`, where `-INT64_MIN` wraps to `INT64_MIN`) obey deterministic 64-bit two's-complement wrap-around semantics with zero undefined behavior.
* **Unary `+` and `-`:** Unary `+x` yields `x` (valid for `int` and `float`). Unary `-x` negates `x` (valid for `int` and `float`).
* **Integer vs. Floating-Point Division (`/`):**
  * For `int` operands, `a / b` performs integer division truncating towards zero (matching C++ `int64_t` division) and yields an `int`. Division by zero (`b == 0`) deterministically raises a runtime error (recoverable via `zonk`), and `INT64_MIN / -1` wraps to `INT64_MIN`.
  * For `float` operands, `a / b` performs IEEE 754 64-bit floating-point division and yields a `float`.
* **Remainder (`%`):** The remainder operator `a % b` is defined **exclusively on `int` operands** (satisfying `(a / b) * b + (a % b) == a`). Remainder by zero (`b == 0`) deterministically raises a runtime error (recoverable via `zonk`), and `INT64_MIN % -1` yields `0`. Applying `%` to `float` operands is a **compile-time error** (floating-point remainder is provided via the standard `math` library).
* **String and Character Concatenation (`+`):** The binary `+` operator (and `+=` compound assignment) concatenates text when at least one operand is a `string` and the other is a `string` or `char` (`string + string`, `string + char`, or `char + string`), yielding a new `string`. Adding `char + char` is not permitted without converting at least one operand to `string` or `int`.

*(Note: Bitwise operators are intentionally omitted from the core grammar and provided via specialized standard libraries.)*

### Comparison and equality operators

Comparison operators compare two operands and yield an untyped/predeclared boolean value of type `bool`.

```
==    equal
!=    not equal
<     less
<=    less or equal
>     greater
>=    greater or equal
```

* **Relational Ordering (`<`, `<=`, `>`, `>=`):** Valid only when both operands have the same ordered type:
  * `int` or `float`: standard numerical ordering.
  * `char`: unsigned 8-bit ASCII/UTF-8 byte value ordering.
  * `string`: lexicographical byte-by-byte UTF-8 code unit comparison (matching C++ `std::string` relational operators).
* **Deep Value Equality (`==`, `!=`):** Two operands may be compared for equality if their types are identical, or if one operand's type is assignable to the other's (such as comparing a `string | null` union variable against `null` or `"ok"`):
  * **Primitives, Strings, Enums, and `null`:** Compared directly by value (`bool`, `int`, `float`, `char`, `string`, `enum`, `null`).
  * **Arrays and Maps (`array<T>`, `map<K, V>`):** Perform **deep structural value equality** (matching C++ `std::vector::operator==` and `std::unordered_map::operator==`), recursively comparing elements or key-value pairs using `==`.
  * **Unions (`T1 | T2`):** Two union values are equal if and only if they currently hold the same active runtime type and their underlying active values are equal under `==` (matching C++ `std::variant::operator==`).
  * **Class Instances (`const fn equals`):** Equality operators `==` and `!=` are disallowed by default on `class` types (resulting in a compile-time error) unless the class explicitly declares `pub const fn equals(other: ClassName): bool`. When `equals` is declared, `a == b` invokes `a.equals(b)` (transpiled to C++ `operator==`), and `a != b` evaluates `!a.equals(b)`.
  * **Function Types:** Function values (`(x: int) => bool`) are not comparable with `==` or `!=` (unless compared against `null` in a nullable union `((x: int) => bool) | null`).

### Logical operators

Logical operators apply to `bool` operands and yield a result of type `bool`. KeetScript does not have implicit "truthy" or "falsy" conversions; operands of `!`, `&&`, and `||` must be explicitly of type `bool`.

```
&&    conditional AND    p && q  is  "if p then q else false"
||    conditional OR     p || q  is  "if p then true else q"
!     NOT                !p      is  "not p"
```

Both `&&` and `||` use **short-circuit evaluation**: the right-hand operand is evaluated if and only if the left-hand operand does not already determine the result. Short-circuiting also participates in flow-based union narrowing (e.g., `if (typeof(x) == "string" && x.length() > 0)` safely narrows `x` to `string` before evaluating `x.length()`).

### Ternary conditional expressions (`? :`)

The ternary conditional operator selects one of two expressions based on a boolean condition (see `ConditionalExpr` in [Expressions](#expressions)):

In `cond ? trueExpr : falseExpr`:
1. `cond` must evaluate to type `bool`. First, `cond` is evaluated.
2. If `cond` is `true`, only `trueExpr` is evaluated and its value becomes the result of the conditional expression; `falseExpr` is not evaluated.
3. If `cond` is `false`, only `falseExpr` is evaluated and its value becomes the result; `trueExpr` is not evaluated.
4. **Result Type:** If `trueExpr` and `falseExpr` have identical types `T`, the conditional expression has type `T`. If `trueExpr` has type `T1` and `falseExpr` has type `T2`, the conditional expression evaluates to the union type **`T1 | T2`**.

```keet
let label: string = score >= 50 ? "Pass" : "Fail";
let flexible: int | string = useDefault ? 0 : "custom";
```

<a id="increment-and-decrement"></a>
### Increment and decrement (`++`, `--`)

To guarantee deterministic expression evaluation and eliminate the undefined/unspecified behavior bugs associated with C/C++ pre- and post-increment expressions (`f(i++, i)`), **`++` and `--` are not value-yielding expressions in KeetScript**:

* `x++` and `x--` cannot appear inside a larger expression, assignment right-hand side, or function argument list (e.g., `let y: int = x++;` and `foo(++x);` are **compile-time errors**).
* Instead, `x++` and `x--` are strictly **standalone postfix mutation statements** (syntactic sugar for `x += 1` and `x -= 1` on mutable `int` or `float` variables), valid only as standalone statements or in the post-iteration clause of a `for` loop header. *(See the Statements chapter for the formal `IncDecStmt` grammar.)*

### Asynchronous `await` expressions

The unary prefix operator `await` suspends execution of the enclosing asynchronous function until the operand asynchronous task completes:

```ebnf
AwaitExpr = "await" UnaryExpr .
```

* **Lexical Restriction:** An `await` expression is permitted **only** inside the body of an `async fn` function declaration, an `async` class method (`async fn` / `async const fn`), or an `async fn` function literal. Using `await` inside a synchronous function or at module top level is a compile-time error.
* **C++17 Task Future Model & Thread-Safety Guarantees:**
  * In the C++17 AOT compilation target, an asynchronous callable `async fn (...): T` compiles to a function returning a task future handle (`keet::Task<T>` backed by `std::future<T>` and the KeetScript work-stealing thread pool).
  * Evaluating `await taskExpr` yields control of the current worker thread / task queue back to the scheduler until `taskExpr` completes, then unwraps and returns the resulting value of type `T` (or rethrows any `keet::error` exception captured inside the task future so it can be handled via `zonk`).
  * **Data-Race Prevention by Design:** Because `async` functions are forbidden from declaring `mut` reference parameters (`mut T`) and `async fn` closures are forbidden from using `[mut]` reference captures (capturing strictly by value `[=]`), tasks scheduled across worker threads operate on isolated, independent value copies with zero shared-reference data races.

## Type conversions and assertions

KeetScript supports explicit value conversions between compatible types as well as runtime type assertions on union types.

### Explicit type conversions (`.toX()` and `T(x)`)

Because KeetScript forbids implicit type coercions (such as between `int` and `float`), conversions between compatible primitive, string, character, and `enum` types must be written explicitly using either of two equivalent forms:

1. **Built-in Conversion Methods (`.toX()`):** Calling a conversion method on the operand, such as `x.toFloat()`, `x.toInt()`, `x.toString()`, `x.toChar()`.
2. **Functional Cast Expressions (`T(x)`):** Invoking a predeclared scalar or string type name as a single-argument conversion function:

```ebnf
Conversion      = ConvertibleType "(" Expression ")" .
ConvertibleType = "int" | "float" | "char" | "string" | "bool" .
```

Both forms generate identical C++ conversion code with strict Zero-UB bounds checks:
* `float(n)` / `n.toFloat()` converts a 64-bit `int` to a 64-bit IEEE 754 `float` (`static_cast<double>(n)`).
* `int(f)` / `f.toInt()` converts a `float` to an `int` by truncating the fractional part towards zero; if `f` is `NaN`, `±Infinity`, or outside the 64-bit signed integer range `[INT64_MIN, INT64_MAX]`, it deterministically raises a runtime error (recoverable via `zonk`). It also converts a `char` or `enum` member to its underlying `int` value, or parses a decimal `string` to `int` (raising a recoverable error if the string is not a valid integer).
* `string(x)` / `x.toString()` converts `int`, `float`, `bool`, or `char` to its textual `string` representation.
* `char(n)` / `n.toChar()` converts an `int` byte value (`0`–`255`) to `char` (raising a recoverable error via `zonk` if out of bounds).
* `EnumName(intValue)` converts an integer to an `enum` instance; if `intValue` does not match any declared enumerator in `EnumName`, it deterministically raises a runtime error (recoverable via `zonk`).

```keet
let count: int = 42;
let ratio: float = float(count) / 100.0; // equivalent to `count.toFloat() / 100.0`
let text: string = string(count);        // equivalent to `count.toString()`
let status: Status = Status(200) zonk Status.InternalError; // checked enum conversion with zonk fallback
```

### Union type assertions (`as`)

While flow-based type narrowing (`if (typeof(x) == "int")`) is the primary idiomatic way to narrow a union type safely, an expression `x` of a union type `U` may also be explicitly narrowed or unwrapped to a target type `T` using the postfix `as` operator:

```ebnf
TypeAssertion = "as" TypeAnnotation .
```

In `x as T`:
* `x` must be of a union type `U` (e.g., `int | string | null`), and `T` must be a valid constituent type (or sub-union) of `U`. Attempting to assert a type `T` that is not part of `U` (or using `as` as a numeric cast between `int` and `float` instead of `float(x)`) results in a **compile-time error**.
* At runtime, `x as T` verifies that the active variant held by `x` matches `T` and extracts the value of type `T` (mapping to `std::get<T>(x)` in C++).
* If `x` does not currently hold a value of type `T` at runtime, the assertion fails and raises a runtime type-assertion error (which may be intercepted with an inline `zonk` recovery clause).

```keet
let payload: int | string = "KeetScript";
let name: string = payload as string;           // unwraps `std::get<std::string>(payload)`
let num: int = (payload as int) zonk 0;         // falls back to 0 if `payload` is not an `int`
```

## Error recovery expressions (`zonk`)

Any fallible expression—such as a call to a function that can fail with an `errtype`, a string-to-number conversion, a union type assertion (`as`), or a bounds check—may be suffixed with an inline `zonk` error-recovery operator to handle failure directly at the expression site:

```ebnf
ZonkSuffix = "zonk" ( LogicalOrExpr | "(" [ Identifier ] ")" Block ) .
```

*(Grammar Disambiguation Note: A scoped recovery `Block` after `zonk` **always** requires parentheses—either `zonk (err) { ... }` to bind the error, or `zonk () { ... }` when the error parameter is omitted. Consequently, `expr zonk {}` is unambiguously parsed as the fallback value form with an empty map literal `{}` (`MapLit`), never conflicting with a recovery `Block`. Furthermore, when a `(` token follows `zonk`, an LL / recursive-descent parser distinguishes between a parenthesized fallback expression (such as `expr zonk (fallbackVal)`) and a scoped recovery block (`expr zonk (err) { ... }` or `expr zonk () { ... }`) by checking if the matching `)` is followed by a `{` block delimiter: if `{` follows `)`, it is parsed as a recovery `Block`, otherwise it is parsed as a parenthesized `LogicalOrExpr` fallback expression.)*

There are two inline forms of `ZonkSuffix`:

1. **Fallback Value Form (`expr zonk fallbackExpr`):**
   If `expr` succeeds, its value is yielded. If `expr` fails at runtime, `fallbackExpr` is evaluated and its value is yielded instead. If `expr` has success type `T` and `fallbackExpr` has type `F`, the combined expression has type `T` (if `F` is assignable to `T`) or `T | F`.
2. **Scoped Recovery Block Form (`expr zonk ( [ err ] ) Block`):**
   If `expr` fails, execution enters `Block`, optionally binding the raised error value to the scoped identifier `err` (whose scope is restricted to `Block` and obeys the no-shadowing rule):
   * **Diverging Control Flow (`never`):** If every execution path through the recovery `Block` terminates with a control-transfer statement (`return`, `break`, `continue`, or a re-raised error via `zonk`), the recovery block is classified as **diverging (type `never`)**. Because control leaves the enclosing scope whenever `expr` fails, the enclosing binding (e.g., `let config: string = readFile(...) zonk () { return; };`) is guaranteed never to receive an uninitialized or invalid value along the failure path. Consequently, the type of the overall `expr zonk (...) Block` expression evaluates strictly to the success type of `expr` (e.g., `string`), without requiring a fallback value or triggering uninitialized variable errors.
   * **Value-Yielding Recovery (`yield`):** If the recovery `Block` does *not* diverge from the enclosing scope, every non-diverging execution path through `Block` must terminate with a `yield expr;` statement (see [Yielding replacement values from `zonk` blocks (`yield`)](#yielding-replacement-values-from-zonk-blocks-yield)), supplying a replacement value whose type is assignable to (or widens into a union with) the target expression type.

```keet
let port: int = int(rawInput) zonk 8080;
let fallbackMap: map<string, int> = loadMap() zonk {}; // unambiguously an empty MapLit fallback

// Value-yielding recovery block (`yield`)
let safePort: int = readPortFromFile("app.conf") zonk (err) {
    io.println(`Warning: ${err.message}; falling back to default port`);
    yield 8080;
};

// Diverging recovery block (type `never`): `config` is guaranteed to be `string` if execution continues
let config: string = readFile("app.conf") zonk (err) {
    io.println(`Failed to load config: ${err.message}`);
    return;
};
```

*(Full semantics for declaring `errtype`, raising errors, and `zonk` / `yield` recovery are specified in the [Error handling](#error-handling) chapter.)*

## Order of evaluation

To ensure predictable behavior across all target C++ compilers (GCC, Clang, MSVC) and avoid C++'s unspecified argument evaluation order pitfalls, KeetScript enforces a **strict left-to-right evaluation order**:

1. **Operands and Sub-Expressions:** Within any binary expression, comparison, index expression `a[i]`, or member call `obj.method(a, b)`, operands and sub-expressions are evaluated strictly from **left to right**. In `a[i]`, `a` is evaluated before `i`. In `obj.method(a, b)`, `obj` is evaluated first, followed by `a`, and then `b`.
2. **Short-Circuiting (`&&`, `||`, `? :`, `zonk`):** The right-hand operand of `&&`, `||`, and `zonk`, as well as the unselected branch of `? :`, is evaluated **only** if required by the outcome of the left-hand condition or expression.
3. **AOT Translation Guarantee:** When an expression contains multiple sub-expressions with potential side effects (such as multiple function calls `f() + g()`), the `keety` compiler sequences them in generated C++ (hoisting temporary evaluations where necessary prior to C++17 or across function argument lists) so that left-to-right execution order is guaranteed regardless of the underlying C++ compiler.


# Statements

Statements control execution.

```ebnf
Statement  = Declaration | SimpleStmt ";" | ReturnStmt | YieldStmt | BreakStmt |
             ContinueStmt | ZonkStmt | Block | IfStmt | SwitchStmt | ForStmt |
             WhileStmt | EmptyStmt .
SimpleStmt = ExpressionStmt | IncDecStmt | AssignStmt .
```

*(See [Blocks](#blocks) for `Block` and `StatementList`. Note: Declarations—`DefineDecl`, `ConstDecl`, `VarDecl`, and `TypeAliasDecl`—already include their own terminating semicolon `;`, while compound block-terminated statements such as `Block`, `IfStmt`, `SwitchStmt`, `ForStmt`, `WhileStmt`, `FunctionDecl`, `ClassDecl`, `EnumDecl`, and `ErrTypeDecl` do not require a trailing semicolon after their closing brace `}`.)*

## Terminating statements

A *terminating statement* prevents execution from continuing to the statement textually following it in the same block (evaluating to the diverging control-flow type `never`). If a [function](#function-declarations) or [method](#method-declarations) has a return type other than `void`, its body [block](#blocks) must end with a terminating statement. Likewise, every execution path through a non-void scoped [`zonk` recovery block](#catching-and-error-recovery-zonk-expressions-and-blocks) must end with a terminating statement (either exiting the enclosing scope or supplying a replacement value via `yield expr;`).

The following statements are terminating:

1. A [`return`](#return-statements) statement or a statement-level [`zonk`](#error-raising-statements-zonk) error-raising statement (`zonk expr;`).
2. A [`yield`](#yielding-replacement-values-from-zonk-blocks-yield) statement (`yield expr;`), which terminates execution of the innermost enclosing `zonk` recovery block and supplies `expr` as the recovered value of the `zonk` expression (note that `yield` terminates the `zonk` block itself, whereas execution of the statement containing the `zonk` expression continues normally).
3. A [`break`](#break-statements) or [`continue`](#continue-statements) statement (which terminates execution within the innermost containing statement list).
4. An [expression statement](#expression-statements), [assignment](#assignments), or [variable/constant declaration](#declarations-and-scope) containing an inline `zonk` recovery block whose recovery block itself ends in a function-exiting terminating statement (`return` or `zonk`), only along the failure branch; the statement as a whole is terminating if and only if its primary expression cannot complete normally.
5. A [block](#blocks) in which the statement list is non-empty and its final statement is a terminating statement.
6. An [`if` statement](#if-statements) in which:
   * an `else` branch is present, and
   * the `if` block, every `elif` block, and the `else` block all end with terminating statements.
7. A [`switch` statement](#switch-statements) in which:
   * a `default` branch is present (or the `case` branches exhaustively cover all enumerators of an `enum` or all constituent types of a union under `typeof`), and
   * every `case` block and the `default` block end with a terminating statement.
8. An unconditional infinite loop (`while (true) { ... }` or `for (;;) { ... }`) whose body contains no `break` statement targeting that loop.

All other statements are not terminating. A statement appearing directly after a terminating statement within the same `StatementList` is unreachable and results in a **compile-time error**.

## Empty statements

The empty statement does nothing. It consists of a standalone semicolon `;` and is useful where the grammar requires a statement but no computation is needed.

```ebnf
EmptyStmt = ";" .
```

## Expression statements

With the exception of pure side-effect-free operations rejected by compiler diagnostics, function and method calls, `await` expressions, and expressions with inline `zonk` recovery may appear in statement context as *expression statements*.

```ebnf
ExpressionStmt = Expression .
```

When executed as a statement (`ExpressionStmt ";"`), the expression is evaluated completely for its side effects and any resulting value is discarded.

```keet
io.println("Processing started");
await flushLogs();
connectToServer() zonk (err) {
    io.println(`Connection failed: ${err.message}`);
};
```

## IncDec statements

The `"++"` and `"--"` statements increment or decrement their operand by the untyped constant value `1`.

```ebnf
IncDecStmt = PrimaryExpr ( "++" | "--" ) .
```

As established in [Increment and decrement](#increment-and-decrement), `x++` and `x--` are **statements, not expressions**; they do not yield a value and cannot be embedded inside larger expressions (such as `y = x++;` or `f(x++);`).

* The operand `PrimaryExpr` must be a mutable (`let`) addressable variable, array element, or class field of type `int` or `float`.
* `x++` is semantically equivalent to the compound assignment `x += 1` (for `int`) or `x += 1.0` (for `float`), except that any side effects in `PrimaryExpr` (such as an array index `arr[i++]`—which is disallowed—or `arr[f()]++`) are evaluated only once.
* `x--` is semantically equivalent to `x -= 1` (for `int`) or `x -= 1.0` (for `float`).

```keet
let counter: int = 0;
counter++;
counter--;
```

## Assignments

An *assignment* replaces the current value stored in a mutable variable, addressable collection element, or mutable class field with a new value specified by an expression.

```ebnf
AssignStmt = ( PrimaryExpr | BlankIdent ) assign_op Expression .
assign_op  = "=" | "+=" | "-=" | "*=" | "/=" | "%=" .
```

KeetScript supports **single-target assignments only**; tuple or multi-variable unpacking assignments (such as `a, b = 1, 2;`) are not part of the grammar.

* **Addressability & Mutability:** Except when the target is the [blank identifier](#blank-identifier) `_`, the left-hand `PrimaryExpr` must denote a mutable storage location: a variable declared with `let`, a mutable reference parameter declared with `mut`, an element of a mutable `array<T>`, `string`, or `map<K, V>` (`a[i]`), or a mutable (`let`) field of a mutable `class` instance (`obj.field` or `this.field` inside a non-`const` method). Assigning to a `define` or `const` binding, a non-`mut` function parameter, or a field of a `const` receiver (`const fn`) is a **compile-time error**.
* **Simple Assignment (`=`):** In `x = expr`, the right-hand `expr` must be [assignable](#assignability) to the declared type of `x`. Due to KeetScript's [value semantics](#representation-of-values), assigning a composite value (`array`, `map`, `class`, or `string`) copies the value deeply into `x`.
* **Compound Assignment (`+=`, `-=`, `*=`, `/=`, `%=`):** A compound assignment `x op= y` is semantically equivalent to `x = x op (y)`, except that `x` (including any selector or index sub-expressions within `x`) is evaluated **only once**:
  * `+=`, `-=`, `*=`, and `/=` apply to `int` and `float` operands of identical type.
  * `+=` additionally applies to `string` targets when the right-hand operand is of type `string` or `char`.
  * `%=` applies **exclusively** to `int` operands.
  * **Compound Assignment on Map Index Targets (`map[k] op= expr`):** Although reading `m[k]` in a value expression returns `V | null`, applying a compound assignment (`+=`, `-=`, `*=`, `/=`, `%=`) to a mutable `map<K, V>` element target `m[k]` (where `V` is `int`, `float`, or `string`) is defined with **default-initialized fallback semantics**: if key `k` already exists in `m`, `op=` updates the existing value in place; if key `k` is absent, `m[k]` is implicitly initialized to the zero/empty value of `V` (`0` for `int`, `0.0` for `float`, `""` for `string`) before applying `op=` (translating directly to `m[k] += expr` on C++ `std::unordered_map<K, V>`).
* **Discarding Values via the Blank Identifier (`_ = expr;`):** The blank identifier `_` may appear on the left-hand side of a simple assignment (`=`) to explicitly evaluate `expr` for its side effects while discarding its return value without introducing a variable binding or triggering unused-result compiler warnings. Using a compound assignment operator with `_` (such as `_ += 1;`) is a **compile-time error**.

```keet
let total: int = 10;
total += 5;                 // total is 15

let greeting: string = "Hello";
greeting += '!';            // greeting is "Hello!"

let counts: map<string, int> = {"a": 1};
counts["a"] += 1;           // key "a" exists -> updates value to 2
counts["b"] += 1;           // key "b" absent -> default-initializes to 0, then adds 1 -> "b": 1

_ = computeAndCacheResult(); // explicitly discards return value
```

## `if` statements

`if` statements specify the conditional execution of one or more branches according to the value of a boolean expression. If the initial condition evaluates to `false`, any subsequent `elif` branches are tested in order; if all conditions evaluate to `false`, the `else` branch (if present) is executed.

```ebnf
IfStmt       = "if" "(" [ IfHeaderInit ] Expression ")" Block
               { "elif" "(" [ IfHeaderInit ] Expression ")" Block }
               [ "else" Block ] .
IfHeaderInit = VarDecl | ConstDecl | SimpleStmt ";" .
```

*(Note: In `IfHeaderInit`, `VarDecl` and `ConstDecl` already include their own terminating semicolon `;`.)*

* **Mandatory Parentheses and Braces:** Every `if` and `elif` condition must be enclosed in parentheses `(...)`, and every `if`, `elif`, and `else` body must be an explicit brace-delimited `Block` `{ ... }`. Braceless single-statement branches are disallowed, eliminating dangling-`else` parsing ambiguities.
* **Boolean Condition Requirement:** The condition `Expression` must evaluate strictly to type `bool`.
* **Short Initialization Statement & Lexical Scope:** An `if` or `elif` condition may be preceded by an optional initialization statement (`IfHeaderInit`, such as `let v: int | null = lookup();`). As specified in [Blocks](#blocks), each `if` statement introduces an implicit lexical block: any variable or constant declared in an `IfHeaderInit` is scoped strictly to that `if`/`elif` branch and any subsequent `elif` or `else` branches of the same `IfStmt` (while obeying the strict no-shadowing rule against enclosing outer scopes).
* **Control-Flow Type Narrowing:** If the condition of an `if` or `elif` branch statically constrains the active type of a union variable `x`—such as via `typeof(x) == "T"` or `x != null`—the compiler narrows the type of `x` within that branch's `Block` (and narrows `x` to the remaining union complement in the `else` block), provided `x` is not reassigned within the block.

```keet
if (let raw: string | null = fetchHeader("Auth"); raw != null) {
    // `raw` is scoped to this IfStmt and narrowed from `string | null` to `string`
    io.println(`Token length: ${raw.length()}`);
} elif (allowAnonymous) {
    io.println("Proceeding anonymously");
} else {
    io.println("Unauthorized");
}
```

## `switch` statements

`switch` statements provide multi-way execution by comparing an operand expression against one or more `case` clauses.

```ebnf
SwitchStmt    = "switch" "(" [ IfHeaderInit ] Expression ")" [ "as" Identifier ] "{" { CaseClause } [ DefaultClause ] "}" .
CaseClause    = "case" "(" ExpressionList ")" Block .
DefaultClause = "default" Block .
```

* **Universal Equality Matching:** The switched `Expression` may evaluate to any type that supports equality comparison (`==`): `int`, `float`, `char`, `string`, `bool`, `enum`, `null`, `array<T>`, `map<K, V>`, or a union of comparable types.
* **Top-to-Bottom Evaluation & Multiple Match Values:** When a `switch` statement executes, the switched expression is evaluated once, and the `case` clauses are evaluated in source order from top to bottom. A single `case` clause may specify multiple comma-separated match expressions (`case (expr1, expr2, ...) { ... }`), where each match expression must be comparable with the switched expression using `==`. The first `case` clause whose `ExpressionList` contains an expression equal (`==`) to the switched value is selected, and its `Block` is executed.
* **Optional `default` Clause:** At most one `default` clause may appear in a `switch` statement. If no `case` matches and a `default` clause is present, the `default` `Block` is executed.
* **Block-Delimited Cases (No Fallthrough):** Every `case` and `default` branch is a self-contained, brace-delimited `Block` `{ ... }` with its own lexical scope. Once the selected `case` or `default` block finishes executing, control exits the `switch` statement automatically—**there is no fallthrough** to subsequent cases.
* **`break` Semantics Inside `switch`:** Because `case` blocks never fall through, an unlabeled `break;` is neither required nor used to terminate a `case` block. Instead, an unlabeled `break;` inside a `case` or `default` block targets the innermost enclosing `for` or `while` loop. Using an unlabeled `break;` inside a `switch` that is not enclosed within a loop is a **compile-time error**.
* **Named `switch` and Early Case Exit (`as Identifier`):** When early exit from a `case` block without breaking an enclosing loop is required, the `switch` statement may declare a name using `as Identifier` (e.g., `switch (code) as matchCode { ... }`). Executing `break matchCode;` immediately exits the `switch` statement (terminating the active `case` or `default` block) and proceeds to the statement following the `switch`, without terminating any enclosing loop.
* **Union Type Narrowing via `switch (typeof(x))`:** When the switched expression is `typeof(x)` for a union variable `x` (e.g., `let x: int | string | null;`), each `case ("T") { ... }` branch statically narrows `x` to the corresponding constituent type `T` throughout that `case` block (identical to `if (typeof(x) == "T")`), provided `x` is not reassigned inside the block.

```keet
switch (statusCode) as handleStatus {
    case (200, 201, 204) {
        let msg: string = "Success";
        io.println(msg);
        break handleStatus; // exits switch immediately
    }
    case (400, 404) {
        io.println("Client error");
    }
    default {
        io.println("Server or unknown status");
    }
}

let payload: int | string | null = getPayload();
switch (typeof(payload)) {
    case ("int") {
        // `payload` is statically narrowed to `int`
        io.println(`Integer doubled: ${payload * 2}`);
    }
    case ("string") {
        // `payload` is statically narrowed to `string`
        io.println(`String length: ${payload.length()}`);
    }
    default {
        io.println("Payload is null");
    }
}
```

## Loops (`while` and `for`)

KeetScript provides two loop constructs for repeated execution: condition-driven `while` loops and `for` loops (supporting both classic 3-clause C-style iteration and `for ... in` collection iteration).

* **Mandatory Header Parentheses and Body Braces:** All loop headers must be enclosed in parentheses `(...)`, and all loop bodies must be explicit brace-delimited `Block`s `{ ... }`.
* **Named Loop Syntax (`as Identifier`):** Rather than using C-style `label:` prefix syntax (which resembles `goto` labels), any `while` or `for` loop may be given an explicit loop name by placing `as Identifier` immediately after the closing parenthesis `)` of the loop header and before the opening brace `{` of the body `Block`. The loop name is scoped strictly to the body of that loop (cannot shadow an enclosing loop name) and may be targeted by [`break`](#break-statements) and [`continue`](#continue-statements) statements inside nested loops or `switch` blocks.

### `while` statements

A `while` statement repeatedly executes its body `Block` as long as its boolean condition `Expression` evaluates to `true`.

```ebnf
WhileStmt = "while" "(" Expression ")" [ "as" Identifier ] Block .
```

The `Expression` must be of type `bool` and is evaluated before each iteration of `Block`. If the expression evaluates to `false` on the first check, `Block` is not executed.

```keet
let attempts: int = 0;
while (attempts < 3) as retryLoop {
    if (tryConnect()) {
        break retryLoop;
    }
    attempts++;
}
```

### `for` statements

A `for` statement specifies repeated execution of a `Block` controlled either by a classic three-part loop clause (`ClassicForClause`) or by a collection iteration clause (`ForInClause`).

```ebnf
ForStmt = "for" "(" ( ClassicForClause | ForInClause ) ")" [ "as" Identifier ] Block .
```

#### Classic 3-clause `for` loops

```ebnf
ClassicForClause = ForInit [ Expression ] ";" [ ForPost ] .
ForInit          = VarDecl | [ SimpleStmt ] ";" .
ForPost          = SimpleStmt .
```

*(Note: Because `VarDecl` already includes its own terminating `;`, `ForInit` always terminates with a semicolon separator before the condition expression.)*

1. **Initialization (`ForInit`):** Executed once before the first evaluation of the condition. Variables declared via `let` in `ForInit` are scoped to the implicit block of the `for` statement (visible in the condition, post statement, and body `Block`, while obeying the no-shadowing rule).
2. **Condition (`Expression`):** Evaluated at the beginning of each iteration. If present, it must have type `bool`; if omitted, it is equivalent to `true`. When the condition evaluates to `false`, the loop terminates.
3. **Post Statement (`ForPost`):** Executed after each execution of the loop body `Block` (and whenever a `continue` statement jumps to the end of the current iteration), before re-evaluating the condition.

```keet
for (let i: int = 0; i < 10; i++) {
    io.println(i.toString());
}
```

#### Collection iteration (`for ... in`)

A `for ... in` clause iterates over the elements of an `array<T>`, the characters (UTF-8 byte code units) of a `string`, or the keys (and values) of a `map<K, V>`.

```ebnf
ForInClause  = ForInBinding "in" Expression .
ForInBinding = ( "let" | "const" ) ( Identifier | BlankIdent ) ":" TypeAnnotation
               [ "," ( Identifier | BlankIdent ) ":" TypeAnnotation ] |
               ( Identifier | BlankIdent ) [ "," ( Identifier | BlankIdent ) ] .
```

* **Explicit Typing for New Loop Bindings:** When declaring new loop variables with `let` (mutable copy per iteration) or `const` (read-only copy per iteration) in the `ForInBinding`, **explicit type annotations are mandatory** on every declared variable:
  * **Array (`array<T>`) or String (`string`) — Value Only (1 binding):**
    `for (let item: T in arrayExpr) Block` (or `let ch: char in stringExpr`). Bound to each element in index order from `0` to `length() - 1`.
  * **Array (`array<T>`) or String (`string`) — Index and Value (2 bindings):**
    `for (let i: int, item: T in arrayExpr) Block`. The first binding must have type `int` (receiving the 0-based index) and the second binding receives the element of type `T` (or `char` for `string`).
  * **Map (`map<K, V>`) — Key Only (1 binding):**
    `for (let k: K in mapExpr) Block`. Bound to each key in the map.
  * **Map (`map<K, V>`) — Key and Value (2 bindings):**
    `for (let k: K, v: V in mapExpr) Block`. The first binding receives the key of type `K` and the second binding receives the associated value of type `V`.
* **Reusing Pre-Declared Variables:** If the loop uses existing mutable variables already declared in an enclosing scope, `let`/`const` and the `: TypeAnnotation` are omitted: `for (item in arrayExpr) Block` or `for (k, v in mapExpr) Block`. The existing variables must be assignable from the collection's element or key/value types.
* **Blank Identifier (`_`):** Either binding position may use `_` (e.g., `for (let _: string, val: int in scores)`) to discard the index/key or value without introducing a name into the loop scope.
* **Value Semantics on Iteration:** The collection `Expression` is evaluated once before the loop begins. On each iteration, the loop variable(s) receive an **independent value copy** of the current element or key/value pair (consistent with [Representation of values](#representation-of-values)). Mutating a `let` loop variable inside `Block` modifies only that local iteration copy and **never** mutates the underlying `array`, `string`, or `map`.
  *(Implementation note: In generated C++, if a `for ... in` binding is declared `const` or is a `let` binding that is not mutated inside `Block`, the `keety` compiler transparently emits `for (const auto& ...)` to avoid unnecessary deep copies while preserving strict value semantics.)*

```keet
let names: array<string> = ["Ada", "Grace", "Alan"];

// Array value-only iteration
for (const name: string in names) {
    io.println(name);
}

// Array index + value iteration
for (let idx: int, name: string in names) {
    io.println(`${idx}: ${name}`);
}

// Map key + value iteration
let ports: map<string, int> = {"http": 80, "https": 443};
for (const proto: string, port: int in ports) {
    io.println(`${proto} -> ${port}`);
}

// Reusing a pre-declared outer variable
let currentName: string = "";
for (currentName in names) {
    io.println(currentName);
}
```

## `return` statements

A `return` statement in a function or method `F` terminates the execution of `F` and optionally provides the result value to the caller.

```ebnf
ReturnStmt = "return" [ Expression ] ";" .
```

* **Void Functions (`: void`):** If `F` has declared return type `void` (or is a `class` constructor), the `return` statement **must not** specify an `Expression` (`return;`).
* **Value-Returning Functions (`: T`):** If `F` has any declared return type `T` other than `void`, every `return` statement in `F` **must** specify an `Expression` whose type is [assignable](#assignability) to `T` (`return expr;`).
* **Lexical Binding:** A `return` statement inside a [`FunctionLit`](#function-literals-functionlit-and-closure-captures) returns from that innermost anonymous function, whereas a `return` statement inside a scoped [`zonk` recovery block](#error-recovery-expressions-zonk) (`expr zonk (err) { return ...; }`) returns from the enclosing function or method.

```keet
fn logIfEmpty(msg: string): void {
    if (msg.length() == 0) {
        return;
    }
    io.println(msg);
}

fn clamp(val: int, minVal: int, maxVal: int): int {
    if (val < minVal) {
        return minVal;
    }
    if (val > maxVal) {
        return maxVal;
    }
    return val;
}
```

## `break` statements

A `break` statement terminates execution of the innermost enclosing `for` or `while` loop, or of a specific enclosing loop or `switch` statement named via `as Identifier`.

```ebnf
BreakStmt = "break" [ Identifier ] ";" .
```

* **Unlabeled `break;`:** Terminates the innermost enclosing `for` or `while` statement within the same function. As noted in [`switch` statements](#switch-statements), an unlabeled `break;` does **not** bind to a `switch` statement; an unlabeled `break;` inside a `switch` exits the enclosing loop.
* **Named `break Identifier;`:** If an `Identifier` is specified, it must match the `as Identifier` name of an enclosing `for`, `while`, or `switch` statement within the current function:
  * When targeting an enclosing loop (`for` or `while`), execution immediately exits that named loop.
  * When targeting an enclosing `switch` statement (`switch (...) as switchName`), execution immediately exits the `switch` statement (terminating the active `case` or `default` block) and transfers control to the statement following the `switch`, without terminating any enclosing loop.
* Using `break` outside of any loop or named construct (or referencing a name that does not enclose the `break` statement) is a **compile-time error**.

## `continue` statements

A `continue` statement begins the next iteration of the innermost enclosing `for` or `while` loop, or of a specific enclosing loop named via `as Identifier`, by skipping the remainder of the loop body.

```ebnf
ContinueStmt = "continue" [ Identifier ] ";" .
```

* **Unlabeled `continue;`:** Jumps to the end of the current iteration of the innermost enclosing `for` or `while` loop (executing `ForPost` in a classic `for` loop, or advancing to the next element in a `for ... in` loop, before re-evaluating the loop condition).
* **Named `continue Identifier;`:** If an `Identifier` is specified, it must match the `as Identifier` name of an enclosing `for` or `while` loop within the current function, advancing that named outer loop to its next iteration.
* Using `continue` outside of any loop (or referencing a loop name that does not enclose the `continue` statement) is a **compile-time error**.

```keet
let grid: array<array<int>> = [[1, 2, 3], [4, -1, 6], [7, 8, 9]];

for (let r: int, row: array<int> in grid) as outerLoop {
    for (let c: int, cell: int in row) as innerLoop {
        if (cell < 0) {
            // Skip the rest of `outerLoop`'s current row and continue with the next row
            continue outerLoop;
        }
        if (cell == 8) {
            // Exit `outerLoop` completely
            break outerLoop;
        }
        io.println(`grid[${r}][${c}] = ${cell}`);
    }
}
```

## Error-raising statements (`zonk`)

In addition to inline [error recovery expressions](#error-recovery-expressions-zonk) (`expr zonk ...`), the `zonk` keyword may be used as a standalone statement to raise a runtime error and unwind execution from the current function or block.

```ebnf
ZonkStmt = "zonk" Expression ";" .
```

* **Error Operand:** The `Expression` in `zonk Expression ";"` must evaluate to either an instance of a declared [`errtype`](#error-types-errtype) or a `string` error message.
* **Control-Flow Unwinding & Divergence (`never`):** Executing `zonk expr;` immediately aborts normal execution of the current block and propagates the error upward to the nearest enclosing `zonk` recovery handler (or terminates the task/program with a diagnostic if unhandled). Because control never continues past `zonk expr;`, `ZonkStmt` is a [terminating statement](#terminating-statements) of type `never`.

```keet
fn divide(a: int, b: int): int {
    if (b == 0) {
        zonk "division by zero";
    }
    return a / b;
}
```

*(Full rules for declaring structured error types with `errtype`, propagating errors across call boundaries, and matching error types in `zonk` blocks are detailed in the Error Handling chapter.)*


# Built-in functions and methods

KeetScript provides a minimal set of global universe built-ins alongside a rich, zero-overhead set of built-in methods on predeclared scalar, string, and collection types (`int`, `float`, `bool`, `char`, `string`, `array<T>`, and `map<K, V>`).

Unlike dynamically typed languages that box primitives or attach runtime prototype chains, KeetScript resolves all built-in methods statically at compile time and translates them directly into inline C++17 expressions, standard library algorithms (`<algorithm>`, `<cmath>`, `<string>`, `<vector>`, `<unordered_map>`), or lightweight runtime helpers.

## Universe built-in functions and operators

To keep the global namespace clean and prevent accidental identifier collisions, the [universe block](#predeclared-identifiers) declares **only** predeclared types, boolean/null constants (`true`, `false`, `null`), the `typeof` type-inspection operator, and functional scalar conversions. All I/O, filesystem, networking, mathematical constants, and operating-system interactions require explicit module imports (such as `import io;`, `import sys;`, or `import math;`).

### Type inspection (`typeof`)

```keet
typeof(x: any): string
```

The built-in `typeof` operator evaluates its operand `x` and returns a runtime `string` identifying the active value's type:

* **Primitive, String, and Special Types:** `"bool"`, `"int"`, `"float"`, `"char"`, `"string"`, or `"null"`.
* **Collection and Callable Types:** `"array"`, `"map"`, or `"function"`.
* **Nominal User-Defined and Error Types (`class`, `enum`, `errtype`, `error`):** The exact declared identifier of the `class`, `enum`, or `errtype` (e.g., `"Counter"`, `"Status"`, or `"FileError"`), or `"Error"` for a plain string-raised error.

When `x` is a variable of a [union type](#union-and-literal-types) or the root [`error`](#the-predeclared-error-type) type, comparing `typeof(x)` against a type name literal in a conditional expression (`if (typeof(x) == "string")`) or switching on `typeof(x)` (`switch (typeof(x))`) statically narrows the type of `x` within the guarded branch.

### Functional cast expressions

As specified in [Explicit type conversions](#explicit-type-conversions-tox-and-tx), predeclared scalar and string type names may be invoked using functional call syntax `T(x)` as an exact equivalent to calling the corresponding `.toT()` built-in method on `x`:

| Functional Cast | Equivalent Method | Operand Types `x` | Result Type | Behavior & Fallibility |
| :--- | :--- | :--- | :--- | :--- |
| `int(x)` | `x.toInt()` | `int`, `float`, `char`, `bool`, `enum`, `string` | `int` | Truncates `float` towards zero (**fallible** via `zonk` if `x` is `NaN`, `±Infinity`, or outside `[INT64_MIN, INT64_MAX]`); converts `char` (`0`–`255`), `bool` (`1` or `0`), or `enum` underlying integer; parses decimal `string` (**fallible** via `zonk`). |
| `float(x)` | `x.toFloat()` | `float`, `int`, `string` | `float` | Widens 64-bit `int` to IEEE 754 `double` (`static_cast<double>`); parses floating-point `string` (**fallible** via `zonk`). |
| `char(x)` | `x.toChar()` | `char`, `int` | `char` | Converts an `int` byte value in `[0, 255]` to `char` (**fallible** via `zonk` if `x < 0` or `x > 255`). |
| `string(x)` | `x.toString()` | `string`, `int`, `float`, `bool`, `char` | `string` | Converts scalar and boolean values to their canonical textual `string` representation. |
| `bool(x)` | `x.toBool()` | `bool`, `int`, `float` | `bool` | Returns `true` if `int != 0` or `float != 0.0` (and not NaN), or `false` otherwise. |

```keet
let rawPort: string = "8080";
let port: int = int(rawPort) zonk 80;
let ratio: float = float(port) / 100.0;
let asciiA: char = char(65) zonk '?';
```

## Built-in methods on primitive scalar types

All built-in methods on `int`, `float`, `bool`, and `char` are **non-mutating (`const fn`)** and may be called on literals, constants (`define`, `const`), mutable variables (`let`), and temporary expressions.

### `int` methods

| Method Signature | Description |
| :--- | :--- |
| `const fn toFloat(): float` | Widens the 64-bit signed integer to an IEEE 754 64-bit `float` (`static_cast<double>`). |
| `const fn toString(): string` | Returns the signed base-10 textual representation of the integer (`std::to_string`). |
| `const fn toChar(): char` | Converts an integer in the range `0 <= x <= 255` to a single-byte `char`. Raises a runtime error (recoverable via `zonk`) if out of range. |
| `const fn toBool(): bool` | Returns `true` if the integer is non-zero (`x != 0`), or `false` if `0`. |
| `const fn toBinary(): string` | Returns the unsigned base-2 binary digits (without a `"0b"` prefix, e.g., `(10).toBinary()` yields `"1010"`). |
| `const fn toOctal(): string` | Returns the base-8 octal representation (without a `"0o"` prefix). |
| `const fn toHex(): string` | Returns the lowercase base-16 hexadecimal representation (without a `"0x"` prefix, e.g., `(255).toHex()` yields `"ff"`). |
| `const fn abs(): int` | Returns the absolute value of the integer. For `INT64_MIN` (`-9223372036854775808`), wraps under two's-complement semantics to `INT64_MIN` (eliminating `std::abs(INT64_MIN)` C++ undefined behavior). |
| `const fn clamp(minVal: int, maxVal: int): int` | Clamps the integer to the inclusive range `[minVal, maxVal]` (`std::clamp`). |

### `float` methods

| Method Signature | Description |
| :--- | :--- |
| `const fn toInt(): int` | Truncates the fractional part towards zero. **Fallible (Zero-UB):** raises a runtime error (recoverable via `zonk`) if the value is `NaN`, `±Infinity`, or outside `[INT64_MIN, INT64_MAX]`. |
| `const fn toString(): string` | Returns the default decimal string representation of the floating-point value. |
| `const fn toFixed(digits: int): string` | Formats the number using fixed-point notation with `digits` digits after the decimal point (`0 <= digits <= 20`, e.g., `3.14159.toFixed(2)` yields `"3.14"`). |
| `const fn toBool(): bool` | Returns `true` if the value is non-zero (`x != 0.0`) and not `NaN`, or `false` otherwise. |
| `const fn round(): int` | Rounds to the nearest integer (halfway cases away from zero) and returns an `int`. **Fallible:** raises a runtime error (recoverable via `zonk`) if `NaN`, `±Infinity`, or out of 64-bit `int` bounds. |
| `const fn floor(): int` | Returns the largest `int` less than or equal to the value. **Fallible:** raises a runtime error (recoverable via `zonk`) if `NaN`, `±Infinity`, or out of 64-bit `int` bounds. |
| `const fn ceil(): int` | Returns the smallest `int` greater than or equal to the value. **Fallible:** raises a runtime error (recoverable via `zonk`) if `NaN`, `±Infinity`, or out of 64-bit `int` bounds. |
| `const fn abs(): float` | Returns the floating-point absolute value (`std::fabs`). |
| `const fn clamp(minVal: float, maxVal: float): float` | Clamps the value to the inclusive range `[minVal, maxVal]` (`std::clamp`). |
| `const fn isNaN(): bool` | Returns `true` if the value is an IEEE 754 Not-a-Number (`std::isnan`). |
| `const fn isInfinite(): bool` | Returns `true` if the value is positive or negative infinity (`std::isinf`). |

### `bool` methods

| Method Signature | Description |
| :--- | :--- |
| `const fn toString(): string` | Returns `"true"` if `true`, or `"false"` if `false`. |
| `const fn toInt(): int` | Returns `1` if `true`, or `0` if `false`. |

### `char` methods

| Method Signature | Description |
| :--- | :--- |
| `const fn toString(): string` | Constructs a 1-byte `string` containing this character (`std::string(1, c)`). |
| `const fn toInt(): int` | Returns the unsigned 8-bit ASCII/UTF-8 byte value (`0` to `255`, `static_cast<int64_t>(static_cast<uint8_t>(c))`). |
| `const fn isDigit(): bool` | Returns `true` if the character is an ASCII decimal digit (`'0'` through `'9'`). |
| `const fn isAlpha(): bool` | Returns `true` if the character is an ASCII alphabetic letter (`'a'`–`'z'` or `'A'`–`'Z'`). |
| `const fn isWhitespace(): bool` | Returns `true` if the character is an ASCII whitespace character (`' '`, `'\t'`, `'\n'`, `'\r'`, `'\v'`, `'\f'`). |
| `const fn upper(): char` | Returns the ASCII uppercase equivalent if `'a'`–`'z'`, or the character unchanged otherwise. |
| `const fn lower(): char` | Returns the ASCII lowercase equivalent if `'A'`–`'Z'`, or the character unchanged otherwise. |

```keet
let delta: int = -42;
let bounded: int = delta.abs().clamp(0, 100); // 42
let hexStr: string = (255).toHex();           // "ff"

let price: float = 19.995;
let formatted: string = price.toFixed(2);     // "20.00"
let rounded: int = price.round();             // 20

let ch: char = 'k';
let upperCh: char = ch.upper();               // 'K'
```

## Built-in methods on `string`

In KeetScript, a `string` (mapping to C++ `std::string`) is **mutable** when bound via `let` and **immutable** when bound via `const` or `define`.

To ensure uniform ergonomics across both mutable and immutable strings, **all built-in `string` methods are non-mutating (`const fn`)**: transformation methods always allocate and return a new `string` without altering the receiver. Direct in-place mutation of a mutable `let` string is performed via byte index assignment (`s[i] = 'A';`) or compound concatenation (`s += " suffix";`).

All string indices and lengths operate on 0-based raw UTF-8 code units (bytes).

### String inspection and search (`const fn`)

| Method Signature | Description |
| :--- | :--- |
| `const fn length(): int` | Returns the number of UTF-8 bytes in the string (`static_cast<int64_t>(s.size())`). |
| `const fn isEmpty(): bool` | Returns `true` if `s.length() == 0` (`s.empty()`). |
| `const fn contains(sub: string): bool` | Returns `true` if `sub` occurs anywhere within the string (`s.find(sub) != std::string::npos`). |
| `const fn containsChar(ch: char): bool` | Returns `true` if the byte `ch` occurs within the string (`s.find(ch) != std::string::npos`). |
| `const fn startsWith(prefix: string): bool` | Returns `true` if the string begins with `prefix`. |
| `const fn endsWith(suffix: string): bool` | Returns `true` if the string ends with `suffix`. |
| `const fn indexOf(sub: string): int` | Returns the 0-based byte offset of the first occurrence of `sub`, or `-1` if `sub` is not found. |

### String transformations (`const fn`)

| Method Signature | Description |
| :--- | :--- |
| `const fn slice(start: int, end: int): string` | Returns a new substring containing bytes from index `start` (inclusive) up to `end` (exclusive). Indices are clamped to `[0, s.length()]`; if `start >= end`, returns `""`. |
| `const fn upper(): string` | Returns a new string with all ASCII lowercase letters converted to uppercase. |
| `const fn lower(): string` | Returns a new string with all ASCII uppercase letters converted to lowercase. |
| `const fn trim(): string` | Returns a new string with leading and trailing ASCII whitespace removed. |
| `const fn replace(oldSub: string, newSub: string): string` | Returns a new string with the **first** occurrence of `oldSub` replaced by `newSub`. |
| `const fn replaceAll(oldSub: string, newSub: string): string` | Returns a new string with **all** non-overlapping occurrences of `oldSub` replaced by `newSub`. |
| `const fn repeat(count: int): string` | Returns a new string consisting of the receiver repeated `count` times (`count <= 0` returns `""`). |
| `const fn lines(): array<string>` | Splits the string into an array of lines, normalizing both `\n` and `\r\n` line breaks. |
| `const fn split(sep: string): array<string>` | Splits the string around occurrences of delimiter `sep` into a newly allocated `array<string>`. |
| `const fn padStart(targetLength: int, padChar: char = ' '): string` | Pads the beginning of the string with `padChar` until its byte length reaches at least `targetLength`. |
| `const fn padEnd(targetLength: int, padChar: char = ' '): string` | Pads the end of the string with `padChar` until its byte length reaches at least `targetLength`. |

### String parsing (`const fn` — fallible `toX` and non-throwing `tryX`)

While C++ zero-cost exceptions incur zero overhead on the non-error path, throwing and catching exceptions inside performance-critical hot loops where malformed inputs are common (such as high-throughput CSV or log ingestion) incurs stack-unwinding overhead. Accordingly, `string` provides both **`zonk`-integrated fallible parsers** (`.toInt()`, `.toFloat()`) and **zero-unwinding nullable parsers** (`.tryInt()`, `.tryFloat()`):

| Method Signature | Description |
| :--- | :--- |
| `const fn toInt(): int` | Parses the string (trimmed of leading/trailing ASCII whitespace and an optional leading `'+'`) as a signed 64-bit integer. **Fallible:** raises a runtime parsing error (handled via `zonk`) if the string contains invalid characters or overflows 64-bit bounds. |
| `const fn toFloat(): float` | Parses the string (trimmed of leading/trailing ASCII whitespace and an optional leading `'+'`) as an IEEE 754 64-bit `float`. **Fallible:** raises a runtime parsing error (handled via `zonk`) if malformed or out of range. |
| `const fn tryInt(): int \| null` | Non-throwing, zero-allocation integer parser: returns the parsed 64-bit `int` on success, or `null` if the string is malformed or out of range (never raises a `zonk` error). |
| `const fn tryFloat(): float \| null` | Non-throwing, zero-allocation floating-point parser: returns the parsed 64-bit `float` on success, or `null` if malformed or out of range (never raises a `zonk` error). |

*(C++17 implementation note: `.tryInt()` lowers directly to non-allocating `std::from_chars` from `<charconv>`, and `.tryFloat()` uses `std::from_chars` or the bundled `fast_float` parser to ensure uniform, locale-independent C++17 support across all GCC `libstdc++`, Clang `libc++`, and MSVC toolchains. Because standard `std::from_chars` rejects a leading `'+'` sign, the runtime helper advances the start pointer past leading ASCII whitespace and a single optional `'+'` character prior to invoking `from_chars`.)*

```keet
const rawHeader: string = "  Content-Length: 1024 \r\n";
let parts: array<string> = rawHeader.trim().split(":");
let key: string = parts[0].lower();                       // "content-length"
let contentLen: int = parts[1].trim().toInt() zonk 0;     // 1024 (concise `zonk` recovery)

// High-frequency hot-loop parsing without exception unwinding:
if (let parsed: int | null = parts[1].trim().tryInt(); parsed != null) {
    io.println(`Fast-parsed length: ${parsed}`);
}
```

## Built-in methods on `array<T>`

An `array<T>` maps to C++ `std::vector<T>`. Built-in array methods are strictly divided by receiver mutability:
* **Non-mutating methods (`const fn`)** do not modify the receiver array and may be called on both `const` and `let` arrays.
* **Mutating methods (`fn`)** modify the underlying `std::vector<T>` in place and may **only** be called on mutable `let` arrays (calling a mutating method on a `const` array is a compile-time error).

### Non-mutating inspection and functional methods (`const fn`)

| Method Signature | Description |
| :--- | :--- |
| `const fn length(): int` | Returns the number of elements in the array (`static_cast<int64_t>(vec.size())`). |
| `const fn isEmpty(): bool` | Returns `true` if the array contains no elements (`vec.empty()`). |
| `const fn has(val: T): bool` | Returns `true` if any element in the array is equal to `val` under `==` (`std::find`). Requires `T` to support `==`. |
| `const fn indexOf(val: T): int` | Returns the 0-based index of the first element equal to `val` (`==`), or `-1` if not found. |
| `const fn slice(start: int, end: int): array<T>` | Returns a new `array<T>` containing deep-copied elements from index `start` (inclusive) to `end` (exclusive), clamped to `[0, length()]`. |
| `const fn join(sep: string): string` | Concatenates all elements into a single `string` separated by `sep` (available when `T` is `string`, `int`, `float`, `bool`, or `char`). |
| `const fn first(): T` | Returns a copy of the first element (`a[0]`). **Fallible:** raises a runtime bounds error (recoverable via `zonk`) if the array is empty. |
| `const fn last(): T` | Returns a copy of the last element (`a[a.length() - 1]`). **Fallible:** raises a runtime bounds error (recoverable via `zonk`) if the array is empty. |
| `const fn sorted(): array<T>` | Returns a newly allocated copy of the array sorted in ascending order using `<` (`std::sort`). Requires `T` to be an ordered type (`int`, `float`, `char`, `string`). |
| `const fn sortedWith(cmp: (a: T, b: T) => bool): array<T>` | Returns a newly allocated copy of the array sorted according to the strict weak ordering comparator `cmp(a, b)` (where `cmp(a, b)` returns `true` if `a` precedes `b`). |
| `const fn find(predicate: (x: T) => bool): T \| null` | Evaluates `predicate` from left to right and returns the first matching element, or `null` if no element satisfies `predicate`. |
| `const fn findIndex(predicate: (x: T) => bool): int` | Returns the 0-based index of the first element satisfying `predicate`, or `-1` if none matches. |
| `const fn map<U>(transform: (x: T) => U): array<U>` | Applies `transform` to each element in order and returns a new `array<U>` of the results. |
| `const fn filter(predicate: (x: T) => bool): array<T>` | Returns a new `array<T>` containing copies of all elements for which `predicate` returns `true`. |
| `const fn reduce<U>(initial: U, accumulator: (acc: U, item: T) => U): U` | Folds the array from left to right starting with `initial`, returning the final accumulated value of type `U`. |
| `const fn any(predicate: (x: T) => bool): bool` | Returns `true` (short-circuiting) if at least one element satisfies `predicate` (`std::any_of`), or `false` if empty/none match. |
| `const fn all(predicate: (x: T) => bool): bool` | Returns `true` (short-circuiting) if every element satisfies `predicate` (`std::all_of`), or `true` if the array is empty. |

### Mutating array methods (`fn` — requires mutable `let` receiver)

| Method Signature | Description |
| :--- | :--- |
| `fn push(val: T): void` | Appends a copy of `val` to the end of the array (`vec.push_back(val)`). |
| `fn pop(): T` | Removes and returns the last element of the array. **Fallible:** raises a runtime error (recoverable via `zonk`) if the array is empty. |
| `fn insert(index: int, val: T): void` | Inserts `val` at `0 <= index <= length()`, shifting subsequent elements right. Raises a runtime bounds error if `index` is out of range. |
| `fn remove(index: int): T` | Removes and returns the element at `0 <= index < length()`, shifting subsequent elements left. Raises a runtime bounds error (recoverable via `zonk`) if out of bounds. |
| `fn clear(): void` | Removes all elements from the array in place (`vec.clear()`), leaving `.length() == 0`. |
| `fn reverse(): void` | Reverses the order of elements in place (`std::reverse`). |
| `fn sort(): void` | Sorts the array in place in ascending order (`std::sort`). Requires `T` to support relational `<`. |
| `fn sortWith(cmp: (a: T, b: T) => bool): void` | Sorts the array in place using the custom comparator `cmp(a, b)`. |

```keet
let nums: array<int> = [5, 2, 9, 1, 5, 6];
nums.push(10);
nums.sort(); // `nums` is now [1, 2, 5, 5, 6, 9, 10]

const evensSquared: array<int> = nums
    .filter(fn (x: int): bool { return x % 2 == 0; })
    .map<int>(fn (x: int): int { return x * x; }); // [4, 36, 100]

let sum: int = evensSquared.reduce<int>(0, fn (acc: int, x: int): int {
    return acc + x;
}); // 140
```

## Built-in methods on `map<K, V>`

A `map<K, V>` maps to C++ `std::unordered_map<K, V>`, where `K` satisfies the hashability and equality requirements of [Map types](#map-types). Like arrays, map methods distinguish between non-mutating (`const fn`) and mutating (`fn`) operations.

### Non-mutating map methods (`const fn`)

| Method Signature | Description |
| :--- | :--- |
| `const fn length(): int` | Returns the number of key-value pairs stored in the map (`static_cast<int64_t>(m.size())`). |
| `const fn isEmpty(): bool` | Returns `true` if the map contains no entries (`m.empty()`). |
| `const fn has(key: K): bool` | Returns `true` if `key` is present in the map (`m.find(key) != m.end()`). **Control-Flow Narrowing:** Checking `m.has(k)` in a conditional branch statically narrows subsequent read lookups `m[k]` in that branch from `V \| null` to `V` (provided `m` and `k` are not mutated). |
| `const fn get(key: K, defaultVal: V): V` | Returns a copy of the value associated with `key` if present, or `defaultVal` if `key` is absent. Never inserts `key` into the map and returns `V` directly rather than `V \| null`. |
| `const fn keys(): array<K>` | Returns a newly allocated `array<K>` containing copies of all keys currently in the map. |
| `const fn values(): array<V>` | Returns a newly allocated `array<V>` containing copies of all values currently in the map. |

### Mutating map methods (`fn` — requires mutable `let` receiver)

| Method Signature | Description |
| :--- | :--- |
| `fn remove(key: K): bool` | Removes `key` and its associated value from the map (`m.erase(key) > 0`). Returns `true` if `key` was present and removed, or `false` if `key` was not found. |
| `fn clear(): void` | Removes all key-value pairs from the map in place (`m.clear()`). |

```keet
let env: map<string, string> = {
    "HOST": "127.0.0.1",
    "MODE": "production",
};

let host: string = env.get("HOST", "0.0.0.0"); // "127.0.0.1" (type `string`, no `| null`)
let port: string = env.get("PORT", "8080");    // "8080" ("PORT" is NOT inserted into `env`)

if (env.has("MODE")) {
    let mode: string = env["MODE"];            // narrowed from `string | null` to `string`
    io.println(`Running in ${mode} mode`);
}

let wasRemoved: bool = env.remove("MODE");     // true
```

## C++ AOT translation and performance guarantees

Because KeetScript is an ahead-of-time (AOT) transpiled language targeting modern C++17, its runtime performance characteristics are deterministically bounded by native C++ mechanics rather than virtual machine or garbage collection heuristics:

1. **Direct Inline Emission & Constant Folding:** Simple scalar, string, and collection operations (such as `x.toFloat()`, `x.abs()`, `x.clamp(a, b)`, `s.length()`, `s.isEmpty()`, `arr.push(v)`, `arr.clear()`, `m.has(k)`, `m.remove(k)`) emit direct inline C++ expressions (`static_cast<double>(x)`, `std::abs(x)`, `std::clamp(x, a, b)`, `static_cast<int64_t>(s.size())`, `s.empty()`, `arr.push_back(v)`, `arr.clear()`, `m.find(k) != m.end()`, `m.erase(k) > 0`) with zero dynamic dispatch or boxing overhead.
2. **Standard `<algorithm>` Lowering & Vectorization:** Sorting, searching, and predicate quantification methods (`.sort()`, `.sortWith(cmp)`, `.reverse()`, `.has(v)`, `.any(pred)`, `.all(pred)`) lower directly to `std::sort`, `std::reverse`, `std::find`, `std::any_of`, and `std::all_of`. This allows the host C++ compiler (GCC, Clang, MSVC) to inline lambda predicates, unroll loops, and emit SIMD vector instructions.
3. **Pre-Allocated Functional Pipelines:** Higher-order array transformations such as `.map<U>(...)` configure capacity ahead of time via `result.reserve(src.size())` in generated C++, avoiding geometric vector reallocation while preserving strict value semantics.
4. **Const-Reference Parameter Elision & Last-Use Move:** For read-only method arguments, `keety` emits `const T&` parameters in generated C++ helpers to avoid copying. Simultaneously, static liveness analysis inserts `std::move` on expiring local variables, ensuring near-zero copying overhead during typical data transformations.
5. **Union Storage Profile (`std::variant`):** Union types incur **zero dynamic heap allocation**. A value of type `T1 | T2` is laid out contiguously in stack or object memory with footprint equal to $\max(\text{sizeof}(T_1), \text{sizeof}(T_2)) + \text{alignment padding} + 1\text{ byte discriminant}$. Type inspection via `typeof` evaluates as an $O(1)$ integer index comparison.
6. **Error Path Cost Profile:**
   * **Happy Path (No Errors Raised):** Functions execute at full native machine speed with **0 extra CPU instructions, 0 heap allocations, and 0 branch checks** on the non-error path (utilizing 64-bit platform table-driven zero-cost exception ABIs).
   * **Failure Path (Error Raised):** Unwinding the stack to a `zonk` recovery handler incurs standard ABI unwinding overhead. Consequently, expected absences (such as missing map keys or unmatching searches) use nullable types (`null`), and high-throughput string parsers provide zero-unwinding `.tryInt()` and `.tryFloat()` methods for hot ingestion loops.


# Error handling

KeetScript provides a unified, expression-oriented error-handling system built around three constructs: structured error type declarations (`errtype`), unified error raising and inline recovery (`zonk`), and scoped replacement value resolution (`yield`).

## Error model and philosophy

KeetScript's error model is designed around three core architectural principles:

1. **Zero-Boilerplate Function Signatures:** Functions and methods do **not** clutter their return type signatures with checked exception clauses (there is no `fn readFile(path: string): string ! FileError` syntax). Any function or method may raise a runtime error using a `zonk expr;` statement while declaring only its normal return type.
2. **Automatic Stack Unwinding with Inline Recovery:** If a fallible expression is evaluated without an attached `zonk` recovery clause, any error it raises automatically unwinds the call stack upward to the nearest enclosing `zonk` recovery handler (or terminates the task/process with a formatted diagnostic if unhandled). When local recovery is desired, the caller attaches `zonk` directly to the fallible expression (`expr zonk fallback` or `expr zonk (err) { ... }`), avoiding deeply nested `try` / `catch` indentation blocks.
3. **Strict Separation of Expected Absence vs. Failure:** Expected, non-exceptional absence of a value—such as looking up a missing key in a `map<K, V>` (`m[k]`) or searching an array (`arr.find(...)`)—is represented explicitly in the type system via nullable union types (`T | null`). Unexpected, invalid, or exceptional operation failures—such as out-of-bounds array indexing, division by zero, malformed numeric parsing (`"abc".toInt()`), failed type assertions (`x as T`), or I/O errors—use the `zonk` and `errtype` mechanism.

## The predeclared `error` type

The [universe block](#predeclared-identifiers) predeclares the root interface type `error`, which represents any runtime error value raised in KeetScript—whether created as a custom [`errtype`](#declaring-structured-error-types-errtype) instance or raised directly from a plain `string` literal (`zonk "connection timed out";`).

Every value conforming to `error` exposes two immutable built-in fields and one built-in formatting method:

| Member | Kind | Signature / Type | Description |
| :--- | :--- | :--- | :--- |
| `.name` | Read-only field | `string` | The nominal type name of the error: the exact declared identifier for a custom `errtype` (e.g., `"FileError"`), or `"Error"` for a plain string-raised error. |
| `.message` | Read-only / initialized field | `string` | A human-readable description of the failure. |
| `.toString()` | Built-in method | `const fn toString(): string` | Returns the canonical formatted diagnostic string `"[<name>]: <message>"` (e.g., `"[FileError]: Failed to open '/etc/config'"`). |

Inside a `zonk (err) { ... }` recovery block, the bound parameter `err` has static type `error`. Every custom `errtype` is implicitly assignable to `error`.

## Declaring structured error types (`errtype`)

An `errtype` declaration introduces a distinct, nominally typed structured error type encapsulating custom diagnostic fields, methods, and an optional constructor.

```ebnf
ErrTypeDecl   = "errtype" Identifier "{" { ErrTypeMember } "}" .
ErrTypeMember = ConstructorDecl | [ "pub" | "priv" ] FieldDecl | MethodDecl .
```

* **Implicit Base Properties (`.name` and `.message`):** Every `errtype` automatically inherits the `.name: string` and `.message: string` fields and the `.toString(): string` method from the root `error` type without redeclaring them in the `errtype` body:
  * `this.name` is automatically initialized to the declared `Identifier` of the `errtype` (e.g., `"FileError"`) and is read-only.
  * `this.message` may be assigned inside the `errtype` constructor; if the constructor does not explicitly assign `this.message` (or if no constructor is declared), `this.message` defaults to `"[<ErrTypeName>]"`.
* **Fields and Visibility:** Custom fields declared inside an `errtype` (`let`, `const`, or `define`) are public (`pub`) by default unless marked `priv`, following the same member visibility rules as [`class` types](#class-types).
* **Constructors:** An `errtype` may declare a constructor bearing the exact name of the `errtype` (`ErrTypeName(...) { ... }`). If an `errtype` declares no fields requiring explicit initialization and omits a constructor, the compiler synthesizes a default constructor `ErrTypeName(message: string = "")`.
* **Value Semantics:** Like all values in KeetScript, `errtype` instances are self-contained value types copied by value when constructed, raised, or assigned.

```keet
export errtype FileError {
    let path: string;
    let code: int;

    FileError(path: string, code: int, message: string = "") {
        this.path = path;
        this.code = code;
        this.message = message.isEmpty()
            ? `Failed to access file at '${path}' (error code ${code})`
            : message;
    }

    const fn isPermissionDenied(): bool {
        return this.code == 13;
    }
}

export errtype TimeoutError {
    // Uses synthesized constructor `TimeoutError(message: string = "")`
}
```

## Raising errors (`zonk` statements)

As introduced in [Error-raising statements (`zonk`)](#error-raising-statements-zonk), a function, method, constructor, or recovery block raises a runtime error by executing a `zonk` statement:

```ebnf
ZonkStmt = "zonk" Expression ";" .
```

* **Valid Operand Types:** The `Expression` in `zonk Expression ";"` must evaluate to one of the following:
  1. An instance of a declared `errtype` (e.g., `zonk FileError("/var/log/app.log", 13);`).
  2. A `string` (e.g., `zonk "invalid packet checksum";`). When a `string` is supplied, the compiler automatically wraps it into a root `error` instance whose `.name` is `"Error"` and whose `.message` is that string.
  3. An existing value of type `error` (such as re-raising a caught error via `zonk err;` inside a recovery block).
  Supplying any other operand type to a `zonk` statement is a **compile-time error**.
* **Diverging Control Flow (`never`):** Executing `zonk expr;` immediately aborts normal execution of the current statement list and begins stack unwinding. Because control never proceeds to the statement following `zonk expr;`, `ZonkStmt` has diverging type `never` and satisfies the [terminating statement](#terminating-statements) requirement for non-`void` functions.

```keet
fn readConfig(path: string): string {
    if (path.isEmpty()) {
        zonk "configuration path cannot be empty";
    }
    if (!fs.exists(path)) {
        zonk FileError(path, 2, "Configuration file not found");
    }
    return fs.readAll(path);
}
```

## Catching and error recovery (`zonk` expressions and blocks)

Any expression may be suffixed with an inline `zonk` clause to intercept and recover from any runtime error raised during the evaluation of that expression (or any nested function call it invokes):

```ebnf
ZonkSuffix = "zonk" ( LogicalOrExpr | "(" [ Identifier ] ")" Block ) .
```

### 1. Fallback value form (`expr zonk fallbackExpr`)

```keet
let port: int = rawPort.toInt() zonk 8080;
let defaultMap: map<string, int> = loadScores() zonk {}; // unambiguously an empty MapLit fallback
```

* First, the left-hand expression `expr` (of type `T`) is evaluated.
* If `expr` completes normally without raising an error, its value is yielded as the result of the `zonk` expression, and `fallbackExpr` is **not** evaluated (short-circuit evaluation).
* If `expr` raises an error at runtime, stack unwinding is intercepted, the error is suppressed, and `fallbackExpr` (of type `F`) is evaluated as the replacement result.
* **Result Type:** If `F` is [assignable](#assignability) to `T`, the overall expression has type `T`; otherwise, it widens to the union type `T | F`.

### 2. Scoped recovery block form (`expr zonk ( [ err ] ) Block`)

```keet
let data: string = readConfig("app.conf") zonk (err) {
    io.println(`Error (${err.name}): ${err.message}`);
    return;
};
```

* If `expr` raises an error at runtime, execution enters the recovery `Block`.
* **Mandatory Parentheses `( [ err ] )`:** To guarantee LL/LR parser disambiguation between an empty map literal fallback (`expr zonk {}`) and a scoped recovery block (`expr zonk () { ... }`), parentheses `(...)` are always required before a recovery `Block`. If an `Identifier` is present inside the parentheses (`zonk (err) { ... }`), the caught error is bound to that identifier as an immutable (`const`) binding of static type `error` whose scope is restricted strictly to `Block` (obeying the [strict no-shadowing rule](#declarations-and-scope) against enclosing scopes). If the error value is not needed, empty parentheses are written (`expr zonk () { ... }`).
* **Exhaustive Recovery Block Resolution:** When `expr` is used in a value-producing context (such as the right-hand side of a variable initialization, assignment, function argument, or `return` expression), every execution path through the recovery `Block` must end with a terminating statement that either:
  1. **Diverges from the enclosing scope (`never`):** Exits the enclosing function or loop via `return`, `break`, `continue`, or re-raises an error via `zonk expr;`. When all paths through `Block` diverge, the `zonk` block itself has type `never`, and the enclosing `expr zonk (...) Block` expression has strictly the success type `T` of `expr`.
  2. **Yields a replacement value (`yield expr;`):** Supplies a replacement value for the failed expression using a [`yield` statement](#yielding-replacement-values-from-zonk-blocks-yield) so that execution of the current function can continue normally.

*(Note: When `expr zonk ( [ err ] ) Block` appears as a standalone [expression statement](#expression-statements) whose result is discarded—e.g., `connectToServer() zonk (err) { io.println(err.message); };`—the recovery block is not required to `yield` a value or diverge.)*

## Yielding replacement values from `zonk` blocks (`yield`)

To eliminate any syntactic or semantic ambiguity between returning from the enclosing function versus supplying a fallback value for a failed `zonk` expression, KeetScript provides the dedicated `yield` statement:

```ebnf
YieldStmt = "yield" Expression ";" .
```

* **`return` vs. `yield` Inside a `zonk` Block:**
  * `return [ expr ];` inside a `zonk` recovery block **always** returns from the enclosing `fn` or method.
  * `yield expr;` inside a `zonk` recovery block terminates only the innermost enclosing `zonk` recovery `Block`, supplies the evaluated result of `expr` as the value of the `expr zonk ...` expression, and allows the enclosing statement and function to continue executing normally.
* **Placement and Type Constraints:**
  * A `yield` statement may appear **only** inside a scoped `zonk` recovery `Block` (and not inside a nested `FunctionLit` closure within that block). Using `yield` outside of a `zonk` recovery block is a **compile-time error**.
  * The operand `Expression` of `yield Expression ";"` must be [assignable](#assignability) to the target type expected for the enclosing `zonk` expression (or widens the `zonk` expression's type to `T | YieldType`).
  * As specified in [Terminating statements](#terminating-statements), `yield expr;` is a terminating statement within the `zonk` block; any statement placed directly after `yield expr;` in the same statement list is unreachable and results in a **compile-time error**.

```keet
fn loadServerPort(configPath: string): int {
    let rawConfig: string = readConfig(configPath) zonk (err) {
        if (typeof(err) == "FileError") {
            io.println(`Warning: ${err.message}. Falling back to default config.`);
            yield "8080"; // Supplies "8080" to `rawConfig`; `loadServerPort` continues!
        }
        // Unrecoverable error: exit `loadServerPort` early with error code -1
        io.println(`Fatal error: ${err.toString()}`);
        return -1;
    };

    let port: int = rawConfig.trim().toInt() zonk (err) {
        io.println(`Invalid port number (${err.message}), defaulting to 8080`);
        yield 8080;
    };

    return port;
}
```

## Inspecting and narrowing error types

Inside a `zonk (err) { ... }` recovery block, `err` has static type `error`, providing immediate access to `err.name`, `err.message`, and `err.toString()`. To access custom fields or methods declared on a specific `errtype` (such as `err.path` or `err.code` on `FileError`), `err` is inspected and narrowed using the same mechanisms as [union types](#union-and-literal-types):

1. **`typeof(err)` Inspection:**
   Evaluating `typeof(err)` returns `err.name`: the exact nominal identifier of the active `errtype` (e.g., `"FileError"`, `"TimeoutError"`), or `"Error"` if the error was raised from a plain `string`.
2. **Flow-Based Type Narrowing (`if` and `switch`):**
   * Checking `if (typeof(err) == "FileError") { ... }` statically narrows `err` from `error` to `FileError` inside that `if` branch.
   * Switching via `switch (typeof(err)) { case ("FileError") { ... } }` statically narrows `err` to the matching `errtype` inside each corresponding `case` block, granting full compile-time typed access to its custom fields and methods.
3. **Explicit Type Assertion (`err as ErrTypeName`):**
   An `error` value `err` may also be explicitly asserted to a declared `errtype` using `err as FileError`. If `err` is not an instance of `FileError` at runtime, the `as` assertion raises a type-assertion error (which may itself be guarded with `zonk`).

```keet
let response: string = fetchRemoteConfig(url) zonk (err) {
    switch (typeof(err)) {
        case ("FileError") {
            // `err` is statically narrowed to `FileError`
            io.println(`File error on '${err.path}' (code ${err.code})`);
            if (err.isPermissionDenied()) {
                return "";
            }
            yield "{}";
        }
        case ("TimeoutError") {
            // `err` is statically narrowed to `TimeoutError`
            io.println(`Request timed out: ${err.message}`);
            yield "{}";
        }
        default {
            // `err` remains of type `error` (includes plain `zonk "..."` string errors)
            io.println(`Unhandled error: ${err.toString()}`);
            zonk err; // Re-raise error to caller
        }
    }
};
```

## Zero-cost C++ AOT translation

The `keety` compiler translates `errtype`, `zonk`, and `yield` into idiomatic, zero-overhead C++17 code:

1. **Runtime Error Hierarchy (`keet::error`):**
   The predeclared `error` type maps to a lightweight C++ base struct `keet::error` deriving from `std::exception`, and every user-declared `errtype` compiles to a derived C++ struct with value semantics:
   ```cpp
   namespace keet {
       struct error : public std::exception {
           std::string name;
           std::string message;

           error(std::string n = "Error", std::string m = "[Error]")
               : name(std::move(n)), message(std::move(m)) {}

           std::string toString() const {
               return "[" + name + "]: " + message;
           }
           const char* what() const noexcept override {
               return message.c_str();
           }
       };
   }
   ```
2. **Zero Overhead on the Non-Error Path:**
   A `zonk expr;` statement transpiles directly to a C++ `throw` expression (`throw FileError(...)` or `throw keet::error("Error", msg)`). Because modern 64-bit C++ ABIs (Itanium C++ ABI on Linux/macOS and x64 SEH on Windows) implement table-driven, **zero-cost exceptions**, functions that do not raise an error execute at full native C++ speed with **zero** extra CPU instructions, **zero** heap allocations, and **zero** branch checks on the happy path.
3. **Lowering of `zonk` Recovery Expressions and `yield`:**
   * A fallback expression `expr zonk fallbackExpr` or a value-yielding block `expr zonk (err) { ... yield val; }` lowers to an inlined immediately invoked lambda expression (IILE) wrapping a `try ... catch (const keet::error& err)` block in C++, where `yield val;` compiles directly to `return val;` from the recovery lambda.
   * A `zonk` recovery block containing outer control-flow transfers (`return` from the enclosing function, `break`, or `continue`) lowers the `try ... catch` block directly into the enclosing C++ statement flow so that `return`, `break`, and `continue` execute natively in the host function.
4. **Foreign C++ Exception Bridging:**
   When native C++ code invoked via FFI or standard library headers throws an exception deriving from `std::exception` (such as `std::runtime_error`, `std::out_of_range`, or `std::bad_alloc`), the generated KeetScript call boundary intercepts the foreign exception and automatically rethrows it as an instance of `keet::error("CppException", e.what())`. This ensures that foreign C++ exceptions are uniformly catchable and recoverable using standard KeetScript `zonk` handlers.


# Modules, imports, and program structure

KeetScript programs are constructed by linking together *modules*. A module is a self-contained lexical compilation unit consisting of a single `.keet` source file that declares constants, variables, types, functions, and error types, and explicitly controls which symbols are exposed to other modules via `export`.

## Source file organization and module identity

Every `.keet` source file constitutes a distinct [module block](#blocks). Unlike languages that require an explicit internal package or module header declaration at the top of each file, a KeetScript module's identity and default namespace identifier are inferred directly from its filename and path relative to the project root:

* A file named `math.keet` or `utils/math.keet` has the default module identifier `math` (and canonical project path `utils.math`).
* Every `.keet` source file consists strictly of three ordered sections:
  1. Optional leading comments and documentation.
  2. Zero or more `ImportDecl` statements, all of which **must** appear at the very top of the source file before any other declaration.
  3. Zero or more top-level declarations (`TopLevelDecl`), which are private to the module by default or marked public with `export`.
* Loose executable statements at the top level of a module (outside a function, method, or constructor body) are disallowed at compile time.

```ebnf
SourceFile = { ImportDecl } { TopLevelDecl } .
```

*(See [Declarations and scope](#declarations-and-scope) for `TopLevelDecl` and `Declaration`.)*

<a id="qualified-identifiers"></a>
## Module path resolution

KeetScript distinguishes syntactically between **standard library / installed package modules** and **local project modules** (or native C++ headers) in `import` declarations:

```ebnf
ModulePath     = QualifiedIdent | StringLit .
QualifiedIdent = Identifier { "." Identifier } .
```

1. **Standard Library and Installed Packages (`QualifiedIdent`):**
   Referenced using unquoted, dot-separated identifier paths:
   ```keet
   import io;
   import math;
   import net.http;
   ```
   The `keety` compiler resolves unquoted `QualifiedIdent` paths first against the built-in KeetScript standard library and then against external packages installed in the project's `keet_modules/` directory.
2. **Local Project Modules and Native C++ Headers (`StringLit`):**
   Referenced using quoted string literals beginning with an explicit relative path prefix (`"./"` or `"../"`) or naming a native C/C++ header (`.h` / `.hpp`):
   ```keet
   import "./config";
   import "../utils/helpers";
   import "./models/user.keet";
   import "./native/fast_hash.hpp";
   ```
   The `.keet` file extension in a quoted relative path literal is optional (`"./config"` and `"./config.keet"` resolve to the same source file).

## Import declarations

An `import` declaration brings a target module, specific exported symbols from a target module, or a native C++ header into the importing file's module block, or re-exports them for downstream consumers. All `ImportDecl` statements must appear at the top of the `.keet` file prior to any `TopLevelDecl`.

```ebnf
ImportDecl      = [ "export" ] "import" ( NamespaceImport | NamedImport ) ";" |
                  "export" NamedImport ";" .
NamespaceImport = ModulePath [ "as" Identifier ] .
NamedImport     = "{" ImportSpecList [ "," ] "}" "from" ModulePath .
ImportSpecList  = ImportSpec { "," ImportSpec } .
ImportSpec      = Identifier [ "as" Identifier ] .
```

KeetScript supports three forms of `import` declarations:

### 1. Namespace imports (default and aliased)

A `NamespaceImport` binds the imported module to a single namespace identifier in the importing file's module scope. All exported declarations of the target module are accessed through that namespace using [qualified identifiers](#qualified-identifiers) (`namespace.Symbol`):

* If no `as Identifier` clause is written:
  * For a `QualifiedIdent` path (e.g., `import io;` or `import net.http;`), the bound namespace identifier is the final segment of the path (`io` or `http`).
  * For a relative `StringLit` path (e.g., `import "./utils/math";`), the bound namespace identifier is the base filename without the `.keet` extension (`math`), which must be a valid KeetScript `Identifier`.
* If an `as Identifier` clause is provided, the module is bound to that explicit alias instead.

```keet
import io;                       // bound as `io` -> io.println(...)
import net.http as http;         // bound as `http` -> http.get(...)
import "./utils/math" as m;      // bound as `m` -> m.sin(...)
```

### 2. Selective (named) symbol imports (`from` syntax)

A `NamedImport` imports one or more specific `export`-marked declarations (`define`, `const`, `let`, `fn`, `class`, `enum`, `errtype`, or `type`) directly into the importing file's module block, allowing them to be referenced without namespace qualification:

* Each `ImportSpec` names an exported symbol in the target module and may optionally rename it locally via `as Identifier`.
* If an overloaded function name is imported via `NamedImport`, all exported overloads of that function name are brought into scope.
* Imported symbols are immutable bindings in the importing module's scope and strictly obey the [no-shadowing rule](#declarations-and-scope): no local declaration in the file may shadow an imported identifier.
* Attempting to import a non-existent or non-exported symbol is a compile-time error.

```keet
import { PI, sin, cos } from math;
import { User, formatName as fmt } from "./models/user";

fn circumference(radius: float): float {
    return 2.0 * PI * radius;
}
```

### 3. Re-exporting declarations (`export import`)

Prefixing an `ImportDecl` with `export` both imports the namespace or named symbols into the current module and simultaneously re-exports them as part of the current module's public interface. This enables clean facade or package-index modules:

```keet
// In `db/index.keet`:
export import "./connection" as conn;
export { Pool, QueryError } from "./pool";
export { Transaction } from "./tx";
```

## Export rules and `const`-view visibility

By default, every top-level declaration in a `.keet` file has **module-private visibility** and cannot be referenced outside that file. Prefixing a top-level declaration with `export` makes it part of the module's public interface.

### Const-view semantics for exported `let` variables

To prevent cross-module global state corruption and data races across C++ translation units, KeetScript enforces **asymmetric `const`-view semantics** on exported mutable variables (`export let`):

1. **Inside the Defining Module (Mutable):**
   A top-level variable declared as `export let activeConnections: int = 0;` is a normal mutable binding within the `.keet` file that declares it. Functions and methods inside that same module may freely assign to or mutate it (`activeConnections += 1;`).
2. **Outside the Defining Module (Read-Only `const` View):**
   To any external module that imports `activeConnections` (whether via namespace access `server.activeConnections` or named import `import { activeConnections } from "./server";`), the binding is exposed strictly as an **immutable `const` view**.
   * Attempting to assign to an imported variable (`server.activeConnections = 10;`), apply a compound assignment (`server.activeConnections += 1;`), increment/decrement it (`server.activeConnections++;`), or invoke a mutating method (`fn`, such as `.push()` or `.clear()`) on an imported collection variable is a **compile-time error**.
   * Non-mutating methods (`const fn`, such as `.length()` or `.has()`) and value reads are permitted.
   * If a module wishes to permit external state modification, it must export an explicit mutator function:

```keet
// --- metrics.keet ---
export let requestCount: int = 0;
export let recentPaths: array<string> = [];

export fn recordRequest(path: string): void {
    requestCount += 1;
    recentPaths.push(path);
}

// --- handler.keet ---
import "./metrics" as metrics;

export fn handle(url: string): void {
    metrics.recordRequest(url);              // Valid: calls exported mutator function
    let total: int = metrics.requestCount;   // Valid: read-only access
    let n: int = metrics.recentPaths.length(); // Valid: `const fn` method call

    // metrics.requestCount += 1;            // Compile-time error: imported `let` is a const view
    // metrics.recentPaths.push(url);        // Compile-time error: cannot call mutating `fn` on const view
}
```

### Visibility of exported composite types (`class`, `enum`, `errtype`)

* **`export class`:** Exporting a class makes the class type and its `pub` (default) constructors, fields, and methods visible to importing modules. Any field, constructor, or method marked `priv` remains strictly inaccessible outside the lexical body of the class, both within the defining module and in importing modules.
* **`export enum`:** Exporting an enum exposes the enum type and all of its scoped member constants (`Status.OK`, `Status.Error`).
* **`export errtype`:** Exporting an error type allows importing modules to construct, raise (`zonk`), and type-narrow (`typeof(err) == "MyError"` or `err as MyError`) instances of that error type.

## Circular dependency prohibition and initialization order

### Strict acyclic dependency graph (DAG)

KeetScript strictly forbids circular dependencies between modules. If module `A` imports module `B`, then `B` may neither directly nor transitively import `A`.

During compilation, `keety` constructs the complete module import graph before type checking. If a cycle is detected, compilation halts immediately with a diagnostic tracing the full import chain:

```
Compile Error: Circular module dependency detected:
    src/main.keet -> src/auth/session.keet -> src/models/user.keet -> src/auth/session.keet
```

### Topological module initialization order

Because the module dependency graph is guaranteed to be a directed acyclic graph (DAG), module-level initialization is deterministic and race-free:

1. **Leaf-First Topological Initialization:** Before `main()` begins execution, the runtime initializes all modules in topological dependency order: if module `A` imports module `B`, all top-level `define`, `const`, and `let` initializers in `B` are completely evaluated before any top-level initializer in `A` runs.
2. **Intra-Module Declaration Order:** Within a single `.keet` module file, top-level `define`, `const`, and `let` bindings are initialized sequentially in the exact source order in which they appear (enforcing declare-before-use for value bindings).
3. **Initialization Errors:** If any top-level module initializer raises an unhandled error via `zonk`, module initialization aborts immediately, prints the formatted error to standard error, and terminates the process with a non-zero exit status before `main()` is entered.

### Single-instantiation and multiple-import deduplication

When multiple modules in a project import the same dependency (for example, both `service.keet` and `controller.keet` import `database.keet`):
1. **Single Module Instance:** The imported module is compiled and instantiated exactly once across the entire program. Its module-level state (`export let`) resides in a single, shared storage location.
2. **Deterministic Deduplication:** In generated C++, `#pragma once` header guards ensure that types and function declarations are included without duplication, and all importing translation units access the same canonical module namespace and single-instance `const`-view accessors.
3. **Diamond Dependency Resolution:** In diamond dependency graphs (where module `A` imports `B` and `C`, and both `B` and `C` import `D`), `D` is initialized exactly once before either `B` or `C`, with zero static initialization order ambiguity.

## Program execution and entry point (`fn main`)

A complete executable KeetScript program is created by compiling a single root `.keet` module (along with all modules it transitively imports) that declares a top-level `main` function. The `main` function serves as the program entry point and must have one of the following four signatures:

```keet
fn main(): void { ... }
fn main(): int { ... }
fn main(args: array<string>): void { ... }
fn main(args: array<string>): int { ... }
```

* **`args` Parameter:** When declared, `args` receives a deep-copied `array<string>` containing the command-line arguments passed to the process, where `args[0]` is the program executable invocation path/name and `args[1...]` are the user-supplied arguments.
* **Exit Status Code:**
  * If `main` has return type `int`, the integer returned by `main` becomes the operating system process exit status code (`0` conventionally indicating success).
  * If `main` has return type `void`, normal completion of `main` automatically exits the process with status code `0`.
* **Unhandled Errors in `main`:** If an error is raised via `zonk` during `main` and propagates out of `main` without being caught by a `zonk` recovery clause, the KeetScript runtime catches the top-level `keet::error`, writes `"Uncaught error: " + err.toString()` to standard error (`stderr`), and terminates the process with exit code `1`.

```keet
import io;
import "./config" as cfg;

fn main(args: array<string>): int {
    if (args.length() < 2) {
        io.println(`Usage: ${args[0]} <port>`);
        return 2;
    }

    let port: int = args[1].toInt() zonk (err) {
        io.println(`Invalid port argument: ${err.message}`);
        return 1;
    };

    io.println(`Starting ${cfg.APP_NAME} on port ${port}...`);
    return 0;
}
```

## Core standard library modules

While the [universe block](#predeclared-identifiers) remains intentionally minimal, KeetScript provides a batteries-included standard library accessible via `import`:

| Module | Primary Exports & Capabilities |
| :--- | :--- |
| `io` | Console and stream I/O: `print(...args: Any, color?: io.Color): void`, `println(...args: Any, color?: io.Color): void`, `eprint(...args: Any, color?: io.Color): void`, `eprintln(...args: Any, color?: io.Color): void`, `flush(): void`, `readLine(): string | null`, `readln(): string`, `eof(): bool`, `input(prompt?: string): string`, `Color` enum, ANSI inline styling helpers (`red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`, `gray`, `bold`). |
| `fs` | File system and path manipulation (fallible operations integrate with `zonk`): `readText(path: string): string`, `readLines(path: string): array<string>`, `writeText(path: string, content: string): bool`, `appendText(path: string, content: string): bool`, `exists(path: string): bool`, `isFile(path: string): bool`, `isDir(path: string): bool`, `fileSize(path: string): int`, `mkdir(path: string): bool`, `remove(path: string): bool`, `removeAll(path: string): int`, `copy(src: string, dst: string): bool`, `rename(src: string, dst: string): bool`, `listDir(path: string): array<string>`, `cwd(): string`, `join(base: string, child: string): string`, `fileName(path: string): string`, `extension(path: string): string`, `parentDir(path: string): string`. |
| `sys` | Process environment and lifecycle control: `args: array<string>`, `exit(code: int): void`, `getEnv(key: string): string | null`, `setEnv(key: string, val: string): void`, `platform(): string`, `cwd(): string`. |
| `math` | IEEE 754 floating-point and integer math utilities: `PI: float`, `E: float`, `sin(x: float): float`, `cos(x: float): float`, `tan(x: float): float`, `sqrt(x: float): float`, `pow(base: float, exp: float): float`, `log(x: float): float`, `min<T: int | float>(a: T, b: T): T`, `max<T: int | float>(a: T, b: T): T`. |
| `time` | Monotonic and wall-clock time utilities: `now(): Timestamp`, `monotonicMs(): int`, `sleep(ms: int): void`, `Duration` class, `Timestamp` class. |

### Standard Library: `io` (Console and Stream I/O)

The built-in `io` standard library module provides high-performance, type-safe console output, interactive standard input reading, stream control, and ANSI terminal color formatting. It is brought into scope via `import io;` or selective import statements (`from "io" import println, Color, green;`).

#### Module Declarations & Signatures

```keet
// Terminal color enumeration
enum Color {
    Default,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    Gray
}

// Console output functions
fn print(...args: Any, color?: Color): void;
fn println(...args: Any, color?: Color): void;
fn eprint(...args: Any, color?: Color): void;
fn eprintln(...args: Any, color?: Color): void;
fn flush(): void;

// Console input functions
fn readLine(): string | null;
fn readln(): string;
fn eof(): bool;
fn input(prompt?: string): string;

// Inline ANSI styling helpers
fn red(s: string): string;
fn green(s: string): string;
fn yellow(s: string): string;
fn blue(s: string): string;
fn magenta(s: string): string;
fn cyan(s: string): string;
fn white(s: string): string;
fn gray(s: string): string;
fn bold(s: string): string;
```

#### Behavior & Semantics
1. **Variadic Output Formatting & Buffering:**
   `io.print` and `io.println` accept zero or more printable arguments of any KeetScript primitive or composite type (`int`, `float`, `bool`, `char`, `string`, arrays, or maps). When multiple arguments are passed, values are space-separated. `println` appends a trailing newline (`\n`). Neither `print` nor `println` automatically flushes standard output. Callers can explicitly flush output streams via `io.flush()`.
2. **Standard Error Output:**
   `io.eprint` and `io.eprintln` format arguments identically to `print` and `println`, directing output to standard error (`stderr`).
3. **Whole-Line Color Formatting:**
   When an `io.Color` enum value is passed as the trailing argument to `print`, `println`, `eprint`, or `eprintln` (e.g., `io.println("All tests passed", io.Color.Green)`), the output line is wrapped with the corresponding ANSI color escape code and reset code. Passing `io.Color` in any position other than the trailing argument triggers a compile-time type mismatch diagnostic (`E3003`).
4. **Inline Text Styling:**
   The styling helpers `io.red(s)`, `io.green(s)`, `io.yellow(s)`, `io.blue(s)`, `io.magenta(s)`, `io.cyan(s)`, `io.white(s)`, `io.gray(s)`, and `io.bold(s)` return a new `string` wrapped with ANSI escape sequences when terminal styling is enabled, or unmodified `s` when styling is disabled. An empty string argument returns `""` without escape codes. Wrapping an already styled string that terminates with `\033[0m` does not append a duplicate trailing reset code. Nested styled strings preserve outer styling across inner `\033[0m` resets.
5. **TTY Detection & `NO_COLOR` Compliance:**
   - Precedence: Explicit programmatic override > `NO_COLOR` environment variable > destination stream interactive terminal detection (`isatty`).
   - If `NO_COLOR` is present in the environment (even if set to an empty string `""`), automatic terminal styling is disabled.
   - Standard output (`stdout`) and standard error (`stderr`) have independent TTY detection.
6. **Console Input (`readLine`, `readln`, `eof`, & `input`):**
   - `io.readLine()` reads a single line from standard input up to the newline character (`\n`), stripping any trailing carriage return (`\r`) for cross-platform compatibility. It returns `string` (including `""` for an empty line), or `null` when EOF is reached without reading any characters. Unrecoverable stream errors throw `IOError`.
   - `io.readln()` reads a single line from standard input and returns an empty string `""` on EOF (preserved for backward compatibility).
   - `io.eof()` returns `true` if a previous read operation on standard input reached the end of the stream (i.e., stream `eofbit` is set), and `false` otherwise.
   - `io.input(prompt?: string)` writes `prompt` to standard output without a newline, flushes the stream immediately before blocking on input, and returns `io.readln()`.

### Standard Library: `fs` (File System Operations)

The built-in `fs` standard library module provides cross-platform file system operations, file stream reading and writing, directory tree traversal, and path normalization. It is brought into scope via `import fs;`, `import fs as alias;`, or selective imports (`from "fs" import readText, writeText, exists;`).

#### Module Declarations & Signatures

```keet
// File Reading & Writing (Fallible, zonk-compatible)
fn readText(path: string): string;
fn readLines(path: string): array<string>;
fn writeText(path: string, content: string): bool;
fn appendText(path: string, content: string): bool;

// File & Directory Queries (Non-throwing predicates & scalar queries)
fn exists(path: string): bool;
fn isFile(path: string): bool;
fn isDir(path: string): bool;
fn fileSize(path: string): int;

// Directory & File Manipulation (Fallible mutations)
fn mkdir(path: string): bool;
fn remove(path: string): bool;
fn removeAll(path: string): int;
fn copy(src: string, dst: string): bool;
fn rename(src: string, dst: string): bool;
fn listDir(path: string): array<string>;

// Path Utilities (Pure string transformations)
fn cwd(): string;
fn join(base: string, child: string): string;
fn fileName(path: string): string;
fn extension(path: string): string;
fn parentDir(path: string): string;
```

#### Example Usage with Error Recovery (`zonk`)

```keet
import fs;
import io;

fn main(): void {
    let configPath: string = fs.join("config", "app.json");
    let rawConfig: string = fs.readText(configPath) zonk "{}";
    io.println("Configuration loaded:", rawConfig);
}
```

### Standard Library: `sys` (System and Process Operations)

The built-in `sys` standard library module provides cross-platform access to command-line arguments, process lifecycle management, environment variables, system and CPU information, working directory manipulations, and subprocess execution. It is brought into scope via `import sys;`, `import sys as alias;`, or selective imports (`from "sys" import args, getEnv, platform, cpuCount;`).

#### Module Declarations & Signatures

```keet
// Command-Line Arguments & Process Lifecycle
fn args(): array<string>;
fn pid(): int;
fn exit(code: int): void;

// Environment Variables
fn getEnv(key: string): string | null;
fn getEnvOr(key: string, fallback: string): string;
fn hasEnv(key: string): bool;
fn setEnv(key: string, value: string): bool;
fn unsetEnv(key: string): bool;

// System & Platform Information
fn platform(): string;
fn arch(): string;
fn cpuCount(): int;

// Directory & Path Primitives
fn cwd(): string;
fn chdir(path: string): bool;
fn homeDir(): string;
fn tempDir(): string;

// Subprocess Execution (Fallible, zonk-compatible)
fn exec(command: string): int;
fn execOutput(command: string): string;
```

#### Example Usage with Error Recovery (`zonk`)

```keet
import sys;
import io;

fn main(): void {
    let home: string = sys.getEnvOr("HOME", "/tmp");
    let cpus: int = sys.cpuCount();
    let gitRev: string = sys.execOutput("git rev-parse --short HEAD") zonk "unknown";
    io.println("Running on platform:", sys.platform(), "with", cpus, "CPUs");
    io.println("Current revision:", gitRev);
}
```

### Standard Library: `math` (Mathematical Functions and Constants)

The built-in `math` standard library module provides mathematical constants, core arithmetic and rounding operations, exponential and logarithmic functions, trigonometric and hyperbolic functions, angle conversions, floating-point classifications, linear interpolation, and thread-safe pseudo-random number generation (PRNG). It is brought into scope via `import math;`, `import math as alias;`, or selective imports (`from "math" import PI, sin, cos, sqrt, clamp, absInt, randomInt;`).

All operations strictly adhere to KeetScript's **Strict Zero-Coercion Policy (`E3002`)**: functions expecting `float` require explicit float operands, functions expecting `int` require integer operands, and polymorphic functions require matching numeric types across all arguments.

Fallible domain operations (such as `clamp` where `minVal > maxVal` or `randomInt`/`randomFloat` where `minVal > maxVal`) throw runtime `keet::error` instances named `"MathError"`, integrating seamlessly with KeetScript's native `zonk` error recovery operators and blocks.

#### Module Constants & Declarations

```keet
// Mathematical Constants
const PI: float;    // 3.141592653589793
const E: float;     // 2.718281828459045
const TAU: float;   // 6.283185307179586
const INF: float;   // +infinity
const NAN: float;   // quiet NaN

// Basic Arithmetic & Polymorphic Functions
fn abs(x: float): float;
fn abs(x: int): int;
fn absInt(x: int): int;
fn min(a: float, b: float): float;
fn min(a: int, b: int): int;
fn minInt(a: int, b: int): int;
fn max(a: float, b: float): float;
fn max(a: int, b: int): int;
fn maxInt(a: int, b: int): int;
fn clamp(val: float, minVal: float, maxVal: float): float;
fn clamp(val: int, minVal: int, maxVal: int): int;
fn clampInt(val: int, minVal: int, maxVal: int): int;
fn sign(x: float): float;
fn sign(x: int): int;
fn signInt(x: int): int;

// Rounding & Truncation
fn floor(x: float): float;
fn ceil(x: float): float;
fn round(x: float): float;
fn trunc(x: float): float;

// Exponential, Logarithmic, Power & Roots
fn sqrt(x: float): float;
fn cbrt(x: float): float;
fn pow(base: float, exp: float): float;
fn hypot(x: float, y: float): float;
fn exp(x: float): float;
fn log(x: float): float;
fn log2(x: float): float;
fn log10(x: float): float;

// Trigonometric, Hyperbolic & Angle Conversions
fn sin(rad: float): float;
fn cos(rad: float): float;
fn tan(rad: float): float;
fn asin(x: float): float;
fn acos(x: float): float;
fn atan(x: float): float;
fn atan2(y: float, x: float): float;
fn sinh(x: float): float;
fn cosh(x: float): float;
fn tanh(x: float): float;
fn toRadians(degrees: float): float;
fn toDegrees(radians: float): float;

// Floating-Point Classification & Interpolation
fn isNan(x: float): bool;
fn isInf(x: float): bool;
fn isFinite(x: float): bool;
fn lerp(a: float, b: float, t: float): float;
fn fmod(x: float, y: float): float;

// Pseudo-Random Number Generation
fn seed(s: int): void;
fn random(): float;
fn randomInt(minVal: int, maxVal: int): int;
fn randomFloat(minVal: float, maxVal: float): float;
```

#### Example Usage with Error Recovery (`zonk`)

```keet
import math;
import io;
from "math" import PI, sin, clamp, randomInt;

fn main(): void {
    let angle: float = math.toRadians(45.0);
    let s: float = sin(angle);
    let roll: int = randomInt(1, 6);

    // Fallible clamp domain error recovered with zonk fallback
    let safeVal: float = clamp(5.0, 10.0, 0.0) zonk 0.0;
    io.println("sin(45 deg):", s, "die roll:", roll, "clamped:", safeVal);
}
```

## C++ AOT translation and header generation


The `keety` compiler maps KeetScript modules directly to standard C++17 translation units with deterministic header generation and zero runtime module-loader overhead:

1. **Paired Header and Implementation Emission (`.hpp` / `.cpp`):**
   Every compiled module `<name>.keet` lowers to a pair of C++17 files:
   * `<name>.hpp`: Contains `#pragma once`, `#include` directives for imported dependencies, forward declarations for all module-level types and functions, `constexpr` definitions (`define`), class/enum/errtype layouts, generic `template` definitions, and public declarations for `export`-marked symbols.
   * `<name>.cpp`: Contains the definitions of non-generic functions and methods, private module-internal bindings, and storage definitions for module-level variables.
2. **C++ Namespace Encapsulation:**
   All declarations inside a module `<name>.keet` are emitted inside a corresponding C++ namespace (e.g., `namespace utils_math { ... }`), preventing symbol collisions across the dependency graph.
   * Namespace imports (`import "./utils/math" as m;`) lower to `#include "utils/math.hpp"` paired with a scoped C++ namespace alias (`namespace m = ::utils_math;`).
   * Selective named imports (`import { sin, PI } from math;`) lower to scoped `using ::keet::math::sin;` and `using ::keet::math::PI;` declarations inside the importing module's namespace.
3. **Enforcing `export let` Const-View in C++:**
   For an exported mutable variable `export let count: int = 0;`, the defining `.cpp` file owns the mutable variable `int64_t count = 0;`, while the generated `.hpp` header exposes a `const int64_t&` reference accessor (`inline const int64_t& count = _internal_count;`) to importing translation units, enforcing read-only semantics at both the KeetScript type-checker level and the C++ compiler level.
4. **Native `main` Entry Point Lowering:**
   The root module's `fn main(...)` is emitted inside its module namespace as `_keet_main(...)`, and `keety` generates a standard C++ `int main(int argc, char* argv[])` wrapper that:
   * Initializes all module-level variables in deterministic topological DAG order;
   * Marshals `(argc, argv)` into a `std::vector<std::string>` when `args: array<string>` is declared;
   * Wraps the invocation of `_keet_main` in a top-level `try ... catch (const keet::error& err)` handler to guarantee clean diagnostic reporting on uncaught errors.

## Native C++ interoperability (FFI)

Because KeetScript compiles directly to C++17 source and header files, it provides zero-marshaling foreign function interoperability (FFI) with native C++ and C libraries via header imports and top-level `extern` declarations:

```ebnf
ExternDecl            = "extern" [ '"C"' | '"C++"' ] ( FunctionSignature ";" | ExternClassDecl ) .
FunctionSignature     = "fn" Identifier [ TypeParameters ] Parameters ":" TypeAnnotation .
ExternClassDecl       = "class" Identifier [ TypeParameters ] "{" { ExternMember } "}" .
ExternMember          = ExternConstructorDecl |
                        [ "pub" | "priv" ] [ "const" ] FunctionSignature ";" |
                        [ "pub" | "priv" ] FieldDecl .
ExternConstructorDecl = Identifier Parameters ";" .
```

*(Grammar Note: `ExternDecl` is permitted strictly at module top level via `TopLevelDecl`—never nested inside function or statement blocks—and both `FunctionSignature ";"` and `ExternConstructorDecl` terminate with `;` rather than a `{ ... }` block body.)*

1. **Importing Native Headers (`import "header.hpp";`):**
   An `import` declaration whose `StringLit` path ends in `.h`, `.hh`, or `.hpp` (such as `import "./native/crypto.hpp";` or `import "sqlite3.h";`) instructs `keety` to emit a direct `#include` directive in the generated C++ translation unit.
2. **Linkage Specifiers (`"C++"` vs. `"C"`):**
   An `extern` declaration may optionally specify a linkage literal (`extern "C++" fn ...` or `extern "C" fn ...`). If omitted, linkage defaults to `"C++"`. When `"C"` is specified, `keety` ensures C-compatible symbol resolution (`extern "C"`) when referencing the declaration from generated C++.
3. **Comprehensive Type Mapping, ABI & Ownership Model:**
   KeetScript core types map with zero marshaling overhead to their native C++17 equivalents:

| KeetScript Type | Native C++17 Equivalent | Passing by Value (In) | Passing via `mut` | Memory Ownership & Lifetime |
| :--- | :--- | :--- | :--- | :--- |
| `int` | `int64_t` | By value (`int64_t`) | `int64_t&` | Register / Stack scalar |
| `float` | `double` | By value (`double`) | `double&` | Register / Stack scalar |
| `bool` | `bool` | By value (`bool`) | `bool&` | Register / Stack scalar |
| `char` | `char` | By value (`char`) | `char&` | Byte code unit |
| `string` | `std::string` | `const std::string&` (or move) | `std::string&` | Deep-copied value, RAII buffer |
| `array<T>` | `std::vector<T>` | `const std::vector<T>&` (or move) | `std::vector<T>&` | Deep-copied value, RAII buffer |
| `array<bool>` | `keet::array<bool>` | `const keet::array<bool>&` | `keet::array<bool>&` | Byte-backed vector (`bool&`) |
| `map<K, V>` | `std::unordered_map<K, V>` | `const std::unordered_map<...>&` | `std::unordered_map<...>&` | Deep-copied value, RAII table |
| `T1 \| T2` | `std::variant<T1, T2>` | `const std::variant<...>&` | `std::variant<...>&` | Contiguous stack/value variant |
| `class C` | `class C` | `const C&` (or move) | `C&` | Stack/value type, RAII destructors |
| `enum E` | `enum class E : int64_t` | By value (`E`) | `E&` | Scoped enumeration value |
| `(x: A) => B` | `std::function<B(A)>` | By value / move | `std::function<B(A)>&` | Callable value object |

   * **Narrower C Integer Types (`int`, `long`, `size_t`):** When binding a C/C++ function whose underlying signature uses narrower or unsigned integer types (such as C `int`, `long`, `uint32_t`, or `size_t`) and is declared as 64-bit `int` in an `extern fn` signature, the `keety` compiler automatically emits standard integer widening/narrowing `static_cast` conversions at the call boundary between KeetScript's `int64_t` and the target C/C++ parameter or return type.
   * **Memory Ownership, RAII, and Opaque Handles:** Because raw pointer syntax is intentionally excluded from KeetScript's core grammar, native C/C++ opaque pointers (`void*`, `sqlite3*`, or OS handles) are represented safely in FFI bindings either as wrapped `extern class` value types (whose C++ destructor manages RAII cleanup automatically upon leaving scope) or as 64-bit integer handles (`type OpaquePtr = int;` backed by `reinterpret_cast<uintptr_t>` inside thin C++ inline wrapper headers). Resources allocated in native C++ and returned to KeetScript are owned by their enclosing `extern class` instance; no manual deallocation is required or permitted.

```keet
import "./native/fast_hash.hpp";

type OpaquePtr = int;

extern "C++" fn crc32_hash(data: string, seed: int): int;
extern "C++" fn transform_buffer(mut buf: array<int>, scale: int): void;
extern "C" fn native_open_context(flags: int): OpaquePtr;
extern "C" fn native_close_context(ctx: OpaquePtr): void;

extern "C++" class DigestEngine {
    DigestEngine(seed: int);
    pub fn update(chunk: string): void;
    pub const fn finalize(): string;
}

export fn computeChecksum(payload: string): string {
    let engine: DigestEngine = DigestEngine(0);
    engine.update(payload);
    return engine.finalize();
}
```
