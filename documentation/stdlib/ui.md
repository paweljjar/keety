# KeetScript Standard Library: `ui` Module & `.kml` Single-File Components

The `ui` module provides KeetScript's compile-time reactive user interface engine, scene graph primitives, event modifiers, microtask scheduler, and multi-target rendering backends (Terminal User Interface (`TuiBackend`), Web Virtual DOM & WebSocket live view (`WebVdomBackend`), and deterministic unit testing (`HeadlessBackend`)).

Alongside the `ui` runtime module (`<keet/std/ui.hpp>`), the `keety` compiler natively compiles **Keet Markup Language (`.kml`)** Single-File Components directly into zero-overhead C++20 classes with bitmask-driven surgical DOM/scene-graph updates.

---

## 1. Importing the Module & `.kml` Components

The `ui` module supports all four standard library import forms (as well as module aliases):

```keet
// 1. Unquoted module import
import ui;

// 2. Quoted module import
import "ui";

// 3. Unquoted selective import
from ui import Component, BoxNode, TextNode, ButtonNode, debounce;

// 4. Quoted selective import
from "ui" import Component, HeadlessBackend, TuiBackend, WebVdomBackend;
```

### Importing `.kml` Single-File Components

A `.kml` component file (e.g., `Counter.kml`) compiles into an exported class matching the file's stem (`Counter`) that inherits from `ui.Component`. You can import `.kml` components directly using a relative path import:

```keet
import "./Counter.kml";
import http;

fn main(): int {
    let server: http.Server = http.createServer();

    server.serveUi("/", fn(): Counter {
        return Counter();
    }, "KeetScript Reactive UI Demo");

    server.listen(3000);
    return 0;
}
```

Selective imports are also supported if desired:
```keet
from "./Counter.kml" import Counter;
```

#### Component Composition & Nesting Inside `.kml`

When building hierarchical UI component trees (e.g., embedding a `<Task />` component inside `App.kml`), component imports follow a clear two-tier resolution strategy:

1. **Explicit `<script>` Imports (Priority):**
   You can explicitly import child `.kml` components inside the parent component's `<script>` block. This allows importing from nested subdirectories or giving components aliases:
   ```html
   <script>
       // Explicit relative path import
       import "./components/Task.kml";
       
       // Or selective import
       from "./components/Task.kml" import Task;
   </script>
   <template>
       <vstack gap={12}>
           {#for item in tasks}
               <Task task={item} onToggle={toggleTask} />
           {/for}
       </vstack>
   </template>
   ```
2. **Zero-Config Fallback (Same-Directory Discovery):**
   If a custom tag (e.g., `<Task />`) is used in `<template>` without an explicit `import` statement in `<script>`, the compiler automatically looks for a neighboring `./<Tag>.kml` file in the same directory.


---

## 2. `.kml` Single-File Component Architecture

A `.kml` file consists of up to three top-level blocks:
1. **`<script>`**: State declarations, derived computations, reactive side effects, and event handler methods written in KeetScript.
2. **`<template>`**: Declarative structural markup, reactive text interpolations (`{expr}`), event bindings (`@click={...}`), two-way bindings (`$bind={...}`), and control flow (`{#if}`, `{#for}`).
3. **`<style>`**: Automatically scoped component CSS (compiled with a deterministic FNV-1a hash attribute such as `data-kml-a1b2c3d4`).

### Complete Example (`Counter.kml`)

```html
<script>
    let count: int = $state(0);
    let doubled: int = $derived(count * 2);
    let parity: string = $derived(count % 2 == 0 ? "Even" : "Odd");

    fn increment(): void {
        count += 1;
    }

    fn decrement(): void {
        count -= 1;
    }

    fn reset(): void {
        count = 0;
    }
</script>

<template>
    <vstack class="card" padding={24} gap={16}>
        <text class="title">KeetScript Reactive UI</text>
        <text class="counter-value">Count: {count}</text>
        <hstack class="stats-row" gap={12}>
            <text class="badge badge-info">Doubled: {doubled}</text>
            {#if count % 2 == 0}
                <text class="badge badge-success">Parity: {parity}</text>
            {:else}
                <text class="badge badge-warning">Parity: {parity}</text>
            {/if}
        </hstack>
        <hstack class="btn-row" gap={10}>
            <button id="btn_dec" class="btn btn-secondary" @click={decrement}>- Decrement</button>
            <button id="btn_reset" class="btn btn-outline" @click={reset}>Reset</button>
            <button id="btn_inc" class="btn btn-primary" @click={increment}>+ Increment</button>
        </hstack>
    </vstack>
</template>

<style>
    .card {
        max-width: 480px;
        margin: 40px auto;
    }
</style>
```

---

## 3. Reactive Runes (`<script>`)

Inside a `.kml` `<script>` block, the compiler provides compile-time reactive runes that build a static dependency graph and assign bit positions in the component's `_dirty` bitmask (`uint64_t`):

| Rune | Syntax | Description |
|---|---|---|
| **`$state`** | `let x: T = $state(initialValue);` | Declares a mutable reactive state slot assigned a unique bit in `_dirty`. Any mutation (`x = ...`, `x += 1`) sets its dirty bit, recomputes dependent `$derived` slots in topological order, and schedules a surgical `update()`. |
| **`$derived`** | `let y: T = $derived(expr);` | Declares a memoized derived value computed from one or more `$state` or `$derived` dependencies. Recomputed automatically whenever upstream dependencies change. |
| **`$effect`** | `$effect(fn(): void { ... });` | Registers a reactive side-effect closure that runs after initial mount and re-runs whenever any `$state` or `$derived` symbol referenced in its body is marked dirty. |

> [!IMPORTANT] Compile-Time Enforcement
> - `$state(...)` variables represent mutable reactive state and must be declared using `let` (declaring `$state` with `const` triggers diagnostic `E5010`).
> - `$derived(...)` values are read-only computations; mutating a `$derived` variable triggers diagnostic `E5011`.
> - Cyclic dependencies between `$derived` declarations are rejected at compile time via Tarjan's SCC analysis (`E5012`).


---

## 4. Template Primitives & Directives (`<template>`)

### 4.1 Built-in Layout & Widget Tags

| Tag | Scene Graph Node | HTML / Web VDOM Mapping | Description |
|---|---|---|---|
| `<box>` | `ui.BoxNode` | `<div>` | Generic Flexbox container (`direction` defaults to `Column`) |
| `<hstack>` | `ui.BoxNode` | `<div class="hstack">` | Horizontal Flexbox container (`direction = Row`) |
| `<vstack>` | `ui.BoxNode` | `<div class="vstack">` | Vertical Flexbox container (`direction = Column`) |
| `<text>` | `ui.TextNode` | `<span>` | Inline or block text node supporting reactive `{expr}` interpolations |
| `<button>` | `ui.ButtonNode` | `<button>` | Interactive focusable button supporting `@click` and `@keydown` |
| `<input>` | `ui.InputNode` | `<input>` | Interactive text input supporting `$bind={stateVar}` and `@input` |
| `<scroll>` | `ui.ScrollNode` | `<div style="overflow:auto">` | Scrollable viewport container with vertical/horizontal scroll offsets |

### 4.2 Directives & Bindings

- **Static Attributes:** `id="btn_inc"`, `class="btn btn-primary"`, `placeholder="Enter name..."`
- **Dynamic Property Bindings:** `padding={24}`, `gap={12}`, `width={100}`
- **Event Listeners (`@event`):**
  - `@click={handler}` — Dispatched on mouse click or keyboard activation (`Enter` / `Space` on focused button).
  - `@input={handler}` — Dispatched when an `<input>` value changes.
  - `@keydown={handler}` — Dispatched on keyboard key press.
- **Two-Way Data Binding (`$bind`):**
  - `<input $bind={username} />` — Automatically binds the input's `value` property to `username` and updates `username` on every `input` event.

### 4.3 Structural Control Flow

#### Conditional Rendering (`{#if ...}`)
```html
{#if score >= 90}
    <text class="badge badge-success">Grade: A</text>
{:elif score >= 80}
    <text class="badge badge-info">Grade: B</text>
{:else}
    <text class="badge badge-warning">Keep practicing</text>
{/if}
```

#### List Rendering (`{#for ...}`)
```html
{#for item, idx in items key={item.id}}
    <hstack gap={8}>
        <text>{idx}: {item.name}</text>
    </hstack>
{/for}
```

---

## 5. Scene Graph & Component API (`ui.*`)

When building custom UI hierarchies or testing components directly in `.keet` code, the `ui` module exports the underlying scene graph and component classes:

### 5.1 `ui.Component`
Base class for all compiled `.kml` components and custom components.

| Method / Field | Signature | Description |
|---|---|---|
| `mount()` | `comp.mount(): UINode` | Builds the initial scene graph, runs initial `$effect` hooks, clears `_dirty`, and returns the root `UINode` |
| `update()` | `comp.update(): void` | Surgically patches only the scene-graph nodes whose dependencies intersect with `_dirty`, then clears `_dirty` |
| `destroy()` | `comp.destroy(): void` | Runs registered cleanup callbacks and releases the component root |
| `renderVdom()` | `comp.renderVdom(): VNode` | Renders the component's current state into a Virtual DOM `VNode` tree for web serialization and diffing |
| `dispatchAction(action)` | `comp.dispatchAction(name: string): bool` | Invokes a named component action/handler by identifier (used by the WebSocket event bridge) |
| `root()` | `comp.root(): UINode` | Returns the mounted root `UINode` |
| `isMounted()` | `comp.isMounted(): bool` | Returns `true` if `mount()` has been called and the component has not been destroyed |
| `isDirty()` | `comp.isDirty(): bool` | Returns `true` if `_dirty != 0` |

### 5.2 Scene Graph Nodes (`ui.UINode` Hierarchy)
- **`ui.UINode`**: Base scene graph node with Flexbox layout properties (`layout`), computed geometry (`computedRect`), focus state (`focusable`, `focused`), dirty flags, and event listener registration (`on(type, callback)`, `dispatchEvent(event)`).
- **`ui.BoxNode`**: Container node managing child `UINode` elements (`appendChild(child)`, `removeChild(child)`, `children()`).
- **`ui.TextNode`**: Leaf node holding reactive text (`text()`, `setText(val)`).
- **`ui.ButtonNode`**: Focusable button widget (`label()`, `setLabel(val)`).
- **`ui.InputNode`**: Focusable text input widget (`value()`, `setValue(val)`, `placeholder()`, `cursorPos()`).
- **`ui.ScrollNode`**: Scrollable container node (`scrollX()`, `scrollY()`, `scrollBy(dx, dy)`).

---

## 6. Event Modifiers & Helpers

| Function | Signature | Description |
|---|---|---|
| `ui.debounce` | `ui.debounce(delayMs: int, handler: fn(UIEvent): void)` | Wraps an event callback so it only fires after `delayMs` milliseconds have elapsed since the last invocation |
| `ui.withPreventDefault` | `ui.withPreventDefault(handler: fn(UIEvent): void)` | Wraps an event callback and automatically calls `ev.preventDefault()` before invoking `handler` |
| `ui.withKeyFilter` | `ui.withKeyFilter(targetKey: string, handler: fn(UIEvent): void)` | Wraps a keyboard event listener so `handler` is only invoked when `ev.key == targetKey` (e.g., `"Enter"`, `"Escape"`) |

---

## 7. Multi-Target Rendering Backends

### 7.1 Web Virtual DOM & HTTP LiveView (`ui.WebVdomBackend` & `server.serveUi`)

The `ui` engine integrates directly with the `http` server module via `server.serveUi(route, factory, title?, scopedCss?)`. Each connected browser tab receives:
1. Server-Side Rendered (SSR) initial HTML (`WebVdomBackend::renderDocument`).
2. An embedded WebSocket client that connects to `<route>/ws`.
3. Sub-millisecond JSON DOM patch arrays (`[{"op":"text","path":[0,1],"value":"Count: 1"}]`) computed by `WebVdomBackend::diff(oldTree, newTree)` whenever an event triggers a state transition.

```keet
import http;
import "./Counter.kml";

fn main(): int {
    let server: http.Server = http.createServer();

    server.serveUi("/", fn(): Counter {
        return Counter();
    }, "Counter LiveView");

    server.listen(3000);
    return 0;
}
```

### 7.2 Terminal User Interface (`ui.TuiBackend`)

Renders a `Component` or `UINode` scene graph directly to an interactive ANSI/VT100 terminal screen using a double-buffered Unicode character grid, Constraint/Flexbox layout solver (`FlexboxEngine`), and keyboard/mouse input parser (`TuiInputParser`):

- **`TuiBackend(width: int, height: int)`**: Creates a TUI renderer for the given terminal dimensions.
- **`backend.renderFrame(root: UINode): string`**: Computes Flexbox layout, rasterizes box-drawing borders and text cells into the back buffer, diffs against the front buffer, and returns the minimal ANSI escape sequence diff string.

### 7.3 Headless Test Backend (`ui.HeadlessBackend`)

Provides a deterministic, zero-display test harness for unit testing `.kml` components in `test` suites:

- **`HeadlessBackend(comp: Component, width?: int, height?: int)`**: Mounts `comp` in a virtual viewport (default `80x24`).
- **`harness.click(nodeId: string): bool`**: Synthesizes a click event on the node with `id == nodeId` and flushes the microtask update queue.
- **`harness.typeText(nodeId: string, text: string): bool`**: Focuses the target `<input>` node, dispatches synthetic input events, and flushes updates.
- **`harness.pressKey(key: string): bool`**: Dispatches a keydown event (`"Tab"`, `"Enter"`, `"ArrowDown"`, etc.) to the currently focused node.
- **`harness.getText(nodeId: string): string`**: Returns the current rendered text or label of `nodeId`.
- **`harness.dumpTree(): string`**: Serializes the scene graph with computed layout coordinates `(x, y, w, h)` for snapshot assertions.

---

## 8. IDE & Language Server Support (`keet-lsp`)

KeetScript provides native, in-memory Language Server Protocol intelligence for `.kml` Single-File Components:

- **Layout Primitives Hover:** Hovering over `<vstack>`, `<hstack>`, `<scroll>`, `<button>`, `<input>`, `<text>`, `<box>`, or `<slot>` displays description and attribute hints.
- **Custom Component Navigation:** Hovering or pressing **F12** (Go-to-Definition) on a custom component tag (e.g. `<Task />`) jumps directly to the underlying `Task.kml` file.
- **Runes & Directives Hover:** Full documentation for reactive runes (`$state`, `$derived`, `$props`, `$effect`) in `<script>`, event directives (`@click`, `@keydown`), and two-way bindings (`$bind.value`) in `<template>`.
- **Template Expression Cross-References:** Hovering or navigating on `{identifier}` expressions inside `<template>` resolves directly to the variable or function declared inside the component's `<script>` block.
- **Context-Aware Autocompletions:** Triggering autocomplete (`<`, `$`, `@`, `{`, `.`) in `.kml` provides tag names, event directives, attributes, and script symbols.


