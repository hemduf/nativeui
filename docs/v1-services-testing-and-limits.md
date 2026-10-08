# NativeUI 1.0 resources, services, testing and limits

This chapter documents stable NativeUI 1.0 resource/service contracts, the project testing and debugging model, and the supported/deferred platform boundary. The landed state/binding contract is documented in [`v1-state-and-binding.md`](v1-state-and-binding.md), and the current package/CMake contract is documented in [`v1-packaging-and-cmake.md`](v1-packaging-and-cmake.md). A canonical Getting Started/reference application and replacement release-qualification plan are not currently delivered: T070 and T071 are closed as not planned, and the roadmap explicitly requires both paths to be replanned.

## Resources

NativeUI separates the generic owned-byte provider seam from its zero-copy embedded-resource table API. Neither layer creates a process-global registry or silently owns application storage.

### ResourceProvider: owned-byte boundary

[`ResourceProvider`](../include/nativeui/resource.hpp) is the polymorphic seam for APIs that require an independently owned encoded payload. `load(resource_id)` receives a borrowed `std::string_view` valid only for the synchronous call. The base interface itself performs no normalization, case folding, path decoding, fallback search, cache lookup or asynchronous scheduling; those policies belong to a concrete provider when needed.

A successful call returns an owned `std::vector<std::byte>` whose lifetime is independent of the provider after return. A **present empty resource** is an engaged empty vector; `std::nullopt` means that this provider cannot supply the requested identifier. This distinction is preserved by the ResourceManager adapter and by higher-level image/SVG caches.

The base interface intentionally does not promise allocation-free, non-throwing or real-time behavior. Implementations may allocate, perform I/O, synchronize or propagate ordinary C++ exceptions. `std::nullopt` is therefore not a generic exception/error translation. Provider loading belongs to resource-preparation/UI-side code unless the concrete provider explicitly documents and proves a stronger contract. The base class adds no cross-thread synchronization and defines no retry/cancellation/reentrancy protocol because `load()` is synchronous and callback-free at this layer.

### EmbeddedResourceEntry: borrowed table storage

[`EmbeddedResourceEntry`](../include/nativeui/embedded_resource.hpp) is a pair of borrowed views:

- `id` is an exact `std::string_view`; it is not normalized, case-folded or path-decoded;
- `bytes` is a borrowed `std::span<const std::byte>`;
- a zero-length payload is valid and means **present but empty**, not missing;
- copying an entry copies only the views; it does not copy the characters or bytes.

The caller therefore owns the entry array, every ID's character storage and every payload buffer. Those objects must outlive any `ResourceManager` or `ResourceView` that can still reference them.

### ResourceManager: validation and zero-copy lookup

[`ResourceManager`](../include/nativeui/resource_manager.hpp) borrows one immutable `std::span<const EmbeddedResourceEntry>`. Construction performs one allocation-free O(N) validation pass and records the result. An empty table is valid. For a non-empty table, every ID must be non-empty and the complete table must be strictly ascending and unique using NativeUI's exact **unsigned-byte lexicographic** comparison.

That ordering is intentionally byte-oriented rather than locale-aware. For example, uppercase/lowercase and UTF-8 bytes are compared exactly as stored. The manager does not sort, normalize or repair an input table.

Validation is atomic at the public boundary. If any entry violates the contract:

- `valid()` is false;
- `validation_error()` returns stable NativeUI-owned diagnostic text;
- `find()` returns `std::nullopt`;
- `contains()` returns false;
- `resources()` returns an empty span rather than exposing a valid-looking prefix.

For a valid manager, `find(id)` uses allocation-free O(log N) binary search and returns a `ResourceView` that directly aliases the original ID and payload storage. A hit on an empty payload is still an engaged result. `contains(id)` has the same exact-ID semantics, while `resources()` returns the original validated table as a borrowed span.

Copies/moves of `ResourceManager` remain lightweight views and do not share mutable state. Concurrent read-only calls are safe as long as the underlying table, strings and payload bytes remain alive and immutable. The direct manager API performs no allocation or internal locking, but NativeUI does not advertise it as a general audio-thread transport: a real-time caller would still need independently proven storage lifetime/residency and bounded input behavior.

### ResourceManagerProvider: explicit ownership conversion

`ResourceManagerProvider` adapts the borrowed manager to `ResourceProvider`. The adapter copies the lightweight manager only; it still does not own or extend the lifetime of the embedded table.

`load(id)` has three distinct outcomes:

- invalid table or missing ID -> `std::nullopt`;
- present empty resource -> engaged empty vector, without turning it into "missing";
- present non-empty resource -> a newly owned byte vector copied from the embedded payload.

The non-empty success path may allocate and propagate allocation failure. The adapter keeps no cache; each successful non-empty call creates an independent copy. Because `load()` may allocate/copy, it is resource-preparation/UI-side work and not audio-real-time safe. Concurrent calls are safe under the same immutable-backing-storage lifetime requirement as direct manager lookup.

```cpp
constexpr std::array<std::byte, 3> kPreset{
    std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};

const std::array<ui::EmbeddedResourceEntry, 2> table{{
    {"preset/default.bin", kPreset},
    {"preset/empty.bin", {}},
}};

ui::ResourceManager resources{table};
if (!resources.valid()) {
    // validation_error() is a stable NativeUI-owned diagnostic string_view.
}

if (const auto preset = resources.find("preset/default.bin")) {
    // preset->bytes is zero-copy and borrows kPreset.
}

ui::ResourceManagerProvider provider{resources};
if (auto owned = provider.load("preset/default.bin")) {
    // *owned is an independent copy.
}
```

The focused executable [`t057_embedded_resources`](../examples/features/t057_embedded_resources.cpp) exercises validation, direct zero-copy lookup, the explicit provider copy boundary and use by the SVG cache. Package/CMake guidance lives in [`v1-packaging-and-cmake.md`](v1-packaging-and-cmake.md).

## Dispatcher: worker-to-UI handoff

NativeUI's [`Dispatcher`](../include/nativeui/dispatcher.hpp) is the supported bounded handoff mechanism when work originating outside the UI thread must schedule a callback onto a concrete NativeUI window/view owner. `DispatcherDuration` is `std::chrono::duration<double>` in **seconds**; scheduling converts it to the owner's steady-clock representation.

A dispatcher is owner-scoped rather than process-global:

- each standalone window exposes the dispatcher for that window;
- each embedded view exposes an independent dispatcher;
- copying a `Dispatcher` copies a weak lifetime-safe handle, not ownership of the queue/backend;
- destroying one owner invalidates its handles and discards its queued tasks/timers rather than transferring them to another window/view;
- NativeUI defines no global event bus and no general background executor.

All public queue/timer mutation is synchronized for ordinary worker/UI-thread use. It may allocate and lock. Callback execution is different: an accepted callback runs only on the owning UI/main thread when that owner pumps a later dispatcher checkpoint.

### Posting, acceptance and reentrancy

`post(callback)` returns `true` only after a non-empty callback has been accepted into that owner's bounded FIFO task queue. It returns `false` when the Dispatcher is invalid/closing, the callback is empty or the queue cannot accept more work. Allocation failure can throw before acceptance.

Acceptance does **not** mean inline execution and does not extend owner lifetime. A callback capture is owned by the queue until execution or owner shutdown, so reference captures must still outlive the deferred invocation.

Each checkpoint freezes a snapshot of at most `kDispatcherMaxTasksPerCheckpoint` callbacks. Work posted reentrantly by a callback is deliberately outside that snapshot and waits for another checkpoint. If one callback throws, that invocation is consumed and the exception propagates through the owning checkpoint path; later already-accepted tasks remain queued and are re-woken rather than silently discarded.

FIFO applies to accepted task order. Due timers are inserted into that same queue in deadline order, using creation order to break equal-deadline ties. Tasks that were already queued remain ahead of timers that merely become due at the checkpoint.

The v1 service is intentionally bounded per owner:

| Limit | NativeUI 1.0 value |
| --- | ---: |
| Pending queued tasks | 65,536 |
| Active one-shot + repeating timers | 8,192 |
| Callbacks begun from one checkpoint snapshot | 1,024 |

### Timer validation and lifetime

`schedule_after(delay, callback)` accepts a finite non-negative delay that fits the owner's steady clock. A zero delay is valid but still waits for a checkpoint. `schedule_every(interval, callback)` requires a finite **strictly positive** interval that remains non-zero after native clock conversion. Empty callbacks, invalid/closing owners, invalid durations and timer-capacity exhaustion return an invalid `TimerHandle`; allocation failures may throw.

Repeating timers are fixed-delay and intentionally do not replay missed periods as catch-up bursts. When a repeat becomes due, its firing is queued and the next deadline becomes the current checkpoint time plus the configured interval. The same owned callback object is reused across firings, so mutable state captured by value in that callback persists between repetitions.

`TimerHandle` is owner-local identity, not ownership. `handle.valid()` only means the value contains a non-empty owner/id identity; it does **not** prove that the timer is still active. `cancel(handle)` returns `false` for stale/already-fired/already-cancelled or cross-owner handles. Successful cancellation removes future scheduling, but cannot retract a firing already moved into the task queue or already running. This is why cancelling a repeating timer from inside its callback prevents later repeats without undoing the current firing.

### Example

```cpp
const ui::Dispatcher dispatcher = window.dispatcher();

std::thread worker{[dispatcher] {
    (void)dispatcher.post([] {
        // Runs later on the owning UI thread.
    });
}};
worker.join();

const ui::TimerHandle timer = dispatcher.schedule_every(
    ui::DispatcherDuration{0.5},
    [] {
        // Runs at most once per due checkpoint; missed periods do not burst.
    });

if (timer) {
    (void)dispatcher.cancel(timer);
}
```

Standalone owners can wake their application event loop without busy polling. Embedded owners remain host-driven: posting work does not create a background poll thread and `EmbeddedView::poll()` remains non-blocking.

### Not real-time safe

`post()`, timer creation and cancellation may allocate and synchronize. They are deliberately **not real-time safe**. Do not call them from an audio/DSP callback and do not treat `Dispatcher` as an audio-to-UI lock-free transport.

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

### Option/result ownership and backend-call boundary

The option structs are owned values. Request entry points take them by value, so callers may move temporary strings, paths and filter vectors into a request without extending the source objects' lifetime. The facade retains what it needs until backend start; a backend's `const ...Options&` parameter is only a **borrow for that synchronous `start_*` call** and must not be retained after the method returns.

Field semantics are explicit:

| Value | Contract |
| --- | --- |
| `FileFilter::description` | owned user-facing label; an empty label is allowed and presentation remains backend policy |
| `FileFilter::extensions` | owned leading-dot extensions validated before start; an empty vector adds no NativeUI extension restriction |
| `OpenFileOptions::title`, `SaveFileOptions::title`, `DirectoryOptions::title` | owned text; NativeUI does not invent a fallback title |
| `initial_directory` | optional owned path forwarded without a public existence preflight |
| `SaveFileOptions::suggested_filename` | optional filename only; directory separators are rejected |
| `FileDialogResult::paths` | owned paths independent of service/backend lifetime after callback delivery |
| `FileDialogResult::error` | owned diagnostic retained only for `Error`; every other normalized status clears it |

Backend completion callables transfer to the backend at `start_*` entry and may be invoked synchronously or later from native/worker threads. `Accepted` from a backend start means the backend took responsibility for the request, not that application completion ran inline. The facade makes a backend completion terminal before Dispatcher marshalling, ignores duplicate/stale completion, and never falls back to running application code on the backend thread if posting/allocation fails. Application callback exceptions therefore surface only from the owning Dispatcher checkpoint.

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

`rgba_pixels()` returns a borrowed tightly packed premultiplied RGBA8888 buffer in top-to-bottom, left-to-right order. The reference is tied to the renderer's current snapshot and may be invalidated by render/resize/move/destruction. It is empty before the first successful render and after resize. `pixel(x,y)` samples zero-based physical raster coordinates from a top-left origin, returns an owned `Rgba8` value, and throws `std::out_of_range` outside the surface or when no successful snapshot exists yet.

The renderer owns no `UI` and provides no internal synchronization. Calls belong to the owning UI/main-thread domain; moving the renderer transfers its surface and snapshot, and the moved-from source is only for destruction or assignment.

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

### Snapshot value and coordinate contract

`debug::InspectorSnapshot` is a fully owned point-in-time value. Its `nodes` vector is emitted in retained pre-order: a parent appears before its descendants, `depth` is zero for the root, and `child_order` is the zero-based position among that parent's retained children. Runtime-produced nodes always carry a non-zero `NodeId`; the root's `parent_id` is `kInvalidNodeId`.

All `InspectorNode::bounds`, `clip_bounds`, and `InspectorSnapshot::dirty_regions` values use root-logical pixels. `bounds` is the last committed retained geometry. If a node is layout-dirty, the snapshot reports that dirty bit while leaving the last published bounds intact rather than inventing speculative geometry. `clip_bounds` is the inherited effective clip entering the node; the root starts from the viewport, and child clipping is intersected as the traversal descends.

The dirty flags have deliberately different meanings:

- `layout_dirty` copies the node's pending retained-layout bit and is not consumed by inspection;
- `paint_dirty` is derived by intersecting the node's bounds with the Tree's current dirty rectangles, so it means "this node intersects pending damage", not "this node caused the damage";
- `dirty_regions` copies the exact pending root-logical paint-damage rectangles and taking a snapshot does not clear them.

`focusable` reflects the component predicate at snapshot time, `focused` marks the current retained focus owner, `pointer_capture_owner` records active capture ownership, and `availability` copies the node's effective inherited visibility/enabled/read-only state. `debug_name` is an owned UTF-8 diagnostic string; v1 currently reports a generic component label and applications must not treat it as a stable RTTI/type identifier.

`InspectorSnapshot::find(id)` performs a linear lookup over the owned vector. The returned pointer borrows from that snapshot only; vector mutation or snapshot destruction invalidates it. A stale/missing ID returns `nullptr`. Copying the snapshot makes an independent value, and neither a snapshot nor a selected ID keeps a live retained node alive.

### Query checkpoints, mutation and failures

`debug::inspector_snapshot(ui)` executes on the owning UI/main thread. Before traversing it flushes queued dynamic mutations at the normal retained checkpoint, so the result describes the reconciled tree visible at that boundary. If a lifecycle transition is already active, it returns an empty snapshot instead of exposing partially mounted/unmounted state.

Snapshotting can allocate while copying vectors and strings, and mutation flushing may execute the same work/errors as the normal retained checkpoint. Those failures are ordinary C++ failures; there is no hidden fallback to a partial snapshot. Snapshotting does **not** clear layout dirtiness or paint damage.

`inspector_enabled(ui)` and `inspector_selected_node(ui)` are non-owning O(1) value reads. Their `const UI&` parameter does not make concurrent access safe: inspector operations obey the same single-owner UI-thread rule as the rest of the retained runtime.

`set_inspector_enabled(ui, enabled)` and `set_inspector_selected_node(ui, id)` are idempotent for an unchanged value. A real change requests paint invalidation only; it does not invalidate layout. That invalidation uses the normal Tree/UI invalidation callback path, so callers must obey the same callback/reentrancy rules as `UI::invalidate()`. Missing, stale and `kInvalidNodeId` selections are accepted and simply produce no live-node highlight.

None of the inspector functions is an audio/DSP real-time API. Snapshotting allocates, and state changes participate in UI invalidation.

### Example

```cpp
#if defined(NATIVEUI_ENABLE_INSPECTOR)
ui::debug::set_inspector_enabled(ui, true);

auto snapshot = ui::debug::inspector_snapshot(ui);
for (const auto& node : snapshot.nodes) {
    if (node.paint_dirty) {
        log_damage(node.id, node.bounds);
    }
}

const ui::NodeId selected = snapshot.nodes.empty()
    ? ui::kInvalidNodeId
    : snapshot.nodes.front().id;
ui::debug::set_inspector_selected_node(ui, selected);

// The copy is independent of later retained reconciliation/destruction.
auto retained_diagnostic_copy = snapshot;
#endif
```

Use the inspector to understand bounds, clips, dirty state, focus/capture and retained hierarchy while debugging. Do not build application behavior that depends on the inspector being compiled in or on `debug_name` values.

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
