# NativeUI 1.0 overview and application lifetime

This chapter documents the stable NativeUI 1.0 architecture and application/window lifetime model that are already delivered. The landed `State<T>` / `Binding<T>` contract is documented in [`v1-state-and-binding.md`](v1-state-and-binding.md), and current `main` is the working source of truth for the public/package surface after T069 was deprecated as a standalone freeze gate. A production reference application and copy-pasteable Getting Started journey are not currently delivered: T070 is closed as not planned, and the roadmap tracks that work as a separate replan with T133–T137 still unresolved.

## What NativeUI is

NativeUI is a C++20 retained-mode UI toolkit for standalone desktop applications, native child views embedded by an external host or adapter, and deterministic headless UI tests.

NativeUI owns the retained UI domain: declarative composition, runtime component-tree ownership, layout, input and focus routing, invalidation, widgets, text, resources and NativeUI drawing abstractions. Its normal public model is intentionally independent from any plug-in SDK or audio engine.

NativeUI does **not** own:

- CLAP, VST3, AU, AAX or other plug-in-format APIs;
- DSP or audio processing;
- plug-in parameter identifiers, automation or edit-gesture semantics;
- real-time audio-thread synchronization;
- a second custom Win32, Cocoa or X11 windowing stack.

A plug-in or host adapter may embed a NativeUI child view, but the adapter remains responsible for the plug-in/host contract. Ordinary NativeUI UI and platform operations are UI/main-thread work unless a public API explicitly states otherwise.

## Supported NativeUI 1.0 platforms

The supported NativeUI 1.0 desktop targets are:

- macOS under the repository's supported CI/toolchain policy;
- Windows x64 with MSVC;
- Linux x64 with X11.

Pugl is the implementation layer that owns native view creation, embedding and event delivery. Skia is the implementation renderer used behind NativeUI drawing abstractions. Normal application/widget code should use NativeUI APIs rather than depending directly on either implementation library or on Win32/AppKit/X11 implementation types.

All public component geometry is expressed in logical pixels. Native view/framebuffer scaling is handled below the retained component model.

## Retained architecture

The stable architecture is deliberately layered:

```text
standalone application / external host adapter
                    |
                    v
          NativeUI public UI model
   composition - components - layout - input
                    |
                    v
          retained runtime component tree
      /             |               \
 layout/input   invalidation       paint
                                     |
                                     v
                         NativeUI drawing layer
                                     |
                                     v
                           private renderer

native view/event integration ------> Pugl ------> Cocoa / Win32 / X11
```

A `UI` owns one retained component tree. Declarative construction creates the initial specification; the resulting runtime tree owns component instances and their parent/child relationships, layout state, focus/input routing and invalidation state. A normal custom component extends this retained model instead of registering itself in a process-global component registry.

Pugl and Skia are implementation dependencies, not alternate application models. The durable architecture is described in [DESIGN.md](../DESIGN.md); the backend-neutral public surface on current `main` is authoritative for this documentation.

## Application and window ownership

NativeUI has two deliberately separate native-view ownership models:

1. **Standalone:** one explicit `Application` owns one standalone event-loop/world backend; every top-level `StandaloneWindow` attaches to that `Application`.
2. **Embedded:** each `EmbeddedView` is a host-owned child-view integration path and does not register with or borrow the standalone `Application` world.

There is no hidden mutable "current application", current-window singleton, process-global NativeUI window registry or `thread_local` application owner.

### Ownership summary

| Object | Owns | Borrows / depends on | Lifetime rule |
| --- | --- | --- | --- |
| `Application` | one standalone program world/event loop and application-local registration state | platform resources used by that world | Construct/use/destroy on the UI thread. It must outlive every attached valid `StandaloneWindow`. |
| `UI` | one retained component tree and its per-UI interaction/layout/invalidation state | application-owned state/resources referenced by the constructed component graph | Treat a `UI&` passed to a live native view wrapper as borrowed: keep the `UI` alive until that wrapper has been destroyed. UI mutation is UI-thread confined. |
| `StandaloneWindow` | one top-level native view plus per-window render/platform/close state | `Application&` and `UI&` | Non-copyable/non-movable. A valid window registers exactly once with its `Application` and unregisters exactly once before teardown completes. |
| `EmbeddedView` | one embedded native child view plus its per-view render/platform state | `UI&` and a host-provided native parent handle | Non-copyable/non-movable. Keep the `UI` and host parent valid for the embedded view lifetime. It never registers with `Application`. |

The public declarations are in [`include/nativeui/window.hpp`](../include/nativeui/window.hpp) and [`include/nativeui/ui.hpp`](../include/nativeui/ui.hpp).


### Retained `UI` lifecycle and host-driving contract

`UI` owns one materialized retained component tree plus its layout, focus/input,
overlay/dialog, semantic and paint-damage state. A standalone or embedded native
view wrapper borrows `UI&`; destroy that wrapper before destroying the `UI`.
The `PlatformServices` object passed to activation belongs to the same host-view
domain and remains borrowed by the active retained tree until deactivation or
teardown.

The host-facing sequence is:

1. construct `UI(root[, theme])` on the UI thread; root factories and mount
   callbacks run synchronously and may throw after retained rollback;
2. create the standalone/embedded native wrapper that borrows the live `UI`;
3. call `activate(platform)` for the view's interaction interval;
4. drive `resize()`, `dispatch()`, invalidation and `paint()` from that same
   UI domain using logical-pixel geometry;
5. call `deactivate(platform)` when the view leaves its active interval;
6. destroy the native wrapper, then destroy the `UI`.

`dispatch()` is deliberately reentrant. Application callbacks can trigger
ordinary state/UI work, overlay changes and nested dispatch; retained generation
and transaction guards prevent older frames from erasing newer nested state.
Dialog/popup completion paths that permit application code to destroy the `UI`
repair their retained ownership before the callback and do not dereference the
destroyed object afterwards.

`paint(canvas, platform)` borrows both arguments only for the synchronous call.
Deferred lifecycle-reentrant resize and overlay placement are reconciled before
drawing. A successful paint consumes only the damage snapshot present at the
paint transaction start, so invalidation raised reentrantly during painting
survives for the next frame. On failure, consumed damage is restored before the
exception propagates.

All of these lifecycle, dispatch and paint operations are UI/main-thread work.
They may allocate or invoke application/component callbacks and are not suitable
for an audio/DSP real-time thread.

## Native-window descriptor and operation contracts

The public native-view surface uses **logical pixels** for every `Size`, `Rect`, minimum/maximum constraint, text-input rectangle and preferred-size notification. `scale_factor()` is the device conversion snapshot used at the platform boundary; application code should not pre-scale geometry before calling these APIs. Native configure events remain authoritative for the size reported by `size()`, so a successful `set_size()` is a submitted request rather than a synchronous guarantee that the OS has already configured that exact extent.

`WindowDesc` is consumed during standalone construction. Its title is copied, optional bounds are values, and no descriptor field is borrowed after construction. Invalid/non-finite size constraints or a minimum larger than the maximum are rejected by the view geometry layer. The explicit `StandaloneWindow(Application&, UI&, WindowDesc)` path records normal platform/descriptor construction failure in `valid()/last_error()` while allocation failure may propagate; embedded construction is not a status-returning factory and may propagate construction errors.

| API family | Ownership/lifetime | Failure/result semantics | Reentrancy/threading |
| --- | --- | --- | --- |
| `Application::run/poll` | application owns one program world; windows are separately owned | `run()` returns non-zero for invalid/terminal failure; `poll()` becomes false at quit/terminal failure | UI/main thread; pumps dispatchers and may enter user callbacks |
| `StandaloneWindow::dispatcher()` / `EmbeddedView::dispatcher()` | returned dispatcher is a value facade; it does not extend native-view lifetime | shutdown rejects later work according to Dispatcher contract | callbacks run at dispatcher checkpoints, not on an audio thread |
| `native_handle()` | borrowed opaque child/top-level native identifier owned by the wrapper | zero means no usable native view; any prior value becomes stale at teardown | UI/main-thread integration only |
| `last_error()` | borrowed `string_view` into wrapper/application storage | empty means no stored diagnostic; later operations may replace the backing string | copy the text if it must survive later mutation/destruction |
| `show()/hide()` | keep the same realized retained/native view | idempotent on the current visibility state; false reports unavailable/platform failure | may synchronously enter native callbacks; hide restores pointer/focus/IME invariants before propagating cleanup exceptions |
| `set_size()/set_min_size()/set_max_size()` | consume value geometry; retain no caller reference | requests use logical pixels; size is clamped to active bounds; invalid constraint pairs/native submission return false | UI/main-thread platform calls, not RT safe |
| preferred-size callback | wrapper owns the `std::function`; callback receives an owned `Size` | advisory only; host may ignore/clamp and NativeUI never resizes the host parent | delivered on UI/main thread at a safe checkpoint; standalone callback may synchronously call `set_size()` |

The `QuitPolicy` and close APIs deliberately separate **event-loop lifetime** from **native-window lifetime**. `QuitPolicy::OnLastWindowClosed` only requests application quit when the last registered window actually completes close/unregistration. `ExplicitOnly` leaves an empty application runnable. Conversely, `Application::request_quit()` never destroys windows.

For a standalone window, `request_close()` is an accepted programmatic close and never calls the user-close veto. Native/user close requests go through `on_close_request()`; returning `CloseDecision::Cancel` keeps the window open, while `Accept` schedules close at a safe checkpoint. `should_close()` therefore means “terminal close is pending/requested or the native backend wants termination”, whereas `is_closed()` means the NativeUI close sequence has actually completed. `on_closed()` runs exactly once for accepted close while the C++ wrapper still exists; direct destructor teardown is intentionally silent.

```cpp
ui::Application app;
if (!app.valid()) {
    // Copy if the diagnostic must outlive the next application operation.
    const std::string error{app.last_error()};
    return 1;
}

ui::UI editor{/* root spec */};
ui::StandaloneWindow window{
    app,
    editor,
    ui::WindowDesc{
        .title = "Editor",
        .size = {900.0f, 620.0f},
        .resizable = true,
        .min_size = ui::Size{480.0f, 320.0f},
        .max_size = std::nullopt,
    }};

window.on_close_request([] { return ui::CloseDecision::Accept; });
window.set_preferred_size_callback(
    [&window](ui::Size preferred) { (void)window.set_size(preferred); });

return app.run();
```

All methods above are platform/UI-domain operations. None is an audio/DSP real-time API, and callers must not retain borrowed `string_view`, native handles, service references, or host/application/UI references beyond the lifetime documented by their owner.

## Standalone: one Application, multiple windows

`Application` is the sole normal NativeUI 1.0 owner of the standalone event loop. It is non-copyable and non-movable and owns exactly one standalone program world. Multiple top-level windows share only that application/world/event-pump domain; their mutable retained UI, focus, capture, text, rendering and close state remain per-window/per-UI.

The canonical lifetime order is:

1. create the `Application`;
2. create the `UI` objects that will back the windows;
3. create each `StandaloneWindow` with the same `Application` and its corresponding `UI`;
4. run or poll the `Application` on the UI thread;
5. close/destroy every standalone window before destroying the `UI` and `Application` objects they borrow.

The existing [`t060_multi_window_application`](../examples/features/t060_multi_window_application.cpp) feature example is the maintained executable demonstration of the shared-Application multi-window model. This chapter does not duplicate that example as a second Getting Started snippet source.

Pre-1.0 independent-world standalone compatibility APIs are intentionally not documented as a supported 1.0 ownership model. Current public headers and package exports on `main` define the supported ownership surface.

### Application initialization and registration

Native initialization failures are represented through object validity/error state rather than by intentionally translating platform/Pugl failures into public platform exceptions. An invalid `Application` cannot successfully enter its event loop or accept a valid window registration. A failed `StandaloneWindow` construction does not create a phantom registered window.

Only successfully initialized windows participate in the `Application` window count and quit policy. This keeps the application lifetime graph explicit and avoids hidden ownership promotion after a partial construction failure.

### Event loop authority

`Application::run()` is the normal blocking standalone loop. `Application::poll()` pumps the same standalone world for all attached top-level windows. Individual v1 standalone windows do not own independent event loops.

Application/window native operations are UI/main-thread operations. Callback-triggered creation, destruction or close work must reach a safe outer checkpoint rather than mutating native ownership reentrantly inside an arbitrary platform callback.

### Quit and close are separate concepts

The default `QuitPolicy::OnLastWindowClosed` requests application quit when the final valid registered window actually unregisters. `QuitPolicy::ExplicitOnly` allows an application with zero windows to remain runnable until explicit quit or an error.

`Application::request_quit()` is idempotent and stops the event loop after the current top-level callback unwinds. It does **not** destroy or close the application's windows. C++ owners remain responsible for window lifetime.

Closing or destroying one window does not implicitly close another. Destroying window A while window B remains registered leaves B attached to the same `Application` and able to continue operating; this is a required part of the multi-window contract.

A `StandaloneWindow` may also have close policy/callback state, but that state is per window. Application quit policy does not replace per-window close handling.

## UI lifetime and instance isolation

A `UI` is one retained tree and is not a process-wide service. Its layout, focus, pointer routing, overlays, invalidation and other retained state are instance-owned.

The native wrappers receive a `UI&`; application code must therefore keep the referenced `UI` alive while the corresponding `StandaloneWindow` or `EmbeddedView` exists. For independent top-level windows, independent `UI` instances are the normal ownership pattern even though the windows share one `Application` event loop.

`UI` is intentionally confined to the UI/main thread. Worker or audio threads must not directly mutate the retained tree. Cross-thread service guidance belongs to [`v1-services-testing-and-limits.md`](v1-services-testing-and-limits.md); this chapter only establishes the ownership boundary.

State/Binding lifetime, equality and callback/reentrancy semantics are part of the landed contract and are documented in [`v1-state-and-binding.md`](v1-state-and-binding.md).

## UI orchestration contract

`UI` owns one mounted retained tree and its Theme, layout, focus, input, overlay and invalidation state. Construction consumes the root specification, mounts immediately and may propagate allocation or component-lifecycle failures after retained rollback. Destruction is `noexcept`, publishes UI death before retained teardown, suppresses late dialog completion, and detaches presentation invalidation before member destruction.

All operations in this section belong to the owning UI/main-thread domain. `UI` has no internal synchronization and none of these APIs is an audio/DSP real-time primitive.

### Theme, measurement and viewport publication

`theme()` returns a **borrow** into UI-owned Theme storage. The reference ends at successful theme replacement or UI destruction; copy any Theme data that must survive either boundary. `set_theme(theme)` takes its replacement by value and moves it into retained ownership, so caller storage need not remain alive. Theme replacement classifies the resolved change and publishes only the required paint/layout invalidation; re-resolution may allocate or invoke component work and exceptions propagate through the ordinary retained retry rules.

`measure(constraints)` consumes logical-pixel constraints synchronously and returns an owned `ChildMetrics` value without publishing viewport geometry or acknowledging paint damage. Dynamic factories/component measurement may run and throw. If a lifecycle transition is already active, measurement is suppressed and returns a default/empty metrics value rather than exposing partially transitioned retained state.

`resize(viewport)` publishes root-logical viewport geometry used by layout, hit testing, overlays and paint. During an active lifecycle transition, NativeUI records the latest requested viewport and applies it at a later safe checkpoint instead of mutating geometry reentrantly. Outside that case, overlay/root layout preparation occurs synchronously and failures propagate while leaving retry state coherent.

### Lifecycle, input and callback ownership

`activate()`, `deactivate()`, `refresh_focus()`, `dispatch()`, `cancel_pointer()` and `paint()` borrow the supplied `PlatformServices` only within the host-view domain documented for that operation. Component/focus/input callbacks may execute synchronously, re-enter ordinary UI/State work and throw.

`dispatch()` is a retained reentrancy boundary: structural work requested by callbacks is committed only at safe outer checkpoints. Popup/dialog completion runs after retained detach and may destroy the `UI`; application code must therefore not retain internal node/component borrows across dispatch. `cancel_pointer()` completes the capture cleanup it owns before propagating a PointerCancel callback exception, preventing a failed callback from leaving the old capture live.

`set_command_handler()` and `set_key_down_handler()` transfer ownership of their `std::function` into UI; an empty function clears the slot. Fallbacks run synchronously only after focused retained routing ignores the corresponding input. The KeyDown event reference is valid only during the callback. Reentrant replacement/clear does not invalidate the currently executing KeyDown callable. Exceptions propagate from the enclosing dispatch after dispatch bookkeeping is restored.

### Invalidation, damage and diagnostic borrows

`set_invalidation_callback()` owns its callable. Installing a callback synchronously replays already-dirty regions before returning; later notifications execute inside the UI operation that publishes new damage. Dirty state is recorded **before** notification, so reentrant or throwing callbacks cannot erase the invalidation they were told about, and a newly installed callback remains installed if replay throws. The zero-argument overload has identical semantics but discards rectangle geometry.

`clear_invalidation_callback()` detaches notification only; it does not clear dirty state. `invalidate()` marks the complete current viewport, `invalidate(Rect)` publishes one root-logical damage region subject to the Tree's clipping/coalescing policy, and `invalidate_layout()` marks geometry dirty and conservatively publishes paint damage because bounds may move.

The read APIs have deliberately different ownership:

| API | Ownership/lifetime contract |
| --- | --- |
| `dirty()`, `layout_dirty()`, `paint_dirty()` | allocation-free point-in-time values; no work is flushed or consumed |
| `dirty_regions()` | borrowed vector of root-logical rectangles; later mutation/reconciliation may change or reallocate it |
| `structural_diagnostic()` | borrowed UI-owned string that may be replaced by later reconciliation |
| `component_availability(id)` | owned snapshot or `nullopt` for invalid/stale/missing identity |
| `component_semantics(id)` | owned semantic snapshot or `nullopt`; does not keep the retained node/component alive |
| `overlay_entries()` | owned creation-order snapshot of public overlay policy/anchor/resolved logical geometry; content/platform objects are not exposed |

Copy borrowed diagnostics before later UI mutation if they must survive. `overlay_entries()` may allocate while building its owned vector.

### Paint transaction and compatibility alias

A successful `paint()` consumes only damage that existed before paint callbacks began; invalidation raised reentrantly remains dirty for the next frame. If paint throws, the pre-existing damage transaction is restored for retry.

With `NATIVEUI_ENABLE_INSPECTOR`, the `ui::debug` helpers remain UI-thread diagnostics with the separate contracts described in [resources, services, testing and limits](v1-services-testing-and-limits.md).

`PluginUI` is only a backward-compatibility alias for `UI`. It adds no plug-in SDK, parameter automation, host lifetime, audio-thread or synchronization semantics.

## EmbeddedView: host-owned child integration

`EmbeddedView` is separate from the standalone `Application` model. It represents one native child view embedded in a parent supplied by an external host/adapter.

Key lifetime rules are:

- `EmbeddedView` does not register with a standalone `Application` and does not borrow its standalone world;
- construction, polling, resizing and destruction are host UI/main-thread operations;
- the host-provided parent handle is borrowed; the host must keep the parent valid while the embedded child exists;
- the referenced `UI` must remain alive while the `EmbeddedView` uses it;
- `EmbeddedView::poll()` is always non-blocking and is suitable for a host UI idle/timer mechanism;
- destroying an `EmbeddedView` tears down NativeUI's child integration; NativeUI does not acquire ownership of the host's parent object.

The combined standalone-parent/embedded-child lifecycle is exercised by [`t041_smoke_harness`](../examples/features/t041_smoke_harness.cpp). Plug-in SDK ownership and host-specific parameter/audio semantics remain outside NativeUI.


### Embedded child visibility

`EmbeddedViewOptions{.initially_visible = false}` supports hosts that separate native child realization from presentation. The option controls the NativeUI child only; it never raises, shows, hides, resizes, or takes ownership of the embedding host window.

A hidden embedded child remains realized. Its retained `UI`, Dispatcher and platform resources stay alive, and the host may continue calling the always-non-blocking `poll()`, updating State, and granting sizes. `show()` and `hide()` are idempotent UI-thread operations. They return false once the native child is terminally closed or otherwise unable to perform the request.

`visible()` reports NativeUI's requested **local child visibility**. It is deliberately not an occlusion/presentation query: a true value does not prove that the host window is unminimized, that all ancestors are shown, that the child is unclipped, or that pixels are currently visible on screen.

Showing is passive: it does not raise the host window or steal host ownership. A successful show invalidates retained presentation so the next frame is fresh. Hiding preserves the realized child but deactivates retained focus/input, cancels active pointer edits, and stops IME before returning. If application cancellation callbacks throw, NativeUI restores those cleanup invariants first and then propagates the exception to the direct UI-thread caller.

`request_close()` remains distinct from `hide()`: close is terminal for the native child lifetime, while hide is reversible. The view continues to borrow the `UI` and host parent in both visible and hidden states, so both must outlive the `EmbeddedView`.

```cpp
ui::EmbeddedView child{
    tree,
    host_parent,
    {640.0f, 420.0f},
    {},
    ui::EmbeddedViewOptions{.initially_visible = false}};

child.poll();   // host-owned and non-blocking while hidden
child.show();   // passive local child presentation
child.hide();   // keeps the realized child and retained UI alive

const bool locally_shown = child.visible();
(void)locally_shown;
```

These operations may enter platform services, invalidate retained rendering, and dispatch lifecycle/application callbacks. They are UI/main-thread operations and are not audio/DSP real-time safe.

## Lifetime checklist

For NativeUI 1.0 application/embedding code, keep these invariants visible in the owning code:

- perform retained UI and native-view operations on the UI/main thread;
- keep `Application` alive longer than every attached `StandaloneWindow`;
- keep each borrowed `UI` alive longer than the native wrapper that uses it;
- keep an embedded native parent alive longer than its `EmbeddedView`;
- let each standalone window keep its own mutable UI/view state even though the application event loop is shared;
- do not introduce a mutable global, singleton or `thread_local` current Application/window owner;
- do not assume `request_quit()` destroys windows;
- do not use standalone compatibility paths as a second 1.0 ownership model;
- keep embedded polling non-blocking.

These rules are also review requirements; see [CODE_REVIEW.md](../CODE_REVIEW.md) for the project's ownership, instance-isolation, threading and reentrancy checks.

## Remaining reconciliation

This chapter is reconciled with the landed T123/T124 state/binding safety work and with the current public/package surface on `main`; T069 is no longer an active freeze gate.

The remaining release-facing work is external to this chapter:

- the production reference application and copy-pasteable Getting Started path need to be replanned after T070 closed as not planned; T133–T137 remain unresolved in the roadmap;
- release qualification/readiness needs a replacement plan after T071 closed as not planned, using the current repository policies and exact-head qualification evidence.

Until those replacement artifacts exist, this chapter links to shipped examples and repository policy rather than inventing a future application or release contract.
