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

`UI` owns one mounted retained tree and its Theme, layout, focus, input, overlay and invalidation state. Construction consumes the root specification, mounts immediately and may propagate allocation or component-lifecycle failures after retained rollback. Destruction is `noexcept` and publishes UI death before retained teardown.

`measure()` and `resize()` use logical UI pixels. Measurement does not publish viewport geometry; resize publishes the viewport used by layout, hit testing, overlays and paint. A resize requested during an active lifecycle transition is deferred to a later safe checkpoint.

`activate()`, `deactivate()`, `refresh_focus()`, `dispatch()`, `cancel_pointer()` and `paint()` are UI-thread operations. The supplied `PlatformServices` belongs to the host-view domain used for that UI. Application/component callbacks may run synchronously and may throw; these APIs are not audio/DSP real-time safe.

`dispatch()` is also a retained reentrancy boundary: structural work requested by callbacks is committed at safe outer checkpoints. Popup and dialog completion callbacks run after retained detach and may re-enter ordinary UI/state work or destroy the UI, so callers must not keep internal node/component borrows across dispatch.

`set_invalidation_callback()` owns its callback and immediately replays existing dirty regions to a newly installed callback. Later notifications are synchronous with the UI operation publishing damage. `dirty_regions()` returns a borrowed logical-region vector; copy it before later mutation if the snapshot must survive. `invalidate()` marks the full viewport, `invalidate(Rect)` one root-logical region, and `invalidate_layout()` retained geometry.

A successful `paint()` consumes only damage that existed before paint callbacks began; invalidation raised reentrantly survives for the next frame. If paint throws, pre-existing damage is restored for retry.

`component_availability()` and `component_semantics()` return value snapshots or `nullopt` for stale/missing NodeIds. `overlay_entries()` returns an owned diagnostic vector. By contrast, `theme()`, `dirty_regions()` and `structural_diagnostic()` return borrows into UI-owned storage and must not outlive replacement/mutation/destruction.

With `NATIVEUI_ENABLE_INSPECTOR`, the `ui::debug` helpers remain UI-thread diagnostics. `PluginUI` is only a compatibility alias for `UI` and adds no plug-in or real-time ownership semantics.

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
