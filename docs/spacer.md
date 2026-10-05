# Spacer

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Spacer` is a fixed-size layout leaf with no painting. Source: `Spacer` / `SpacerComponent` in [layout_builders.inc](../include/nativeui/detail/layout_builders.inc) and [layout_components.inc](../include/nativeui/detail/layout_components.inc).

No independent family needs to be ported from MyGo: the equivalent is composed through `Box` and its constraints. This additional NativeUI component remains public.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API:

```cpp
explicit Spacer(float height);
Spacer(float width, float height);
explicit Spacer(Size size);
Spec spec() &&;
```

Verified existing example:

```cpp
auto column = ui::Column{ui::Label{"A"}, ui::Spacer{0.0f, 12.0f}, ui::Label{"B"}};
```

`Spacer(float)` means height, with zero width. Preserve `SpacerComponent(Size)`.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

Size is copied when the builder is consumed. No child, binding, callback or animation. The spacer uses no global state and acquires no resources.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

No focus, hit target or capture. It does not react to pointer, wheel or keyboard input and triggers neither confirmation nor cancellation. Its space does not prevent the parent from receiving events.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Minimum and preferred size are the sanitized size: each negative or non-finite axis becomes zero. The parent retains authority over the allocated bounds. Zero width/height remains legal. `Flex{Spacer{...}}` can provide expandable space through the wrapper weights.

- Do not use the presence of pixels as the criterion for intrinsic size.
- A zero-width spacer in Row reserves only the normal gap between children.
- In Column, zero height still represents a logical child and follows the current gap count.
- External constraints do not write a new Size into the recipe.
- An application that wants to remove gaps uses Collapsed/If instead of an invisible spacer.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No pixels are painted. Changing size through a rebuild requests layout; no style, focus ring or hover state. Do not paint the theme background in place of the spacer.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Role `None`, with no child; no semantic entry is exposed. Decorative space must not be announced as empty text or a navigable separator.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Construction and unmounting have no user-visible effects. An allocation exception before Spec publication leaves nothing registered. Create/destroy cycles and two spacers have no cross-dependencies.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on the `Size` and `Component` types. Cover all overloads, reduced constraints and non-finite values; no dependency on Skia, timers or platforms.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/spacer.hpp` and `src/spacer.cpp`.

Preserve all three overloads and the public `SpacerComponent`. `spacer.cpp` contains measurement/minimum/paint operations and sanitization; `layout.hpp` re-exports the individual header.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `spacer_height_overload`: `(12)` equals `(0,12)`.
- `spacer_size_overload`: Size and two floats are equivalent.
- `spacer_sanitization`: negative/NaN/inf axes are independently reduced to zero.
- `spacer_in_flex`: allocation by weight without changing the minimum.
- `spacer_no_input_semantics`: no focus/hit target/semantic node.
- `spacer_headless`: pixels unchanged after paint.

Create the future public example `examples/features/spacer.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
