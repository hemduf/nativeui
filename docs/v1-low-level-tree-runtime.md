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

`measure(constraints)` returns root `ChildMetrics` in logical units. `layout(viewport)` publishes retained logical geometry for a logical viewport size.

Failed layout is transactional: partial geometry is rolled back and the tree remains dirty for a later retry.

## Invalidation and damage

Tree separates layout and paint dirtiness:

- `dirty()` — either kind of work remains;
- `layout_dirty()` — geometry must be recomputed;
- `paint_dirty()` — repaint damage exists;
- `dirty_regions()` — borrowed logical damage rectangles.

`invalidate()` dirties the full viewport, `invalidate(rect)` dirties one root-logical region, and `invalidate_layout()` schedules retained layout work.

A host can install `set_invalidation_callback()`. The region-aware overload receives logical rectangles, and already-dirty regions are replayed immediately when a callback is installed.

## Input and focus

`dispatch(event, platform)` routes a normalized `InputEvent` through the retained focus/hit-test/pointer-capture model. Dynamic reconciliation and availability/focus synchronization occur at safe checkpoints around outer dispatch.

`focus_next()` / `focus_previous()` traverse eligible retained focus targets. Global command and KeyDown handlers are fallback seams after the focused retained route returns `Ignored`.

`cancel_pointer()` delivers PointerCancel to active capture owners and clears their capture/interaction state.

See [Input, focus and commands](v1-input-focus-and-commands.md) for the event-level contract.

## Painting

`paint(SkCanvas&, platform)` performs the full retained traversal and ensures layout first. Failed paint restores pre-existing damage instead of publishing a falsely clean frame.

`paint_region(Painter&, platform, repaint_region)` performs a selective root-logical traversal for renderer-owned damage reconstruction and does not consume Tree's own dirty state.

## Theme and diagnostics

`theme()` returns a borrowed reference to the tree-owned Theme. `set_theme()` classifies the change and invalidates paint or layout only as required. Do not retain the borrowed Theme reference across theme replacement.

`component_availability(NodeId)` returns current effective availability or nullopt for a stale/missing node. `structural_diagnostic()` returns a borrowed diagnostic string for bounded structural-reconciliation failures.

## Ownership/threading boundary

Tree owns retained nodes, focus, capture, invalidation and lifecycle state per instance. It adds no synchronization and must be driven from the UI/main thread.

Do not call Tree directly from an audio callback. Cross-thread producers must transfer data into the UI domain through a reviewed thread-safe bridge.

For normal view/application ownership use [NativeUI 1.0 overview and application lifetime](v1-overview-and-application-lifetime.md).
