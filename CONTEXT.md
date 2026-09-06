# NativeUI compact recovery context

**Updated:** 2026-09-07

## Goal

Generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. UI only: no DSP/audio and no plugin parameter model.

## Current architecture

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
- Rendering: Skia Ganesh/OpenGL.
- Headless Skia raster renderer is implemented for display-free pixel/golden tests.
- Dependencies: CMake + CPM only.
- Skia: prebuilt static artifacts from `olilarkin/skia-builder`.
- Public coordinates: logical pixels.
- Pugl native geometry/framebuffer: physical pixels; divide input/geometry by `puglGetScaleFactor()`, scale Skia canvas once.

## Pinned dependencies

- Pugl repository: `lv2/pugl`
- Pugl commit: `b7637149ebe53124e5be90559e02a0185bbcbd73`
- skia-builder release: `chrome/m149`
- macOS default asset: `skia-build-mac-universal-gpu-release.zip`
- Windows: x64 `/MD` default, `/MT` selectable
- Linux: x64 GPU release

## Implemented today

Core/runtime:

- `State<T>` observable with RAII subscription;
- declarative DSL compiled into runtime component tree;
- `Row`, `Column`, `Stack`, `Padding`, `Spacer`;
- focus traversal;
- hit testing and logical pointer routing;
- pointer capture at toolkit level;
- separate layout/paint invalidation with bounded logical dirty-region aggregation;
- generic `Painter`/`PaintContext` over Skia.

Widgets/text:

- `Header`;
- `Label` / `TextLabel` with shared `TextStyle` + `TextService` measurement;
- T027 branch adds platform-neutral `FontManager`, named family/style selection, embedded font aliases and ordered fallback families;
- measurement and painting share the same UTF-8 font-run resolver; `CanvasContext2D` accepts full `TextStyle` as well as the legacy size/color overload;
- `Knob`;
- `Toggle`;
- `TextInput` with UTF-8 committed text, selection, caret, clipboard, undo/redo, word navigation, double/triple click, horizontal scrolling, placeholder, max length, submit, Escape/revert;
- interactive `Canvas` with local `CanvasContext2D` and local input callback.

Windowing:

- `StandaloneWindow` uses `PUGL_PROGRAM`;
- `EmbeddedView` uses `PUGL_MODULE` + native parent;
- embedded `poll()` is non-blocking;
- embedded size can be changed externally;
- clipboard bridge through Pugl;
- Ganesh resources are created/destroyed only while the Pugl GL context is active.

Build:

- CPM fetches Pugl source and skia-builder binary asset;
- Skia archive layout auto-detects both CPM-flattened and manual `build/` root layouts;
- `SkFontMetrics.h` is explicitly included;
- no direct macOS `gl3.h` include; Pugl owns platform GL headers/proc lookup.

## Feature example policy

Every feature ticket must add a dedicated executable with `--self-test`. Existing M1/M2 features T007–T019 and current text-system features follow this policy. Infrastructure-only tickets may update an existing example when appropriate.

## Current limitations

- constraints, Row/Column alignment/distribution, flex, Grid, clipping and generic Scroll layout are implemented; interactive ScrollView remains;
- invalidation is not yet a full dirty-region system;
- golden PPM baseline/diff tooling exists; broader rendering coverage remains;
- pointer capture is toolkit-level only;
- no multiline text / TextArea;
- advanced IME composition/pre-edit/candidate positioning missing;
- no standard Button/Slider/ComboBox/List/ScrollView/Tabs/Menu widgets yet;
- no centralized style/theme state system beyond basic colors/painting;
- no accessibility layer;
- no Wayland backend;
- packaging/install/export and full host integration matrix remain incomplete; standalone/embedded smoke harness exists.

## Build commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/nativeui_demo
```

Offline/local dependency overrides:

```bash
cmake -S . -B build \
  -DNATIVEUI_PUGL_SOURCE=/path/to/pugl \
  -DNATIVEUI_SKIA_ROOT=/path/to/extracted/skia-builder
```

## Recovery status

- Milestones M0, M1 and M2 are complete; M4 text-system work is in progress.
- Strict sequential workflow is mandatory; priority may not skip numeric tickets.
- Last completed sequential ticket: `T026` — Text/Label component and centralized text measurement. PR #53 is merged; real macOS arm64 Release CTest was **40/40** before advancing.
- Current ticket: `T027` — font manager/fallback abstraction, issue #27 `Doing`, branch `feature/t027-font-manager`, draft PR #55.
- T027 implementation is present: neutral font service, named family/weight/slant, embedded fonts, ordered explicit fallbacks plus platform Unicode fallback, shared measure/paint run resolution, Label/Canvas fluent consumption, deterministic embedded-font fixtures, dedicated unit test, feature example and platform smoke probes.
- T027 TDD red state was recorded before the API existed. Review passes found and corrected moved-typeface metadata, insufficient fallback-paint assertions, and missing full-`TextStyle` Canvas drawing.
- Validation blocker: the current GitHub Actions PR run is still queued/pending and has not allocated jobs. Do **not** merge #55 until the relevant build/tests are green. The artifact container has no DNS and cannot independently fetch the pinned Skia/Pugl dependencies.
- Next ticket after T027 is validated/merged/closed: `T028`.
- `T041` exists ahead of sequence and must not be used to skip T028–T040.
- Latest proven baseline on `main` (2026-09-06, macOS arm64, real Skia): Release configure/build pass; CTest **40/40**, including all 17 then-current feature self-tests. Normal CTest preserves golden baseline hashes. Native window smoke execution remains opt-in.
- Git source repository: [hemduf/nativeui](https://github.com/hemduf/nativeui), default branch `main`. `.gitignore` excludes generated/local files, recovery ZIPs, `tickets/` and `TICKETS.md`; `.gitattributes` preserves binary PPM baselines and normalizes text.
- GitHub Issues are authoritative for ticket content/status. A fresh clone must resume from GitHub, not optional local ticket exports.

## Non-negotiable invariants

- no plugin parameter/audio semantics in NativeUI;
- no SDL/GLFW/Qt/JUCE/NanoVG;
- widgets never depend on Pugl/Win32/AppKit/Xlib;
- Pugl remains the default native view/event layer;
- Skia remains the renderer;
- dependencies are CMake + CPM;
- Skia comes from skia-builder binaries, not a NativeUI Skia build pipeline;
- adding a widget must not require a central component enum/switch.

## T025 compact note

- Public `ui::TextEditModel` lives in `include/nativeui/text_edit.hpp` and has no Skia/Pugl/platform dependency.
- It owns UTF-8-safe cursor/selection byte offsets, word/codepoint/document navigation, insertion/deletion, max-length enforcement, word selection and bounded undo/redo.
- `TextInputComponent` owns view behavior: focus snapshot, horizontal scrolling, pixel-to-byte hit testing, clipboard/platform requests and paint.

## T026 compact note

- Public text styling/measurement lives in `include/nativeui/text.hpp`: `TextStyle`, `TextMetrics`, `TextService`, `TextAlign`, `FontWeight`.
- `Label` is the reusable single-line text widget; wrapping remains out of scope. `TextLabel` is an alias.
- `Painter::text()` consumes `TextStyle`; `Painter::measure_text()` delegates to `TextService`.
- `Header` uses the same shared text style/measurement path.
- Cross-platform golden policy excludes platform-shaped glyph interiors and compares deterministic geometry; same-platform tests verify visible glyphs and alignment.

## T027 compact note (in progress)

- `TextStyle` adds `family`, `fallback_families`, `FontSlant`, and numeric regular/bold Skia-compatible weights.
- `FontManager` is platform-neutral. CoreText/DirectWrite/Fontconfig construction remains private in `src/skia_core.cpp`.
- Embedded fonts are registered from owned/copied bytes under NativeUI aliases. Registry publication is copy-on-write; normal measure/paint loads an immutable snapshot without taking the registration mutex.
- Resolution order per Unicode scalar: explicit primary family -> ordered explicit fallbacks -> platform Unicode fallback -> best `.notdef` face.
- Adjacent scalars using the same face are grouped into UTF-8 byte runs. `TextService::measure()` and `Painter::text()` consume the same resolved layout, so alignment width and painted faces cannot diverge.
- Tests use tiny deterministic generated fonts: primary contains `A`; fallback contains `Ω` and `日`. Selection, embedded registration, mixed-run measurement and independent painting of all three glyphs are covered.
- `nativeui_example_t027_fonts --self-test` exercises platform default selection and a named fallback chain without opening a window. Standalone/embedded smoke executables also probe platform font resolution when native smoke tests are enabled.

## Pugl reject-offer portability hotfix

- Pugl pinned at `b7637149...` declares `puglRejectOffer()` but only X11 defines it.
- Never call `puglRejectOffer()` directly from portable NativeUI code. Use `reject_pugl_drop_offer()` in `pugl_skia_setup.inc`.
- X11 uses explicit rejection; macOS/Windows rely on native unaccepted-offer semantics.

## T041 compact note

- `nativeui_smoke_standalone` exercises PUGL_PROGRAM construction, native handle, non-blocking poll, resize, close and teardown.
- `nativeui_smoke_embedded` creates a standalone parent then attaches a PUGL_MODULE child via the parent native handle; repeated child `poll()` calls are checked for non-blocking behavior.
- CTest registration is opt-in with `-DNATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS=ON`; tests carry labels `integration;platform;smoke` and a 20-second timeout.
