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

Rendering includes backend-neutral `Path`, move/line/quad/cubic/close commands, fill and styled stroke with cap/join/miter settings. Public path state contains only NativeUI geometry/enums; conversion to the pinned Skia `SkPathBuilder` stays inside `Painter`.

Text/widgets include Header, Label/TextLabel, Knob, Toggle, TextInput, TextEditModel and interactive Canvas. T027 adds a platform-neutral font service with named family/weight/slant, embedded font aliases, ordered explicit fallback families and platform Unicode fallback. Measurement and painting share the same UTF-8 resolved-run path.

Windowing uses `StandaloneWindow` (`PUGL_PROGRAM`) and `EmbeddedView` (`PUGL_MODULE`), with non-blocking embedded polling, resize support, clipboard bridging and GL resource lifetime constrained to an active Pugl GL context.

Every feature ticket must ship `examples/features/tNNN_<feature>.cpp` with interactive mode and `--self-test`; feature sources also compile against `NativeUI::Core` in display-less CI.

## Current sequential status

Strict sequencing from `AGENTS.md` is mandatory. A recovery pass after T027 found unfinished lower-numbered M3 tickets T020–T023, so T028 is paused until they are completed in numeric order.

- Current ticket: **T020 — generic path drawing API**.
- Draft PR #57 implements T020 with dedicated unit, transform, degenerate-path, golden and feature-example coverage.
- Review passes A/B/C are complete with no unresolved thread.
- Code head `c4a3881ce2f85886ea6ad432e2e172110a0abccc` passed CI run #58 on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan.
- A review cleanup restored unrelated golden-test rationale comments; completion documentation is now being added on top of that green code head. The exact final documentation head must pass the same matrix before merge.
- After T020 is merged/closed, the next sequential ticket is **T021 — gradients and richer paint styles**, then T022 and T023. Only then resume T028.
- Draft PR #56 for T028 is intentionally paused and must not be merged ahead of T020–T023.

## T020 implementation notes

- `ui::Path` is copyable, backend-neutral and stores a compact command stream using NativeUI `Point` only.
- `Painter::fill_path()` and `Painter::stroke_path()` translate the command stream through Skia m149 `SkPathBuilder`; no `SkPath` appears in the public API.
- `StrokeStyle` supports width, butt/round/square caps, miter/round/bevel joins and miter limit.
- Empty paths and non-positive stroke widths are safe no-ops.
- Canvas transform coverage verifies path geometry remains local to the Canvas coordinate system.
- Dedicated `nativeui_path_tests` covers move/line/quad/cubic/close, fill/stroke, cap/join style use, empty paths and degenerate geometry.
- `path_scene` golden compares stable solid interiors away from anti-aliased edges.
- `nativeui_example_t020_paths` provides interactive usage and deterministic `--self-test` rendering probes.

## T027 portability notes retained for recovery

- `TextStyle` adds `family`, `fallback_families`, `FontSlant`, and numeric Skia-compatible weights.
- `FontManager` is public and platform-neutral; CoreText/DirectWrite/Fontconfig construction remains private in `src/skia_core.cpp`.
- Embedded font registration owns/copies bytes and publishes immutable registry snapshots; normal measure/paint reads do not acquire the registration mutex.
- UBSan `vptr` alone is disabled at the pinned prebuilt Skia ABI boundary; ASan and remaining UBSan checks stay enabled.
- Linux LSan keeps `detect_leaks=1`; only Fontconfig's process-lifetime `FcFontRenderPrepare` allocation path is suppressed.
- Windows CI explicitly uses MSVC to match `Skia.lib`; `NOMINMAX` prevents Win32 macro pollution.
- macOS uses the universal Skia artifact on `macos-15-intel`.
- Xcode 16.4 libc++ portability uses `std::atomic<std::shared_ptr<T>>` only when supported, otherwise the standard shared_ptr atomic load/store API.

## Current limitations / next work

- T021–T023 still need gradients/richer paints, image resources and SVG/icon resources.
- T028/T029 still need multiline text/TextArea and advanced IME composition work after the lower-numbered rendering tickets are done.
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
