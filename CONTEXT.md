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

## Implemented baseline

Core/runtime includes observable `State<T>`, declarative DSL/runtime component tree, Row/Column/Stack/Padding/Spacer, constraints/alignment/flex/Grid/Scroll layout, clipping/transforms, focus scopes/traversal, logical pointer routing and toolkit pointer capture, command/gesture/drop primitives, bounded invalidation, generic Painter/Canvas, and headless/golden rendering support.

Rendering includes backend-neutral paths, paint styles and images. `Path` supports move/line/quad/cubic/close, fill and styled stroke. T021 provides linear/radial gradients, ordered stops, opacity and compact blend modes. T022 provides backend-neutral decoded `Image` handles, source-rectangle drawing, `Fill`/`Contain`/`Cover`, and an application-supplied `ResourceProvider` with reusable `ImageCache`; no widget performs filesystem I/O. Skia conversion/ownership stays private to Core.

Text/widgets include Header, Label/TextLabel, Knob, Toggle, TextInput, TextEditModel and interactive Canvas. T027 provides a platform-neutral font service with named family/weight/slant, embedded font aliases, ordered explicit fallback families and platform Unicode fallback. Measurement and painting share the same UTF-8 resolved-run path.

Windowing uses `StandaloneWindow` (`PUGL_PROGRAM`) and `EmbeddedView` (`PUGL_MODULE`), with non-blocking embedded polling, resize support, clipboard bridging and GL resource lifetime constrained to an active Pugl GL context.

Every feature ticket ships `examples/features/tNNN_<feature>.cpp` with interactive mode and `--self-test`; feature sources also compile against `NativeUI::Core` in display-less CI.

## Current sequential status

Strict numeric sequencing from `AGENTS.md` is mandatory. A recovery pass after T027 found unfinished lower-numbered M3 tickets; T028 remains paused until rendering tickets through T023 are complete.

- Last completed ticket: **T022 — image/resource drawing**.
- PR #59 exact head `cd87e463b680bbbd7c9d8112adab14519d34c3ac` passed CI run #106 (`34086535149`) on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan, then squash-merged to `main` as `317c9d94c217a423b9843ff7eb066b6c8c6559c7`.
- Review A/B/C are complete with no unresolved review thread. Review B's only finding was corrected by moving image services into standalone `src/skia_image.cpp`.
- T022 coverage includes in-memory decode/render, source rectangles, Fill/Contain/Cover, explicit missing/decode errors, reusable provider-backed caching, isolated `image.hpp` compilation, deterministic scaling comparison and `t022_images --self-test`.
- Next sequential ticket: **T023 — SVG/icon resources**.
- Draft PR #56 for T028 must remain paused until T023 is Done.

## T022 implementation notes

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

## Current limitations / next work

- T023 still needs SVG/icon resources.
- T028/T029 still need multiline text/TextArea and advanced IME composition after T023.
- Standard Button/Slider/ComboBox/List/ScrollView/Tabs/Menu widgets remain later milestones.
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
