# NativeUI compact recovery context

**Updated:** 2026-09-07

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

- Pugl: `lv2/pugl` commit `b7637149ebe53124e5be90559e02a0185bbcbd73`.
- Skia: `olilarkin/skia-builder` release `chrome/m149`.
- macOS Skia asset: `skia-build-mac-universal-gpu-release.zip`.
- Windows: x64 MSVC `/MD` default, `/MT` selectable.
- Linux: x64 GPU release.

## Scheduling model

NativeUI now uses dependency-driven scheduling. GitHub `Dependencies:` are the only hard ticket-to-ticket gates. Ticket numbers and milestone order are not implicit dependencies.

- `Ready`: all explicit dependencies are Done and no real external blocker exists.
- `Doing`: implementation/review/validation is active.
- `Blocked`: an explicit dependency is unfinished or a real external blocker is documented.
- Keep at most three implementation lanes active by default.
- A branch waiting only on CI does not globally block the project and need not consume an implementation lane.
- Independent PRs start from `main` and may merge as soon as their own Definition of Done is satisfied.

`AGENTS.md` is the source of truth for these rules.

## Implemented baseline

Core/runtime includes observable `State<T>`, declarative DSL/runtime component tree, Row/Column/Stack/Padding/Spacer, constraints/alignment/flex/Grid/Scroll layout, clipping/transforms, focus scopes/traversal, logical pointer routing and toolkit pointer capture, command/gesture/drop primitives, bounded invalidation, generic Painter/Canvas, and headless/golden rendering support.

Rendering includes backend-neutral paths, gradients/paint styles and decoded image resources. T022 provides backend-neutral `Image`, source-rectangle drawing, `Fill`/`Contain`/`Cover`, an application-supplied `ResourceProvider` and reusable `ImageCache`; no widget performs filesystem I/O. Skia conversion/ownership stays private to Core.

Text/widgets include Header, Label/TextLabel, Knob, Toggle, TextInput, TextEditModel, multiline TextArea and interactive Canvas. T027 provides the platform-neutral font service. T028 provides UTF-8-aware multiline editing, cross-line selection/navigation, viewport scrolling, caret/selection painting and paint-only caret blink behavior.

Windowing uses `StandaloneWindow` (`PUGL_PROGRAM`) and `EmbeddedView` (`PUGL_MODULE`), with non-blocking embedded polling, resize support, clipboard bridging and GL resource lifetime constrained to an active Pugl GL context.

Every feature ticket ships `examples/features/tNNN_<feature>.cpp` with interactive mode and `--self-test`; feature sources also compile against `NativeUI::Core` in display-less CI.

## Current work and DAG frontier

- Last completed feature ticket: **T028 — multiline TextArea**. PR #56 final head `4af63a380ad5c39afed79891242f2d64aa414dd8` passed the Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan matrix and was squash-merged as `ccf53d234cb81a4fb546acba2990bb1478351496`.
- **T023 — SVG/icon resources** remains `Doing` in draft PR #60. Current head is `b36896641c30c4b8debea2ed669a22ed9ca2e5d6`; CI run #137 is pending on that head. T023 is independent and must not block other Ready work.

Ready now:

- P0: **T042** multi-instance/attach-detach stress tests, **T047** install/export CMake package.
- P1/high unblock value: **T030** Button, **T032** Slider/RangeSlider, **T034** ScrollView.
- Other P1: **T029** advanced IME, **T033** ProgressBar/Meter, **T043** resize/scale hardening, **T044** pointer capture evaluation.
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
```

Recommended lane allocation while T023 is only waiting on CI:

1. widget critical path: T030, then T032/T034 according to merge/conflict state;
2. release path: T047 -> T048;
3. robustness/performance path: T042 -> T051.

T029/T033/T043/T044/T046 are valid independent fallback work whenever a lane is free or another branch is waiting on external validation.

## T022 implementation notes retained for recovery

- `Image` is a copyable platform-neutral handle whose public header contains no Skia/Pugl/platform type.
- `Image::decode` copies encoded bytes into Skia-owned data; callers do not retain source-buffer lifetime obligations.
- `CanvasContext2D::draw_image` supports whole-image or source-rectangle drawing plus `ImageFit::Fill`, `Contain` and `Cover`.
- Invalid images and non-positive/non-finite draw rectangles are safe no-ops.
- `ResourceProvider` resolves application-defined IDs to encoded byte vectors; filesystem/bundle/archive policy stays in the application layer.
- `ImageCache` caches successful images and explicit `NotFound`/`DecodeFailed` results; `clear()` invalidates the cache.

## T027 portability notes retained for recovery

- `FontManager` is public and platform-neutral; CoreText/DirectWrite/Fontconfig construction remains private in `src/skia_core.cpp`.
- Embedded font registration owns/copies bytes and publishes immutable registry snapshots; normal measure/paint reads do not acquire the registration mutex.
- UBSan `vptr` alone is disabled at the pinned prebuilt Skia ABI boundary; ASan and remaining UBSan checks stay enabled.
- Linux LSan keeps `detect_leaks=1`; only Fontconfig's process-lifetime `FcFontRenderPrepare` allocation path is suppressed.
- Windows CI explicitly uses MSVC to match `Skia.lib`; `NOMINMAX` prevents Win32 macro pollution.
- macOS uses the universal Skia artifact on `macos-15-intel`.
- Xcode 16.4 libc++ portability uses `std::atomic<std::shared_ptr<T>>` only when supported, otherwise the standard shared_ptr atomic load/store API.

## Current limitations

- T023 SVG/icon resources are not merged yet.
- T029 advanced IME composition remains to be implemented.
- Standard Button/Slider/ComboBox/List/ScrollView/Tabs/Menu widgets remain incomplete.
- Theme/style inheritance, accessibility, Wayland, packaging/install/export and full host integration remain incomplete.

## Build commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/nativeui_demo
```

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
- a normal new widget must not require a central component enum/switch.

## Pugl reject-offer portability note

Pinned Pugl declares `puglRejectOffer()` but only X11 defines it. Portable NativeUI code must use `reject_pugl_drop_offer()` in `pugl_skia_setup.inc`: X11 rejects explicitly; macOS/Windows rely on native unaccepted-offer semantics.
