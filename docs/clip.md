# Clip

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Clip` confines paint and hit testing to the allocated rectangle while preserving the child’s intrinsic measurement. Sources: `Clip` and `ClipComponent` in [layout_builders.inc](../include/nativeui/detail/layout_builders.inc) / [layout_components.inc](../include/nativeui/detail/layout_components.inc).

MyGo expresses this capability through `Element.Clip` and box state in `ui/layout.go`. Do not turn this wrapper into scrolling or a backend-specific raster mask.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API:

```cpp
template<class Child> explicit Clip(Child&& child);
Spec spec() &&;
```

Verified existing example:

```cpp
auto clipped = ui::Clip{ui::Row{ui::Label{"Long text"}}};
```

Preserve the public `ClipComponent` and its clipping hooks. This extraction adds no radius, state or callback.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

One owned `Spec`; no observation or external model. The effective clip is the intersection of this rectangle with ancestor clips. Its identity remains that of an independent retained node, without a mutable offscreen bitmap.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

- No focus of its own; children may be focusable.
- Hit testing never targets a portion outside the effective intersection.
- A captured child continues to receive events according to the Tree capture contract; clipping does not break its recovery.
- Wheel input and keys travel to the child/parent according to normal rules.
- Escape has no clip-specific meaning.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Forward parent constraints and report the sole child’s minimum/preferred size. Place the child within the wrapper bounds; clipping does not change its content metrics. Empty bounds imply an empty painted/targetable area without changing the model. Do not round to pixels during layout.

- An empty clip does not remove the child model or create implicit Collapsed visibility.
- The container does not promise rounded clipping; a separate API must provide that capability.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

The wrapper does not paint. Update the invalidation region when bounds/clip change; clear the old visible region. Painter clip scoping uses RAII and is restored even if child painting throws. Siblings must never accidentally inherit the clip.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Role `None`. Accessible bounds remain logical according to the runtime; clipping creates no action. Descendants ineligible due to availability are absent, without inventing an “outside the viewport = destroyed” rule for accessible objects.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

The runtime owns the clip stack; `Clip` retains no reference to it beyond paint. Tree handles unmounting during capture. After a paint exception, the next sibling and the next frame recover the clip required by their normal contract.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends only on layout/paint/hit-testing services. Cover nested clips, scroll transformations, Stack overlap, child overflow and a zero rectangle. Do not expose a `SkCanvas` primitive in the API.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/clip.hpp` and `src/clip.cpp`.

The template constructor remains in `clip.hpp`; `ClipComponent` publicly declares its capability. `clip.cpp` contains measurement, placement, clip hooks and no-op paint. Preserve `layout.hpp` and avoid a second implementation of the clipping stack.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `clip_paint_intersection`: two nested clips, with pixels outside the intersection unchanged.
- `clip_hit_rejection`: an overflowing child receives no pointer-down outside the clip.
- `clip_capture_teardown`: removal during capture followed by no stale access.
- `clip_restore_after_throw`: throwing paint, with the sibling and next frame correct.
- `clip_scroll_transform`: logical coordinates after offset and scaling.
- Preserve the regressions in `t075_scoped_clipping_tests`.

Create the future public example `examples/features/clip.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
