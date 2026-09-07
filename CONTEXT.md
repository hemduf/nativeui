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

Rendering now includes backend-neutral paths plus paint styles. `Path` supports move/line/quad/cubic/close, fill and styled stroke. T021 adds immutable/copyable `LinearGradient` and `RadialGradient` descriptions, ordered `GradientStop`s, clamped opacity and compact SourceOver/Multiply/Screen/Plus blending while preserving the existing simple `Color` overloads. All Skia path/shader/blend conversion stays private to `Painter`.

Text/widgets include Header, Label/TextLabel, Knob, Toggle, TextInput, TextEditModel and interactive Canvas. T027 provides a platform-neutral font service with named family/weight/slant, embedded font aliases, ordered explicit fallback families and platform Unicode fallback. Measurement and painting share the same UTF-8 resolved-run path.

Windowing uses `StandaloneWindow` (`PUGL_PROGRAM`) and `EmbeddedView` (`PUGL_MODULE`), with non-blocking embedded polling, resize support, clipboard bridging and GL resource lifetime constrained to an active Pugl GL context.

Every feature ticket ships `examples/features/tNNN_<feature>.cpp` with interactive mode and `--self-test`; feature sources also compile against `NativeUI::Core` in display-less CI.

## Current sequential status

Strict numeric sequencing from `AGENTS.md` is mandatory. A recovery pass after T027 found unfinished lower-numbered M3 tickets T020–T023; T028 remains intentionally paused until those rendering tickets are complete.

- Last completed ticket: **T021 — gradients and richer paint styles** (implementation/review complete on PR #58; merge only after this exact documentation head passes the final matrix).
- T021 implementation head `5b23b4609f4fad7d6963b4600f93bc09b999973e` passed CI run #82 on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan, including 27/27 sanitizer tests.
- Review passes A/B/C are complete with no unresolved thread.
- Next sequential ticket after T021 merge/closure and recovery snapshot/ref: **T022 — image/resource drawing**, then T023.
- Draft PR #56 for T028 must not be merged ahead of T022–T023.

## T021 implementation notes

- Public gradient values contain only NativeUI `Point`, `Color`, stop vectors and enums; no Skia/Pugl/platform type leaks into the value API.
- Existing solid-color `fill_rect` / `fill_rounded_rect` overloads are unchanged.
- Linear gradients support the two-color convenience form and arbitrary ordered stops; radial gradients support the same stop model.
- Stops must be finite, strictly increasing and inside `[0, 1]`; malformed lists fall back deterministically instead of being passed unchecked to Skia.
- Invalid/non-positive radial radius falls back safely.
- `PaintOptions::opacity` is finite/clamped to `[0, 1]`; blend modes map explicitly to pinned Skia m149.
- `nativeui_paint_style_tests` covers two-stop/multi-stop/radial rendering plus opacity and multiply behavior.
- `nativeui_gradient_golden_tests` uses stable constant plateau regions away from interpolation boundaries; the fixture is a compact exact-size P6 PPM.
- `nativeui_example_t021_gradients` provides interactive usage and a deterministic headless `--self-test`.
- `paint_style.hpp` has isolated public-header compile coverage.

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

- T022/T023 still need image resources and SVG/icon resources.
- T028/T029 still need multiline text/TextArea and advanced IME composition after the lower-numbered rendering tickets are done.
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
