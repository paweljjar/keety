# KeetScript Standard Library: `net` Module Implementation Plan

## 1. Executive Summary & Architectural Decisions

The `net` standard library module (`import net;`, `import net as n;`, `from "net" import ...;`) brings cross-platform networking, URL/query string parsing and encoding, DNS resolution, an HTTP/1.1 & HTTPS client, and handle-based TCP and UDP sockets to KeetScript.

### Core Design Principles
1. **Option A — Full Networking Suite on Native KeetScript Types:**
   - Operates purely on KeetScript's native types (`string`, `int`, `bool`, `array<string>`, `map<string, string>`), requiring zero new compiler AST/IR types.
   - Covers 5 cohesive functional areas across **40 `camelCase` functions**:
     1. **URL Parsing, Encoding & Query String Utilities** (`urlEncode`, `urlDecode`, `parseUrl`, `urlScheme`, `urlHost`, `urlPort`, `urlPath`, `urlQuery`, `buildQuery`, `parseQuery`)
     2. **DNS Resolution & IP Address Utilities** (`resolveHost`, `resolveIpv4`, `isIpv4`, `isIpv6`, `isIp`, `isLoopback`, `hostname`)
     3. **HTTP/1.1 & HTTPS Client** (`get`, `getStatus`, `post`, `postJson`, `put`, `deleteReq`, `request`, `download`)
     4. **Handle-Based TCP Sockets** (`tcpConnect`, `tcpConnectTimeout`, `tcpListen`, `tcpAccept`, `tcpLocalPort`, `tcpPeerAddr`, `tcpSend`, `tcpRecv`, `tcpRecvLine`, `tcpSetTimeout`, `tcpClose`)
     5. **Handle-Based UDP Sockets** (`udpBind`, `udpLocalPort`, `udpSendTo`, `udpRecvFrom`, `udpClose`)
2. **Option A — JSON Response Metadata for Rich HTTP & UDP Responses:**
   - Simple HTTP helpers (`net.get`, `net.post`, `net.postJson`, `net.put`, `net.deleteReq`) return the response body `string` directly, while `net.getStatus` returns the HTTP status code `int`.
   - `net.request(method, url, headers, body, timeoutMs)` returns a structured RFC 8259 JSON string:
     ```json
     {
       "status": 200,
       "ok": true,
       "body": "...",
       "headers": { "content-type": "application/json", ... },
       "url": "http://..."
     }
     ```
     which integrates directly with `json.getInt(resp, "status")`, `json.getBool(resp, "ok")`, `json.getString(resp, "body")`, and `json.getString(resp, "headers.content-type")`.
   - Similarly, `net.udpRecvFrom(handle, maxBytes)` returns a JSON string `{"data":"...","host":"127.0.0.1","port":54321}`.
3. **Option A — Safe Handle-Based TCP/UDP Socket Table:**
   - Socket descriptors are managed in a thread-safe (`std::mutex`-protected) runtime handle registry mapping monotonically increasing positive `int64_t` handles (`1, 2, 3, ...`) to OS socket descriptors (`SOCKET` on Windows, `int` file descriptor on POSIX).
   - Prevents raw file-descriptor misuse, double-close undefined behavior, and TCP/UDP socket kind confusion; invalid or closed handles deterministically throw `keet::error("NetError", ...)`.
4. **Option A — Zero External C++ Library Dependencies:**
   - **POSIX (macOS / Linux):** Uses `<sys/socket.h>`, `<netdb.h>`, `<arpa/inet.h>`, `<netinet/in.h>`, `<netinet/tcp.h>`, `<poll.h>`, `<unistd.h>`, and `<fcntl.h>`.
   - **Windows (Win32 / MSVC):** Uses `<winsock2.h>`, `<ws2tcpip.h>`, `#pragma comment(lib, "ws2_32.lib")`, and a thread-safe RAII `WinsockInitializer` (`WSAStartup(MAKEWORD(2, 2), ...)`). Additionally, `ToolchainDriver` links `ws2_32.lib` on MSVC builds.
   - **HTTP & HTTPS Transport:** Native socket-based HTTP/1.1 engine for `http://` (supporting custom headers, `Content-Length`, `Transfer-Encoding: chunked`, connection timeouts, and `301`/`302`/`303`/`307`/`308` redirects up to 5 hops), paired with an automatic OS `curl` subprocess fallback for `https://` URLs so TLS requests work out-of-the-box on macOS, Linux, and Windows without OpenSSL header/linker dependencies.
5. **Strict Language Guardrails:**
   - **Strictly `camelCase`** function names across all 40 functions (zero `snake_case` aliases).
   - **No trailing `!`** on return types in documentation, signatures, or diagnostics.
   - **Strict Zero-Coercion Policy (`E3002`):** Port numbers, byte counts, handles, and timeouts strictly require `int` (`int64_t`); no implicit `float` $\leftrightarrow$ `int` widening or narrowing.
   - **Native `zonk` Error Recovery:** All fallible operations throw `keet::error("NetError", ...)` with descriptive messages.

---

## 2. End-to-End Compiler Pipeline Architecture

```mermaid
flowchart LR
    subgraph Source["KeetScript Source (.keet)"]
        A["import net;\nlet body: string = net.get(url) zonk \"\";\nlet s: int = net.tcpConnect(h, p);"]
    end

    subgraph Frontend["Stage 1–3: Resolver, Sema & TypeChecker"]
        B["ModulePathResolver\nrecognizes 'net', 'std.net', 'keet.net'"]
        C["sema::populateNetModuleScope\n40 exported camelCase functions"]
        D["TypeChecker\ncheckNetMemberCall & checkNetDirectCall\nEnforces E3002, E3003, E3011, E1005"]
    end

    subgraph MiddleEnd["Stage 4–5: KeetIR & Lowering"]
        E["AstLowerer\nTracks net imports & aliases\nLowers to @keet.net.<fn>"]
    end

    subgraph Backend["Stage 6: C++20 Emitter, Driver & Runtime"]
        F["CppEmitter & HeaderEmitter\nEmits ::keet::net::* calls\n#include <keet/std/net.hpp>"]
        G["runtime/keet/net.hpp\nURL/Query + DNS + HTTP/1.1 +\nThread-Safe TCP/UDP Handle Table"]
    end

    A --> B --> C --> D --> E --> F --> G
```

---

## 3. Complete `net` Module API Specification (40 Functions)

### 3.1 URL Parsing, Encoding & Query String Utilities (10 Functions)

| Function Signature | Return Type | Fallible (`zonk`) | Description |
|---|---|---|---|
| `net.urlEncode(text: string): string` | `string` | No | Percent-encodes unreserved characters per RFC 3986 (`A-Z`, `a-z`, `0-9`, `-`, `_`, `.`, `~` left unescaped; all other bytes encoded as `%XX`). |
| `net.urlDecode(text: string): string` | `string` | Yes (`NetError`) | Decodes `%XX` hex sequences and converts `+` to space (`' '`). Throws `NetError` on incomplete or non-hex `%` sequences. |
| `net.parseUrl(url: string): map<string, string>` | `map<string, string>` | Yes (`NetError`) | Parses an absolute URL into a map containing keys `"scheme"`, `"host"`, `"port"`, `"path"`, `"query"`, and `"fragment"`. Throws `NetError` if `url` is missing a scheme or host. |
| `net.urlScheme(url: string): string` | `string` | Yes (`NetError`) | Extracts the lowercase scheme (e.g., `"http"`, `"https"`, `"ws"`). Throws `NetError` on malformed URL. |
| `net.urlHost(url: string): string` | `string` | Yes (`NetError`) | Extracts the hostname or IPv4 / bracket-stripped IPv6 address from `url`. Throws `NetError` on malformed URL. |
| `net.urlPort(url: string): int` | `int` | Yes (`NetError`) | Extracts the explicit port number from `url`, or returns the default port for known schemes (`80` for `http`/`ws`, `443` for `https`/`wss`, `-1` for other schemes without explicit port). Throws `NetError` on invalid port. |
| `net.urlPath(url: string): string` | `string` | Yes (`NetError`) | Extracts the path component of `url` (returns `"/"` if empty). Throws `NetError` on malformed URL. |
| `net.urlQuery(url: string): string` | `string` | Yes (`NetError`) | Extracts the raw query component without the leading `?` (or `""` if none). Throws `NetError` on malformed URL. |
| `net.buildQuery(params: map<string, string>): string` | `string` | No | Serializes `params` into a deterministic, lexicographically sorted URL query string `key1=val1&key2=val2` using `urlEncode`. |
| `net.parseQuery(query: string): map<string, string>` | `map<string, string>` | Yes (`NetError`) | Parses a URL query string (with or without a leading `?`) into decoded key-value pairs using `urlDecode`. Throws `NetError` on malformed `%` encoding. |

### 3.2 DNS Resolution & IP Address Utilities (7 Functions)

| Function Signature | Return Type | Fallible (`zonk`) | Description |
|---|---|---|---|
| `net.resolveHost(host: string): array<string>` | `array<string>` | Yes (`NetError`) | Resolves `host` via `getaddrinfo` into a deduplicated list of numeric IPv4 and IPv6 address strings. Throws `NetError` if `host` is empty or resolution fails. |
| `net.resolveIpv4(host: string): string` | `string` | Yes (`NetError`) | Resolves `host` and returns the first IPv4 (`AF_INET`) address string. Throws `NetError` if no IPv4 address can be resolved. |
| `net.isIpv4(ip: string): bool` | `bool` | No | Returns `true` if `ip` is a valid dotted-quad IPv4 address (`inet_pton(AF_INET, ...)`). |
| `net.isIpv6(ip: string): bool` | `bool` | No | Returns `true` if `ip` is a valid IPv6 address (`inet_pton(AF_INET6, ...)`). |
| `net.isIp(ip: string): bool` | `bool` | No | Returns `true` if `ip` is a valid IPv4 or IPv6 address. |
| `net.isLoopback(ip: string): bool` | `bool` | No | Returns `true` if `ip` is `"localhost"`, `"127.0.0.1"` / `127.0.0.0/8`, `"::1"`, or `"[::1]"`. |
| `net.hostname(): string` | `string` | Yes (`NetError`) | Returns the local system's network hostname (`gethostname`). Throws `NetError` on OS failure. |

### 3.3 HTTP/1.1 & HTTPS Client (8 Functions)

| Function Signature | Return Type | Fallible (`zonk`) | Description |
|---|---|---|---|
| `net.get(url: string): string` | `string` | Yes (`NetError`) | Performs an HTTP/HTTPS `GET` request (30s default timeout) and returns the response body. Throws `NetError` on invalid URL, connection failure, or timeout. |
| `net.getStatus(url: string): int` | `int` | Yes (`NetError`) | Performs an HTTP/HTTPS `GET` request and returns the HTTP response status code (e.g., `200`, `404`, `500`). Throws `NetError` on connection/transport failure. |
| `net.post(url: string, body: string, contentType: string): string` | `string` | Yes (`NetError`) | Performs an HTTP/HTTPS `POST` request with `body` and `Content-Type: <contentType>`, returning the response body. |
| `net.postJson(url: string, jsonBody: string): string` | `string` | Yes (`NetError`) | Performs an HTTP/HTTPS `POST` request with `Content-Type: application/json` and `Accept: application/json`, returning the response body. |
| `net.put(url: string, body: string, contentType: string): string` | `string` | Yes (`NetError`) | Performs an HTTP/HTTPS `PUT` request with `body` and `Content-Type: <contentType>`, returning the response body. |
| `net.deleteReq(url: string): string` | `string` | Yes (`NetError`) | Performs an HTTP/HTTPS `DELETE` request and returns the response body. |
| `net.request(method: string, url: string, headers: map<string, string>, body: string, timeoutMs: int): string` | `string` | Yes (`NetError`) | Executes an HTTP/HTTPS request with custom method, headers, body, and timeout (`timeoutMs > 0`), returning a JSON string `{"status":200,"ok":true,"body":"...","headers":{...},"url":"..."}`. |
| `net.download(url: string, destPath: string): bool` | `bool` | Yes (`NetError`) | Downloads `url` and writes the response body to `destPath`. Throws `NetError` on transport failure, HTTP status $\ge 400$, or file write error. |

### 3.4 Handle-Based TCP Sockets (11 Functions)

| Function Signature | Return Type | Fallible (`zonk`) | Description |
|---|---|---|---|
| `net.tcpConnect(host: string, port: int): int` | `int` | Yes (`NetError`) | Connects a TCP socket to `host:port` (10,000 ms default connect timeout) and returns a positive integer socket handle. |
| `net.tcpConnectTimeout(host: string, port: int, timeoutMs: int): int` | `int` | Yes (`NetError`) | Connects a TCP socket to `host:port` with a non-blocking `poll`/`select` timeout of `timeoutMs` milliseconds and returns a socket handle. |
| `net.tcpListen(host: string, port: int, backlog: int): int` | `int` | Yes (`NetError`) | Creates a TCP server socket with `SO_REUSEADDR`, binds to `host:port` (`port == 0` binds an OS-assigned ephemeral port), listens with `backlog`, and returns a listener handle. |
| `net.tcpAccept(serverHandle: int): int` | `int` | Yes (`NetError`) | Accepts an incoming client connection on `serverHandle` and returns a new connected TCP client socket handle. |
| `net.tcpLocalPort(handle: int): int` | `int` | Yes (`NetError`) | Queries `getsockname` on `handle` and returns the bound local port (`1..65535`). Especially useful after `tcpListen(host, 0, backlog)`. |
| `net.tcpPeerAddr(handle: int): string` | `string` | Yes (`NetError`) | Queries `getpeername` on `handle` and returns the remote peer address formatted as `"ip:port"`. |
| `net.tcpSend(handle: int, data: string): int` | `int` | Yes (`NetError`) | Sends all bytes of `data` over `handle` (looping over partial `send` calls) and returns total bytes sent. |
| `net.tcpRecv(handle: int, maxBytes: int): string` | `string` | Yes (`NetError`) | Receives up to `maxBytes` bytes (`maxBytes > 0`) from `handle`. Returns `""` on clean peer EOF; throws `NetError` on socket error or timeout. |
| `net.tcpRecvLine(handle: int, maxBytes: int): string` | `string` | Yes (`NetError`) | Reads bytes from `handle` up to `\n` or `maxBytes`, stripping any trailing `\r\n` or `\n`. |
| `net.tcpSetTimeout(handle: int, timeoutMs: int): void` | `void` | Yes (`NetError`) | Sets `SO_RCVTIMEO` and `SO_SNDTIMEO` on `handle` (`timeoutMs >= 0`). |
| `net.tcpClose(handle: int): void` | `void` | Yes (`NetError`) | Closes the underlying OS TCP socket and removes `handle` from the runtime registry. Throws `NetError` if `handle` is not an active TCP socket. |

### 3.5 Handle-Based UDP Sockets (4 Functions)

| Function Signature | Return Type | Fallible (`zonk`) | Description |
|---|---|---|---|
| `net.udpBind(host: string, port: int): int` | `int` | Yes (`NetError`) | Creates and binds a UDP (`SOCK_DGRAM`) socket to `host:port` (`port == 0` binds an OS-assigned ephemeral port) and returns a UDP socket handle. |
| `net.udpLocalPort(handle: int): int` | `int` | Yes (`NetError`) | Queries `getsockname` on a UDP `handle` and returns the bound local port (`1..65535`). |
| `net.udpSendTo(handle: int, host: string, port: int, data: string): int` | `int` | Yes (`NetError`) | Sends datagram `data` from UDP `handle` to `host:port` and returns the number of bytes sent. |
| `net.udpRecvFrom(handle: int, maxBytes: int): string` | `string` | Yes (`NetError`) | Receives a datagram up to `maxBytes` bytes on UDP `handle` and returns a JSON string `{"data":"...","host":"...","port":12345}`. |
| `net.udpClose(handle: int): void` | `void` | Yes (`NetError`) | Closes the underlying OS UDP socket and removes `handle` from the runtime registry. Throws `NetError` if `handle` is not an active UDP socket. |

*(Note: Including `udpLocalPort` brings the total to **41 functions**, making ephemeral-port UDP testing just as ergonomic as `tcpLocalPort`.)*

---

## 4. Runtime Implementation Design (`runtime/keet/net.hpp`)

### 4.1 Cross-Platform Socket Abstraction (`keet::net::detail`)
- **Platform Headers & Types:**
  - On `_WIN32`:
    - `#ifndef WIN32_LEAN_AND_MEAN` / `#define WIN32_LEAN_AND_MEAN`
    - `#include <winsock2.h>` and `#include <ws2tcpip.h>`
    - `#pragma comment(lib, "ws2_32.lib")`
    - `using socket_t = SOCKET;` with `constexpr socket_t kInvalidSocket = INVALID_SOCKET;`
    - `ensure_winsock_initialized()` using a function-local `static WinsockInit` struct calling `WSAStartup(MAKEWORD(2, 2), &wsa)`.
  - On POSIX (`__APPLE__`, `__linux__`, etc.):
    - `#include <arpa/inet.h>`, `#include <fcntl.h>`, `#include <netdb.h>`, `#include <netinet/in.h>`, `#include <netinet/tcp.h>`, `#include <poll.h>`, `#include <sys/socket.h>`, `#include <sys/types.h>`, `#include <unistd.h>`
    - `using socket_t = int;` with `constexpr socket_t kInvalidSocket = -1;`
    - Suppresses `SIGPIPE` on send (`MSG_NOSIGNAL` on Linux, `SO_NOSIGPIPE` on macOS).
- **Thread-Safe Socket Handle Registry:**
  ```cpp
  enum class SocketKind { TcpStream, TcpListener, Udp };
  struct SocketEntry {
      socket_t fd;
      SocketKind kind;
  };
  ```
  Managed by a singleton `SocketRegistry` with `std::mutex`, `std::unordered_map<int64_t, SocketEntry>`, and `int64_t next_handle = 1`.

### 4.2 URL & HTTP/1.1 Engine Details
- **URL Parser (`detail::ParsedUrl`):**
  - Parses `scheme://[host[:port]][/path][?query][#fragment]`, supporting IPv4, domain names, and bracketed IPv6 (`http://[::1]:8080/test`).
  - Validates port range `1..65535`.
- **HTTP/1.1 Socket Client (`detail::http_request_socket`):**
  - Connects via `connect_with_timeout`, sends canonical HTTP/1.1 request lines (`<METHOD> <path_and_query> HTTP/1.1\r\nHost: <host>\r\nConnection: close\r\nUser-Agent: keety-net/0.2\r\n...`), reads response headers up to `\r\n\r\n`, parses status line (`HTTP/1.x <status> <reason>`), normalizes response header names to lowercase, and decodes body via either `Transfer-Encoding: chunked` or `Content-Length` / EOF.
  - Automatically follows up to 5 HTTP redirects (`301`, `302`, `303`, `307`, `308`) when `Location` header is present.
- **HTTPS Fallback (`detail::https_request_curl`):**
  - When `scheme == "https"`, invokes system `curl` using safe temporary header/body files (`std::filesystem::temp_directory_path()`) so HTTPS endpoints work out-of-the-box without requiring OpenSSL at compile/link time.

---

## 5. Phased Implementation Plan

### Phase 1: C++20 Runtime Implementation (`runtime/keet/net.hpp`) & Windows Linker Support
1. Create `runtime/keet/net.hpp` implementing `namespace keet::net` and all 41 functions.
2. Create `runtime/keet/std/net.hpp` forwarding to `<keet/net.hpp>`.
3. Update `runtime/keet/runtime.hpp` to `#include <keet/net.hpp>`.
4. Update `src/driver/toolchain_driver.cpp` so MSVC linker invocations include `ws2_32.lib` on Windows.

### Phase 2: Module Resolver & Universe Scope Registration
1. Add `"net"` to `stdlibModules` in `src/resolver/module_path_resolver.cpp`.
2. Update `include/keety/sema/universe.hpp` and `src/sema/universe.cpp`:
   - Add `"net"`, `"std.net"`, `"keet.net"` to `isStdlibModule`.
   - Implement `populateNetModuleScope(Scope& moduleScope)` registering all 41 exported function symbols.
   - Update `createStdlibModuleScope` to dispatch `"net"` to `populateNetModuleScope`.
3. Update `src/driver/cli_driver.cpp` (`execute_check`, `execute_emit`, `build_binary`) to register `<stdlib>/net`.

### Phase 3: Static Type Checker Integration (`TypeChecker`)
1. Update `include/keety/typechecker/type_checker.hpp` and `src/typechecker/type_checker.cpp`:
   - Add `kNetCandidates` array with all 41 function names for typo suggestions (`didYouMean`).
   - Implement `checkNetMemberCall` and `checkNetDirectCall` enforcing exact parameter count and types under Strict Zero-Coercion (`E3002`, `E3003`, `E3011`).
   - Handle `import net;`, `import net as n;`, and `from "net" import ...;` (`E1005` on unknown symbol).

### Phase 4: KeetIR Lowering & C++20 Code Emission (`AstLowerer` & `CppEmitter`)
1. Update `include/keety/ir/ast_lowerer.hpp` and `src/ir/ast_lowerer.cpp`:
   - Track `has_net_import_`, `net_aliases_`, and `imported_net_symbols_`.
   - Lower `net.<fn>(...)`, `<alias>.<fn>(...)`, and selectively imported `<fn>(...)` calls to `@keet.net.<fn>`.
2. Update `include/keety/emitter/cpp_emitter.hpp` and `src/emitter/cpp_emitter.cpp`:
   - Track `net` imports/aliases and emit `::keet::net::<fn>(...)` in both AST and IR code generation paths.

### Phase 5: Unit Tests, Conformance Tests, VS Code Extension & Documentation
1. Create `tests/unit/test_runtime_net.cpp`:
   - Test URL encoding/decoding, URL parsing, query building/parsing, IP validation (`isIpv4`, `isIpv6`, `isIp`, `isLoopback`), `hostname()`, and `resolveHost("localhost")`.
   - Test handle-based TCP loopback client/server (`tcpListen` on port `0`, `tcpLocalPort`, `tcpConnect`, `tcpAccept`, `tcpSend`, `tcpRecv`, `tcpRecvLine`, `tcpPeerAddr`, `tcpClose`) using `std::thread`.
   - Test handle-based UDP loopback (`udpBind` on port `0`, `udpLocalPort`, `udpSendTo`, `udpRecvFrom` + `keet::json` verification, `udpClose`).
   - Test HTTP/1.1 client (`get`, `getStatus`, `post`, `postJson`, `request`, `download`) against an embedded loopback TCP test responder thread on `127.0.0.1:0` (100% deterministic, zero external internet dependency in CI).
   - Test `NetError` exceptions on invalid URLs, invalid handles, and unreachable ports.
2. Update `tests/unit/test_universe_symbols.cpp` and `tests/unit/test_type_checker.cpp` with `net` scope and static type-checking tests.
3. Create `tests/conformance/positive/12_net/net_operations.keet` and `tests/conformance/test_conformance_net.cpp`, and register them in `tests/CMakeLists.txt`.
4. Create `documentation/stdlib/net.md`, update `README.md`, and add `net` autocompletion/highlighting to `tools/vscode-keetscript/`.

---

## 6. File Impact Matrix

| File Path | Action | Phase | Description |
|---|---|---|---|
| `documentation/NET_Module_Implementation_Plan.md` | Create | Plan | Complete 5-phase architectural and API specification |
| `runtime/keet/net.hpp` | Create | Phase 1 | Header-only C++20 `keet::net` runtime implementation |
| `runtime/keet/std/net.hpp` | Create | Phase 1 | Standard library header wrapper for `#include <keet/std/net.hpp>` |
| `runtime/keet/runtime.hpp` | Modify | Phase 1 | Include `<keet/net.hpp>` in umbrella runtime header |
| `src/driver/toolchain_driver.cpp` | Modify | Phase 1 | Link `ws2_32.lib` on MSVC builds for Winsock2 |
| `src/resolver/module_path_resolver.cpp` | Modify | Phase 2 | Register `"net"` as a built-in stdlib module |
| `include/keety/sema/universe.hpp` | Modify | Phase 2 | Declare `populateNetModuleScope` |
| `src/sema/universe.cpp` | Modify | Phase 2 | Register `"net"` in `isStdlibModule` and `createStdlibModuleScope` |
| `src/driver/cli_driver.cpp` | Modify | Phase 2 | Register `<stdlib>/net` in `check`, `emit`, and `build` |
| `include/keety/typechecker/type_checker.hpp` | Modify | Phase 3 | Declare `checkNetMemberCall` and `checkNetDirectCall` |
| `src/typechecker/type_checker.cpp` | Modify | Phase 3 | Implement static type checking and diagnostics for `net` |
| `include/keety/ir/ast_lowerer.hpp` | Modify | Phase 4 | Track `net` imports, aliases, and selective symbols |
| `src/ir/ast_lowerer.cpp` | Modify | Phase 4 | Lower `net` calls to `@keet.net.<fn>` |
| `include/keety/emitter/cpp_emitter.hpp` | Modify | Phase 4 | Track `net` imports and aliases in C++ emitter |
| `src/emitter/cpp_emitter.cpp` | Modify | Phase 4 | Emit `::keet::net::<fn>(...)` calls |
| `tests/unit/test_runtime_net.cpp` | Create | Phase 5 | Unit tests for URL, DNS, HTTP loopback, TCP, UDP, and `NetError` |
| `tests/unit/test_universe_symbols.cpp` | Modify | Phase 5 | Unit test for `net` module scope population |
| `tests/unit/test_type_checker.cpp` | Modify | Phase 5 | Unit tests for `net` static type checking and diagnostics |
| `tests/conformance/positive/12_net/net_operations.keet` | Create | Phase 5 | End-to-end KeetScript conformance test for `net` |
| `tests/conformance/test_conformance_net.cpp` | Create | Phase 5 | Conformance test runner for `12_net` |
| `tests/CMakeLists.txt` | Modify | Phase 5 | Register `test_runtime_net.cpp` and `test_conformance_net.cpp` |
| `documentation/stdlib/net.md` | Create | Phase 5 | User-facing documentation for the `net` module |
| `README.md` | Modify | Phase 5 | Add `net` to the Standard Library table |
| `tools/vscode-keetscript/src/stdlibData.ts` | Modify | Phase 5 | Add `net` completions, hover docs, and snippets |
| `tools/vscode-keetscript/syntaxes/keet.tmLanguage.json` | Modify | Phase 5 | Add `net` to `support.class.stdlib.keet` |
| `tools/vscode-keetscript/README.md` | Modify | Phase 5 | Document `net.` dot-completions |
