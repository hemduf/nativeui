# Column

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Column` stacks children vertically with uniform padding, horizontal alignment and flex factors. Sources: [layout_builders.inc](../include/nativeui/detail/layout_builders.inc), [layout_components.inc](../include/nativeui/detail/layout_components.inc), `ColumnComponent`.

MyGo reference: `ui/layout.go`, `flexLayout` and `justifyOffsets`. MyGo combines more box properties; the NativeUI extraction preserves its explicit historical values.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API:

```cpp
template<class... Children> explicit Column(Children&&... children);
Column&& gap(float value) &&;
Column&& padding(float value) &&;
Column&& align(Align value) &&;
Column&& justify(Justify value) &&;
Spec spec() &&;
```

Verified existing example:

```cpp
auto column = ui::Column{ui::Label{"Title"}, ui::Label{"Content"}}
    .padding(12.0f).gap(6.0f).align(ui::Align::Stretch);
```

Current values: gap `16`, padding `24`, Start/Start. `ColumnComponent(float gap, float padding, Align, Justify)` remains public.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

- No application state of its own; `Spec` objects and child order are owned.
- Preferred metrics and minima are derived from children on each accepted pass.
- Padding belongs to this instance and is not a global theme override.
- Changes to descendant data request measurement without a user notification from the container.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

The container does not take focus, capture or wheel input. Fields retain their interactions, gestures and confirmation behavior. Tab follows construction order. Hidden/Collapsed affect eligibility through the runtime; `Column` does not synthesize an additional cancellation.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

- Preferred width: the maximum child width + padding on both sides.
- Height: the sum of heights + N−1 gaps + padding on both sides; an empty column retains both padding amounts.
- Constraints are inset by the padding; the Y axis is unbounded for intrinsic measurement.
- Y placement receives flex factors, while X placement receives `Align`.
- `SpaceBetween` distributes the surplus after padding and gaps.
- Non-finite or negative padding/gap values become zero. The inner area never becomes negative.
- No implicit scrolling or clipping; use [ScrollView](scroll_view.md) when content overflows.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No independent painting. A metric change invalidates layout and moved regions; a color change in a single child requests only that child’s paint. `StyleScope` is inherited by descendants but does not replace explicit `Column` padding/gap values.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Role `None`, with no focus stop or action. Semantic order follows child order; justification does not reorder it. A label above a field does not establish a semantic relationship on its own: use [Field](field.md).

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Measure and then publish placements through a Tree transaction. An exception in a child preserves the last coherent layout; the next pass recalculates everything. The container retains neither `ChildMetrics&` nor an invalidator capturing a child after the pass.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Reuses the shared flex layout; do not port the entire MyGo box engine. Cases to cover: empty content, one collapsed child, padding greater than the bounds, a minimum greater than the available space, flex children with zero weight and resizing while focus is active.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/column.hpp` and `src/column.cpp`.

Move `ColumnComponent` and its functions into `column.cpp`, maintaining the public declarations. `layout.hpp` becomes a compatible include for the individual headers. `Column::padding(float)` remains available even though [Padding](padding.md) exists separately.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `column_padding_intrinsic`: minimum/preferred size with empty content and multiple children.
- `column_cross_stretch`: constrained width, Start/Center/End/Stretch.
- `column_grow_shrink`: vertical allocation and respect for minima.
- `column_small_bounds`: excessive padding without negative width/height.
- `column_focus_order`: identical Tab order under every justification.
- `column_layout_fault`: a throwing measurement followed by an accepted resize.

Create the future public example `examples/features/column.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
