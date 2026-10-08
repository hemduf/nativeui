# Row

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Row` arranges children along the horizontal axis, with flex allocation and cross-axis alignment. It exists in [layout_builders.inc](../include/nativeui/detail/layout_builders.inc); `RowComponent` in [layout_components.inc](../include/nativeui/detail/layout_components.inc) is public and paints no pixels.

MyGo `ui/layout.go`, with the `flexLayout`, `resolveFlexible` and `justifyOffsets` functions, provides a more general flex layout. This component retains its model without automatic line wrapping; this port does not introduce implicit flex-wrap.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API to preserve:

```cpp
template<class... Children> explicit Row(Children&&... children);
Row&& gap(float value) &&;
Row&& align(Align value) &&;
Row&& justify(Justify value) &&;
Spec spec() &&;
```

Verified existing example:

```cpp
auto row = ui::Row{ui::Label{"Name"}, ui::Label{"Value"}}
    .gap(8.0f).align(ui::Align::Center).justify(ui::Justify::Start);
```

The defaults are `gap=18`, `Align::Start` and `Justify::Start`. Preserve `RowComponent(float, Align, Justify)` and the public enums.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

- The builder owns the ordered list of `Spec` objects; the runtime owns the mounted components.
- `Row` has no binding, selection state or callback of its own.
- The runtime manages child identity; a content change invalidates the parent layout.
- `Flex` factors belong to the children, rather than to a mutable container table.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

- The container has no focus stop or activation.
- Pointer and keyboard input target descendants in the normal runtime order.
- Tab follows composition order; visual alignment does not change reading order.
- Wheel input, capture, confirmation and cancellation are delegated to the children.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

- Preferred size: the sum of widths and gaps between children, with the maximum height.
- Minimum: the same formula using the child minima.
- Measurement is unbounded along the X axis; height remains constrained by the parent.
- Placement uses `allocate_main_axis`: grow distributes surplus space; shrink removes space without crossing the minima.
- `Stretch` affects height; `SpaceBetween` distributes only positive free space.
- A negative or non-finite gap becomes zero, as it does today. An empty row measures zero.
- No automatic clipping; a deficit beyond the minima causes overflow, handled through [Clip](clip.md) or [ScrollView](scroll_view.md).

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

- The container draws no background, border or focus ring.
- Changing alignment or gap during a rebuild changes only placement/metrics.
- Color and typography remain those of the descendants and [StyleScope](style_scope.md).
- Widget styles do not change explicit layout values.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

The target role remains `None`: flatten the wrapper while preserving descendant order. The row has no name, value, action or selected state of its own. A semantically named row must be wrapped in a group explicitly provided by the application.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

A child measurement or mount failure must not publish partial placement. Keep the existing layout transaction; after recovery, rebuild positions from accepted metrics without reusing references to removed children.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on `ChildMetrics`, `Constraints`, `Align`, `Justify` and the flex core. Test collapsed children, a minimum greater than the available width, zero width and resizing. The runtime reconciles a child removed during its callback without manual traversal after removal.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/row.hpp` and `src/row.cpp`.

Preserve the historical `layout.hpp` include, variadic constructors, `float` signatures and `RowComponent`. Extract layout from `layout_components.inc`; the template constructor only converts and owns `Spec` objects. The placement algorithm remains non-template code in `row.cpp`.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `row_intrinsic_and_empty`: 0, 1 and 3 children, with the gap applied exactly N−1 times.
- `row_flex_minimum`: grow/shrink distribution; minima are never crossed.
- `row_alignment_justify`: Start/Center/End/Stretch and SpaceBetween, including a deficit.
- `row_nonfinite_gap`: NaN/inf/negative values are reduced to zero.
- `row_removed_during_layout`: exception/reconciliation followed by a valid resize.
- Reuse the contracts of the existing `layout_flex_tests`, `layout_alignment_tests` and `layout_constraints_tests`.

Create the future public example `examples/features/row.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
