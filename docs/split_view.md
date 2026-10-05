# SplitView

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

Two panes adjustable through a separator, side by side or stacked. The reviewed NativeUI version has no SplitView builder; composing Row/Column does not provide the splitter input contract. Foundations: [layout.hpp](../include/nativeui/layout.hpp), [component_base.hpp](../include/nativeui/component_base.hpp).

MyGo reference `ui/split.go`: `Split`, `SplitVertical`, `split`. MyGo maintains the first pane’s size in DIP, with 40 DIP minima and a 1 DIP line. The target adds explicit callbacks and cancellation recovery.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Proposed target API:

```cpp
enum class SplitOrientation { Horizontal, Vertical };
template<class First, class Second>
SplitView(Binding<double> first_extent, First&& first, Second&& second);
template<class First, class Second>
SplitView(State<double>& first_extent, First&& first, Second&& second);
SplitView&& orientation(SplitOrientation) &&;
SplitView&& minimum_panes(double first, double second) &&;
SplitView&& step(double value) &&;
SplitView&& on_change(std::function<void(double)> callback) &&;
SplitView&& on_commit(std::function<void(double)> callback) &&;
SplitView&& style(SplitViewStyle) &&;
Spec spec() &&;
```

Future example:

```cpp
ui::State<double> sidebar_width{220.0};
auto split = ui::SplitView{sidebar_width, ui::Label{"Sources"}, ui::Label{"Editor"}}
    .minimum_panes(40.0,40.0).orientation(ui::SplitOrientation::Horizontal);
```

Defaults: Horizontal, minima 40/40, step 10 DIP, line 1 DIP, hit grip 6 DIP. The style retains these metrics as configurable fields without assuming a new Theme slot.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

The application owns the desired extent of the first pane. The view calculates an effective extent clamped to the viewport without rewriting the model merely because of a resize. Gestures write to Binding, then call on_change if the value differs; release calls on_commit once if the edit changed. The gesture origin and external generation support safe cancellation; an external write during dragging ends the drag without rolling back the external value.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

- Down in the grip captures the pointer and focuses the separator; move adjusts the chosen axis from the origin.
- Up commits and releases; PointerCancel/Escape restores the origin if no concurrent external value has replaced the gesture.
- Orientation-specific arrows adjust by one step; Shift multiplies by ten; Home/End move to the bounds.
- Wheel input does not change the splitter and bubbles.
- Tab follows pane 1, separator, pane 2; disabled blocks input, while read-only permits focus without adjustment.
- Visibility loss or unmounting cancels capture without an application teardown callback.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

The first shared-axis dimension is the effective extent, followed by the line, then the remainder; the cross axis is shared. If the minima cannot be satisfied, distribute available space proportionally between the minima and a bounded line without negative sizes. Clip child overflow to its pane. The hit grip overlaps panes without reserving a 6 DIP gap. Convert the double model value to a validated float during placement.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

SplitViewStyle defines the line, grip, hover/drag/focus colors and metrics. Accent the line during hover/drag and show a ring on the grip for focus-visible. Extent changes request pane layout; color changes request only separator paint. No backend drawing in the header.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

The Splitter role does not currently exist: target `Custom` with the name “Separator”, a value/range and Focus/Increment/Decrement/SetValue actions when eligible. A specialized role would be a separate extension. Panes retain their descendants and logical order.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Prepare a finite, clamped candidate before the setter. Reset capture/guards before calling on_commit; a throwing callback does not replay commit. A stale invalidator after removal is a no-op through a weak owner. Unsubscribe and cancel capture state without invoking user code during destruction.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on Row/Column, clipping, capture/focus, Binding and style. Reject negative or non-finite minima/step with invalid_argument during construction; step must be strictly positive. Non-finite external extent: display the first minimum without an automatic external rewrite; the next valid extent recovers. Exactly two children are required; no general pane management in v1.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/split_view.hpp` and `src/split_view.cpp`.

split_view.hpp declares orientation/style/the public splitter Component and templates that convert two children. split_view.cpp contains the effective extent, gesture transaction, measure/layout and paint. No separate SplitVertical alias; this variant remains within the SplitView header/source pair.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `split_orientations`: horizontal/vertical geometry and arrows.
- `split_minimum_small_view`: insufficient space without negative rectangles.
- `split_live_commit_cancel`: counted writes/callbacks; Escape restores the origin.
- `split_external_during_drag`: the external value wins, with no stale rollback.
- `split_resize_no_model_write`: effective clamping does not rewrite the extent.
- `split_removed_under_capture`: capture and stale callbacks are cleaned up.
- `split_commit_throw`: at-most-once commit and a subsequent gesture remains possible.

Create the future public example `examples/features/split_view.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
