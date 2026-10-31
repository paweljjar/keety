# KeetScript Conformance Test Suite & Benchmark Specification

This specification defines the official validation and verification framework for KeetScript implementations and the `keety` compiler. It establishes the test harness architecture, positive semantic test suites, negative compiler diagnostic test cases, and reference performance benchmark suites comparing generated C++ against native, handwritten C++17.

---

## 1. Test Harness Architecture (`keety test`)

The test harness evaluates compliance against [The KeetScript Language Specification.md](file:///Users/pawel/Documents/Keet%20Documentation/The%20KeetScript%20Language%20Specification.md) across two distinct tiers:

```
tests/
├── conformance/
│   ├── positive/      # Must compile cleanly and execute with expected outputs
│   │   ├── 01_scalars/
│   │   ├── 02_values_mut/
│   │   ├── 03_unions/
│   │   ├── 04_oop/
│   │   ├── 05_control_errors/
│   │   └── 06_io/
│   └── negative/      # Must fail compilation at designated stage with expected error codes
│       ├── E1000_modules/
│       ├── E2000_scoping/
│       ├── E3000_types/
│       └── E4000_borrowing/
└── benchmarks/
    ├── 01_numeric_vector/
    ├── 02_map_variant/
    ├── 03_string_parsing/
    └── 04_async_concurrency/
```

### Execution Command:
```bash
keety test [--conformance | --benchmarks | --all] [--filter <pattern>]
```

---

## 2. Positive Conformance Test Cases (Semantic Soundness)

### Group 1: Scalars & Zero-UB Guarantees
* **Test 1.1 (`scalars_wraparound.keet`):**
  * **Objective:** Verify deterministic 64-bit two's-complement overflow.
  * **Input:**
    ```keet
    let maxInt: int = 9223372036854775807;
    let wrapped: int = maxInt + 1;
    let minInt: int = -9223372036854775808;
    let negatedMin: int = -minInt;
    ```
  * **Assertion:** `wrapped == -9223372036854775808` and `negatedMin == -9223372036854775808`.
* **Test 1.2 (`scalars_divzero_recover.keet`):**
  * **Objective:** Verify deterministic runtime error raising on integer division by zero and recovery via `zonk`.
  * **Input:**
    ```keet
    fn safeDiv(a: int, b: int): int {
        return (a / b) zonk -1;
    }
    ```
  * **Assertion:** `safeDiv(10, 2) == 5` and `safeDiv(10, 0) == -1`.
* **Test 1.3 (`scalars_float_clamp_bounds.keet`):**
  * **Objective:** Verify float conversion bounds checking and `.clamp()`.
  * **Input:**
    ```keet
    let overflowInt: int = int(1e25) zonk -999;
    let nanInt: int = int(0.0 / 0.0) zonk -111;
    let clamped: float = (15.5).clamp(0.0, 10.0);
    ```
  * **Assertion:** `overflowInt == -999`, `nanInt == -111`, and `clamped == 10.0`.

---

### Group 2: Value Semantics & `mut` Borrowing
* **Test 2.1 (`value_semantics_collections.keet`):**
  * **Objective:** Ensure deep value copy on assignment and parameter passing.
  * **Input:**
    ```keet
    let a: array<int> = [1, 2, 3];
    let b: array<int> = a;
    b.push(4);
    ```
  * **Assertion:** `a.length() == 3` and `b.length() == 4`.
* **Test 2.2 (`mut_parameter_swap.keet`):**
  * **Objective:** Verify in-place mutation of caller's addressable variables via `mut`.
  * **Input:**
    ```keet
    fn swap<T>(mut x: T, mut y: T): void {
        let temp: T = x;
        x = y;
        y = temp;
    }
    let u: int = 10, v: int = 20;
    swap(mut u, mut v);
    ```
  * **Assertion:** `u == 20` and `v == 10`.

---

### Group 3: Union Types & Flow Narrowing
* **Test 3.1 (`union_flow_narrowing.keet`):**
  * **Objective:** Validate static union narrowing and runtime type inspection.
  * **Input:**
    ```keet
    fn formatValue(v: int | string | null): string {
        if (typeof(v) == "int") {
            return `int:${v.toString()}`;
        } elif (typeof(v) == "string") {
            return `str:${v}`;
        } else {
            return "empty";
        }
    }
    ```
  * **Assertion:** `formatValue(42) == "int:42"`, `formatValue("hi") == "str:hi"`, `formatValue(null) == "empty"`.
* **Test 3.2 (`union_narrowing_invalidation.keet`):**
  * **Objective:** Verify that reassignment inside a guarded block invalidates narrowed types.
  * **Input:**
    ```keet
    let item: int | string = 100;
    if (typeof(item) == "int") {
        let doubled: int = item * 2; // Valid: narrowed to int
        item = "reassigned";         // Invalidates narrowing!
        // let strLen: int = item.length(); // Compile-time error: item is now int | string again
    }
    ```
  * **Assertion:** Compiles and executes cleanly without type errors.

---

### Group 4: OOP, Inheritance & Constructors
* **Test 4.1 (`class_inheritance_constructor.keet`):**
  * **Objective:** Validate derived class constructor calling base constructor via `: inherited(...)`.
  * **Input:**
    ```keet
    class Entity {
        pub let id: int;
        Entity(id: int) { this.id = id; }
        pub fn describe(): string { return `ID:${this.id}`; }
    }

    class Player inherits Entity {
        pub let name: string;
        Player(id: int, name: string) : inherited(id) {
            this.name = name;
        }
        inherited pub fn describe(): string {
            return `${inherited.describe()};Name:${this.name}`;
        }
    }
    let p: Player = Player(1, "Ada");
    ```
  * **Assertion:** `p.describe() == "ID:1;Name:Ada"`.

---

### Group 5: Control Flow & Error Recovery
* **Test 5.1 (`switch_named_loop_break.keet`):**
  * **Objective:** Validate that `break switchName;` exits `switch` without terminating outer loop.
  * **Input:**
    ```keet
    let count: int = 0;
    for (let i: int = 0; i < 5; i++) as loopHeader {
        switch (i) as switchHeader {
            case (2) {
                break switchHeader; // Exits switch only!
            }
            default {
                count += 1;
            }
        }
    }
    ```
  * **Assertion:** `count == 4`.
* **Test 5.2 (`zonk_block_yield.keet`):**
  * **Objective:** Validate scoped `zonk (err) { yield val; }` expression value resolution.
  * **Input:**
    ```keet
    let res: int = "bad_num".toInt() zonk (err) {
        yield 404;
    };
    ```
  * **Assertion:** `res == 404`.

---

### Group 6: Standard I/O Module & Console Operations
* **Test 6.1 (`io_console.keet`):**
  * **Objective:** Validate standard `io` module imports, console printing, inline and line-level ANSI styling, and stream input reading.
  * **Input:**
    ```keet
    import io;

    fn main(): int {
        io.print("KeetScript standard I/O test running...\n");
        io.print("Primitives: ", 123, " ", 45.67, " ", true, " ", 'K', "\n");
        io.println("Line with trailing color", io.Color.Green);

        let styled_ok: string = io.green("SUCCESS");
        let styled_warn: string = io.yellow("WARNING");
        let styled_bold: string = io.bold("BOLD");
        io.println("Status:", styled_ok, styled_warn, styled_bold);

        let line1: string = io.readln();
        if (line1 != "hello keetscript") {
            return 1;
        }

        let line2: string = io.input("Enter value: ");
        if (line2 != "sample input") {
            return 2;
        }

        let defaultColor: io.Color = io.Color.Default;
        let redColor: io.Color = io.Color.Red;
        if (defaultColor == redColor) {
            return 3;
        }

        io.println("All standard I/O conformance checks passed!");
        return 0;
    }
    ```
  * **Assertion:** Exits cleanly with status code `0` and outputs formatted text to stdout.

---

## 3. Negative Compiler Diagnostic Test Cases (Compile-Time Rejection)

| Test File | Target Error Code | Expected Diagnostic Trigger |
| :--- | :--- | :--- |
| `neg_shadowing.keet` | `E2001` | Local variable shadows an identifier declared in an enclosing scope. |
| `neg_mut_alias.keet` | `E4001` | Passing the same mutable storage path twice in a single call: `swap(mut x, mut x)`. |
| `neg_container_elem.keet` | `E4002` | Passing container and its element simultaneously to `mut`: `f(mut arr[i], mut arr)`. |
| `neg_map_mut.keet` | `E4003` | Passing map element `m[k]` as a `mut` reference argument. |
| `neg_export_const_write.keet`| `E2002` | Attempting to assign to an imported `export let` variable through its `const`-view. |
| `neg_circular_module.keet` | `E1001` | Cyclic module dependency graph: `A` imports `B` and `B` imports `A`. |
| `neg_missing_type.keet` | `E3001` | Declaring variable or constant without explicit type annotation: `let x = 10;`. |
| `neg_implicit_widening.keet` | `E3002` | Assigning integer directly to float without cast: `let f: float = 42;`. |
| `neg_async_mut.keet` | `E4004` | Declaring `mut` reference parameter inside an `async fn`. |
| `neg_io_type_mismatch.keet` | `E3003` | Passing invalid argument type to standard `io` function or member: `io.input(42)`. |

---


## 4. Reference Benchmark Suite (KeetScript vs. Idiomatic C++17)

### Benchmark Methodology:
* **Compilers:** Clang 16+ and GCC 12+.
* **Optimization Flags:** `-O3 -DNDEBUG -march=native`.
* **Environment:** Isolated, pinned CPU core; 5 warm-up runs followed by 10 measurement iterations reporting the statistical median.
* **Target Threshold:** The generated C++ must achieve execution time $\le 1.05\times$ (within 5%) of hand-optimized, idiomatic native C++17.

---

### Benchmark 1: Heavy Numeric Vector Processing
* **Task:** Initialize a vector of 10,000,000 integers, filter even numbers, square each element, and compute the sum using parallel/vectorized reductions.
* **KeetScript Implementation:**
  ```keet
  let data: array<int>;
  for (let i: int = 0; i < 10000000; i++) {
      data.push(i);
  }
  let sum: int = data
      .filter(fn (x: int): bool { return x % 2 == 0; })
      .map<int>(fn (x: int): int { return x * x; })
      .reduce<int>(0, fn (acc: int, x: int): int { return acc + x; });
  ```
* **Native C++17 Reference:**
  ```cpp
  std::vector<int64_t> data;
  data.reserve(10000000);
  for (int64_t i = 0; i < 10000000; ++i) data.push_back(i);
  std::vector<int64_t> filtered;
  filtered.reserve(data.size() / 2);
  for (int64_t x : data) if (x % 2 == 0) filtered.push_back(x * x);
  int64_t sum = std::accumulate(filtered.begin(), filtered.end(), 0LL);
  ```
* **Performance Target:** Ratio $\frac{T_{\text{Keet}}}{T_{\text{C++}}} \le 1.02$.

---

### Benchmark 2: Hash Map & Union Variant Dispatch
* **Task:** Insert 1,000,000 polymorphic records of union type `int | string` into a hash map, followed by 1,000,000 lookups with `typeof` branch dispatch.
* **KeetScript Implementation:**
  ```keet
  let lookup: map<string, int | string>;
  for (let i: int = 0; i < 1000000; i++) {
      let key: string = i.toString();
      lookup[key] = i % 2 == 0 ? i : `val_${i}`;
  }
  let totalLen: int = 0;
  for (const k: string, v: int | string in lookup) {
      if (typeof(v) == "int") {
          totalLen += v;
      } else {
          totalLen += (v as string).length();
      }
  }
  ```
* **Native C++17 Reference:** Uses `std::unordered_map<std::string, std::variant<int64_t, std::string>>` and `std::holds_alternative<int64_t>(v)`.
* **Performance Target:** Ratio $\frac{T_{\text{Keet}}}{T_{\text{C++}}} \le 1.04$.

---

### Benchmark 3: High-Throughput String Ingestion
* **Task:** Parse 5,000,000 formatted numeric strings from an in-memory buffer using zero-unwinding `.tryInt()` methods.
* **KeetScript Implementation:**
  ```keet
  let total: int = 0;
  for (const raw: string in lines) {
      if (let parsed: int | null = raw.tryInt(); parsed != null) {
          total += parsed;
      }
  }
  ```
* **Native C++17 Reference:** Uses `std::from_chars` directly from `<charconv>`.
* **Performance Target:** Ratio $\frac{T_{\text{Keet}}}{T_{\text{C++}}} \le 1.01$ (identical throughput).

---

### Benchmark 4: Asynchronous Task Concurrency
* **Task:** Dispatch 50,000 lightweight asynchronous computation tasks to background worker threads using `async` functions and synchronize completions via `await`.
* **KeetScript Implementation:**
  ```keet
  async fn computeHash(val: int): int {
      return (val * 2654435761) % 1000000;
  }

  let tasks: array<async () => int>;
  for (let i: int = 0; i < 50000; i++) {
      tasks.push(async fn (): int { return await computeHash(i); });
  }
  ```
* **Native C++17 Reference:** Uses a work-stealing thread pool and `std::future<int64_t>`.
* **Performance Target:** Ratio $\frac{T_{\text{Keet}}}{T_{\text{C++}}} \le 1.05$.

