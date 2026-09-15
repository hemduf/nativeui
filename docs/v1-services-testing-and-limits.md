# NativeUI 1.0 resources, services, testing and limits

This chapter documents stable NativeUI 1.0 resource/service contracts, the project testing and debugging model, and the supported/deferred platform boundary. It deliberately avoids freezing still-changing `State<T>` / `Binding<T>` semantics, final package/API names owned by T069, the canonical Getting Started/reference application owned by T070, or exact release-candidate policy owned by T071.

## Resources

NativeUI can consume immutable embedded resource tables without introducing a process-global registry or runtime filesystem dependency.

[`ui::EmbeddedResourceEntry`](../include/nativeui/embedded_resource.hpp) is the small public metadata type used by generated or application-owned resource tables. Each entry is an immutable view containing a resource ID and byte span.

[`ui::ResourceManager`](../include/nativeui/resource_manager.hpp) is a non-owning view over one sorted immutable table:

- construction validates the complete table once;
- IDs must be non-empty, unique and strictly sorted by their exact byte ordering;
- a valid manager performs exact, case-sensitive lookup with binary search;
- direct lookup returns a zero-copy `ResourceView` into the original immutable storage;
- the caller-owned entry table, IDs and payload storage must outlive the manager and every returned view;
- copies of a manager remain lightweight borrowed views; no mutable registry, cache or singleton is created;
- concurrent read-only lookup is safe while the borrowed table remains alive and immutable.

Invalid tables fail as a whole rather than exposing a valid prefix. Direct `find()`, `contains()` and `resources()` then behave as absent/empty operations.

`ResourceManagerProvider` is the compatibility adapter for APIs that require owned resource bytes. A successful non-empty load copies the selected payload into an owned vector. That operation may allocate and is explicitly a resource-preparation/UI-side operation, not a real-time audio operation.

The focused executable [`t057_embedded_resources`](../examples/features/t057_embedded_resources.cpp) demonstrates the public resource lookup boundary and its deterministic `--self-test` path. Final application packaging/CMake guidance is intentionally documented elsewhere after the T069/T070 contract is frozen; this chapter does not duplicate or guess that surface.

## Dispatcher: worker-to-UI handoff

NativeUI's [`Dispatcher`](../include/nativeui/dispatcher.hpp) is the supported bounded handoff mechanism when work originating outside the UI thread must schedule a callback onto a concrete NativeUI window/view owner.

A dispatcher is owner-scoped rather than process-global:

- each standalone window exposes the dispatcher for that window;
- each embedded view exposes an independent dispatcher;
- copying a `Dispatcher` copies a weak lifetime-safe handle, not ownership of the queue/backend;
- destroying one owner prevents its pending work from becoming work owned by another window/view;
- NativeUI defines no global event bus and no general background executor.

`Dispatcher::post()` is thread-safe for ordinary worker-thread use. An accepted callback executes on the owning UI/main thread at a later event-loop checkpoint; it is not invoked inline by `post()`. Standalone owners can wake their application event loop without busy polling. Embedded owners remain host-driven: posting work does not create a background poll thread and `EmbeddedView::poll()` remains non-blocking.

The v1 service is intentionally bounded per owner:

| Limit | NativeUI 1.0 value |
| --- | ---: |
| Pending queued tasks | 65,536 |
| Active timers | 8,192 |
| Tasks executed from one checkpoint snapshot | 1,024 |

Timer callbacks use the same owner queue. Zero-delay work still waits for a dispatcher checkpoint; repeating timers use bounded fixed-delay behavior rather than catch-up bursts; stale or cross-owner timer handles cannot cancel another owner's timer.

### Not real-time safe

`post()` and timer creation/cancellation may allocate and synchronize. They are deliberately **not real-time safe**. Do not call them from an audio/DSP callback and do not treat `Dispatcher` as an audio-to-UI lock-free transport.

If a plug-in or audio application needs a real-time boundary, its adapter/application owns that boundary first—for example an explicitly reviewed atomic snapshot or bounded lock-free queue. Ordinary worker-side code may then use `Dispatcher` after leaving the real-time domain to perform the final UI-thread handoff.

The focused executable [`t065_ui_dispatcher`](../examples/features/t065_ui_dispatcher.cpp) is the maintained feature demonstration and self-test for worker posting, timers and deterministic fake-time behavior.

## Desktop services

[`DesktopServices`](../include/nativeui/desktop_services.hpp) provides bounded asynchronous desktop integration for standalone NativeUI windows. The public service covers:

- open one file;
- open multiple files;
- save a file;
- select a directory;
- open an absolute HTTP(S) URL;
- cancel an active request.

Every request completes asynchronously through the owning `Dispatcher`, including immediate validation/capacity/unsupported results. Application callbacks are therefore not invoked inline from the request call.

The public status model distinguishes successful acceptance/cancellation from bounded or unavailable cases:

- `Accepted` — the requested operation completed successfully;
- `Cancelled` — the user or caller cancelled the operation;
- `Busy` — the per-owner service already has the allowed number of that request family active;
- `ResourceLimit` — a bounded underlying shared resource cannot accept more work;
- `Unsupported` — the operation is unavailable for that owner/runtime by design;
- `InvalidArgument` — the request failed public input validation;
- `Error` — the backend attempted the operation but encountered an operational failure.

V1 keeps one file chooser request active per `DesktopServices` owner and up to sixteen concurrent URL requests. There is no hidden overflow queue.

### Standalone and embedded behavior

A `StandaloneWindow` exposes its window-owned `DesktopServices` facade. Platform implementation remains fixed behind the public API: AppKit services on macOS, Windows shell/file-dialog services on Windows, and XDG Desktop Portal on Linux/X11.

The built-in `EmbeddedView` service intentionally returns `Unsupported` for file dialogs and URL launch so NativeUI does not create host-global UI or shell side effects inside an embedding host. An embedding application may provide an explicit `DesktopServicesBackend` when that application owns the host policy and lifetime.

On Linux, an unavailable desktop portal is `Unsupported`; NativeUI does not fall back to GTK, Qt, zenity or shell-command launch paths. Built-in URL opening is limited to validated absolute HTTP(S) URLs rather than exposing arbitrary shell schemes.

Desktop services are UI/application facilities, not audio-thread facilities. Requests, option objects, callbacks and platform backends may allocate, synchronize or perform OS work and must remain outside real-time processing.

See [`t064_desktop_services`](../examples/features/t064_desktop_services.cpp) for the deterministic fake-backed `--self-test` and explicit interactive native-service demonstration.

## Testing and examples

NativeUI keeps small focused executable examples as part of the feature contract rather than maintaining a separate untested tutorial API. The index in [`examples/features/README.md`](../examples/features/README.md) is the starting point for those examples.

### Focused feature examples and `--self-test`

Feature examples follow two complementary modes:

- normal interactive mode demonstrates the public API as an application developer would use it;
- `--self-test` performs deterministic non-interactive acceptance checks and returns a non-zero result on failure.

The focused example remains the canonical small demonstration for its feature. T122 documentation should link to those examples rather than copying large code snippets that can silently drift. The final copy-pasteable Getting Started application belongs to T070.

### Component gallery

[`nativeui_example_t049_gallery`](../examples/features/t049_gallery.cpp) is the aggregate visual catalogue and manual-QA surface. It composes representative layout, text, widget, navigation, rendering and visual-state behavior using public NativeUI APIs.

The gallery intentionally does **not** replace the focused feature examples and is not a production application architecture tutorial. Its own deterministic self-test checks construction/wiring of the represented public surface without turning the gallery into a second hidden integration framework.

### Headless, golden, lifecycle and platform validation

The repository validation model uses complementary layers:

- unit/integration and headless tests for deterministic retained behavior;
- feature-example self-tests for public feature wiring;
- deterministic rendering/golden coverage where a visual contract needs pixel-level evidence;
- lifecycle/multi-instance stress for ownership, teardown and coexistence behavior;
- macOS, Windows and Linux/X11 platform jobs for native integration;
- sanitizer coverage where supported;
- package/external-consumer contracts for installed/public consumption.

Remote CI is a qualification layer rather than the inner RED/GREEN loop. Active implementation remains Draft, coherent batches receive normal/path-relevant validation, and heavyweight lifecycle/release gates are reserved for a frozen final candidate according to [`CI_POLICY.md`](../CI_POLICY.md).

## Debug inspector

The T050 inspector is an optional developer diagnostic surface. `NATIVEUI_ENABLE_INSPECTOR` defaults **OFF**; when disabled there is no runtime activation path and normal production behavior is unchanged.

When compiled in, inspector state belongs to each `UI` independently. It snapshots copied retained-tree diagnostics and paints a passive post-content overlay. It does not become a second component tree, consume application overlay slots, receive hit testing/focus, or run a persistent timer/redraw loop. Missing/stale node IDs are handled safely through value-based snapshots rather than raw retained-tree pointers.

Use the inspector to understand bounds, clips, dirty state, focus/capture and retained hierarchy while debugging. Do not build application behavior that depends on the inspector being present.

## Contributor policy map

The project policy documents have distinct roles:

| Document | Purpose |
| --- | --- |
| [`AGENTS.md`](../AGENTS.md) | development workflow, ticket lifecycle, TDD/batching, examples and completion rules |
| [`CODE_REVIEW.md`](../CODE_REVIEW.md) | mandatory ownership, isolation, threading, lifetime, ABI/platform and validation review |
| [`CI_POLICY.md`](../CI_POLICY.md) | remote-validation cadence, backpressure and final-candidate rules |
| [`DESIGN.md`](../DESIGN.md) | durable architecture and platform/rendering design decisions |
| [`VALIDATION.md`](../VALIDATION.md) | platform/build validation guidance and supporting evidence |

These documents are sources of truth for contributors; feature documentation should link to them instead of restating partial variants of their rules.

## NativeUI 1.0 support boundary and known limitations

### Supported desktop platforms

NativeUI 1.0's supported desktop targets are:

- macOS;
- Windows x64 with MSVC;
- Linux x64 with X11.

Normal NativeUI application code uses NativeUI public abstractions. Pugl, Skia, AppKit, Win32/X11 implementation details and headers below `nativeui/detail/` are implementation boundaries rather than supported normal-user APIs.

### Wayland

Native Wayland is deferred beyond the NativeUI 1.0 Linux/X11 contract. NativeUI 1.0 documentation must not describe XWayland or another toolkit/backend as equivalent to shipped native Wayland support, and application code should not depend on an undocumented alternate windowing path.

### Native accessibility bridges

Native NSAccessibility, Windows UI Automation and Linux AT-SPI2 bridges are deferred to NativeUI 1.2 under T068. NativeUI 1.0 must therefore not be documented as shipping those native accessibility-provider bridges. This does not expand the scope of T122 into accessibility implementation work.

### Plug-in SDKs, DSP and real-time processing

NativeUI is a UI toolkit. It does not own CLAP/VST3/AU/AAX APIs, audio/DSP processing, plug-in parameter identifiers/normalization/automation, or host edit-gesture semantics.

A plug-in adapter may host a NativeUI view, but the adapter remains responsible for its plug-in ABI, parameter/audio synchronization and real-time-safe data exchange. NativeUI retained state, `Dispatcher`, `DesktopServices` and ordinary platform/UI APIs must not be treated as audio-thread primitives.

### Private/backend APIs

Direct Skia, Pugl, AppKit, Win32/X11 or `nativeui/detail/` implementation APIs are not the normal v1 extension surface. Documentation and application code should use NativeUI drawing, window/view, resource and service abstractions instead of reaching through those private boundaries.

## Deferred reconciliation

This chapter intentionally covers only stable T122 areas 7, 9 and 11. Before T122 can be completed, the complete documentation set still needs final reconciliation with:

- T123/T124 for the final `State<T>` / `Binding<T>` lifetime and callback contract;
- T069 for the frozen public C++/CMake inventory and backend-neutral package boundary;
- T070 for the canonical reference application, Getting Started path and navigation boundary;
- T071 for final NativeUI 1.0 release/support wording and exact release-candidate policy.

Until those gates land, this chapter does not guess their unfinished names or duplicate their acceptance artifacts.
