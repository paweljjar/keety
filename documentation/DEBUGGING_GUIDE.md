# KeetScript Debugging Guide

This document describes how to debug KeetScript applications using native debuggers (LLDB, GDB) and Visual Studio Code, leveraging Keety's **Source-to-Source Line Mapping** infrastructure.

---

## 1. Overview of Source-to-Source Line Mapping

Keety compiles `.keet` source files ahead-of-time (AOT) to intermediate C++ (`.hpp` and `.cpp`), which is then compiled and linked into native binaries using platform toolchains (Clang, GCC, MSVC).

To provide a seamless first-class debugging experience, Keety inserts `#line <line> "<file>.keet"` directives directly before:
- Function and method declarations
- Statements (variable declarations, expressions, control flow, loops, returns)
- Lowered IR basic blocks and conditional branches

When compiling in debug mode, the native compiler embeds DWARF or CodeView debug tables directly referencing originating `.keet` file paths and line numbers instead of generated C++ files.

---

## 2. Building for Debugging

To compile your application with debug symbols and line mapping enabled:

```bash
# Build with debug symbols (-g) and default optimizations
keety build -g app.keet

# Alternatively, build with -O0 for maximum debugging clarity (unoptimized variables and control flow)
keety build -O0 app.keet
```

### Suppressing Line Directives

If you want to inspect generated C++ files without `#line` directives (for example, when debugging compiler backend code generation or C++ code generation issues):

```bash
# Emit C++ without line directives
keety emit app.keet --cpp --no-line-directives

# Compile native binary without line directives
keety build app.keet --no-line-directives
```

---

## 3. Debugging with LLDB (macOS & Linux)

### Launching the Debugger

```bash
lldb ./app
```

### Setting Breakpoints on `.keet` Files

You can set breakpoints directly on `.keet` source filenames and line numbers:

```lldb
(lldb) breakpoint set -f app.keet -l 15
Breakpoint 1: where = app`compute(int) + 24 at app.keet:15:5, address = 0x0000000100000f10
```

### Running and Stepping

```lldb
# Start execution
(lldb) run

# Step over the current statement
(lldb) next (or n)

# Step into function calls
(lldb) step (or s)

# Step out of current function
(lldb) finish

# Continue execution
(lldb) continue (or c)
```

### Inspecting Variables and Backtraces

```lldb
# Print call stack showing .keet filenames and line numbers
(lldb) bt

# Print local variables
(lldb) frame variable

# Evaluate expressions
(lldb) print my_variable
```

---

## 4. Debugging with GDB (Linux & Windows MinGW)

```bash
# Start GDB
gdb ./app

# Set breakpoint by .keet file and line
(gdb) break app.keet:15

# Run application
(gdb) run

# View backtrace
(gdb) backtrace

# Inspect variables
(gdb) info locals
(gdb) print my_var
```

---

## 5. Visual Studio Code Configuration

You can debug KeetScript natively in Visual Studio Code using the **CodeLLDB** or **C/C++** extension.

### `launch.json` Example (CodeLLDB)

Create or update `.vscode/launch.json` in your workspace:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug KeetScript Application",
            "type": "lldb",
            "request": "launch",
            "program": "${workspaceFolder}/app",
            "args": [],
            "cwd": "${workspaceFolder}",
            "preLaunchTask": "keety-build-debug",
            "stopOnEntry": false
        }
    ]
}
```

### `tasks.json` Example

Configure `.vscode/tasks.json` to build before launching:

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "keety-build-debug",
            "type": "shell",
            "command": "keety build -g app.keet",
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "problemMatcher": ["$gcc"]
        }
    ]
}
```

With this setup:
1. Open any `.keet` file in VS Code.
2. Click in the gutter to place a breakpoint (red dot).
3. Press **F5** to start debugging. Execution will pause on the selected `.keet` line.

---

## 6. Compiler Diagnostics & Error Mapping

Because `#line` directives are active during intermediate compilation, compiler errors and warnings from backend C++ compilers report originating `.keet` line locations:

```
app.keet:42:15: error: type mismatch in assignment
   42 |     let total: int = "text";
      |                      ^~~~~~
```
