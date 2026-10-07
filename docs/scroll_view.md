# ScrollView

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`ScrollView` combines Scroll, overlay bars, wheel input, optional panning and revealing focused descendants. Source: [scroll_view.inc](../include/nativeui/detail/scroll_view.inc), `ScrollView`, `ScrollViewComponent`, `ScrollbarComponent`, `ensure_visible`.

MyGo `ui/scroll.go`: `TrackScroll`, `ScrollIntoView`, `reveal`, `nearest`. This extraction preserves the current retained behavior.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API:

```cpp
template<class Child> ScrollView(ScrollState& state, Child&& child);
ScrollView&& pointer_pan(bool enabled=true) &&;
ScrollView&& style(ScrollbarStyle value) &&;
Spec spec() &&;
```

Verified existing example:

```cpp
ui::ScrollState offset{ui::ScrollAxis::Both};
auto view = ui::ScrollView{offset, ui::Label{"Content"}}.pointer_pan();
```

Preserve `ScrollAlignment`, the public `ensure_visible` functions, the ScrollViewComponent overloads and `ScrollbarStyle`. Panning is disabled by default.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

External State is borrowed through a lifetime token; style is owned. No gesture-end callback is added. Observers may change the offset or destroy the controller: revalidate the token after every notification.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

- Excluded from Tab: focusable is false, pointer_targetable is true.
- A bubbled wheel event is handled when the offset changes; at the limits, allow the ancestor to scroll.
- Opt-in panning: capture on down, move relative to the origin, release on up/cancel.
- Cancel preserves the already published offset, without an artificial application commit/cancel.
- Thumb dragging and track paging follow their current logic.
- Reverse hit testing: visible bars take priority over the content they cover.
- Descendant focus calls ensure_visible with Nearest.
- Hidden/disable/unmount end capture and panning.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Natural measurement on scrollable axes. Bars do not take space away from content: they are layout overlays painted after it. They are visible only when content exceeds the viewport. The shared corner shortens the tracks. Tiny bounds produce finite, non-negative thicknesses/rectangles.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

Uses the existing ScrollbarStyle. Hover/thumb changes require local paint; thickness changes require overlay placement. Offset changes repaint the viewport. No global scrollbar registry or permanent animation while hidden.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Wrapper role `None`. The current enum has no Scrollbar role: do not claim that role is available. Future scroll semantics must extend the contract separately; accessible descendants retain their availability and logical bounds.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

State expires during panning: set pan to false and release capture without reading state again. Observer exception: restore bookkeeping/capture before propagation so the next wheel event works. Never retain InputContext in an observe callback.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on [Scroll](scroll.md), ScrollState, ScrollbarStyle, focus reveal and clipping. Cover nested scrolling, overflow on only one axis, a zero viewport, non-finite thickness, a descendant larger than the viewport and state destroyed midway through panning.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/scroll_view.hpp` and `src/scroll_view.cpp`.

Gestures/placement and internal bars belong in scroll_view.cpp, with the ScrollViewComponent API preserved. Re-export the existing ScrollbarStyle header. `layout.hpp` remains the historical entry point; do not duplicate the ScrollState controller.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `scroll_view_wheel_bubble`: inner limit, outer movement.
- `scroll_view_pan_cancel`: capture released, offset preserved.
- `scroll_view_bar_priority`: track before content.
- `scroll_view_reveal`: Nearest with a large descendant.
- `scroll_view_dead_state`: expiration during a callback/pan.
- `scroll_view_tiny_bounds`: Both without a negative rectangle.
- `scroll_view_notify_fault`: a valid new gesture after an exception.

Create the future public example `examples/features/scroll_view.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
