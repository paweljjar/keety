# KeetScript Reactive UI Engine Demo (`ui_demo`)

This example demonstrates the ahead-of-time (AOT) compiled, compile-time reactive UI engine of KeetScript (`ui`) and Keet Markup Language (`.kml`).

## Project Structure

```
examples/ui_demo/
├── pedigree.json       # Official KeetScript package manifest (scaffolded via kattery init)
├── .gitignore          # Package build artifact ignores
├── README.md           # Documentation and run instructions
└── src/
    ├── Counter.kml     # Single-File Component (SFC) with $state, $derived, scoped styles
    └── main.keet       # KeetScript application entry point running live HTTP & WebSocket UI server
```

### Components

- **`src/Counter.kml`**: A Single-File Component (SFC) featuring:
  - `<script>` with reactive runes: `$state(...)` and `$derived(...)`
  - `<template>` with layout primitives (`<box>`, `<hstack>`, `<text>`, `<button>`) and conditional blocks (`{#if} ... {:else} ... {/if}`)
  - Directives: `@click` event bindings
  - Scoped `<style>` definitions with compile-time FNV-1a class hashing
- **`src/main.keet`**: Pure KeetScript application entry point that starts an HTTP and WebSocket server hosting the reactive UI:
  1. **HTTP Application Serving**: Web server hosting via `server.serveUi("/", ...)` and `server.serveUi("/counter", ...)`.
  2. **Live WebSocket Event Bridge**: Real-time two-way event synchronization (`ws://localhost:3000/_keet_ws`).
  3. **Virtual DOM Engine**: Live RFC 8259 JSON tree serialization and RFC 6902-style diff patches sent to client browsers without page reloads.

## Running the Web Server

From the `examples/ui_demo` directory:

```bash
# Run the application via kattery package manager
kattery run start
```

Or using the `keety` compiler driver directly:

```bash
keety run src/main.keet
```

### Accessing the Web UI

Once the server is running, open your web browser at:
- **`http://localhost:3000`** (or `http://localhost:3000/counter`)

Clicking the `+ Increment`, `- Decrement`, and `Reset` buttons dispatches live DOM click events over WebSocket, causing the KeetScript reactive engine on the server to recalculate state and push differential VDOM patches to the browser in real time.
