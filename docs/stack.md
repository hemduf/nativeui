# Stack

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Stack` layers children in composition order. Source: `Stack` / `StackComponent` in [layout_builders.inc](../include/nativeui/detail/layout_builders.inc) and [layout_components.inc](../include/nativeui/detail/layout_components.inc).

MyGo uses its boxes and absolute placement in `ui/layout.go`, `layoutAbsolute`. No CSS engine port is required for this already retained component.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API:

```cpp
template<class... Children> explicit Stack(Children&&... children);
Spec spec() &&;
```

Verified existing example:

```cpp
auto layered = ui::Stack{ui::Label{"Background"}, ui::Label{"Foreground"}};
```

`StackComponent` remains public. The extraction adds no z-order binding or callback.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

The builder owns Spec objects in order; the runtime maintains child identity. No layer registry or retained pointer to a node. Reordering children follows reconciliation contracts rather than an opaque local sort.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

Paint in order, with reverse hit testing: the eligible top child receives pointer input. A transparent, non-targetable child does not block others. Tab retains logical runtime order; Stack creates no modal scope. Wheel/keyboard/cancellation behavior remains that of the targeted child.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Minimum/preferred size = the per-axis maximum among children; empty = zero. All children receive the shared bounds. No implicit offset or clipping; use [Padding](padding.md), [Clip](clip.md) and explicit placement wrappers. Reduced bounds do not produce negative sizes.

- Preferred size is not the sum of layer sizes.
- The child placed last is not automatically declared modal.
- Each layer’s minima remain available to the parent even when they exceed the bounds.
- A layer’s overflow remains its own content, without compensating translation of its siblings.
- Layers without pointer_targetable do not intercept input merely because of their z-order.
- A replacement child at the same position does not receive the previous capture through address reuse.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No painting of its own; clear old regions when removing a layer. Do not confuse Stack with an overlay: Stack belongs to ordinary layout, while popups remain managed by the Overlay service. Local invalidation accounts for covered layers.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Role `None`. Semantic order remains logical child order; non-semantic decoration does not replace reading the controls underneath. A visible overlapping layer does not automatically establish modality.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Removing a layer during focus/capture uses the same safe checkpoints as other children. A paint failure restores painter state and does not leave the next frame with a changed layer order. Stack owns no native callback.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on Tree for paint and reverse hit testing. Test independent overlays, hidden/collapsed children, unequal sizes and reordering. Stack must not reserve a raster surface for each child.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/stack.hpp` and `src/stack.cpp`.

The variadic constructor in `stack.hpp` remains the adapter. `stack.cpp` contains the public `StackComponent` methods. Preserve the historical `layout.hpp` include and the current paint order.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `stack_max_intrinsic`: maximum measurement, zero when empty.
- `stack_shared_bounds`: identical placement for all children.
- `stack_reverse_hit`: the last targetable child takes priority.
- `stack_hidden_layer`: a hidden layer does not intercept pointer input.
- `stack_remove_top`: repaint restores the lower layer without artifacts.
- `stack_paint_throw`: painter state is restored after a failure.

Create the future public example `examples/features/stack.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
