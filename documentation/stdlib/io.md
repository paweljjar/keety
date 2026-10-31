# KeetScript Standard Library: `io` Module

The `io` module provides type-safe console output, line-oriented standard input reading, stream control, and ANSI terminal color formatting. It is part of the KeetScript standard library and is built into the `keety` compiler and runtime.

---

## 1. Importing the Module

The `io` module can be imported using either a qualified module import or selective member imports:

```keet
// Qualified import
import io;

fn main(): void {
    io.println("Hello, KeetScript!");
}
```

```keet
// Selective import
from "io" import println, green, Color;

fn main(): void {
    println("Build status:", green("SUCCESS"), Color.Green);
}
```

---

## 2. API Reference

### 2.1 Enumerations

#### `io.Color`
Represents terminal ANSI text colors for whole-line styling:

```keet
enum Color {
    Default,  // Reset / default terminal foreground
    Red,      // ANSI 31m
    Green,    // ANSI 32m
    Yellow,   // ANSI 33m
    Blue,     // ANSI 34m
    Magenta,  // ANSI 35m
    Cyan,     // ANSI 36m
    White,    // ANSI 37m
    Gray      // ANSI 90m
}
```

### 2.2 Console Output Functions

#### `io.print(...args: Any, color?: io.Color): void`
Prints zero or more arguments to standard output (`stdout`) separated by spaces, without a trailing newline and without automatically flushing the stream.
- Supports primitive types, booleans (`true`/`false`), `null`, optionals, variants, arrays (`[1, 2, 3]`), and maps (`{k: v}`).
- If the final argument is an `io.Color`, the entire formatted line is styled using the specified color code.
- Passing `io.Color` anywhere other than the trailing position is a compile-time error (`E3003`).

#### `io.println(...args: Any, color?: io.Color): void`
Identical to `io.print`, but appends a newline character (`\n`) to standard output. It does **not** automatically flush the output stream.

#### `io.eprint(...args: Any, color?: io.Color): void`
Identical to `io.print`, but writes formatted output to standard error (`stderr`).

#### `io.eprintln(...args: Any, color?: io.Color): void`
Identical to `io.println`, but writes formatted output followed by a newline character (`\n`) to standard error (`stderr`).

#### `io.flush(): void`
Explicitly flushes the standard output stream buffer.

### 2.3 Console Input Functions

#### `io.read_line(): string | null`
Reads a single line of text from standard input (`stdin`) up to the terminating newline (`\n`).
- Strips any trailing carriage return (`\r`) automatically for cross-platform CRLF compatibility.
- Returns `string` on success, including `""` for an empty line.
- Returns `null` when end-of-file (EOF) is reached without reading any characters.
- Throws an `IOError` if an unrecoverable stream error occurs (`badbit`).

#### `io.readln(): string`
Reads a single line of text from standard input (`stdin`) up to the terminating newline (`\n`).
- Preserved for backward compatibility. Returns an empty string `""` on end-of-file (EOF) or stream error.
- Use `io.read_line()` when distinguishing between an empty line and EOF is required.

#### `io.eof(): bool`
Returns `true` if standard input has reached end-of-file (EOF), and `false` otherwise. In standard stream I/O semantics, `eof()` becomes true after a read operation has attempted to read past the end of the input stream (i.e., the stream `eofbit` is set).

#### `io.input(prompt?: string): string`
Writes the optional `prompt` string to standard output, flushes standard output immediately before waiting for input, and returns the result of `io.readln()`.

### 2.4 Inline ANSI Styling Helpers

All styling helpers accept a single `string` argument and return a new styled `string`:

| Function | ANSI Code | Description |
| :--- | :--- | :--- |
| `io.red(s: string): string` | `\033[31m` | Red foreground text |
| `io.green(s: string): string` | `\033[32m` | Green foreground text |
| `io.yellow(s: string): string` | `\033[33m` | Yellow foreground text |
| `io.blue(s: string): string` | `\033[34m` | Blue foreground text |
| `io.magenta(s: string): string` | `\033[35m` | Magenta foreground text |
| `io.cyan(s: string): string` | `\033[36m` | Cyan foreground text |
| `io.white(s: string): string` | `\033[37m` | White foreground text |
| `io.gray(s: string): string` | `\033[90m` | Gray foreground text |
| `io.bold(s: string): string` | `\033[1m` | Bold text |

#### Styling Guarantees
- **Empty Strings:** When styling an empty string `""`, no escape codes are emitted, returning `""`.
- **No Duplicate Trailing Resets:** When wrapping a styled string that already terminates with `\033[0m`, no redundant trailing reset code is appended.
- **Nested Styling:** When nested styling helpers are combined with surrounding text (e.g., `io.bold("Prefix " + io.red("Alert") + " Suffix")`), inner reset sequences (`\033[0m`) re-assert the enclosing style (`\033[1m`), ensuring subsequent text retains outer styling until the final reset.

---

## 3. Terminal Detection, Stream Independence, and `NO_COLOR`

1. **Precedence Hierarchy:**
   - **Explicit User Override:** Programmatic configuration (via `keet::io::set_color_enabled(bool)`) takes highest precedence.
   - **`NO_COLOR` Environment Variable:** If `NO_COLOR` is present in the environment (even if set to an empty string `""`), automatic terminal styling is disabled.
   - **Automatic TTY Detection:** If `NO_COLOR` is unset and no explicit override is provided, ANSI escape codes are emitted only if the destination stream is connected to an interactive terminal (`isatty` on POSIX, `_isatty` on Windows).
2. **Stream Independence:**
   `stdout` and `stderr` are evaluated independently. Redirecting `stdout` to a file does not disable color output on `stderr` when `stderr` remains attached to an interactive terminal.
3. **C++ Testing Override:**
   In C++ unit test harnesses, `keet::io::set_color_enabled(bool)` and `keet::io::reset_color_enabled()` allow deterministic testing without altering the developer's terminal environment.

---

## 4. C++ AOT Lowering

The `keety` compiler lowers `io` calls directly into standard C++ calls and runtime primitives defined in `<keet/io.hpp>`:

| KeetScript Expression | Emitted C++ |
| :--- | :--- |
| `io.print("Count:", n)` | `::keet::io::print("Count:", n);` |
| `io.println("Done", io.Color.Green)` | `::keet::io::println("Done", ::keet::io::Color::Green);` |
| `io.eprint("Error:", err)` | `::keet::io::eprint("Error:", err);` |
| `io.eprintln("Fatal error")` | `::keet::io::eprintln("Fatal error");` |
| `io.read_line()` | `::keet::io::read_line();` |
| `io.readln()` | `::keet::io::readln();` |
| `io.eof()` | `::keet::io::eof();` |
| `io.input("Name: ")` | `::keet::io::input("Name: ");` |
| `io.flush()` | `::keet::io::flush();` |
| `io.green("OK")` | `::keet::io::green("OK");` |
| `let c: io.Color = io.Color.Red;` | `::keet::io::Color c = ::keet::io::Color::Red;` |
