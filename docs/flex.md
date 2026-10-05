# Flex

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Flex` provides its child’s grow/shrink weights to the Row/Column container. `FlexComponent` and the builder exist in [layout_components.inc](../include/nativeui/detail/layout_components.inc) and [layout_builders.inc](../include/nativeui/detail/layout_builders.inc).

MyGo: `ui/layout.go`, `resolveFlexible`. The target contract extracts the wrapper; it preserves NativeUI allocation behavior and does not reproduce a complete CSS property.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API:

```cpp
template<class Child> explicit Flex(Child&& child);
Flex&& grow(float value) &&;
Flex&& shrink(float value) &&;
Spec spec() &&;
```

Verified existing example:

```cpp
auto row = ui::Row{ui::Label{"Name"},
    ui::Flex{ui::Label{"Description"}}.grow(1.0f).shrink(1.0f)};
```

Default weights: zero/zero. Preserve `FlexFactors` and `FlexComponent(float grow, float shrink)`.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

Factors are copied into the retained component. No binding or callback. The child retains its own model; grow/shrink does not imply permission to mutate it. Do not store these weights in a registry tied to the child’s address.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

Transparent to pointer input, keyboard input, focus and confirmation. Descendant interactions retain coordinates relative to the final placement. No capture or shortcut. The wrapper does not consume wheel input, preserving ancestor scrolling.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

- Minimum/preferred size comes from the child and is not multiplied by the weights.
- `flex_factors()` exposes weights to the parent; the allocation core treats negative/non-finite values as zero.
- The parent distributes surplus space according to grow and the deficit according to shrink, respecting minima.
- The wrapper places its child across the entire resulting rectangle.
- Outside Row/Column, the weights produce no independent scaling.
- Nested wrappers do not implicitly multiply weights.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No painting, with transparent theme inheritance. Changing factors through a rebuild requires layout; the renderer has no Flex style. No automatic expansion animation or hover color.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Role `None`, with descendants flattened. Allocated size appears in their bounds; no numeric or range semantic value is attached to grow/shrink factors.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

No subscriptions to release; removing the child removes its retained state according to Tree. A layout failure does not publish a partially changed factor. Do not retain a reference to `ChildMetrics` between passes.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on the `flex_factors()` contract and the shared allocator. Edge cases: all weights zero, inf/NaN, an empty child, minima exceeding the bounds, a deficit after gap/padding and a very large factor. Precision remains `float` for current compatibility.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/flex.hpp` and `src/flex.cpp`.

Extract non-template `FlexComponent` methods into `flex.cpp`. The construction template remains a make_spec adapter. Preserve `layout.hpp` includes and the public API without silently converting existing weights to double.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `flex_intrinsic_passthrough`: weights do not alter minimum/preferred size.
- `flex_weight_sanitization`: zero/negative/NaN/inf values.
- `flex_proportional_grow`: weights 1:2, with the expected distribution.
- `flex_shrink_freeze_minimum`: shrinkage is redistributed after a minimum is reached.
- `flex_without_linear_parent`: no independent effect on Stack.
- `flex_nested_lifetime`: removal/recreation preserves the correct descendant model.

Create the future public example `examples/features/flex.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
