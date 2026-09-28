# NativeUI 1.0 resources, services, testing and limits

This chapter documents stable NativeUI 1.0 resource/service contracts, the project testing and debugging model, and the supported/deferred platform boundary. The landed state/binding contract is documented in [`v1-state-and-binding.md`](v1-state-and-binding.md), and the current package/CMake contract is documented in [`v1-packaging-and-cmake.md`](v1-packaging-and-cmake.md). A canonical Getting Started/reference application and replacement release-qualification plan are not currently delivered: T070 and T071 are closed as not planned, and the roadmap explicitly requires both paths to be replanned.

## Resources

NativeUI separates the generic resource-provider seam from its zero-copy embedded resource manager.

[`ResourceProvider`](../include/nativeui/resource.hpp) is the polymorphic provider contract for APIs that need owned encoded bytes. `load(resource_id)` performs an exact application-defined lookup and returns an owned `std::vector<std::byte>` or `std::nullopt` when unavailable. Provider storage policy is intentionally open: files, bundles, archives, generated memory and other backends can implement the same seam.

Because a provider may allocate or perform I/O, `load()` is a resource-preparation/UI-side operation unless a concrete provider documents a stronger contract. It must not be assumed real-time safe.

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

The focused executable [`t057_embedded_resources`](../examples/features/t057_embedded_resources.cpp) demonstrates the public resource lookup boundary and its deterministic `--self-test` path. Package/CMake guidance lives in [`v1-packaging-and-cmake.md`](v1-packaging-and-cmake.md); until the replanned reference-application track produces a maintained Getting Started artifact, this chapter does not invent that tutorial surface.

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

## PlatformServices callback bridge

[`PlatformServices`](../include/nativeui/paint.hpp) is the platform-neutral service interface borrowed by retained paint/input/focus execution. It is **not owned** by a component or callback context: the concrete implementation must stay alive for every UI/Tree operation that can invoke it. The interface crosses native UI facilities and is UI/main-thread work, not an audio-real-time service.

Text measurement has backend-neutral defaults. `text_metrics(text, style)` delegates to `TextService::measure(text, style)`, while `text_width(text, size)` is the width-only convenience form. Results use logical UI units and own their numeric result; incoming string views are borrowed only for the call.

`set_text_input(active, area, cursor_offset)` mirrors focused retained editing into the platform IME/text-input bridge. `area` and `cursor_offset` are logical geometry; the concrete view converts them to device/platform coordinates. Disabling input allows zero/default geometry. Custom/headless services may keep the default no-op implementation when they have no native IME integration.

Pointer capture remains **retained-tree owned**. `begin_pointer_capture()` and `end_pointer_capture()` are noexcept platform lifecycle hooks for the real none↔owner transition; they carry no target identity and must not become a second platform-owned capture registry.

Clipboard operations are request-oriented. `set_clipboard_text(text)` publishes UTF-8 plain text; the string view is borrowed for the duration of the call, so an asynchronous implementation must copy it before returning. `request_clipboard_text()` initiates platform delivery through the normal input/data path and deliberately returns no synchronous string value.

Drag/drop is synchronous at offer time. `accept_drop(type, region)` receives a borrowed offered type plus the logical target region and returns whether the current offer was accepted; the default implementation rejects support. `reject_drop(region)` explicitly rejects the current offer and defaults to a no-op for backends without drag/drop support.

These low-level services are normally surfaced to components through borrowed `PaintContext`, `InputContext` and `FocusContext` operations. Application code should prefer those callback contexts instead of retaining a `PlatformServices&` beyond the callback/lifecycle that supplied it.

## Desktop services

[`DesktopServices`](../include/nativeui/desktop_services.hpp) provides bounded asynchronous desktop integration for standalone NativeUI windows. The public service covers:

- open one file;
- open multiple files;
- save a file;
- select a directory;
- open an absolute HTTP(S) URL;
- cancel an active request.

With a non-empty callback and a valid owning `Dispatcher`, completion is marshalled asynchronously through that Dispatcher, including immediate validation/capacity/unsupported results. Application callbacks are therefore not invoked inline from a normal request call, even when a custom backend calls its completion synchronously. If the callback is empty or the Dispatcher is already invalid, the request returns `kInvalidDesktopRequestId` and there is no callback delivery.

The public status model distinguishes successful acceptance/cancellation from bounded or unavailable cases:

- `Accepted` — the requested operation completed successfully;
- `Cancelled` — the user or caller cancelled the operation;
- `Busy` — the per-owner service already has the allowed number of that request family active;
- `ResourceLimit` — a bounded underlying shared resource cannot accept more work;
- `Unsupported` — the operation is unavailable for that owner/runtime by design;
- `InvalidArgument` — the request failed public input validation;
- `Error` — the backend attempted the operation but encountered an operational failure.

V1 keeps one file chooser request active per `DesktopServices` owner and up to sixteen concurrent URL requests. There is no hidden overflow queue.

### Request identity and return contract

`DesktopRequestId` is an owner-local 64-bit identity; `0` is invalid. The facade returns a non-zero ID only after the backend reports that it **accepted** the request. Validation failure, missing/unsupported backend, capacity exhaustion or backend start failure returns the invalid ID. For those immediate failures, a valid callback/Dispatcher still receives the corresponding status asynchronously.

All file/open/save/directory requests share the single chooser slot. URL requests use the separate sixteen-request bound. IDs are not process-global handles and do not keep the owning `DesktopServices`, window or backend alive.

A backend is allowed to complete synchronously before its `start_*` call returns. NativeUI makes the request terminal at the backend-completion boundary and posts application delivery to the Dispatcher; duplicate/stale completion for the same ID is ignored after the request has become terminal.

### Option validation

File-filter extensions include the leading dot. NativeUI accepts an extension only when it starts with `.` followed by an ASCII alphanumeric character; later characters may additionally be `.`, `_`, `+` or `-`. Examples such as `.wav`, `.tar.gz` and `.m4a` are valid; a bare `wav` is not.

`SaveFileOptions::suggested_filename` is a filename, not a path. A value containing `/` or `\\` is rejected as `InvalidArgument`.

`open_url()` accepts only absolute HTTP(S) URLs (scheme matching is ASCII case-insensitive) with a non-empty authority. Relative URLs and other schemes are `InvalidArgument`; the public API does not expose arbitrary shell/scheme execution.

### File result normalization

NativeUI normalizes backend file-dialog results before application delivery:

- an Accepted `open_file`, `save_file` or `select_directory` result must contain exactly one path;
- an Accepted `open_files` result must contain at least one path;
- invalid Accepted cardinality becomes `Error`;
- non-Accepted results expose an empty path list;
- `error` text is retained/generated only for `Error`; other statuses clear it.

This keeps application-side cardinality deterministic even for custom backends.

### Cancellation and destruction

`cancel(id)` asks the backend to cancel a currently active owner-local request. A `true` return means the backend accepted that cancellation request; it is not a synchronous application completion. Invalid/stale IDs, missing backend, backend rejection or a backend exception return `false`.

Destroying `DesktopServices` closes the facade first, suppresses every later application callback, removes its active request ownership and best-effort calls backend cancellation. Destruction never falls back to invoking user callbacks. The backend is held by `shared_ptr` for the service lifetime; the Dispatcher remains a weak owner handle.

Backend/native completion may arrive from a platform/worker thread, but application callback execution is dispatched onto the owning UI event-loop checkpoint. Initiating/cancelling desktop requests remains application/UI work; none of this API is an audio-real-time boundary.

### Example

```cpp
auto& services = window.desktop_services();

ui::OpenFileOptions options;
options.title = "Open audio";
options.filters = {
    ui::FileFilter{.description = "Audio", .extensions = {".wav", ".aiff"}}
};

const auto request = services.open_file(
    std::move(options),
    [](ui::FileDialogResult result) {
        if (result.status == ui::DesktopServiceStatus::Accepted) {
            // Exactly one path for open_file().
            use_file(result.paths.front());
        }
    });

if (request == ui::kInvalidDesktopRequestId) {
    // Immediate rejection/status delivery may still be queued when the
    // callback and Dispatcher were valid.
}
```

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

The focused example remains the canonical small demonstration for its feature. T122 documentation should link to those examples rather than copying large code snippets that can silently drift. A copy-pasteable Getting Started application should be linked only when the replanned T133–T137 track produces one.

### Component gallery

[`nativeui_example_t049_gallery`](../examples/features/t049_gallery.cpp) is the aggregate visual catalogue and manual-QA surface. It composes representative layout, text, widget, navigation, rendering and visual-state behavior using public NativeUI APIs.

The gallery intentionally does **not** replace the focused feature examples and is not a production application architecture tutorial. Its own deterministic self-test checks construction/wiring of the represented public surface without turning the gallery into a second hidden integration framework.

### Headless rendering

[`HeadlessRenderer`](../include/nativeui/headless.hpp) renders a normal `UI` into a deterministic Skia raster surface without Pugl, OpenGL or a display server.

```cpp
ui::HeadlessRenderer renderer{{320.0f, 200.0f}, 2.0f};

if (!renderer.render(ui)) {
    // Raster surface/canvas/pixel view creation failed.
}

const auto size = renderer.logical_size();   // 320 x 200 logical
const int width = renderer.pixel_width();    // rounded physical width
const ui::Rgba8 sample = renderer.pixel(10, 10);
```

Constructor/resize inputs must use finite positive logical dimensions and scale; invalid values throw `std::invalid_argument`. Physical dimensions are rounded from logical size × scale and kept at least one pixel.

`render(UI&)` resizes the UI to the renderer's logical viewport, clears the raster target, applies the configured scale and runs the normal NativeUI paint path. It returns `false` only when the backing raster surface/canvas/pixel view cannot be produced; ordinary component/layout/paint exceptions remain C++ exceptions.

`rgba_pixels()` returns a borrowed tightly packed RGBA8888 buffer in top-to-bottom, left-to-right order. The reference is tied to the renderer's current snapshot and may be invalidated by render/resize/move/destruction. `pixel(x,y)` samples physical raster coordinates and throws `std::out_of_range` outside the surface.

This API is intended for deterministic tests, golden/reference images and offscreen validation. It is not an audio-thread facility.

### Headless, golden, lifecycle and platform validation

The repository validation model uses complementary layers:

- unit/integration and headless tests for deterministic retained behavior;
- feature-example self-tests for public feature wiring;
- deterministic rendering/golden coverage where a visual contract needs pixel-level evidence;
- lifecycle/multi-instance stress for ownership, teardown and coexistence behavior;
- macOS, Windows and Linux/X11 platform jobs for native integration;
- sanitizer coverage where supported;
- package/external-consumer contracts for installed/public consumption.

Remote CI is a qualification layer rather than the inner RED/GREEN loop. Active implementation remains Draft, coherent batches receive normal/path-relevant validation, and heavyweight lifecycle/release gates are reserved for a frozen final candidate according to the active workflow and validation rules in [`AGENTS.md`](../AGENTS.md), [`VALIDATION.md`](../VALIDATION.md) and [`ROADMAP.md`](../ROADMAP.md).

## Debug inspector

[`inspector.hpp`](../include/nativeui/inspector.hpp) is an optional developer diagnostic surface. `NATIVEUI_ENABLE_INSPECTOR` defaults **OFF**; when disabled the runtime inspector functions are not declared and normal production behavior is unchanged.

When compiled in, inspector state belongs to each `UI` independently. It snapshots copied retained-tree diagnostics and paints a passive post-content overlay. It does not become a second component tree, consume application overlay slots, receive hit testing/focus, or run a persistent timer/redraw loop. Missing/stale node IDs are handled safely through value-based snapshots rather than raw retained-tree pointers.

The public debug value model is fully owned:

- `debug::InspectorNode` copies NodeId/parent identity, bounds/clip, dirty flags, focus/capture flags, effective availability, child order and depth;
- `debug::InspectorSnapshot` owns its node list and dirty-region list, so callers can retain a snapshot after the live tree changes;
- `InspectorSnapshot::find(id)` returns a pointer borrowed only from that snapshot;
- `debug::inspector_snapshot(ui)` copies the current diagnostic state;
- `set_inspector_enabled()` and `set_inspector_selected_node()` affect only that UI instance and request repaint.

Stale/missing selected NodeIds are safe diagnostic values; the inspector does not keep retained nodes alive.

Use the inspector to understand bounds, clips, dirty state, focus/capture and retained hierarchy while debugging. Do not build application behavior that depends on the inspector being present.

## Contributor policy map

The project policy documents have distinct roles:

| Document | Purpose |
| --- | --- |
| [`AGENTS.md`](../AGENTS.md) | development workflow, ticket lifecycle, TDD/batching, examples and completion rules |
| [`CODE_REVIEW.md`](../CODE_REVIEW.md) | mandatory ownership, isolation, threading, lifetime, ABI/platform and validation review |
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

## Remaining release reconciliation

This chapter is reconciled with the landed state/binding and package/public-surface work represented by current `main`. Those contracts should be kept synchronized as the implementation evolves rather than treated as future freeze gates.

The remaining release-facing dependencies are replacement plans, not T070/T071 gates:

- replan the canonical reference application / Getting Started path after T070 closed as not planned; T133–T137 remain unresolved;
- define the replacement NativeUI 1.0 release-qualification/readiness ticket after T071 closed as not planned, then use current repository policy and exact-head evidence.

Until those replacement artifacts exist, this chapter links to stable implementation-facing sources of truth and avoids inventing their final tutorial or release wording.
