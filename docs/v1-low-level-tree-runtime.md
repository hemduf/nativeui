# NativeUI 1.0 low-level retained Tree runtime

Most applications should construct a [`UI`](../include/nativeui/ui.hpp). `UI` owns a retained [`Tree`](../include/nativeui/component_tree.hpp) and adds normal view/overlay/dialog policy.

`Tree` is the public low-level runtime for advanced integrations and tests that deliberately control retained lifecycle, measurement/layout, input/focus, invalidation and painting themselves. It is non-copyable, instance-owned and UI/main-thread confined.

## Construction

`compile(Spec)` converts a declarative hierarchy into an exclusively owned retained root with `NodeId` values assigned from 1. The root is transferred into Tree:

```cpp
auto root = ui::compile(ui::make_spec(content));
ui::Tree tree{std::move(root)};
```

Normal application code generally lets `UI` compile the tree internally.

## Lifecycle

The expected low-level sequence is:

```text
compile -> mount -> measure/layout -> activate_focus
                     |                    |
                     +--- dispatch/paint -+
                                  |
                          deactivate_focus -> unmount
```

`mount()` is parent-to-child and rolls back partial retained state on failure. `activate_focus(platform)` activates components and establishes the focus/input domain.

`deactivate_focus(platform)` clears hover, pointer capture and focus before retained component deactivation. If callbacks throw, cleanup continues and the first captured exception is rethrown after the remaining cleanup.

`unmount()` tears down child-to-parent and deactivates first when required. Tree's destructor is no-throw best-effort cleanup, so integrations that need to observe lifecycle failures should use explicit calls.

## Measurement and layout

`measure(constraints)` returns root `ChildMetrics` in logical units without publishing geometry. It can flush pending dynamic structure first, so the syntactically const call is not a pure read: factories/measurement callbacks may allocate or throw.

`layout(viewport)` publishes retained logical geometry for a logical-pixel viewport after dynamic reconciliation. An unchanged clean viewport is a no-op. Failed layout is transactional: partial geometry is rolled back to the last coherent publication, the tree remains dirty, and the exception propagates for a later retry.

## Invalidation and damage

Tree separates layout and paint dirtiness:

- `dirty()` — either kind of work remains;
- `layout_dirty()` — geometry must be recomputed;
- `paint_dirty()` — repaint damage exists;
- `dirty_regions()` — borrowed logical damage rectangles.

`invalidate()` dirties the full viewport, `invalidate(rect)` dirties one root-logical region, and `invalidate_layout()` schedules retained layout work.

A host can install `set_invalidation_callback()`. Tree owns the callback. Region-aware notification is synchronous on the UI thread when newly exposed logical damage is published; the zero-argument overload discards region detail. Already-dirty regions are replayed immediately during installation.

Callbacks may re-enter Tree or throw. Dirty/layout state is recorded before notification, and a newly installed callback remains installed if replay throws. `invalidate(rect)` is bounded by the current viewport/DirtyRegion policy; `invalidate_layout()` also conservatively dirties the full viewport because bounds can move.

## Input, focus and reentrancy

`activate_focus(platform)` establishes the active interaction domain and borrows that PlatformServices object for the active interval. Keep it alive until deactivation/teardown and use the same logical platform domain for dispatch/focus calls.

`dispatch(event, platform)` routes normalized events through retained focus/hit-test/capture. Most events require an active Tree; `DropOffer` / `DropData` may target a mounted inactive Tree. Pointer positions use root-logical coordinates.

Dispatch is reentrant. Each frame receives a Tree-local generation token so an older outer pointer event cannot erase a newer nested interaction that reuses the same `PointerId`. Exception paths restore dispatch bookkeeping before propagating; the outermost successful return reconciles queued dynamic/availability/focus work.

The public pointer-interaction index/token/begin/end helpers expose this generation mechanism for advanced integrations/tests. They are allocation-free, track at most 16 IDs and normally should not replace normal `dispatch()`/capture lifecycle.

Global Command/KeyDown handlers are owned fallback callbacks after focused retained routing returns `Ignored`. The KeyDown slot keeps the executing callable alive across reentrant replacement/clear.

`cancel_pointer()` delivers PointerCancel and completes the cleanup it owns before an exception propagates.

See [Input, focus and commands](v1-input-focus-and-commands.md) for the event-level contract.

## Painting

`paint(SkCanvas&, platform)` borrows its targets synchronously, prepares pending structure/focus/layout and performs the full retained traversal. A successful frame consumes only damage that existed before the first paint callback; reentrant invalidation remains dirty for the next frame. A failing callback causes pre-existing damage to be merged back before the exception propagates.

`paint_region(Painter&, platform, repaint_region)` is a renderer-facing selective traversal in root-logical coordinates. It may invoke the same application paint/layout machinery but **never consumes Tree dirty state**, so it must not be treated as acknowledgement of presentation.

Both paths belong to the UI/render domain and are not audio-real-time safe.

## Theme and diagnostics

`theme()` returns a borrowed reference to the tree-owned Theme. `set_theme()` classifies the change and invalidates paint or layout only as required. Do not retain the borrowed Theme reference across theme replacement.

`component_availability(NodeId)` returns current effective availability or nullopt for a stale/missing node. `structural_diagnostic()` returns a borrowed diagnostic string for bounded structural-reconciliation failures.

## Ownership/threading boundary

Tree owns retained nodes, focus, capture, invalidation and lifecycle state per instance. It adds no synchronization and must be driven from the UI/main thread.

Do not call Tree directly from an audio callback. Cross-thread producers must transfer data into the UI domain through a reviewed thread-safe bridge.

For normal view/application ownership use [NativeUI 1.0 overview and application lifetime](v1-overview-and-application-lifetime.md).
