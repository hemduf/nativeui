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
- Rendering: Skia Ganesh/OpenGL plus headless Skia raster rendering for display-free tests.
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

Text/widgets include Header, Label/TextLabel, Knob, Toggle, TextInput, TextEditModel and interactive Canvas. T027 adds a platform-neutral font service with named family/weight/slant, embedded font aliases, ordered explicit fallback families and platform Unicode fallback. Measurement and painting share the same UTF-8 resolved-run path.

Windowing uses `StandaloneWindow` (`PUGL_PROGRAM`) and `EmbeddedView` (`PUGL_MODULE`), with non-blocking embedded polling, resize support, clipboard bridging and GL resource lifetime constrained to an active Pugl GL context.

Every feature ticket must ship `examples/features/tNNN_<feature>.cpp` with an interactive mode and `--self-test`; feature sources also compile against `NativeUI::Core` in display-less CI.

## Current sequential status

Strict numeric sequencing from `AGENTS.md` is mandatory.

- Last fully closed ticket: **T026** — Text/Label component and centralized text measurement, PR #53 merged; real macOS arm64 Release CTest was **40/40** before advancing.
- Current completion ticket: **T027** — font manager/fallback abstraction, issue #27, branch `feature/t027-font-manager`, draft PR #55.
- T027 implementation, required tests and review passes A/B/C are complete.
- Code head `139f8a5` fixed the Xcode 16.4 libc++ portability problem by using `std::atomic<std::shared_ptr<T>>` only when the specialization is available and falling back to the standard shared_ptr atomic load/store API otherwise. Registry readers still consume immutable snapshots without taking the registration mutex.
- GitHub Actions run #31 on `139f8a5` is green on **Linux X11**, **Windows/MSVC**, **Linux ASan+UBSan**, and **macOS Intel/CoreText/AppKit**. The macOS lane configured, built and tested successfully.
- This recovery-document update is part of T027's final completion commit sequence and therefore creates a new final head. **Do not merge PR #55 until the matrix for the resulting documentation head is also green.**
- No T028 implementation has started.
- After PR #55 is green/merged, close #27 as `status:done`, create/provide `nativeui_T027.zip`, then advance to **T028 — Add multiline TextArea**.
- T041 exists ahead of sequence and must not be used to skip T028–T040.

## T027 implementation note

- `TextStyle` adds `family`, `fallback_families`, `FontSlant`, and numeric regular/bold Skia-compatible weights.
- `FontManager` is public and platform-neutral; CoreText/DirectWrite/Fontconfig construction remains private in `src/skia_core.cpp`.
- Embedded font registration owns/copies bytes and publishes immutable registry snapshots. Registration is serialized; normal measure/paint reads do not acquire that mutex.
- Resolution per Unicode scalar: explicit primary -> ordered explicit fallbacks -> platform Unicode fallback -> best `.notdef` face.
- Adjacent scalars using the same face are grouped into UTF-8 byte runs; `TextService::measure()` and `Painter::text()` use the same resolved layout.
- Deterministic tiny test fonts prove primary `A` and fallback `Ω`/`日` selection, mixed-run measurement and painting.
- `nativeui_example_t027_fonts --self-test` exercises platform default selection and a named fallback chain. Standalone/embedded smoke executables also probe platform font resolution when native smoke tests are enabled.

## T027 CI portability record

- UBSan `vptr` alone is disabled at the pinned prebuilt Skia ABI boundary; ASan and remaining UBSan checks stay enabled.
- Linux LSan keeps `detect_leaks=1`; only Fontconfig's process-lifetime `FcFontRenderPrepare` allocation path is suppressed.
- Windows CI explicitly uses MSVC to match `Skia.lib`; `NOMINMAX` prevents Win32 macro pollution in NativeUI C++ platform code.
- `macos-15-intel` is used for the macOS lane because the pinned Skia artifact is universal and the arm64 hosted pool remained persistently queued.
- Run #30 exposed the libc++ atomic-shared_ptr specialization gap; `139f8a5` fixed it and run #31 passed all four required lanes.

## Current limitations / next work

- T028/T029 still need multiline text/TextArea and advanced text-system work.
- Standard Button/Slider/ComboBox/List/ScrollView/Tabs/Menu widgets remain later milestones.
- Theme/style inheritance, accessibility, Wayland, packaging/install/export and full host integration remain incomplete.
- Advanced IME composition/pre-edit/candidate positioning remains explicit future platform work.

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
