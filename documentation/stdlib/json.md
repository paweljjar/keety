# KeetScript Standard Library: `json` Module

The `json` module provides RFC 8259-compliant JSON validation, dot-path querying, typed extraction, collection parsing, immutable JSON object/array builders, polymorphic native-value serialization (`json.stringify`), pretty-printing, minification, string escaping, and file I/O. It is built into the KeetScript standard library and compiler toolchain (`keety`).

All operations work directly on KeetScript's native types (`string`, `int`, `float`, `bool`, `array<T>`, `map<string, T>`, and union/nullable types) and adhere to KeetScript's **Strict Zero-Coercion Policy (`E3002`)**.

Fallible operations (such as malformed JSON syntax, missing dot-paths, type mismatches during extraction, `NaN`/`Infinity` float serialization, or file I/O failures) throw runtime `keet::error` instances named `"JsonError"`, integrating seamlessly with KeetScript's native `zonk` error recovery operators and blocks.

---

## 1. Importing the Module

The `json` module can be imported using qualified module imports, module aliases, or selective member imports:

```keet
// Qualified import
import json;

fn main(): void {
    let raw: string = "{\"host\":\"localhost\",\"port\":8080}";
    if (json.isValid(raw)) {
        let host: string = json.getString(raw, "host");
        let port: int = json.getInt(raw, "port");
    }
}
```

```keet
// Module alias
import json as j;

fn main(): void {
    let doc: string = j.setString(j.emptyObject(), "status", "ok");
    let formatted: string = j.format(doc, 2);
}
```

```keet
// Selective import
from "json" import getString, getIntOr, stringify;

fn main(): void {
    let raw: string = "{\"service\":\"keety\"}";
    let name: string = getString(raw, "service");
    let retries: int = getIntOr(raw, "retries", 3);
    let serialized: string = stringify([1, 2, 3]);
}
```

---

## 2. Validation & Type Inspection

| Function | Return Type | Description |
|---|---|---|
| `json.isValid(text: string): bool` | `bool` | Returns `true` if `text` is strictly valid RFC 8259 JSON; `false` otherwise (never throws) |
| `json.typeOf(text: string): string` | `string` | Returns `"object"`, `"array"`, `"string"`, `"number"`, `"bool"`, `"null"`, or `"invalid"` |
| `json.isObject(text: string): bool` | `bool` | Returns `true` if `text` is a valid JSON object (`{...}`) |
| `json.isArray(text: string): bool` | `bool` | Returns `true` if `text` is a valid JSON array (`[...]`) |
| `json.size(text: string): int` | `int` | Returns the entry/element count of a JSON object or array; throws `JsonError` on other types or invalid JSON |
| `json.keys(text: string): array<string>` | `array<string>` | Returns the sorted top-level keys of a JSON object; throws `JsonError` if not an object |

---

## 3. Dot-Path Queries & Typed Extraction

Dot-path strings navigate nested JSON objects and arrays using `.`-separated segments (e.g., `"server.host"`, `"users.0.name"`, `"matrix.1.2"`). An empty path `""` refers to the root JSON value.

| Function | Return Type | Description |
|---|---|---|
| `json.hasKey(text: string, path: string): bool` | `bool` | Returns `true` if `text` is valid JSON and `path` resolves to an existing key or array index (never throws) |
| `json.isNull(text: string, path: string): bool` | `bool` | Returns `true` if `text` is valid JSON and `path` resolves to a JSON `null` literal (never throws) |
| `json.getString(text: string, path: string): string` | `string` | Extracts the unescaped string at `path`; throws `JsonError` if missing or not a string |
| `json.getInt(text: string, path: string): int` | `int` | Extracts an exact 64-bit integer at `path`; throws `JsonError` if missing or not an integer |
| `json.getFloat(text: string, path: string): float` | `float` | Extracts a floating-point number at `path` (accepts integer or float JSON numbers); throws `JsonError` on failure |
| `json.getBool(text: string, path: string): bool` | `bool` | Extracts a boolean at `path`; throws `JsonError` if missing or not a boolean |
| `json.getArray(text: string, path: string): array<string>` | `array<string>` | Extracts a JSON array at `path`, returning each element as an unescaped string (if string) or serialized JSON text |
| `json.getObject(text: string, path: string): string` | `string` | Extracts a nested JSON object at `path` as a serialized JSON string; throws `JsonError` if not an object |

---

## 4. Non-Throwing Fallback Extractors (`*Or`)

Fallback extractors return `defaultVal` instead of throwing when the input JSON is invalid, the dot-path does not exist, or the value at `path` has a different type:

| Function | Return Type | Description |
|---|---|---|
| `json.getStringOr(text: string, path: string, defaultVal: string): string` | `string` | Returns `json.getString(text, path)` on success, or `defaultVal` on any error |
| `json.getIntOr(text: string, path: string, defaultVal: int): int` | `int` | Returns `json.getInt(text, path)` on success, or `defaultVal` on any error |
| `json.getFloatOr(text: string, path: string, defaultVal: float): float` | `float` | Returns `json.getFloat(text, path)` on success, or `defaultVal` on any error |
| `json.getBoolOr(text: string, path: string, defaultVal: bool): bool` | `bool` | Returns `json.getBool(text, path)` on success, or `defaultVal` on any error |

---

## 5. Typed Collection Parsers

| Function | Return Type | Description |
|---|---|---|
| `json.parseMap(text: string): map<string, string>` | `map<string, string>` | Parses a JSON object into a `map<string, string>` (strings unescaped; non-strings serialized as JSON); throws `JsonError` if not an object |
| `json.parseArray(text: string): array<string>` | `array<string>` | Parses a JSON array into `array<string>` (strings unescaped; non-strings serialized as JSON); throws `JsonError` if not an array |
| `json.parseIntArray(text: string): array<int>` | `array<int>` | Parses a homogeneous JSON array of integers into `array<int>`; throws `JsonError` if any element is not an integer |
| `json.parseFloatArray(text: string): array<float>` | `array<float>` | Parses a homogeneous JSON array of numbers into `array<float>`; throws `JsonError` if any element is not a number |
| `json.parseBoolArray(text: string): array<bool>` | `array<bool>` | Parses a homogeneous JSON array of booleans into `array<bool>`; throws `JsonError` if any element is not a boolean |

---

## 6. Immutable Object & Array Builders

Builder functions parse `text`, apply a mutation at `path` (creating intermediate nested objects automatically when setting keys), and return the updated JSON string:

| Function | Return Type | Description |
|---|---|---|
| `json.emptyObject(): string` | `string` | Returns `"{\}"` |
| `json.emptyArray(): string` | `string` | Returns `"[]"` |
| `json.setString(text: string, path: string, value: string): string` | `string` | Sets `path` to a JSON string value and returns the updated JSON document |
| `json.setInt(text: string, path: string, value: int): string` | `string` | Sets `path` to a JSON integer value and returns the updated JSON document |
| `json.setFloat(text: string, path: string, value: float): string` | `string` | Sets `path` to a finite float value and returns the updated JSON document; throws `JsonError` on `NaN`/`Infinity` |
| `json.setBool(text: string, path: string, value: bool): string` | `string` | Sets `path` to a JSON boolean value and returns the updated JSON document |
| `json.setNull(text: string, path: string): string` | `string` | Sets `path` to `null` and returns the updated JSON document |
| `json.setRaw(text: string, path: string, rawJson: string): string` | `string` | Validates `rawJson` and embeds it as a nested JSON value at `path` |
| `json.removeKey(text: string, path: string): string` | `string` | Removes the object key or array index at `path` and returns the updated JSON document |

---

## 7. Polymorphic Serialization, Formatting, Escaping & File I/O

`json.stringify` is compiler-assisted and polymorphic: it accepts `int`, `float`, `bool`, `char`, `string`, `null`, `array<T>`, `map<string, V>`, and union/nullable types (`T | null`, `int | string`, etc.) where all variants are JSON-serializable.

| Function | Return Type | Description |
|---|---|---|
| `json.stringify(value: Any): string` | `string` | Serializes any JSON-compatible native KeetScript value to an RFC 8259 JSON string |
| `json.format(text: string, indent: int): string` | `string` | Validates `text` and pretty-prints it with `indent` spaces per level (`0` to `16`); throws `JsonError` on invalid JSON or out-of-range indent |
| `json.minify(text: string): string` | `string` | Validates `text` and removes all insignificant whitespace |
| `json.escape(str: string): string` | `string` | Escapes a raw string into a double-quoted JSON string literal (`"..."`) |
| `json.unescape(str: string): string` | `string` | Unescapes a JSON string literal (with or without outer quotes); throws `JsonError` on invalid escape sequences |
| `json.readFile(path: string): string` | `string` | Reads UTF-8 file at `path`, validates its JSON content, and returns the canonical JSON string; throws `JsonError` on I/O or parse failure |
| `json.writeFile(path: string, text: string, indent: int): bool` | `bool` | Validates `text`, formats with `indent` spaces (`0` for minified), and writes to `path`; throws `JsonError` on failure |

---

## 8. Practical Usage Examples

### 8.1 Building & Formatting a Configuration Document
```keet
import json;
import io;

fn buildConfig(): string {
    let cfg: string = json.emptyObject();
    cfg = json.setString(cfg, "service.name", "keety-worker");
    cfg = json.setInt(cfg, "service.port", 9090);
    cfg = json.setBool(cfg, "service.tls", true);
    cfg = json.setRaw(cfg, "service.allowedOrigins", json.stringify(["https://example.com", "https://keet.dev"]));
    return json.format(cfg, 2);
}

fn main(): void {
    let pretty: string = buildConfig();
    io.println(pretty);
}
```

### 8.2 Safe Extraction with `zonk` and `*Or` Helpers
```keet
import json;
import io;

fn readServerPort(payload: string): int {
    // Recover from malformed JSON or missing/non-int field using zonk
    let port: int = json.getInt(payload, "server.port") zonk (err) {
        io.eprintln("Warning: invalid port in payload: ", err.message);
    };
    if (port <= 0) {
        return json.getIntOr(payload, "server.defaultPort", 8080);
    }
    return port;
}
```
