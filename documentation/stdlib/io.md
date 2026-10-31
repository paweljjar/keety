# KeetScript Standard Library: `io` Module

The `io` module provides type-safe console output, line-oriented standard input reading, and ANSI terminal color formatting. It is part of the KeetScript standard library and is built into the `keety` compiler and runtime.

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
Prints zero or more arguments to standard output (`stdout`) separated by spaces, without a trailing newline.
- If the final argument is an `io.Color`, the entire formatted line is styled using the specified color code.
- Passing `io.Color` anywhere other than the trailing position is a compile-time error (`E3003`).

#### `io.println(...args: Any, color?: io.Color): void`
Identical to `io.print`, but appends a newline character (`\n`) and flushes standard output.

### 2.3 Console Input Functions

#### `io.readln(): string`
Reads a single line of text from standard input (`stdin`) up to the terminating newline (`\n`).
- Strips any trailing carriage return (`\r`) automatically for Windows/POSIX CRLF compatibility.
- Returns an empty string `""` on end-of-file (EOF) or stream error.

#### `io.input(prompt?: string): string`
Writes the optional `prompt` string to standard output, flushes the stream immediately, and returns the result of `io.readln()`.

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

---

## 3. Terminal Detection and `NO_COLOR`

1. **Automatic TTY Detection:**
   ANSI color escape sequences are emitted only when `stdout` is connected to an interactive terminal (`isatty(fileno(stdout))` on POSIX, `_isatty` on Windows).
2. **`NO_COLOR` Support:**
   If the `NO_COLOR` environment variable is set (regardless of its value), color styling is automatically suppressed.
3. **C++ Testing Override:**
   In C++ unit test harnesses, `keet::io::set_color_enabled(bool)` allows deterministic verification of escape codes regardless of terminal redirection.

---

## 4. C++ AOT Lowering

The `keety` compiler lowers `io` calls directly into C++20 standard library calls and runtime primitives defined in `<keet/io.hpp>`:

| KeetScript Expression | Emitted C++20 |
| :--- | :--- |
| `io.print("Count:", n)` | `::keet::io::print("Count:", n);` |
| `io.println("Done", io.Color.Green)` | `::keet::io::println("Done", ::keet::io::Color::Green);` |
| `io.readln()` | `::keet::io::readln();` |
| `io.input("Name: ")` | `::keet::io::input("Name: ");` |
| `io.green("OK")` | `::keet::io::green("OK");` |
| `let c: io.Color = io.Color.Red;` | `::keet::io::Color c = ::keet::io::Color::Red;` |
