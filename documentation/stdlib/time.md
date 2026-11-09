# KeetScript Standard Library: `time` Module

The `time` module provides wall-clock time querying, high-resolution monotonic clocks, thread sleep and delay primitives, time unit conversion constants, thread-safe calendar timestamp formatting, and UTC date/time component extraction. It is built into the KeetScript standard library and compiler toolchain (`keety`).

All operations adhere to KeetScript's **Strict Zero-Coercion Policy (`E3002`)**: integer time representations and delays require `int`, while fractional second representations require `float`.

Fallible domain operations (such as negative sleep durations, `NaN`/`Infinity` in `sleepSec`, empty format patterns, or invalid timestamps) throw runtime `keet::error` instances named `"TimeError"`, integrating seamlessly with KeetScript's native `zonk` error recovery operators and blocks.

---

## 1. Importing the Module

The `time` module can be imported using any of the four standard import forms (as well as module aliases):

```keet
// 1. Unquoted module import
import time;

// 2. Quoted module import
import "time";

// 3. Unquoted selective import
from time import nowMs, sleepMs, MS_PER_SEC, monotonicSec, elapsedSec;

// 4. Quoted selective import
from "time" import nowMs, sleepMs, MS_PER_SEC, monotonicSec, elapsedSec;
```

```keet
// Module alias import
import time as t;

fn main(): void {
    let t0: int = time.nowMs();
    sleepMs(10);
    let elapsed: int = time.elapsedMs(t0);
    let iso: string = t.formatIso8601(t.now());
}
```

---

## 2. Time Unit Conversion Constants

All time conversion constants are 64-bit integers (`int`):

| Constant | Value | Description |
|---|---|---|
| `time.NS_PER_US` | `1000` | Nanoseconds per microsecond ($10^3$) |
| `time.NS_PER_MS` | `1000000` | Nanoseconds per millisecond ($10^6$) |
| `time.NS_PER_SEC` | `1000000000` | Nanoseconds per second ($10^9$) |
| `time.US_PER_MS` | `1000` | Microseconds per millisecond ($10^3$) |
| `time.US_PER_SEC` | `1000000` | Microseconds per second ($10^6$) |
| `time.MS_PER_SEC` | `1000` | Milliseconds per second ($10^3$) |
| `time.SEC_PER_MIN` | `60` | Seconds per minute |
| `time.SEC_PER_HOUR` | `3600` | Seconds per hour ($60 \times 60$) |
| `time.SEC_PER_DAY` | `86400` | Seconds per standard day ($24 \times 3600$) |

---

## 3. Wall-Clock Time Queries

Wall-clock functions query the host system real-time clock (`std::chrono::system_clock`) relative to the Unix epoch (1970-01-01 00:00:00 UTC):

| Function | Return Type | Description |
|---|---|---|
| `time.now(): int` | `int` | Current Unix timestamp in milliseconds (alias for `nowMs`) |
| `time.nowSec(): int` | `int` | Current Unix timestamp in whole seconds |
| `time.nowMs(): int` | `int` | Current Unix timestamp in milliseconds |
| `time.nowUs(): int` | `int` | Current Unix timestamp in microseconds |
| `time.nowNs(): int` | `int` | Current Unix timestamp in nanoseconds |

---

## 4. Monotonic High-Resolution Timers

Monotonic clock functions query a steady, monotonically increasing clock (`std::chrono::steady_clock`) unaffected by system clock adjustments:

| Function | Return Type | Description |
|---|---|---|
| `time.monotonicMs(): int` | `int` | Monotonic clock reading in milliseconds |
| `time.monotonicUs(): int` | `int` | Monotonic clock reading in microseconds |
| `time.monotonicNs(): int` | `int` | Monotonic clock reading in nanoseconds |
| `time.monotonicSec(): float` | `float` | Monotonic clock reading in fractional seconds (`double`), ideal for frame delta-time |
| `time.elapsedMs(startMs: int): int` | `int` | Returns elapsed milliseconds (`time.monotonicMs() - startMs`) |
| `time.elapsedSec(startNs: int): float` | `float` | Returns elapsed fractional seconds (`(time.monotonicNs() - startNs) / 1e9`) |

---

## 5. Thread Sleep Operations

Sleep functions pause the execution of the current thread:

| Function | Return Type | Description |
|---|---|---|
| `time.sleep(ms: int): void` | `void` | Sleep current thread for `ms` milliseconds; throws `TimeError` if `ms < 0` |
| `time.sleepMs(ms: int): void` | `void` | Sleep current thread for `ms` milliseconds; throws `TimeError` if `ms < 0` |
| `time.sleepUs(us: int): void` | `void` | Sleep current thread for `us` microseconds; throws `TimeError` if `us < 0` |
| `time.sleepSec(seconds: float): void` | `void` | Sleep current thread for fractional `seconds`; throws `TimeError` if `seconds < 0.0` or non-finite |

---

## 6. Formatting & Calendar Component Extraction

Calendar formatting and extraction functions convert Unix epoch timestamps (in milliseconds) to formatted strings or numerical date components using thread-safe POSIX `gmtime_r`/`localtime_r` and Windows `gmtime_s`/`localtime_s`:

| Function | Return Type | Description |
|---|---|---|
| `time.formatIso8601(epochMs: int): string` | `string` | Formats timestamp as `"YYYY-MM-DDTHH:MM:SSZ"` |
| `time.formatUtc(epochMs: int, pattern: string): string` | `string` | Formats UTC timestamp using `strftime` pattern (e.g. `"%Y-%m-%d %H:%M:%S"`); throws `TimeError` on empty pattern |
| `time.formatLocal(epochMs: int, pattern: string): string` | `string` | Formats local timezone timestamp using `strftime` pattern; throws `TimeError` on empty pattern |
| `time.yearUtc(epochMs: int): int` | `int` | UTC calendar year (e.g. `2026`) |
| `time.monthUtc(epochMs: int): int` | `int` | UTC calendar month in range `1` to `12` |
| `time.dayUtc(epochMs: int): int` | `int` | UTC calendar day of month in range `1` to `31` |
| `time.hourUtc(epochMs: int): int` | `int` | UTC hour in range `0` to `23` |
| `time.minuteUtc(epochMs: int): int` | `int` | UTC minute in range `0` to `59` |
| `time.secondUtc(epochMs: int): int` | `int` | UTC second in range `0` to `59` |

---

## 7. Practical Usage Examples

### 7.1 High-Resolution Benchmarking
```keet
import time;
import io;

fn benchmarkTask(): void {
    let start_ns: int = time.monotonicNs();

    // Perform intensive task
    time.sleepMs(15);

    let duration_s: float = time.elapsedSec(start_ns);
    io.println("Task completed in ", duration_s, " seconds");
}
```

### 7.2 Game Loop Delta-Time
```keet
import time;

fn gameLoop(): void {
    let mut last_frame: float = time.monotonicSec();

    let mut frame_count: int = 0;
    while (frame_count < 60) {
        let current_time: float = time.monotonicSec();
        let dt: float = current_time - last_frame;
        last_frame = current_time;

        // Simulate frame step
        time.sleepMs(16);
        frame_count = frame_count + 1;
    }
}
```

### 7.3 Zonk Error Recovery
```keet
import time;
import io;

fn safeSleep(duration_ms: int): void {
    time.sleep(duration_ms) zonk (err) {
        io.println("Failed to sleep: ", err.message);
    };
}

fn safeDateFormat(epoch_ms: int, pattern: string): string {
    return time.formatUtc(epoch_ms, pattern) zonk "1970-01-01 00:00:00";
}
```
