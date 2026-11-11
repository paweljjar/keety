# KeetScript Standard Library: `http` Module

The `http` module provides a high-performance, non-blocking HTTP/1.1, RFC 6455 WebSocket, and Reactive UI LiveView server engine for KeetScript. Built directly into the standard runtime (`<keet/http.hpp>` and `<keet/ui/http_serve_ui.hpp>`), it features zero external dependencies, thread pool connection management, an ultra-fast radix tree router, an Express-style asynchronous middleware pipeline, declarative schema validation, full-duplex WebSockets, and Server-Side Rendered (SSR) + WebSocket VDOM diffing for `.kml` UI components.

All operations adhere to KeetScript's **Strict Zero-Coercion Policy (`E3002`)** and integrate seamlessly with KeetScript's error handling and `zonk` recovery primitives.

---

## 1. Importing the Module

The `http` module can be imported using any of the four standard import forms (as well as module aliases):

```keet
// 1. Unquoted module import
import http;

// 2. Quoted module import
import "http";

// 3. Unquoted selective import
from http import app, createServer;

// 4. Quoted selective import
from "http" import app, createServer, cors, jsonParser, logger, recover;
```

```keet
// Module alias import
import http as h;

fn main(): void {
    let server: http.Server = h.app();
    server.use(cors());
    server.use(logger());
    server.use(recover());
    server.listen(8080);
}
```

---

## 2. Server Lifecycle & Instantiation

| Function / Method | Return Type | Description |
|---|---|---|
| `http.app()` | `HttpServer` | Creates a new Express-style HTTP server application (`reusePort = false`) |
| `http.app(reusePort: bool)` | `HttpServer` | Creates a server explicitly enabling or disabling `SO_REUSEPORT` socket sharing |
| `http.createServer()` | `HttpServer` | Creates a new HTTP server instance with default configuration |
| `http.createServer(reusePort: bool)` | `HttpServer` | Creates a new HTTP server instance with explicit `reusePort` setting |
| `http.createServer(handler)` | `HttpServer` | Creates a server with a global request handler callback |
| `server.reusePort(enabled: bool)` | `HttpServer` | Fluently configures `SO_REUSEPORT` before calling `server.listen()` |
| `server.isReusePort(): bool` | `bool` | Returns `true` if `SO_REUSEPORT` socket sharing is enabled |
| `server.listen(port: int)` | `HttpServer` | Binds and starts listening on `port` (use `0` for an OS-assigned ephemeral port). Throws `NetError` if port is already in use |
| `server.stop(): void` | `void` | Gracefully stops the listener socket and drains worker threads |
| `server.port(): int` | `int` | Returns the actual bound local port number |
| `server.isRunning(): bool` | `bool` | Returns `true` if the server listener and thread pool are running |

> [!NOTE] Port Conflict & Error Recovery
> By default, `reusePort` is `false`. Attempting to call `server.listen(port)` when the port is already taken throws a `NetError` (`Address already in use`), which can be safely recovered using KeetScript's `zonk` primitive:
> ```keet
> let app: HttpServer = http.app();
> let ok: bool = tryListen(app, 8080) zonk (err) {
>     io.println("Port conflict: " + err.message);
>     yield false;
> };
> ```
> When `reusePort` is set to `true`, the operating system allows multiple processes or workers to bind to the same port with kernel-level load balancing (`SO_REUSEPORT`).

---

## 3. Radix Tree Routing & HTTP Methods

Routes are matched using a high-throughput Radix Tree with deterministic precedence:
**Exact Path > Parameterized Token (`:param`) > Wildcard (`*path`)**.

```keet
import http;

fn registerRoutes(mut app: HttpServer): void {
    // Exact route
    app.get("/health", fn(mut req: Request, mut res: Response): void {
        res.status(200).send("OK");
    });

    // Parameterized route (:id extracted into req.param("id"))
    app.get("/users/:id", fn(mut req: Request, mut res: Response): void {
        res.status(200).send("User ID: " + req.param("id"));
    });

    // Multi-segment parameter extraction
    app.get("/orgs/:org/repos/:repo", fn(mut req: Request, mut res: Response): void {
        let org: string = req.param("org");
        let repo: string = req.param("repo");
        res.json("{\"org\":\"" + org + "\",\"repo\":\"" + repo + "\"}");
    });

    // Multi-segment wildcard route (*filepath captures remainder)
    app.get("/static/*filepath", fn(mut req: Request, mut res: Response): void {
        let path: string = req.param("filepath");
        res.status(200).send("Requested: " + path);
    });
}
```

### Standard HTTP Verbs

| Method | Description |
|---|---|
| `app.get(path, handler)` | Registers a `GET` route handler |
| `app.post(path, handler)` | Registers a `POST` route handler |
| `app.put(path, handler)` | Registers a `PUT` route handler |
| `app.delete(path, handler)` / `app.del(...)` | Registers a `DELETE` route handler |
| `app.patch(path, handler)` | Registers a `PATCH` route handler |
| `app.head(path, handler)` | Registers a `HEAD` route handler |
| `app.options(path, handler)` | Registers an `OPTIONS` route handler |

---

## 4. Request & Response API

### Request (`Request`)
- `req.method`: HTTP method string (`"GET"`, `"POST"`, etc.).
- `req.path`: Normalized path string without query parameters.
- `req.url`: Full original URL.
- `req.body`: Raw request body string.
- `req.remoteIp`: Client IPv4 or IPv6 address.
- `req.header(name: string, [default: string]): string`: Case-insensitive header lookup.
- `req.hasHeader(name: string): bool`: Checks if header exists.
- `req.param(name: string, [default: string]): string`: Extracted route parameter (`:param`).
- `req.hasParam(name: string): bool`: Checks if route parameter exists.
- `req.queryParam(name: string, [default: string]): string`: Query parameter lookup.
- `req.cookie(name: string, [default: string]): string`: Cookie lookup.
- `req.hasCookie(name: string): bool`: Checks if cookie is present.

### Response (`Response`)
- `res.status(code: int): Response`: Sets HTTP response status code (fluent).
- `res.header(name: string, value: string): Response`: Sets response header.
- `res.type(contentType: string): Response`: Sets `Content-Type` header (e.g. `"application/json"`).
- `res.send(body: string): Response`: Sends response string and completes the request.
- `res.json(jsonStr: string): Response`: Sends body with `Content-Type: application/json`.
- `res.text(text: string): Response`: Sends body with `Content-Type: text/plain`.
- `res.html(html: string): Response`: Sends body with `Content-Type: text/html`.
- `res.redirect(url: string, [code: int]): Response`: Sends HTTP redirect response.
- `res.cookie(name: string, value: string, ...): Response`: Appends `Set-Cookie` header.

---

## 5. Middleware Pipeline & Continuation Chain

Middlewares execute in registration order. Middlewares receive `(mut req: Request, mut res: Response, next: NextFunction)` and call `next()` to yield to downstream handlers:

```keet
import http;
import io;

fn main(): void {
    let app: HttpServer = http.app();

    // Custom logging and timing middleware
    app.use(fn(mut req: Request, mut res: Response, next: NextFunction): void {
        io.println("Incoming " + req.method + " " + req.path);
        next();
    });

    app.get("/", fn(mut req: Request, mut res: Response): void {
        res.status(200).send("Hello from middleware chain!");
    });

    app.listen(8080);
}
```

---

## 6. Standard Middleware Ecosystem

The `http` module includes built-in middlewares for common web services:

| Middleware | Signature | Description |
|---|---|---|
| **CORS** | `http.cors([options])` | Adds `Access-Control-Allow-Origin`, methods, headers, and handles pre-flight `OPTIONS` |
| **JSON Parser** | `http.jsonParser([options])` | Parses JSON request bodies into `req.json_value` with payload limits |
| **URL-Encoded** | `http.urlencoded([options])` | Parses `application/x-www-form-urlencoded` payloads into `req.body_params` |
| **Request Logger** | `http.logger([options])` | Formats and outputs access logs with response status codes and elapsed latency |
| **Static Directory** | `http.staticDir(dir: string, [options])` | Serves static assets from disk with automatic MIME-types, ETags, and HTTP 304 |
| **Panic Recovery** | `http.recover()` | Catches unhandled panics and exceptions, safely emitting `500 Internal Server Error` |

```keet
import http;

fn configure(mut app: HttpServer): void {
    app.use(http.cors());
    app.use(http.logger());
    app.use(http.jsonParser());
    app.use(http.recover());
    app.use(http.staticDir("./public"));
}
```

---

## 7. Declarative Schema Validation

Validate request bodies, headers, query parameters, and path variables against strict schemas:

```keet
import http;

fn main(): void {
    let app: HttpServer = http.app();
    let userSchema: SchemaValidator = http.schema("UserSchema");

    // Routes decorated with validation middleware automatically reject invalid payloads with HTTP 400
    app.post("/api/users", http.validate(userSchema), fn(mut req: Request, mut res: Response): void {
        res.status(201).json("{\"message\":\"User created successfully\"}");
    });

    app.listen(8080);
}
```

---

## 8. Full-Duplex WebSockets (`http.WebSocket`)

Define real-time bidirectional WebSocket endpoints using `app.ws(path, handler)`. The server automatically performs the RFC 6455 handshake (`101 Switching Protocols`), manages frame masking/unmasking, and dispatches events:

```keet
import http;
import io;

fn main(): void {
    let app: HttpServer = http.app();

    app.ws("/chat", fn(mut ws: WebSocket): void {
        io.println("New WebSocket connection from: " + ws.remoteIp());

        // Handle incoming messages from client
        ws.onMessage(fn(msg: string): void {
            io.println("Message: " + msg);
            ws.send("ECHO: " + msg);
        });

        // Handle socket closure
        ws.onClose(fn(code: int, reason: string): void {
            io.println("Client disconnected (" + code.toString() + "): " + reason);
        });
    });

    app.listen(8080);
}
```

### WebSocket Methods
- `ws.send(data: string): bool`: Sends a text frame to the client.
- `ws.sendBinary(data: string): bool`: Sends a binary frame to the client.
- `ws.ping([data: string]): bool`: Transmits a WebSocket ping control frame.
- `ws.pong([data: string]): bool`: Transmits a WebSocket pong control frame.
- `ws.close([code: int], [reason: string]): void`: Closes the WebSocket session.
- `ws.isOpen(): bool`: Returns `true` if the connection is active.
- `ws.id(): string`: Unique session identifier.
- `ws.remoteIp(): string`: Client IP address.

---

## 9. Reactive UI Application Serving (`server.serveUi`)

The `http` server natively integrates with KeetScript's `ui` module and `.kml` Single-File Components via `server.serveUi(...)`:

```keet
server.serveUi(route: string, factory: fn(): Component, title?: string, scopedCss?: string): Server
```

When `server.serveUi` is registered on a route (e.g., `"/"` or `"/counter"`):
1. **`GET <route>`**: Instantiates a fresh component via `factory()`, mounts it, renders the initial Virtual DOM tree (`renderVdom()`), and serves a complete HTML5 document containing the Server-Side Rendered (SSR) markup, scoped CSS, and a lightweight (~50-line) WebSocket VDOM client runtime.
2. **`WS <route>/ws`**: Automatically provisions a dedicated per-session WebSocket endpoint. Each browser tab holds its own isolated `Component` instance. Incoming browser events (`click`, `input`, `keydown`) invoke the component's handlers, compute minimal VDOM patches (`WebVdomBackend::diff`), and stream JSON patch arrays back over the WebSocket.

```keet
import http;
import io;
import "./Counter.kml";

fn main(): int {
    let server: http.Server = http.createServer();

    // Serve reactive Counter.kml component on "/"
    server.serveUi("/", fn(): Counter {
        return Counter();
    }, "KeetScript Reactive UI Demo");

    // Coexists seamlessly with standard REST API endpoints
    server.get("/api/health", fn(req: http.Request, mut res: http.Response): void {
        res.status(200).json("{\"status\":\"ok\",\"engine\":\"keet-ui\"}");
    });

    io.println("Listening on http://127.0.0.1:3000");
    server.listen(3000);
    return 0;
}
```

For full details on `.kml` Single-File Components and reactive runes (`$state`, `$derived`, `$effect`), see the [`ui` Module Documentation](ui.md).

---

## 10. Performance & Microservice Benchmarks

Measured on release builds (`-O3 -march=native`) on ARM64 Apple Silicon:

| Benchmark Target | Metric | Result |
|---|---|---|
| **Radix Router Throughput** | Concurrent Requests / Sec | **> 20,000,000 req/sec** |
| **Route Dispatch Latency** | Median Latency per Request | **~0.045 µs** |
| **HTTP Parser Wire Speed** | Streaming Parse Throughput | **~1.2 GB/sec** |
| **WebSocket Framing** | Frame Decode / Unmask / Encode | **> 15,000,000 frames/sec** |
| **Connection Overhead** | Memory per Active Connection | **< 4 KB** |
