# Padding

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Padding` reserves uniform inner spacing around a child. Sources: `Padding` / `PaddingComponent` in [layout_builders.inc](../include/nativeui/detail/layout_builders.inc) and [layout_components.inc](../include/nativeui/detail/layout_components.inc).

MyGo `ui/layout.go`, `padX`, `padY`, `contentX`, `contentY`, handles padding and borders separately. NativeUI retains the uniform wrapper here without importing the entire box model.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API:

```cpp
template<class Child> Padding(float padding, Child&& child);
Spec spec() &&;
```

Verified existing example:

```cpp
auto padded = ui::Padding{12.0f, ui::Label{"Content"}};
```

Preserve `PaddingComponent(float)` and `float` precision. An asymmetric variant is outside the specified extraction.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

Padding and the child Spec are owned. No external state, notification or callback of its own. Inner size is calculated from the current pass’s bounds without a global dimension cache.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

No focus stop or activation. Spacing does not become a hit target; only eligible descendants react. Keyboard and wheel input retain their routing. Confirmation/cancellation belongs to the child control.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Preferred/minimum size: child metrics + padding on both sides of each axis. Child constraints are inset; placement is at `(x+p,y+p)` with `max(0,w−2p)` and `max(0,h−2p)`. Negative or non-finite padding becomes zero. Padding greater than the bounds preserves an empty, finite inner rectangle.

- Current code uses `std::max(0.0f,padding)`; the finite-value guarantee above is a target robustness requirement, rather than proof that +inf is currently handled.
- No border is added to measurement; padding and stroke are distinct.
- Measuring a child at a reduced width can increase its height through wrapping.
- The parent receives recalculated metrics rather than a size from an old viewport.
- Constraint insetting must preserve min<=max after sanitization.
- A scale-factor change remains a backend conversion, rather than multiplying the padding in the recipe.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No painting or background color. Do not paint spacing in a surface color. Changing padding through a rebuild requests layout; a child paint change does not request intrinsic sizes again if its metrics are unchanged.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Role `None`, with the child flattened. The control’s accessible bounds include only its layout bounds, rather than the wrapper’s entire spacing area. Padding does not change the control’s name/description.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Do not retain constraint references between passes. Measurement/layout exceptions remain recoverable through a Tree transaction. Unmounting creates no application size notification.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on geometry types and constraint insetting. Edge cases: empty content, excessive padding, child clipping, scroll transformations and non-finite values. `Column::padding` remains separate and compatible.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/padding.hpp` and `src/padding.cpp`.

Extract `PaddingComponent` into `padding.cpp`, with its public declaration in `padding.hpp`. The template child constructor remains in the header. Preserve `layout.hpp` and the historical uniform meaning.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `padding_intrinsic`: add 2p to minimum/preferred sizes.
- `padding_inset_bounds`: exact inner placement.
- `padding_excess`: small bounds, with width/height never negative.
- `padding_nonfinite`: NaN/inf/negative values are sanitized.
- `padding_hit_geometry`: spacing does not activate the child.
- `padding_fault_recovery`: child failure followed by a correct next pass.

Create the future public example `examples/features/padding.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
