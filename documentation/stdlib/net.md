# KeetScript Standard Library: `net` Module

The `net` module provides RFC 3986 URL encoding/decoding, URL parsing, query string building/parsing, DNS resolution, IP address validation, an HTTP/1.1 & HTTPS client, and handle-based TCP and UDP sockets. It is built into the KeetScript standard library and compiler toolchain (`keety`) with zero external C++ library dependencies (using POSIX/BSD sockets on macOS/Linux and Winsock2 on Windows).

All operations work directly on KeetScript's native types (`string`, `int`, `bool`, `array<string>`, and `map<string, string>`) and adhere to KeetScript's **Strict Zero-Coercion Policy (`E3002`)**.

Fallible operations (such as malformed URLs, DNS resolution failures, connection timeouts, invalid socket handles, or I/O errors) throw runtime `keet::error` instances named `"NetError"`, integrating seamlessly with KeetScript's native `zonk` error recovery operators and blocks.

---

## 1. Importing the Module

The `net` module can be imported using qualified module imports, module aliases, or selective member imports:

```keet
// Qualified import
import net;

fn main(): void {
    let host: string = net.urlHost("https://api.example.com:8443/v1/status");
    let port: int = net.urlPort("https://api.example.com:8443/v1/status");
}
```

```keet
// Module alias
import net as n;

fn main(): void {
    let encoded: string = n.urlEncode("hello world!");
    let decoded: string = n.urlDecode(encoded);
}
```

```keet
// Selective import
from "net" import isIpv4, isLoopback, resolveIpv4;

fn main(): void {
    if (isIpv4("127.0.0.1") && isLoopback("127.0.0.1")) {
        let ip: string = resolveIpv4("localhost");
    }
}
```

---

## 2. URL & Query String Utilities

| Function | Return Type | Description |
|---|---|---|
| `net.urlEncode(text: string): string` | `string` | Percent-encodes `text` according to RFC 3986 unreserved characters (`A-Z`, `a-z`, `0-9`, `-`, `_`, `.`, `~`) |
| `net.urlDecode(text: string): string` | `string` | Decodes RFC 3986 percent-encoded sequences (`%XX`) and `+` spaces; throws `NetError` on invalid `%` hex sequences |
| `net.parseUrl(url: string): map<string, string>` | `map<string, string>` | Parses `url` into keys `"scheme"`, `"host"`, `"port"`, `"path"`, `"query"`, and `"fragment"`; throws `NetError` if malformed |
| `net.urlScheme(url: string): string` | `string` | Extracts the lowercase scheme (`"http"`, `"https"`, etc.) from `url` |
| `net.urlHost(url: string): string` | `string` | Extracts the host or unbracketed IPv6 address from `url` |
| `net.urlPort(url: string): int` | `int` | Returns explicit port from `url`, or default `80` (`http`), `443` (`https`), or `0` |
| `net.urlPath(url: string): string` | `string` | Extracts the path component from `url` (defaults to `"/"`) |
| `net.urlQuery(url: string): string` | `string` | Extracts the query string (without leading `'?'`) from `url` |
| `net.buildQuery(params: map<string, string>): string` | `string` | Serializes `params` into a sorted, URL-encoded `k1=v1&k2=v2` query string |
| `net.parseQuery(query: string): map<string, string>` | `map<string, string>` | Parses a query string (with or without leading `'?'`) into a decoded `map<string, string>` |

---

## 3. DNS & IP Address Utilities

| Function | Return Type | Description |
|---|---|---|
| `net.resolveHost(host: string): array<string>` | `array<string>` | Resolves `host` via `getaddrinfo` into a deduplicated list of IPv4 and IPv6 addresses; throws `NetError` on failure |
| `net.resolveIpv4(host: string): string` | `string` | Resolves `host` to its first IPv4 address string; throws `NetError` if no IPv4 record exists |
| `net.isIpv4(addr: string): bool` | `bool` | Returns `true` if `addr` is a valid dotted-quad IPv4 address (never throws) |
| `net.isIpv6(addr: string): bool` | `bool` | Returns `true` if `addr` is a valid IPv6 address (with or without `[...]` brackets; never throws) |
| `net.isIp(addr: string): bool` | `bool` | Returns `true` if `addr` is either a valid IPv4 or IPv6 address (never throws) |
| `net.isLoopback(addr: string): bool` | `bool` | Returns `true` if `addr` is `"localhost"`, `127.0.0.0/8`, or `::1` (never throws) |
| `net.hostname(): string` | `string` | Returns the local machine's network hostname; throws `NetError` on OS failure |

---

## 4. HTTP/1.1 & HTTPS Client

`net.request` returns a structured JSON string with keys `"status"` (`int`), `"ok"` (`bool`, `true` for 200..299), `"body"` (`string`), `"headers"` (`object` with lowercase header keys), and `"url"` (`string`), which composes directly with the `json` standard library module.

| Function | Return Type | Description |
|---|---|---|
| `net.get(url: string): string` | `string` | Performs an HTTP/HTTPS `GET` request and returns the response body; throws `NetError` on transport error |
| `net.getStatus(url: string): int` | `int` | Performs an HTTP/HTTPS `GET` request and returns the HTTP status code (`200`, `404`, etc.) |
| `net.post(url: string, body: string, contentType: string): string` | `string` | Performs an HTTP/HTTPS `POST` request with `Content-Type: contentType` and returns the response body |
| `net.postJson(url: string, jsonBody: string): string` | `string` | Performs an HTTP/HTTPS `POST` request with `Content-Type: application/json` and `Accept: application/json` |
| `net.put(url: string, body: string, contentType: string): string` | `string` | Performs an HTTP/HTTPS `PUT` request and returns the response body |
| `net.deleteReq(url: string): string` | `string` | Performs an HTTP/HTTPS `DELETE` request and returns the response body |
| `net.request(method: string, url: string, headers: map<string, string>, body: string, timeoutMs: int): string` | `string` | Executes a customizable HTTP/HTTPS request and returns a JSON response envelope (`status`, `ok`, `body`, `headers`, `url`) |
| `net.download(url: string, destPath: string): bool` | `bool` | Downloads `url` to `destPath` (`true` if status is `200..299`); throws `NetError` on network or file I/O failure |

---

## 5. Handle-Based TCP Sockets

TCP sockets are referenced by thread-safe positive integer handles (`int`). Passing port `0` to `net.tcpListen` binds an ephemeral OS port that can be queried via `net.tcpLocalPort(listenerHandle)`.

| Function | Return Type | Description |
|---|---|---|
| `net.tcpConnect(host: string, port: int): int` | `int` | Opens a TCP connection to `host:port` (10s default timeout) and returns a stream socket handle |
| `net.tcpConnectTimeout(host: string, port: int, timeoutMs: int): int` | `int` | Opens a TCP connection to `host:port` with an explicit timeout in milliseconds |
| `net.tcpListen(host: string, port: int, backlog: int): int` | `int` | Binds and listens on `host:port` (`SO_REUSEADDR` enabled) and returns a listener handle |
| `net.tcpAccept(listenerHandle: int): int` | `int` | Accepts an incoming connection on `listenerHandle` and returns a new stream socket handle |
| `net.tcpLocalPort(handle: int): int` | `int` | Returns the bound local port number of a TCP listener or stream handle |
| `net.tcpPeerAddr(handle: int): string` | `string` | Returns the remote peer address formatted as `"ip:port"` |
| `net.tcpSend(handle: int, data: string): int` | `int` | Sends all bytes of `data` over the TCP stream and returns the total bytes sent |
| `net.tcpRecv(handle: int, maxBytes: int): string` | `string` | Reads up to `maxBytes` from the TCP stream (returns `""` on clean EOF) |
| `net.tcpRecvLine(handle: int, maxBytes: int): string` | `string` | Reads a line up to `\n` (stripping trailing `\r\n` or `\n`) up to `maxBytes` |
| `net.tcpSetTimeout(handle: int, timeoutMs: int): void` | `void` | Sets send and receive timeouts (`SO_RCVTIMEO` / `SO_SNDTIMEO`) on `handle` in milliseconds |
| `net.tcpClose(handle: int): void` | `void` | Closes the TCP socket handle and removes it from the registry (throws `NetError` if invalid/closed) |

---

## 6. Handle-Based UDP Sockets

`net.udpRecvFrom` returns a JSON string `{"data":"...","host":"...","port":12345}` representing the datagram payload and sender endpoint.

| Function | Return Type | Description |
|---|---|---|
| `net.udpBind(host: string, port: int): int` | `int` | Binds a UDP socket on `host:port` (use `0` for an ephemeral port) and returns a UDP handle |
| `net.udpLocalPort(handle: int): int` | `int` | Returns the bound local port number of the UDP socket handle |
| `net.udpSendTo(handle: int, host: string, port: int, data: string): int` | `int` | Sends datagram `data` to `host:port` and returns the number of bytes transmitted |
| `net.udpRecvFrom(handle: int, maxBytes: int): string` | `string` | Receives a datagram up to `maxBytes` and returns JSON `{"data":"...","host":"...","port":...}` |
| `net.udpClose(handle: int): void` | `void` | Closes the UDP socket handle (throws `NetError` if invalid/closed) |

---

## 7. Practical Usage Examples

### 7.1 HTTP Request with `json` Inspection and `zonk` Recovery
```keet
import net;
import json;
import io;

fn checkApiHealth(endpoint: string): bool {
    let headers: map<string, string> = {"Accept": "application/json"};
    let resp: string = net.request("GET", endpoint, headers, "", 3000) zonk (err) {
        io.eprintln("Network request failed: ", err.message);
        return false;
    };

    let status: int = json.getIntOr(resp, "status", 0);
    let ok: bool = json.getBoolOr(resp, "ok", false);
    io.println(`Status: ${status.toString()}, OK: ${ok.toString()}`);
    return ok;
}
```

### 7.2 Ephemeral TCP Echo Exchange
```keet
import net;
import io;

fn main(): int {
    let listener: int = net.tcpListen("127.0.0.1", 0, 8);
    let port: int = net.tcpLocalPort(listener);

    let client: int = net.tcpConnect("127.0.0.1", port);
    let serverConn: int = net.tcpAccept(listener);

    net.tcpSend(client, "ping\n");
    let received: string = net.tcpRecvLine(serverConn, 256);
    io.println("Server received: ", received);

    net.tcpClose(client);
    net.tcpClose(serverConn);
    net.tcpClose(listener);
    return 0;
}
```
