# NativeUI compact recovery context

**Updated:** 2026-09-06

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

Widgets:

- `Header`;
- `Label` / `TextLabel` with shared `TextStyle` + `TextService` measurement;
- `Knob`;
- `Toggle`;
- `TextInput` with UTF-8 committed text, selection, caret, clipboard, undo/redo, word navigation, double/triple click, horizontal scrolling, placeholder, max length, submit, Escape/revert;
- interactive `Canvas` with local `CanvasContext2D` and local input callback.

Canvas demo:

- 16-step sequencer rendering;
- click toggles steps;
- click-drag paints/erases steps;
- keyboard selection with Left/Right;
- Space/Enter toggle;
- Up adds;
- Down/Backspace/Delete removes.

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

## Known issue just addressed

Reverse focus must normalize macOS/AppKit BackTab (`U+0019`) to `Tab` with `shift=true`. Ensure this remains covered by tests when input translation is refactored.

## Feature example policy

Every feature ticket must add a dedicated executable with `--self-test`. Existing M1/M2 features T007–T015 now have executables under `examples/features/`. Infrastructure-only T001–T006 are exempt.

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
- packaging/install/export and full host integration matrix remain incomplete; standalone/embedded smoke harness now exists.

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
- Strict sequential workflow is now mandatory; priority may not skip numeric tickets.
- Last completed sequential ticket: `T025` — reusable text edit model.
- `T041` exists ahead of sequence and must not be used to skip T027–T040.
- Current ticket: `T026` — Text/Label component and centralized text measurement; implementation complete, real-Skia golden validation still failing.
- Next ticket after T026 is validated: `T027` — font manager/fallback abstraction.
- Latest local baseline (2026-09-06, macOS arm64, real Skia): Release configure and build pass; CTest 39/40 pass. Only `nativeui_golden_tests` fails on `label_scene` (597/1104 compared pixels differ, max delta 181, first pixel `(8,4)`). Label unit tests and all 17 feature self-tests pass. Native platform smoke execution remains opt-in and was not run.
- Git source repository: [hemduf/nativeui](https://github.com/hemduf/nativeui), branch `main`. `.gitignore` excludes generated/local files, recovery ZIPs, `tickets/` and `TICKETS.md`; `.gitattributes` preserves binary PPM baselines and normalizes text. Source, tests, examples, CMake, CI and continuation documentation are versioned.
- All 52 tickets live in [GitHub Issues](https://github.com/hemduf/nativeui/issues?q=is%3Aissue): 22 closed/completed, 30 open at import, with priority/status labels and nine roadmap milestones. GitHub is authoritative for ticket content and status. Optional ignored local copies are available on the original machine; a fresh clone resumes from GitHub without them. T026 remains Doing; no feature ticket was advanced during migration or source publication.
- Feature examples are mandatory; T026 adds `examples/features/t026_label.cpp` with `--self-test`.

## T025 compact note

- Public `ui::TextEditModel` lives in `include/nativeui/text_edit.hpp` and has no Skia/Pugl/platform dependency.
- It owns UTF-8-safe cursor/selection byte offsets, word/codepoint/document navigation, insertion/deletion, max-length enforcement, word selection and bounded undo/redo.
- `TextInputComponent` now owns only view behavior: focus snapshot, horizontal scrolling, pixel-to-byte hit testing, clipboard/platform requests and paint.
- External `State<std::string>` updates call `model.set_text(..., preserve_selection=true, clear_history=true)`.
- Escape revert uses `set_text(snapshot, preserve_selection=false, clear_history=false)`.

## Non-negotiable invariants

- no plugin parameter/audio semantics in NativeUI;
- no SDL/GLFW/Qt/JUCE/NanoVG;
- widgets never depend on Pugl/Win32/AppKit/Xlib;
- Pugl remains the default native view/event layer;
- Skia remains the renderer;
- dependencies are CMake + CPM;
- Skia comes from skia-builder binaries, not a NativeUI Skia build pipeline;
- adding a widget must not require a central component enum/switch.

## T017 compact note

- Portable commands: Copy/Cut/Paste/SelectAll/Undo/Redo.
- `InputEvent::primary` is platform-normalized by Pugl (Command macOS, Ctrl Windows/Linux).
- Command precedence: focused target -> nearest `CommandScope` -> outer scopes -> global UI handler.
- TextInput consumes command events instead of raw primary-modifier keydowns.

## T018 compact note

- Drag/drop is distinct from clipboard paste.
- `DropOffer` carries MIME types + logical position; `DropData` carries MIME + bytes + logical position.
- Components accept/reject through neutral InputContext methods; Pugl details stay inside `pugl_skia.cpp`.

## T019 compact note

- `Painter` owns a tracked Skia save/restore depth with protected component scopes.
- Every component paint is automatically isolated; leaked transforms are cleaned before siblings paint.
- `push_clip/pop_clip` use the same tracked state stack.
- `CanvasContext2D` coordinates are genuinely local: CanvasComponent establishes the bounds-origin translation before the callback, so scale/rotate/concat do not transform the Canvas layout position.
- APIs: `save`, `restore`, `translate`, `scale`, `rotate(radians)`, `concat(Transform2D)`.

## T024 compact note

- Golden format: versioned binary PPM (`P6`) under `tests/golden/baselines/`.
- Normal CTest is read-only with respect to baselines.
- Explicit update: `nativeui_golden_tests --update-goldens` or CMake target `nativeui_update_goldens`.
- Mismatch artifacts: `golden-artifacts/<name>.actual.ppm` and `.diff.ppm` with CI-readable stats.
- Cross-platform policy: compare deterministic geometry interiors; exclude font pixels and unstable anti-aliased edges unless a ticket explicitly establishes tolerance.
- Current golden cases: Canvas solid geometry, Row layout placement, Toggle geometry.

## Pugl reject-offer portability hotfix

- Pugl pinned at `b7637149...` declares `puglRejectOffer()` but only X11 defines it.
- Never call `puglRejectOffer()` directly from portable NativeUI code. Use `reject_pugl_drop_offer()` in `pugl_skia_setup.inc`.
- X11 uses explicit rejection; macOS/Windows rely on the native backend's unaccepted-offer rejection semantics.
- This rule exists because a real macOS arm64 link failed with undefined `_puglRejectOffer`.

## T041 compact note

- `nativeui_smoke_standalone` exercises PUGL_PROGRAM construction, native handle, non-blocking poll, resize, close and teardown.
- `nativeui_smoke_embedded` creates a real standalone parent then attaches a PUGL_MODULE child via the parent native handle; repeated child `poll()` calls are checked for non-blocking behavior.
- `StandaloneWindow::last_error()` and `EmbeddedView::last_error()` expose runtime Pugl/renderer failures after successful construction. Constructor failures still throw with the precise Pugl stage.
- CTest registration is opt-in with `-DNATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS=ON`; tests carry labels `integration;platform;smoke` and a 20-second timeout.
- Native smoke runtime was not executed in the artifact container because it has no desktop session; run it on macOS/Windows/Linux X11 before release.

## T026 compact note

- Public text styling/measurement lives in `include/nativeui/text.hpp`: `TextStyle`, `TextMetrics`, `TextService`, `TextAlign`, `FontWeight`.
- `Label` is the reusable single-line text widget; wrapping remains off/out of scope. `TextLabel` is an alias.
- `Painter::text()` consumes `TextStyle`; `Painter::measure_text()` delegates to `TextService`.
- `Header` now uses the same shared text style/measurement path without changing its intended regular-weight title appearance.
- Golden text policy remains cross-platform: exclude platform-shaped glyph interiors and compare deterministic scene geometry; measurement consistency is tested separately.


### Current validation gate (T026)

Real macOS/Skia baseline now reaches 39/40. `toggle_on` and the Label stripe probe pass, but the `label_scene` golden still differs. Inspect `build/golden-artifacts/label_scene.actual.ppm` and `.diff.ppm` against the versioned baseline; resolve the mismatch before advancing T026. No toolkit code or golden baseline was changed during Git preparation.
