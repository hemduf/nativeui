# Scroll

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Scroll` provides a viewport, clipping and ScrollState translation, without the gestures/bars of [ScrollView](scroll_view.md). Sources: [layout_builders.inc](../include/nativeui/detail/layout_builders.inc), [layout_components.inc](../include/nativeui/detail/layout_components.inc), `ScrollComponent`, and [scroll_view.inc](../include/nativeui/detail/scroll_view.inc), whose `RetainedScrollComponent` is used by the builder.

MyGo `ui/scroll.go`: `ScrollState`, `TrackScroll`, `reveal`. Preserve the separation between the layout primitive and the interactive container.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API:

```cpp
template<class Child> Scroll(ScrollState& state, Child&& child);
Spec spec() &&;
```

Verified existing example:

```cpp
ui::ScrollState offset{ui::ScrollAxis::Vertical};
auto scroll = ui::Scroll{offset, ui::Column{ui::Label{"Content"}}};
```

`ScrollState(ScrollAxis=Vertical)`, `set_offset(Point)`, `scroll_by(Point)`, `observe(std::function<void(Point)>)`, the offset/viewport/content/max getters and the lifetime token remain public. No Binding<Point> replaces them.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

An external controller owns the offset and its listeners and is borrowed through LifetimeToken. Use a separate controller for each independent viewport. Axis is fixed at construction; offset is sanitized and constrained to the permitted axes. Reentrant observation retains the current ScrollState discipline.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

No focus/capture of its own and no promise of ScrollView gestures. Movement comes from the controller. Descendant coordinates account for translation. Keyboard, wheel input, confirmation and cancellation remain normally routed to other components.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

- Measure without bounds on the scrollable axis and constrain the orthogonal axis.
- The viewport comes from the bounds; content retains its intrinsic size.
- Publish sizes and the clamped offset as coherent state.
- Apply translation by `−offset`, then intersect the viewport and ancestor clips.
- Empty content/viewport and content shrinkage recalculate max_offset.
- Preserve float Point/Size values and lifetime protections.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No painting; an offset change requests a viewport repaint without recomputing unchanged intrinsic sizes. A content-size change requests layout and new maximum offsets. Scroll draws no bars and creates no independent animation.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Layout role `None`, with descendants preserved. Accessible bounds are transformed once in logical coordinates. No artificial Slider represents the offset. A removed descendant is not kept alive solely for semantic queries.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

The token invalidates all access after state destruction, including inside an observe callback. Unsubscribe before unmounting. Listener failure: restore the dispatcher/guards and preserve accepted writes that have not started; subsequent scrolling remains possible.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on a single declaration of the shared ScrollState, layout and clipping. Cases: non-finite/negative values, Both, removed content, state destroyed during an observer and separate controllers. No additional native service.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/scroll.hpp` and `src/scroll.cpp`.

Keep `ScrollComponent` public even though the builder uses the safe internal runtime. `scroll.cpp` contains the retained core. `scroll.hpp` provides access to ScrollState/ScrollAxis; `scroll_view.hpp` reuses the same types and `layout.hpp` remains compatible.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `scroll_clamp_axis`: axes, limits, negative/NaN/inf values.
- `scroll_transform_clip`: geometry, pixels and hit testing.
- `scroll_content_shrink`: clamp the offset again.
- `scroll_state_lifetime`: destruction during observe without UAF.
- `scroll_observer_throw`: recovery after an exception.
- Preserve `scroll_layout_tests` and the `ScrollState` contracts.

Create the future public example `examples/features/scroll.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
