# KeetScript Standard Library: `math` Module

The `math` module provides standard mathematical functions, trigonometric operations, angle conversions, floating-point classifications, mathematical constants, and thread-safe pseudo-random number generation (PRNG). It is built into the KeetScript standard library and compiler toolchain (`keety`).

All operations adhere to KeetScript's **Strict Zero-Coercion Policy (`E3002`)**: functions expecting `float` require explicit float operands, functions expecting `int` require integer operands, and polymorphic functions require matching numeric types across all arguments.

Fallible domain operations (such as `clamp` where `minVal > maxVal` or `randomInt`/`randomFloat` where `minVal > maxVal`) throw runtime `keet::error` instances named `"MathError"`, integrating seamlessly with KeetScript's native `zonk` error recovery operators and blocks.

---

## 1. Importing the Module

The `math` module can be imported using any of the four standard import forms (as well as module aliases):

```keet
// 1. Unquoted module import
import math;

// 2. Quoted module import
import "math";

// 3. Unquoted selective import
from math import PI, sin, cos, sqrt, clamp, absInt, randomInt;

// 4. Quoted selective import
from "math" import PI, sin, cos, sqrt, clamp, absInt, randomInt;
```

```keet
// Module alias import
import math as m;

fn main(): void {
    let radius: float = 5.0;
    let circumference: float = 2.0 * math.PI * radius;
    let s: float = m.sin(m.PI / 2.0);
    let clamped: float = clamp(sqrt(16.0), 0.0, 10.0);
}
```

---

## 2. Mathematical Constants

All mathematical constants are defined as 64-bit IEEE 754 floating-point numbers (`float`):

| Constant | Value | Description |
|---|---|---|
| `math.PI` | `3.14159265358979323846` | Archimedes' constant $\pi$ (ratio of circumference to diameter) |
| `math.E` | `2.71828182845904523536` | Euler's number $e$ (base of natural logarithm) |
| `math.TAU` | `6.28318530717958647692` | Circle constant $\tau = 2\pi$ (radians in a full turn) |
| `math.INF` | `std::numeric_limits<double>::infinity()` | Positive infinity ($+\infty$) |
| `math.NAN` | `std::numeric_limits<double>::quiet_NaN()` | Quiet Not-a-Number (NaN) representation |

---

## 3. API Reference

### 3.1 Basic Arithmetic & Polymorphic Functions

#### `math.abs(x: float): float` / `math.abs(x: int): int`
Returns the absolute value of `x`. Overloaded polymorphically for both `float` and `int`.

#### `math.absInt(x: int): int`
Explicitly returns the absolute value of integer `x`.

#### `math.min(a: float, b: float): float` / `math.min(a: int, b: int): int`
Returns the smaller of `a` and `b`. Both arguments must share the same type (`E3002` on type mismatch).

#### `math.minInt(a: int, b: int): int`
Explicitly returns the smaller of integer `a` and `b`.

#### `math.max(a: float, b: float): float` / `math.max(a: int, b: int): int`
Returns the greater of `a` and `b`. Both arguments must share the same type (`E3002` on type mismatch).

#### `math.maxInt(a: int, b: int): int`
Explicitly returns the greater of integer `a` and `b`.

#### `math.clamp(val: float, minVal: float, maxVal: float): float` / `math.clamp(val: int, minVal: int, maxVal: int): int`
Clamps `val` to the range `[minVal, maxVal]`. All arguments must share the same type.
- **Error Condition:** Throws `"MathError"` if `minVal > maxVal`.
- **Zonk Example:**
  ```keet
  let safe: float = math.clamp(x, low, high) zonk low;
  ```

#### `math.clampInt(val: int, minVal: int, maxVal: int): int`
Explicit integer clamping. Throws `"MathError"` if `minVal > maxVal`.

#### `math.sign(x: float): float` / `math.sign(x: int): int`
Returns `-1` if $x < 0$, `0` if $x = 0$, and `1` if $x > 0$.

#### `math.signInt(x: int): int`
Explicit integer signum function.

---

### 3.2 Rounding & Truncation

All rounding functions accept a `float` and return a `float` representing the nearest integer value:

#### `math.floor(x: float): float`
Returns the largest integer value less than or equal to `x` ($\lfloor x \rfloor$).

#### `math.ceil(x: float): float`
Returns the smallest integer value greater than or equal to `x` ($\lceil x \rceil$).

#### `math.round(x: float): float`
Returns the integer value nearest to `x`, rounding half-way cases away from zero.

#### `math.trunc(x: float): float`
Truncates `x` toward zero by discarding fractional digits.

---

### 3.3 Power, Root, Exponential & Logarithmic Functions

#### `math.sqrt(x: float): float`
Returns the square root of `x` ($\sqrt{x}$).

#### `math.cbrt(x: float): float`
Returns the cube root of `x` ($\sqrt[3]{x}$).

#### `math.pow(base: float, exp: float): float`
Returns `base` raised to power `exp` ($\text{base}^{\text{exp}}$).

#### `math.hypot(x: float, y: float): float`
Returns the hypotenuse $\sqrt{x^2 + y^2}$ without intermediate overflow or underflow.

#### `math.exp(x: float): float`
Returns $e^x$.

#### `math.log(x: float): float`
Returns the natural (base $e$) logarithm of `x` ($\ln x$).

#### `math.log2(x: float): float`
Returns the binary (base $2$) logarithm of `x` ($\log_2 x$).

#### `math.log10(x: float): float`
Returns the common (base $10$) logarithm of `x` ($\log_{10} x$).

---

### 3.4 Trigonometric, Hyperbolic & Angle Conversions

#### `math.sin(rad: float): float`
Returns the sine of `rad` (in radians).

#### `math.cos(rad: float): float`
Returns the cosine of `rad` (in radians).

#### `math.tan(rad: float): float`
Returns the tangent of `rad` (in radians).

#### `math.asin(x: float): float`
Returns the principal arc sine of `x` in radians ($[-\pi/2, \pi/2]$).

#### `math.acos(x: float): float`
Returns the principal arc cosine of `x` in radians ($[0, \pi]$).

#### `math.atan(x: float): float`
Returns the principal arc tangent of `x` in radians ($[-\pi/2, \pi/2]$).

#### `math.atan2(y: float, x: float): float`
Returns the arc tangent of $y/x$ using signs to determine the quadrant in radians ($[-\pi, \pi]$).

#### `math.sinh(x: float): float`
Returns the hyperbolic sine of `x`.

#### `math.cosh(x: float): float`
Returns the hyperbolic cosine of `x`.

#### `math.tanh(x: float): float`
Returns the hyperbolic tangent of `x`.

#### `math.toRadians(degrees: float): float`
Converts `degrees` to radians ($\text{degrees} \times \pi / 180$).

#### `math.toDegrees(radians: float): float`
Converts `radians` to degrees ($\text{radians} \times 180 / \pi$).

---

### 3.5 Classification, Interpolation & Modulo

#### `math.isNan(x: float): bool`
Returns `true` if `x` is NaN (Not-a-Number).

#### `math.isInf(x: float): bool`
Returns `true` if `x` is positive or negative infinity ($\pm\infty$).

#### `math.isFinite(x: float): bool`
Returns `true` if `x` is a finite number (neither NaN nor infinite).

#### `math.lerp(a: float, b: float, t: float): float`
Computes linear interpolation between `a` and `b` with weight `t` ($a + t(b - a)$).

#### `math.fmod(x: float, y: float): float`
Computes the floating-point remainder of $x / y$.

---

### 3.6 Pseudo-Random Number Generation (PRNG)

The PRNG uses a thread-local 64-bit Mersenne Twister engine (`std::mt19937_64`) initialized with non-deterministic hardware entropy (`std::random_device`).

#### `math.seed(s: int): void`
Seeds the thread-local PRNG with integer `s`. Re-seeding with the same value reproduces the exact same sequence of pseudo-random numbers.

#### `math.random(): float`
Generates a uniformly distributed pseudo-random floating-point number in the half-open interval $[0.0, 1.0)$.

#### `math.randomInt(minVal: int, maxVal: int): int`
Generates a uniformly distributed pseudo-random integer in the closed interval $[\text{minVal}, \text{maxVal}]$.
- **Error Condition:** Throws `"MathError"` if $\text{minVal} > \text{maxVal}$.

#### `math.randomFloat(minVal: float, maxVal: float): float`
Generates a uniformly distributed pseudo-random floating-point number in the half-open interval $[\text{minVal}, \text{maxVal})$.
- **Error Condition:** Throws `"MathError"` if $\text{minVal} > \text{maxVal}$.

---

## 4. Error Handling and the `zonk` Operator

Operations that have preconditions on their domain arguments (`clamp`, `clampInt`, `randomInt`, `randomFloat`) throw a `keet::error` named `"MathError"` when bounds are inverted ($\text{minVal} > \text{maxVal}$).

These exceptions integrate directly with KeetScript's `zonk` operator and recovery blocks:

```keet
import math;
import io;

fn generateSafeOffset(userMin: int, userMax: int): int {
    // If userMin > userMax, randomInt throws MathError; zonk recovers with userMin
    let offset: int = math.randomInt(userMin, userMax) zonk userMin;
    return offset;
}

fn computeNormalizedRatio(raw: float, low: float, high: float): float {
    return math.clamp(raw, low, high) zonk { err ->
        io.eprintln("Invalid bounds: " + err.message);
        yield low;
    };
}
```

---

## 5. Strict Zero-Coercion Policy (`E3002`)

In accordance with KeetScript specification, the compiler enforces strict zero-coercion on all mathematical functions:

```keet
import math;

fn main(): void {
    // ❌ Error E3002: Implicit numeric conversion from 'int' to 'float' is disallowed
    let a: float = math.sin(1);

    // ✅ Correct: Use float literal or explicit conversion
    let b: float = math.sin(1.0);
    let c: float = math.sin(float(1));

    // ❌ Error E3002: Mixed types in polymorphic call
    let m1 = math.min(10, 20.5);

    // ✅ Correct: Matching types
    let m2: int = math.min(10, 20);
    let m3: float = math.min(10.0, 20.5);
}
```
