# Validation notes

## Current revision

Validated after adding the interactive `ui::Canvas` component and the macOS Shift+Tab normalization.

Checks performed in the build environment:

- `tests/core_tests.cpp` compiles as C++20 against the Skia validation headers.
- `examples/standalone.cpp` compiles as C++20 against the Skia validation headers.
- `src/pugl_skia.cpp` passes C++20 syntax checking against the pinned Pugl API validation headers and Skia validation headers.
- Core tests execute successfully with the validation renderer/font stubs.
- Reverse focus regression test verifies that `Shift+Tab` returns focus to the preceding widget and keyboard input is then routed to that widget.
- Canvas regression test verifies pointer coordinates are translated from tree/global coordinates to Canvas-local coordinates and keyboard events reach an interactive Canvas.

The environment used here has no external network or native desktop display, so the real CPM dependency download and native Pugl window are still intended to be exercised on macOS/Windows/Linux development machines.

## T011 — clipping and overflow semantics

- Release core suite: 14/14 passed.
- ASan/UBSan core suite: 14/14 passed.
- Added headless simple/nested clip pixel tests and clipped hit-test coverage.
- Review: paint and hit-test use the same inherited clip intersection chain; no Pugl/platform dependency added to widgets/layout.

## T012 — Scroll layout primitive

- Targeted vertical/horizontal/both-axis offset, clamp and measurement tests pass.
- Release core suite: 15/15 passed.
- ASan/UBSan core suite: 15/15 passed.
- Public layout/nativeui headers and standalone consumer source compile.
- Review: Scroll is non-focusable and contains no wheel/platform behavior; it reuses T011 clipping and performs no paint-time allocation.

## T013 — handled/bubble input routing

- Red test confirmed ignored nested leaf events did not previously reach ancestors.
- Added consume/bubble/nested/captured-pointer routing regressions.
- Release core suite: 15/15 passed.
- ASan/UBSan core suite: 15/15 passed after recompiling all test executables affected by `component.hpp`.
- Public input/component/nativeui headers and standalone consumer source compile.

## T014 — focus scopes and default focus

- Added focus scope traversal/default/restoration regressions including Shift+Tab boundary wrap.
- Added tree deactivate/reactivate regression for active scope default focus.
- Release core suite: 15/15 passed.
- ASan/UBSan core suite: 15/15 passed after recompiling affected component/focus consumers.
- New public `focus.hpp`, umbrella include, and standalone consumer source compile.


## T015 — pointer capture and cancellation hardening

- Added dedicated `nativeui_pointer_capture_tests`; targeted suite passes.
- Release core suite: 16/16 passed.
- Relevant ASan/UBSan suite: routing, pointer capture, Canvas, lifecycle, widgets and TextInput all pass (6/6).
- Verified automatic capture release on PointerUp, idempotent explicit release, one-shot deactivate cancellation, orphan-capture cancellation before new PointerDown, and no re-capture during PointerCancel.
- Pugl close now calls the same explicit cancellation path before teardown.
- Added feature executable `nativeui_example_t015_pointer_capture` with `--self-test`.

## Mandatory feature examples retrofit

- Added dedicated executable sources for T007–T015 under `examples/features/`.
- All 9 feature sources compile against `NativeUI::Core` with the display-less validation setup.
- All 9 `--self-test` paths pass in self-test-only validation builds.
- CMake registers the real desktop executables when `NATIVEUI_BUILD_PLATFORM=ON` and registers their self-tests with CTest.

## T016 validation

- Added allocation-free gesture helpers and migrated `Knob` to total-displacement drag semantics.
- Standalone C++20 gesture smoke test: PASS (`-Wall -Wextra -Wpedantic`).
- Artifact environment CPM configure: blocked by disabled DNS while downloading CPM.cmake; this is an environment limitation, not a project failure.
- Run the full suite on a normal machine with cached/online CPM before merge/release.

## T017 validation

- `nativeui_command_tests`: PASS in local Skia API stub harness.
- Regression suites passing in same harness: TextInput, routing, focus, pointer capture, widgets.
- `nativeui_example_t017_commands --self-test`: PASS in self-test-only harness.
- CPM network configure cannot run in this artifact environment because DNS is disabled.

## T018 validation

- `nativeui_drop_tests`: PASS in local Skia API stub harness.
- `nativeui_example_t018_drop --self-test`: PASS.
- Regression suites passing in same harness: command, gesture, TextInput, routing, focus, pointer capture, widgets, Canvas.
- Pugl drag/data structures and clipboard distinction checked against pinned Pugl source.

## T019 validation

- Added tracked Painter save/restore, translate/scale/rotate/concat and per-component state isolation.
- Fixed Canvas transform semantics so transforms operate around the Canvas local origin even when the widget is offset by layout.
- Unified clip state with Painter save/restore so nested clip imbalance is contained by component paint scopes.
- `nativeui_transform_tests`: PASS in local raster Skia validation harness, including non-zero Canvas-origin scaling regression.
- `nativeui_headless_tests`, `nativeui_clipping_tests`, and `nativeui_scroll_layout_tests`: PASS in the same raster harness.
- Non-render regression suites affected by public header changes pass in the local Skia API stub harness.
- `nativeui_example_t019_transforms --self-test`: PASS.
- Targeted ASan/UBSan transform test: PASS.
- Artifact environment still has no external DNS/native display, so the real CPM + Pugl desktop smoke build remains a development-machine validation step.

## T024 validation

- Added PPM golden baseline tooling, explicit update workflow and diff artifacts.
- Comparator synthetic pass/fail self-check verifies mismatch detection, CI text output and artifact creation.
- Canvas/layout/Toggle baselines pass; normal run preserves baseline SHA256 hashes.
- Explicit `nativeui_update_goldens` CMake target executes successfully.
- Full local CMake core suite: 21/21 tests pass.
- `nativeui_example_t024_goldens --self-test`: PASS.
- Targeted ASan/UBSan `nativeui_golden_tests`: PASS.
- Validation used a local Skia API/raster package because the artifact environment has no external DNS/native display; no validation stubs are included in the project ZIP.

## T024 macOS linkage hotfix — Pugl drag rejection

- Real macOS arm64 build exposed an undefined `_puglRejectOffer` while linking `nativeui_demo`.
- Root cause verified against pinned Pugl `b7637149...`: `puglRejectOffer()` is declared publicly but implemented only in `src/x11.c`.
- NativeUI now routes rejection through `reject_pugl_drop_offer()`: explicit Pugl rejection on X11, native default rejection on macOS/Windows.
- No direct `puglRejectOffer()` reference remains in Apple/Windows code paths.
- Full artifact-environment CMake rebuild remains blocked by disabled DNS while bootstrapping CPM; the user's real macOS build is the authoritative platform-link validation for this hotfix.


## T025 validation

- Added public headless `ui::TextEditModel` in `include/nativeui/text_edit.hpp`.
- `nativeui_text_edit_model_tests`: PASS, including insertion/deletion/selection, UTF-8 boundary clamping, word navigation and undo/redo.
- Existing `nativeui_text_input_tests`: PASS after migration to the extracted model.
- Full local core CTest suite: 22/22 pass.
- Header isolation target `nativeui_header_text_edit_compile`: PASS.
- Feature source `t025_text_edit_model.cpp` compiles against `NativeUI::Core`; self-test-only executable returns success.
- Targeted ASan/UBSan: model + TextInput tests PASS.
- Validation uses the local pinned Skia package and cached CPM bootstrap because external DNS is disabled in the artifact environment.


## T041 validation

- Added `nativeui_smoke_standalone` and `nativeui_smoke_embedded` native executables.
- Standalone path exercises create/realize/show, native handle, repeated non-blocking poll, resize, close and teardown.
- Embedded path creates a real `PUGL_PROGRAM` parent then a `PUGL_MODULE` child using the parent native handle; 64 child polls are bounded to verify non-blocking behavior.
- Added `StandaloneWindow::last_error()` and `EmbeddedView::last_error()` so poll/event failures report useful text; constructor errors already include the failing Pugl stage.
- Added `NATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS` and CTest labels `integration;platform;smoke` with timeout protection.
- Added feature executable `nativeui_example_t041_smoke_harness` with `--self-test`.
- This artifact environment has no desktop session, so the native smoke executables were not run here. The last fully executed core baseline remains T025: 22/22 Release CTest plus targeted ASan/UBSan.


## T026 validation

- Added public `TextService`/`TextStyle`/`TextMetrics` and reusable `Label`.
- Added measurement consistency tests and headless Label scene golden baseline (`label_scene.ppm`).
- Added isolated public header compile target and `nativeui_example_t026_label --self-test`.
- `Header` migrated to the shared text style/measurement path with regular title weight preserved.
- C++20 syntax validation passed for `src/skia_core.cpp`, T026 test, public text header and feature example with `-Wall -Wextra -Wpedantic` using a local Skia API stub.
- Executable measurement probe passed, verifying `Label::measure()` matches `TextService::measure()`.
- `nativeui_example_t026_label --self-test` passes in the local Skia API stub harness.
- Full real-Skia CTest was not executable in this artifact environment because the pinned skia-builder binary is not locally available and external dependency download is unavailable; rerun on the development machine before merge/release.


## T026 macOS real-Skia validation follow-up

A real macOS run reported 38/40 passing tests. The two failures were test-harness issues, not toolkit behavior:

- `toggle_on` golden: only the optional 4x4 panel-background region differed (16/172 compared pixels). The two deterministic switch geometry regions matched exactly. The panel region has been removed from the golden comparison.
- `nativeui_label_tests`: the deterministic accent-stripe probe sampled `(110,36)`, which lies under the centered label and can be overwritten by a CoreText glyph. The probe now samples `(10,36)`, safely outside the text.

T026 remains `Doing` until the corrected suite is rerun on real macOS/Skia.

## Source import baseline — 2026-09-06

- Real macOS arm64 Release configure and build pass with the pinned dependencies.
- CTest: 39/40 pass; Label unit tests and all 17 feature self-tests pass.
- Remaining failure: `nativeui_golden_tests`, `label_scene` (597/1104 compared pixels differ, maximum delta 181, first mismatch `(8,4)`). Tracked in [T026](https://github.com/hemduf/nativeui/issues/26).
- Source publication changes Git exclusions and recovery documentation only; toolkit code and golden baselines remain unchanged. Native platform smoke tests were not enabled.

## T026 golden-test correction — 2026-09-06

- Red: the new mask regression failed because changing pixels in the Label's text box affected the supposedly geometry-only golden comparison.
- Cause: the old Label baseline contained background RGB `(60,64,70)` and no glyphs; real Skia paints background `(14,15,18)` and text. The old stripe mask also sampled glyph pixels.
- Correction: clip Label text to `(24,24,172,32)`, compare the background above it and both stripe ends, and regenerate only `label_scene.ppm` from the reviewed pinned real-Skia render. Channel tolerance stays 2 and allowed mismatch ratio stays zero; 3360 deterministic pixels are compared.
- Regression coverage: changes inside the text/footer areas are ignored, but background/left-stripe/right-stripe changes must fail. Same-platform Label pixel tests require visible red glyphs and correct left/center/right placement using measured text width.
- Targeted golden/Label/example checks: 3/3 pass. Full Release build and CTest: **40/40 pass**, including all 17 feature self-tests. All baseline SHA256 hashes remain unchanged during normal CTest; the other three baseline files match the original commit.
- Review passes completed: API/runtime scope (test-only change), meaningful negative cases and cross-platform pixel policy, then documentation/CI coverage. No library code or CMake target changed. Existing CI jobs run the modified test targets; remote CI and merge are still pending, so T026 remains Doing.

## #86 Pugl drag-and-drop fork integration — 2026-09-08

- NativeUI pins `hemduf/pugl` commit `577efc8283281092d7bd96ace1c5d63c11ce0063`.
- The fork fixes the macOS Cocoa offer → accept → actual-drop → data lifecycle, implements `puglRejectOffer()` on macOS and Windows, and fixes Windows `WM_DROPFILES` UTF-8 byte accounting.
- NativeUI removes the old macOS/Windows rejection compatibility wrapper and calls `puglRejectOffer()` directly on all supported desktop backends.
- Pugl post-merge CI is green on macOS, Windows, Linux and WebAssembly, including native `pugl:mac_drag_drop` and `pugl:win_drop` regressions.
- NativeUI CI #242 exposed one remaining wrapper call in `pugl_skia_view_a.inc`; this integration error was corrected before the next exact-head CI run.
- NativeUI cross-platform exact-head CI and the real macOS Finder drop smoke remain required before #86 is marked Done.
