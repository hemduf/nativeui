# NativeUI 1.0 core geometry and constraints API

This chapter documents the public geometry and box-constraint primitives used throughout NativeUI. The declarations live in [`geometry.hpp`](../include/nativeui/geometry.hpp) and [`constraints.hpp`](../include/nativeui/constraints.hpp). They are backend-neutral C++ value types and use **logical pixels** unless a consuming API explicitly says otherwise.

## Coordinate model

NativeUI public layout, hit-testing, painting inputs, and component geometry use logical coordinates. Platform/rendering adapters are responsible for conversion to physical framebuffer coordinates.

The foundational values are `ui::Size`, `ui::Point`, `ui::Rect`, `ui::Transform2D`, and `ui::Color`. These are ordinary value types: they do not own native objects, renderer state, component state, callbacks, or heap storage.

## `Size` and `Point`

`Size {w, h}` represents extents and `Point {x, y}` represents a position. Their fields are plain copied `float` values: neither type clamps, normalizes, allocates, retains external storage, or invokes callbacks. Negative/non-finite values therefore remain representable until a consuming API defines a stronger contract.

| declaration | units / meaning | validation and lifetime |
| --- | --- | --- |
| `Size::w`, `Size::h` | logical horizontal/vertical extents by default | stored verbatim; use `Constraints` when valid layout extents are required |
| `Point::x`, `Point::y` | logical position in the caller-named coordinate space | stored verbatim; no finite-value repair |
| `kPi` | radians helper constant | compile-time value; no runtime initialization/ownership |

These are detached value types, so returning or storing a copy creates no borrow into UI/tree state.

## `Rect`

A `Rect {x, y, w, h}` is non-empty only when both `w > 0` and `h > 0`; zero, negative, and NaN extents are empty.

`Rect::contains(Point)` includes the rectangle edges. `Rect::contains(Rect)` requires the candidate rectangle to be non-empty and fully enclosed, including coincident edges.

`intersect(a, b)` returns only a positive-area intersection. Rectangles that merely touch return `Rect{}`. `unite(a, b)` returns the smallest axis-aligned rectangle covering the non-empty inputs. `overlaps_or_touches(a, b)` counts edge/corner contact as true but returns false for empty inputs.

```cpp
ui::Rect a{0, 0, 40, 40};
ui::Rect b{40, 10, 20, 20};
bool adjacent = ui::overlaps_or_touches(a, b); // true
ui::Rect overlap = ui::intersect(a, b);         // empty
ui::Rect combined = ui::unite(a, b);            // {0,0,60,40}
```

## `Transform2D`

`Transform2D` stores the affine matrix `[m00 m01 m02; m10 m11 m12; 0 0 1]`. `m00/m01/m10/m11` are dimensionless linear coefficients; `m02/m12` are logical translation terms. Points are column vectors. NativeUI provides `identity()`, `translation(x, y)`, `scaling(x, y)`, and `rotation(radians)`.

The constructors intentionally do **not** share one validation policy. `translation()` and `scaling()` store their float inputs verbatim, including non-finite values; `rotation()` instead returns identity for a non-finite angle so an invalid trigonometric input does not poison downstream geometry. A zero/negative finite scale is representable, but zero scale is not invertible.

`map_point()` and matrix multiplication return owned values, allocate nothing, retain no references and perform no repair. A non-finite input matrix/point can therefore produce non-finite output.

### Composition order

For `a * b`, `b` is applied first and `a` second:

```cpp
auto scale = ui::Transform2D::scaling(2.0f, 2.0f);
auto move = ui::Transform2D::translation(10.0f, 0.0f);
auto transform = move * scale;
ui::Point p = transform.map_point({3.0f, 4.0f}); // {16,8}
```

### Inversion

`inverse()` returns `std::optional<Transform2D>`. It returns no value for non-finite matrices, zero-scale or near-singular linear parts, invalid determinants, or inverse elements that cannot be represented as finite `float` values.

```cpp
if (auto inv = transform.inverse()) {
    ui::Point local = inv->map_point(world);
}
```

## `Color`

`Color` stores copied floating-point RGBA channels: `r`, `g`, `b`, and `a`, with alpha defaulting to `1.0f`. The value type itself does not clamp channels to `[0, 1]`, assign a color space, allocate, or retain renderer state. Consuming APIs define any additional normalization/materialization.

`ui::colors` exposes compile-time convenience constants for background/panel surfaces, borders/focus, primary/muted text, accent, knob surfaces, track/toggle/input surfaces, selection and caret colors. These are literal defaults rather than semantic theme bindings; application theming should use the theme/style APIs when colors must vary by theme.

## `Constraints`

`Constraints` represents minimum and maximum sizes per axis. `min` and `max` are public owned `Size` snapshots in logical pixels; `kUnboundedExtent` is positive infinity and is the supported sentinel for an unbounded **maximum**. Minima never use infinity after normal construction.

Because the fields remain public value data, a caller can directly write a non-normalized state after construction. Helpers that create/consume ranges document whether they normalize defensively; `bounded_width()` / `bounded_height()` are pure queries and do not mutate/repair the object.

### Constructor normalization

The `Constraints(Size minimum, Size maximum)` constructor guarantees a usable range:

- negative finite minima become zero;
- non-finite minima become zero;
- positive infinity is accepted for a maximum and means unbounded;
- invalid/non-finite maxima other than positive infinity become the normalized minimum;
- every maximum is at least its corresponding minimum.

### Factories

- `unbounded()` — zero minima and unbounded maxima;
- `loose(maximum)` — copies the maximum and normalizes it as a constructor maximum; +infinity remains unbounded while NaN/negative infinity collapse to zero;
- `tight(size)` — identical finite minima/maxima; negative and **all** non-finite extents, including +infinity, normalize to zero.

All three return detached owned values, allocate nothing, retain no caller storage and invoke no callbacks.

```cpp
auto exact = ui::Constraints::tight({120.0f, 32.0f});
auto roomy = ui::Constraints::loose({640.0f, 480.0f});
auto free = ui::Constraints::unbounded();
```

### Queries and transforms

`bounded_width()` and `bounded_height()` report whether each maximum is finite. `loosen()` preserves maxima but resets minima to zero.

`inset(horizontal, vertical)` removes the supplied logical padding from both sides of each axis. Invalid inset values become zero; unbounded maxima remain unbounded.

```cpp
ui::Constraints outer{{100, 40}, {300, 120}};
ui::Constraints inner = outer.inset(8.0f, 4.0f);
// min {84,32}, max {284,112}
```

### `constrain(size)`

`constrain()` clamps a requested size into the normalized range. NaN and negative infinity fall back to the minimum. Positive infinity resolves to a finite maximum when one exists; on an unbounded axis it also falls back to the minimum.

```cpp
ui::Constraints c{{20, 10}, {100, 50}};
ui::Size a = c.constrain({80, 100}); // {80,50}
ui::Size b = c.constrain({-5, 20});  // {20,20}
```

## Ownership, threading, and allocation

Geometry and constraint values have no hidden registry, callback, allocation, native handle, or shared mutable state. They can be copied freely. Thread restrictions are inherited from the surrounding NativeUI operation rather than from these values.

## Related APIs

- [`v1-composition-layout-and-widgets.md`](v1-composition-layout-and-widgets.md) — retained composition and layout.
- [`v1-rendering-styling-and-animation.md`](v1-rendering-styling-and-animation.md) — painting and transforms.
- [`geometry.hpp`](../include/nativeui/geometry.hpp) — authoritative geometry declarations.
- [`constraints.hpp`](../include/nativeui/constraints.hpp) — authoritative constraint declarations.