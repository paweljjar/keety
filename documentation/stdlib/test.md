# KeetScript Standard Library: `test` Module

The `test` module provides a comprehensive, built-in unit testing framework, assertion engine, and test discovery runner for KeetScript applications. It is part of the KeetScript standard library and is built directly into the `keety` compiler and runtime.

---

## 1. Importing the Module

The `test` module can be imported using qualified module syntax or selective member imports:

```keet
// Qualified import
import test;

fn main(): void {
    test.suite("Math Suite", fn(): void {
        test.case("addition", fn(): void {
            test.assertEq(2 + 2, 4);
        });
    });
}
```

```keet
// Selective import
from "test" import suite, case, assertEq, assertTrue, assertZonks;

fn main(): void {
    suite("Validation", fn(): void {
        case("positive balance", fn(): void {
            assertTrue(100 > 0);
        });
    });
}
```

---

## 2. API Reference

### 2.1 Test Suite & Case Registration

#### `test.suite(name: string, fn: (): void): void`
Defines a named test suite block to group logically related test cases. Suites can be nested hierarchically; child suites are concatenated in reporting using the ` > ` separator (e.g. `Math > Arithmetic > addition`).

#### `test.case(name: string, fn: (): void): void`
Registers an individual test case with the testing registry. When executed, if the callback completes without throwing an assertion error or uncaught exception, the test case is marked as passed. If any assertion fails or an exception is thrown, the test case fails with full diagnostic details (expected, actual, file, line).

### 2.2 Assertion Primitives

#### `test.assertEq(actual: Any, expected: Any, msg?: string): void`
Asserts that `actual` and `expected` are deeply equal.
- Supports primitives (integers, floats, booleans, strings), arrays, maps, and objects.
- On failure, throws an `AssertionError` capturing the expected value, the actual value, source file location, and line number.
- An optional custom failure message can be supplied.

```keet
test.assertEq(user.score, 100, "score must match initial configuration");
```

#### `test.assertNe(actual: Any, expected: Any, msg?: string): void`
Asserts that `actual` and `expected` are not equal. On failure, throws an `AssertionError`.

```keet
test.assertNe(token, "", "token should not be empty");
```

#### `test.assertTrue(condition: bool, msg?: string): void`
Asserts that a boolean expression evaluates to `true`. On failure, throws an `AssertionError`.

```keet
test.assertTrue(items.length > 0, "collection must not be empty");
```

#### `test.assertFalse(condition: bool, msg?: string): void`
Asserts that a boolean expression evaluates to `false`. On failure, throws an `AssertionError`.

```keet
test.assertFalse(session.isExpired(), "session should be active");
```

#### `test.assertZonks(fn: (): void, expectedError?: string, msg?: string): void`
Asserts that invoking the callback `fn` throws an error (zonks).
- If `expectedError` is provided, verifies that the caught error's name matches `expectedError` (e.g. `"DivisionByZero"`, `"KeyNotFound"`).
- If the callback succeeds without throwing, or throws a different error type, an `AssertionError` is raised.

```keet
test.assertZonks(fn(): void {
    let result: int = 10 / 0;
}, "DivisionByZero", "dividing by zero must zonk");
```

#### `test.fail(msg?: string): void`
Unconditionally fails the current test case with the specified failure message.

```keet
if (unreachableBranchTaken) {
    test.fail("unreachable branch executed");
}
```

### 2.3 Standalone Runner

#### `test.run(filter?: string): int`
Executes all registered test cases, inspects command-line flags (`--filter`, `--json`, `--tap`, `--fail-fast`), prints the formatted test report, and returns `0` if all tests pass or `1` if any test fails. When compiling a KeetScript test file with `keety test` or `keety run`, the runner entry point is automatically invoked.

---

## 3. CLI Test Runner (`keety test`)

### 3.1 Command Syntax

```bash
keety test [path] [options]
```

- When `path` is a specific `.keet` file: Compiles and executes only that test file.
- When `path` is a directory (or omitted): Recursively scans the directory and runs all discovered test suites.

### 3.2 Discovery Heuristics
The CLI test runner automatically discovers test files matching:
1. `*.test.keet` (e.g. `auth.test.keet`, `math.test.keet`)
2. `test_*.keet` (e.g. `test_api.keet`, `test_parser.keet`)
3. Any `.keet` file inside a `tests/` directory containing `import test;` or `from "test" import ...;`

Directories such as `.git`, `.keety_build`, `build`, and `node_modules` are automatically ignored.

### 3.3 Command-Line Options

| Flag | Argument | Description |
| :--- | :--- | :--- |
| `-f, --filter` | `<pattern>` | Filter test cases by regex pattern or substring matching name or suite. |
| `--json` | None | Emit structured JSON test summary to standard output for CI and tooling. |
| `--tap` | None | Emit standard Test Anything Protocol (TAP version 13) output. |
| `--fail-fast` | None | Immediately abort test execution upon the first failed assertion. |
| `--no-color` | None | Disable ANSI color escape codes in terminal output. |
| `-v, --verbose` | None | Display verbose compilation and execution diagnostics. |

---

## 4. Output Formats

### 4.1 Human-Readable Console Reporter (Default)

```text
 PASS  tests/math.test.keet (3 tests, 4ms)
   ✓ integer addition preserves sign (1ms)
   ✓ safe division zero fallback (1ms)
   ✓ trigonometry clamping (2ms)

 FAIL  tests/auth.test.keet (1 failed, 2 passed, 8ms)
   ✓ password hashing (3ms)
   ✕ token validation with expired timestamp (2ms)
     AssertionError: assertEq failed
       Expected: "expired"
       Actual:   "valid"
       --> tests/auth.test.keet:42

Test Files  1 passed, 1 failed (2 total)
Tests       5 passed, 1 failed (6 total)
Duration    12.4ms
```

### 4.2 Structured JSON Reporter (`--json`)

When `--json` is specified, terminal output is suppressed and a single valid JSON document is emitted to standard output:

```json
{
  "total": 6,
  "passed": 5,
  "failed": 1,
  "skipped": 0,
  "totalDurationNs": 12400000,
  "testFiles": {
    "total": 2,
    "passed": 1,
    "failed": 1
  },
  "results": [
    {
      "name": "token validation with expired timestamp",
      "suite": "Auth",
      "file": "auth.test.keet",
      "passed": false,
      "durationNs": 2000000,
      "failure": {
        "message": "assertEq failed",
        "expected": "\"expired\"",
        "actual": "\"valid\"",
        "file": "tests/auth.test.keet",
        "line": 42,
        "errorType": "AssertionError"
      }
    }
  ]
}
```

### 4.3 TAP Version 13 Reporter (`--tap`)

```text
TAP version 13
1..2
ok 1 - math.test.keet > integer addition preserves sign # time=1ms
not ok 2 - auth.test.keet > Auth > token validation with expired timestamp # time=2ms
  ---
  message: "assertEq failed"
  expected: "\"expired\""
  actual: "\"valid\""
  file: "tests/auth.test.keet"
  line: 42
  ...
```

---

## 5. Package Manager Integration (`kattery test`)

In any package with a `pedigree.json` manifest defining a test script:

```json
{
  "name": "my-service",
  "version": "1.0.0",
  "scripts": {
    "test": "keety test"
  }
}
```

Running `kattery test` (or `kattery test -- --json`, `kattery test -- -f math`) automatically delegates to `keety test`, passing forwarded arguments directly through.
