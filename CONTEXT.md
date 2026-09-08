# NativeUI compact recovery context

**Updated:** 2026-09-08

## Mission and architecture

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. It owns declarative composition, component lifetime, layout, input/focus, generic state/binding, drawing/widgets, text editing, styling/invalidation/resources, packaging and tests. It does **not** own plugin APIs, audio/DSP, host parameter semantics or a custom native windowing layer.

```text
Declarative C++ DSL
        |
        v
Runtime Component Tree
  | layout | input/focus | state
        |
        v
     Skia paint
        |
        v
Pugl OpenGL view / event bridge
  | Win32 | Cocoa | X11
```

- Windowing/embedding: Pugl.
- Rendering: Skia Ganesh/OpenGL plus headless Skia raster rendering.
- Third-party acquisition: CMake + CPM only.
- Public coordinates are logical pixels; native geometry/framebuffer is physical pixels.
- Widgets/layout stay platform-neutral and do not include Pugl/Win32/AppKit/Xlib headers.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `7665c96763a64a77cfc01009fb3e69adb0eee586`.
- Skia: `olilarkin/skia-builder` release `chrome/m149`.
- macOS Skia asset: `skia-build-mac-universal-gpu-release.zip`.
- Windows: x64 MSVC `/MD` default, `/MT` selectable.
- Linux: x64 GPU release.

## Scheduling model

NativeUI uses dependency-driven scheduling. GitHub `Dependencies:` are the only hard ticket-to-ticket gates. Ticket numbers and milestone order are not implicit dependencies.

- `Ready`: all explicit dependencies are Done and no real external blocker exists.
- `Doing`: implementation/review/validation is active.
- `Blocked`: an explicit dependency is unfinished or a real external blocker is documented.
- Keep at most three implementation lanes active by default.
- A branch waiting only on CI does not globally block the project and need not consume an implementation lane.
- Independent PRs start from `main` and may merge as soon as their own Definition of Done is satisfied.

`AGENTS.md` is the source of truth for these rules.

## Implemented baseline

Core/runtime includes observable `State<T>`, declarative DSL/runtime component tree, Row/Column/Stack/Padding/Spacer, constraints/alignment/flex/Grid/Scroll layout, clipping/transforms, focus scopes/traversal, logical pointer routing and toolkit pointer capture, command/gesture/drop primitives, bounded invalidation, generic Painter/Canvas, and headless/golden rendering support.

Rendering includes backend-neutral paths, gradients/paint styles, decoded image resources and SVG/icon resources. T022 provides backend-neutral `Image`, source-rectangle drawing, `Fill`/`Contain`/`Cover`, an application-supplied `ResourceProvider` and reusable `ImageCache`. T023 provides backend-neutral `SvgIcon`, centered contain-fit rendering and per-instance `SvgCache` reuse/failure caching over the same application-owned provider model. Widgets do not perform filesystem I/O, and Skia conversion/ownership stays private to Core.

Text/widgets include Header, Label/TextLabel, Knob, Toggle, TextInput, TextEditModel, multiline TextArea and interactive Canvas. T027 provides the platform-neutral font service. T028 provides UTF-8-aware multiline editing, cross-line selection/navigation, viewport scrolling, caret/selection painting and paint-only caret blink behavior.

Windowing uses `StandaloneWindow` (`PUGL_PROGRAM`) and `EmbeddedView` (`PUGL_MODULE`), with non-blocking embedded polling, resize support, clipboard bridging and GL resource lifetime constrained to an active Pugl GL context. General text clipboard writes use canonical `text/plain`; this is required by the pinned Pugl macOS MIME→UTI mapping and avoids passing a nil UTI to `NSPasteboard`.

The cross-cutting plugin-host safety baseline from #62/PR #63 is now merged. `StandaloneWindow` and `EmbeddedView` are non-movable because their internals retain stable back-references; embedded-font aliases are immutable-by-alias within the process-shared registry; retained UI/platform/resource APIs have explicit UI/resource-preparation thread contracts; and macOS platform builds require a consumer/plugin-specific `NATIVEUI_OBJC_RUNTIME_PREFIX` so the statically linked Pugl Objective-C runtime classes do not collide between plug-in/application consumers.

Every feature ticket ships `examples/features/tNNN_<feature>.cpp` with interactive mode and `--self-test`; feature sources also compile against `NativeUI::Core` in display-less CI.

## Current work and DAG frontier

- Last completed cross-cutting safety ticket: **#62 — plugin-host CODE_REVIEW revalidation**. PR #63 exact head `e197315c85dc6fb5213040f988833b0837c301d7` passed CI run #165 (`34103924635`) on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan. macOS additionally passed the two-consumer Objective-C runtime-isolation check and the clipboard/multi-instance lifecycle smoke. PR #63 was squash-merged as `4922b85ae8ebb2f004611081f257946ac60e0fa1`; issue #62 is Done/closed.
- Last completed rendering feature: **T023 — SVG/icon resources**. PR #60 adds backend-neutral parsed SVG handles, aspect-preserving centered contain rendering, viewBox-only support, per-instance provider-backed caching and explicit static/self-contained SVG semantics while preserving the #62 plugin-host/runtime-prefix contracts.
- Last completed text feature ticket: **T028 — multiline TextArea**. PR #56 final head `4af63a380ad5c39afed79891242f2d64aa414dd8` passed the Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan matrix and was squash-merged as `ccf53d234cb81a4fb546acba2990bb1478351496`.
- T028 post-merge macOS clipboard regression is fixed. PR #61 final head `57f56e70aa5852d0a90949af8ce1dcd94f76ebeb` passed CI run #152 (`34098862681`) on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan; macOS additionally passed the real clipboard plus multi-`EmbeddedView` lifecycle smoke. PR #61 was squash-merged as `e84aa9576f4197e249029e2028b45f6ab1283556`.
- The expanded platform review exposed a separate pre-existing macOS crash when multiple `StandaloneWindow` / `PUGL_PROGRAM` worlds coexist. It is tracked independently as **#64 — Platform — multiple StandaloneWindow instances crash on macOS** and must not be conflated with the fixed T028 clipboard regression.
- **T029 / PR #85** is active for advanced IME composition and remains draft while the platform bridge work and final validation are completed.
- **#86 / PR #87** is active for the Pugl drag-and-drop dependency update. NativeUI targets reviewed `hemduf/pugl` commit `7665c96763a64a77cfc01009fb3e69adb0eee586`, removes its platform-specific reject wrapper, enables `PUGL_ACCEPT_DROP` before realization and uses the public `puglRejectOffer()` API directly. The dependency review additionally restored the Win32 `PUGL_DATA_OFFER` → accept/reject → `PUGL_DATA` contract, hardened actual drop coordinates/UTF-8/lifetime and kept all drop decision state per view. A fresh exact-head NativeUI CI plus the real macOS Finder smoke remain completion gates.

Ready now:

- P0: **T042** multi-instance/attach-detach stress tests, **T047** install/export CMake package, **T053** consumer-scoped macOS Pugl/Objective-C bridge.
- P1/high unblock value: **T030** Button, **T032** Slider/RangeSlider, **T034** ScrollView.
- Other P1: **T033** ProgressBar/Meter, **T043** resize/scale hardening, **T044** pointer capture evaluation, **#64** multi-`StandaloneWindow` macOS lifecycle.
- P2: **T046** Wayland strategy/prototype.

Important explicit dependency chains:

```text
T030 -> T031
T030 + T032 -> T037 -> T038 -> T039 / T040
T034 -> T035 / T036
T030 + T031 + T032 + T036 -> T045
T030 + T032 + T034 + T035 + T036 -> T049
T042 -> T051 -> T052
T047 -> T048 -> T052
#62 -> T053
T047 + T053 -> T054
T047 -> T056 -> T057
```

Recommended next increment: complete the active T029 and #86 validation lanes while continuing an independent Ready P0 lane when concurrency permits.

## T022 implementation notes retained for recovery

- `Image` is a copyable platform-neutral handle whose public header contains no Skia/Pugl/platform type.
- `Image::decode` copies encoded bytes into Skia-owned data; callers do not retain source-buffer lifetime obligations.
- `CanvasContext2D::draw_image` supports whole-image or source-rectangle drawing plus `ImageFit::Fill`, `Contain` and `Cover`.
- Invalid images and non-positive/non-finite draw rectangles are safe no-ops.
- `ResourceProvider` resolves application-defined IDs to encoded byte vectors; filesystem/bundle/archive policy stays in the application layer.
- `ImageCache` caches successful images and explicit `NotFound`/`DecodeFailed` results; `clear()` invalidates the cache.

## T023 implementation notes retained for recovery

- `SvgIcon` is a copyable backend-neutral handle; Skia SVG DOM types remain private to Core.
- SVG source bytes are parsed during resource preparation and are not retained by callers; parsing/loading is not a real-time audio-thread API.
- `CanvasContext2D::draw_svg` uses a centered aspect-preserving contain fit, clips to the destination and treats non-positive/non-finite destinations as no-ops.
- ViewBox-only icons derive intrinsic dimensions from root `viewBox` source metadata without reading inline Skia SVG members across the prebuilt ABI boundary.
- V1 SVG resources are static and self-contained; NativeUI does not fetch external file/network resources or drive SVG animation.
- `SvgCache` borrows its `ResourceProvider`, which must outlive it; parsed successes and explicit failures are cached per cache instance and `clear()` affects only that instance.
- SVG linkage uses the pinned skia-builder SVG module plus its required shaper/unicode static dependencies.
- Completion coverage includes paths, transforms, gradients, malformed input, invalid destinations, viewBox-only resources, cache reuse/failure reuse, same-ID isolation across two caches, deterministic path golden comparison, isolated public headers and `t023_svg_icons --self-test`.

## T027 portability notes retained for recovery

- `FontManager` is public and platform-neutral; CoreText/DirectWrite/Fontconfig construction remains private in `src/skia_core.cpp`.
- Embedded font registration owns/copies bytes and publishes immutable registry snapshots; normal measure/paint reads do not acquire the registration mutex.
- UBSan `vptr` alone is disabled at the pinned prebuilt Skia ABI boundary; ASan and remaining UBSan checks stay enabled.
- Linux LSan keeps `detect_leaks=1`; only Fontconfig's process-lifetime `FcFontRenderPrepare` allocation path is suppressed.
- Windows CI explicitly uses MSVC to match `Skia.lib`; `NOMINMAX` prevents Win32 macro pollution.
- macOS uses the universal Skia artifact on `macos-15-intel`.
- Xcode 16.4 libc++ portability uses `std::atomic<std::shared_ptr<T>>` only when supported, otherwise the standard shared_ptr atomic load/store API.

## Current limitations

- T029 advanced IME composition is in progress.
- Multiple simultaneous `StandaloneWindow` / `PUGL_PROGRAM` worlds can crash on macOS; tracked in #64. Plugin/editor multi-instance validation uses independent `EmbeddedView` / `PUGL_MODULE` instances and is green.
- Real macOS Finder drag/drop validation for the final Pugl pin remains pending under #86 even though the fork's native drag/drop regressions and dependency review are green.
- Windows `WM_DROPFILES` preserves NativeUI's logical offer/accept/reject contract only after the OS-level drop; native hover-time acceptance feedback would require a different Windows backend mechanism such as OLE `IDropTarget`.
- Standard Button/Slider/ComboBox/List/ScrollView/Tabs/Menu widgets remain incomplete.
- Theme/style inheritance, accessibility, Wayland, packaging/install/export and full host integration remain incomplete.

## Build commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/nativeui_demo
```

On macOS platform builds, provide a consumer/application/plugin-specific Objective-C runtime prefix, normally derived from the final bundle identifier:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DNATIVEUI_OBJC_RUNTIME_PREFIX=ComVendorProduct_
```

Core-only macOS builds (`NATIVEUI_BUILD_PLATFORM=OFF`) do not require the prefix.

Offline dependency overrides:

```bash
cmake -S . -B build \
  -DNATIVEUI_PUGL_SOURCE=/path/to/pugl \
  -DNATIVEUI_SKIA_ROOT=/path/to/extracted/skia-builder
```

## Non-negotiable invariants

- no plugin parameter/audio semantics in NativeUI;
- no SDL/GLFW/Qt/JUCE/NanoVG;
- widgets never depend on Pugl/Win32/AppKit/Xlib;
- Pugl remains the native view/event layer;
- Skia remains the renderer and comes from pinned skia-builder binaries;
- dependencies stay CMake + CPM;
- a normal new widget must not require a central component enum/switch;
- mutable instance-dependent process-global/singleton/thread-local state is forbidden;
- macOS runtime-visible classes in the statically linked platform bridge must use a consumer/plugin-specific collision-resistant prefix.

## Pugl drag-and-drop portability note

The pinned `hemduf/pugl` commit implements `puglRejectOffer()` on macOS, Windows and X11, so portable NativeUI code calls that public API directly. The previous macOS/Windows compatibility no-op is removed. Cocoa delivers accepted drag data exactly once at the actual drop boundary. On Windows, `WM_DROPFILES` has no native hover-time negotiation, but the reviewed backend now emits `PUGL_DATA_OFFER` after the OS drop and before exposing the payload, so NativeUI's generic accept/reject path is preserved. Rejected data is suppressed, accepted data is delivered once at the actual drop coordinates, and all decision/payload state remains scoped to the concrete view.
