# KeetScript Standard Library: `fs` Module

The `fs` module provides type-safe, cross-platform file system operations, file stream reading and writing, directory tree traversal, and path normalization. It is built into the KeetScript standard library and compiler toolchain (`keety`).

All fallible file system operations throw `keet::error` instances named `"FileSystemError"`, which integrate seamlessly with KeetScript's native `zonk` error recovery operators and blocks.

---

## 1. Importing the Module

The `fs` module can be imported using any of the four standard import forms (as well as module aliases):

```keet
// 1. Unquoted module import
import fs;

// 2. Quoted module import
import "fs";

// 3. Unquoted selective import
from fs import readText, writeText, exists;

// 4. Quoted selective import
from "fs" import readText, writeText, exists;
```

```keet
// Module alias import
import fs as files;

fn main(): void {
    let cwd: string = fs.cwd();
    if (!files.exists("config.json")) {
        writeText("config.json", "{}");
    }
}
```

---

## 2. API Reference

### 2.1 File Reading & Writing (Fallible Operations Supporting `zonk`)

#### `fs.readText(path: string): string`
Reads the entire contents of a UTF-8 encoded text file into a string.
- Throws a `FileSystemError` if the file does not exist, cannot be opened, or encounters an I/O read failure.
- Integrates with `zonk` for clean fallback defaults:
  ```keet
  let config: string = fs.readText("app.json") zonk "{}";
  ```

#### `fs.readLines(path: string): array<string>`
Reads a text file into an array of strings, line by line.
- Automatically strips carriage return (`\r`) and newline (`\n`) characters for deterministic behavior across Unix and Windows line endings.
- Throws a `FileSystemError` on missing file or read failure.

#### `fs.writeText(path: string, content: string): bool`
Creates a new file or completely overwrites an existing file with the provided text content.
- Returns `true` on success.
- Throws a `FileSystemError` if the file cannot be opened for writing or if an I/O write error occurs.

#### `fs.appendText(path: string, content: string): bool`
Appends text to the end of an existing file, or creates the file if it does not already exist.
- Returns `true` on success.
- Throws a `FileSystemError` on failure.

---

### 2.2 File & Directory Queries (Non-Throwing Predicates & Scalar Queries)

#### `fs.exists(path: string): bool`
Returns `true` if a file, directory, or special entry exists at the specified path; returns `false` otherwise.
- Non-throwing; internally uses `std::error_code`.

#### `fs.isFile(path: string): bool`
Returns `true` if the target path exists and points to a regular file; returns `false` otherwise.
- Non-throwing.

#### `fs.isDir(path: string): bool`
Returns `true` if the target path exists and points to a directory; returns `false` otherwise.
- Non-throwing.

#### `fs.fileSize(path: string): int`
Returns the size of the target file in bytes as a 64-bit integer (`int64_t`).
- Throws a `FileSystemError` if the file does not exist or its metadata cannot be queried.

---

### 2.3 Directory & File Manipulation (Fallible Mutations)

#### `fs.mkdir(path: string): bool`
Recursively creates the directory specified by `path` and any non-existent parent directories (equivalent to POSIX `mkdir -p`).
- Returns `true` if the directory was created or already existed.
- Throws a `FileSystemError` if creation fails due to permissions or invalid paths.

#### `fs.remove(path: string): bool`
Deletes an individual file or empty directory.
- Returns `true` if the target was deleted, or `false` if it did not exist.
- Throws a `FileSystemError` if the directory is non-empty or permissions prevent deletion.

#### `fs.removeAll(path: string): int`
Recursively deletes a directory and all of its contents, or deletes a file.
- Returns the total count of filesystem entries removed as an integer (`int64_t`).
- Throws a `FileSystemError` on failure.

#### `fs.copy(src: string, dst: string): bool`
Recursively copies a file or directory from `src` to `dst`. Existing destination files are overwritten.
- Returns `true` on success.
- Throws a `FileSystemError` on failure.

#### `fs.rename(src: string, dst: string): bool`
Moves or renames a file or directory from `src` to `dst`.
- Returns `true` on success.
- Throws a `FileSystemError` on failure.

#### `fs.listDir(path: string): array<string>`
Reads the directory at `path` and returns a sorted array of the names of all entries inside it (excluding `.` and `..`).
- Throws a `FileSystemError` if the directory does not exist or cannot be opened.

---

### 2.4 Path Utilities (String Transformations)

All path functions use platform-normalized forward slashes (`/`) in return values for consistency across operating systems.

#### `fs.cwd(): string`
Returns the normalized path of the current working directory of the host process.

#### `fs.join(base: string, child: string): string`
Joins two path components cleanly using standard path separators and resolves redundant `.` and `..` segments.

#### `fs.fileName(path: string): string`
Extracts the filename component including extension from a path:
```keet
fs.fileName("path/to/script.keet"); // returns "script.keet"
```

#### `fs.extension(path: string): string`
Extracts the file extension including the leading dot, or returns `""` if none exists:
```keet
fs.extension("archive.tar.gz"); // returns ".gz"
fs.extension("Makefile");       // returns ""
```

#### `fs.parentDir(path: string): string`
Returns the parent directory path:
```keet
fs.parentDir("a/b/c.txt"); // returns "a/b"
```

---

## 3. Error Handling & `zonk` Integration

Fallible operations in the `fs` module integrate with KeetScript's error recovery mechanism. Errors can be recovered using either fallback values or scoped recovery blocks:

### 3.1 Inline Fallback Expressions
```keet
import fs;

fn loadPortConfig(configPath: string): string {
    // If configPath is missing or cannot be read, use default "{}"
    return fs.readText(configPath) zonk "{}";
}
```

### 3.2 Scoped Recovery Blocks
```keet
import fs;
import io;

fn readUserSettings(path: string): string {
    let settings: string = fs.readText(path) zonk (err) {
        io.eprintln("Failed to read settings from '", path, "': ", err.message);
        yield "{\"theme\": \"dark\"}";
    };
    return settings;
}
```
