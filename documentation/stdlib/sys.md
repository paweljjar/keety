# KeetScript Standard Library: `sys` Module

The `sys` module provides access to system environment variables, process lifecycle management, hardware query primitives, platform detection, and subprocess execution. It is built into the KeetScript standard library and compiler toolchain (`keety`).

All fallible system operations throw `keet::error` instances named `"SystemError"`, which integrate seamlessly with KeetScript's native `zonk` error recovery operators and blocks.

---

## 1. Importing the Module

The `sys` module can be imported using any of the four standard import forms (as well as module aliases):

```keet
// 1. Unquoted module import
import sys;

// 2. Quoted module import
import "sys";

// 3. Unquoted selective import
from sys import args, getEnv, getEnvOr, platform, cpuCount;

// 4. Quoted selective import
from "sys" import args, getEnv, getEnvOr, platform, cpuCount;
```

```keet
// Module alias import
import sys as system;

fn main(): void {
    let p: int = sys.pid();
    let os: string = system.platform();
    let home: string = getEnvOr("HOME", "/tmp");
    let cpus: int = cpuCount();
}
```

---

## 2. API Reference

### 2.1 Command-Line Arguments & Process Lifecycle

#### `sys.args(): array<string>`
Returns an array of strings representing the command-line arguments passed to the running process.
- The arguments are automatically initialized upon entry to `main()` by the runtime wrapper (`::keet::sys::initArgs(argc, argv)`).
- `sys.args()[0]` contains the executable invocation name or path.
- Example:
  ```keet
  let cmd_args: array<string> = sys.args();
  if (cmd_args.len() > 1) {
      io.println("First argument: " + cmd_args[1]);
  }
  ```

#### `sys.pid(): int`
Returns the process ID of the current running process as a 64-bit integer (`int64_t`).
- Uses POSIX `getpid()` on Unix/Linux/macOS and `_getpid()` on Windows.

#### `sys.exit(code: int): void`
Terminates the calling process immediately with the specified exit status code.
- Calls `std::exit(code)`.

---

### 2.2 Environment Variables

#### `sys.getEnv(key: string): string | null`
Retrieves the value of an environment variable.
- Returns `string` if the variable exists in the environment.
- Returns `null` if the variable is not set.
- Integrates with KeetScript flow-based type narrowing:
  ```keet
  let path: string | null = sys.getEnv("PATH");
  if (path != null) {
      // In this branch, 'path' is narrowed to type 'string'
      io.println("PATH is: " + path);
  }
  ```

#### `sys.getEnvOr(key: string, fallback: string): string`
Convenience helper that retrieves an environment variable's value or returns a fallback string if unset.
- Example:
  ```keet
  let port: string = sys.getEnvOr("PORT", "8080");
  ```

#### `sys.hasEnv(key: string): bool`
Checks whether an environment variable exists in the current environment.
- Returns `true` if defined, `false` otherwise.

#### `sys.setEnv(key: string, value: string): bool`
Sets or updates an environment variable for the current process.
- Uses `setenv` on POSIX and `_putenv_s` on Windows.
- Throws a `SystemError` if setting the environment variable fails.
- Example:
  ```keet
  sys.setEnv("APP_ENV", "production");
  ```

#### `sys.unsetEnv(key: string): bool`
Removes an environment variable from the current process environment.
- Uses `unsetenv` on POSIX and `_putenv_s(key, "")` on Windows.
- Throws a `SystemError` if unsetting fails.

---

### 2.3 System & Platform Information

#### `sys.platform(): string`
Returns the target operating system name as a lowercase string:
- `"macos"` for Apple macOS / OS X.
- `"linux"` for Linux kernels.
- `"windows"` for Microsoft Windows.
- `"unknown"` for unrecognized platforms.
- Example:
  ```keet
  if (sys.platform() == "macos") {
      io.println("Running on macOS");
  }
  ```

#### `sys.arch(): string`
Returns the target CPU architecture identifier:
- `"x86_64"` for 64-bit x86 / AMD64.
- `"arm64"` for 64-bit ARM (AArch64 / Apple Silicon).
- `"x86"` for 32-bit x86.
- `"unknown"` for unrecognized architectures.

#### `sys.cpuCount(): int`
Returns the number of concurrent hardware threads available on the host system.
- Always returns an integer $\ge 1$.

---

### 2.4 Directory & Path Primitives

#### `sys.cwd(): string`
Returns the canonical current working directory path of the process as a generic UTF-8 string.
- Returns `"."` if an error occurs.

#### `sys.chdir(path: string): bool`
Changes the current working directory of the process to the specified path.
- Throws a `SystemError` if the path does not exist, is not a directory, or lacks permissions.
- Integrates with `zonk`:
  ```keet
  let changed: bool = sys.chdir("/tmp") zonk false;
  ```

#### `sys.homeDir(): string`
Returns the current user's home directory path.
- On POSIX systems, inspects `HOME`.
- On Windows, inspects `USERPROFILE` or `HOMEDRIVE` + `HOMEPATH`.
- Returns an empty string `""` if not resolved.

#### `sys.tempDir(): string`
Returns the operating system's canonical temporary directory path (e.g. `/tmp` or Windows `%TEMP%`).

---

### 2.5 Subprocess Execution

#### `sys.exec(command: string): int`
Executes a shell command synchronously and returns the process exit status code.
- Decomposes POSIX exit codes using `WIFEXITED` / `WEXITSTATUS`.
- Throws a `SystemError` if the command execution fails to start.
- Example:
  ```keet
  let code: int = sys.exec("make build");
  if (code != 0) {
      io.println("Build failed!");
  }
  ```

#### `sys.execOutput(command: string): string`
Executes a command via subprocess pipe (`popen` / `_popen`), captures standard output into a string, and closes the pipe.
- Throws a `SystemError` if the subprocess fails to launch or exits with a non-zero exit status code.
- Fully compatible with `zonk` for safe fallback execution:
  ```keet
  let git_rev: string = sys.execOutput("git rev-parse --short HEAD") zonk "unknown";
  ```
