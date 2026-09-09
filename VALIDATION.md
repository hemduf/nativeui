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
- Public layout/nativeui headers and standalone consumer source compile with the new API.
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

- NativeUI pins reachable `hemduf/pugl` commit `0187a800276776c50a4d4890b3e6de7a100ff876`.
- The fork fixes the macOS Cocoa offer → accept → actual-drop → data lifecycle and implements `puglRejectOffer()` on macOS and Windows.
- NativeUI removes the old macOS/Windows rejection compatibility wrapper, calls `puglRejectOffer()` directly on all supported desktop backends and enables `PUGL_ACCEPT_DROP` before realization.
- Review found that the first Win32 fork pin still bypassed the portable offer contract by dispatching `PUGL_DATA` directly from `WM_DROPFILES`. Pugl #34/#35 corrected this in TDD order: the test-only head failed `pugl:win_drop` on Windows while the other platforms stayed green, then the implementation restored `PUGL_DATA_OFFER` → accept/reject → `PUGL_DATA` before payload exposure.
- The reviewed Win32 correction keeps actual drop coordinates, suppresses rejected data, avoids re-entrant data delivery from `puglAcceptOffer()`, preserves multi-file/non-ASCII UTF-8 payload accounting, resets decision state per view, releases `HDROP` on all paths and frees owned drop/clipboard buffers at view teardown.
- The associated generic blob clear/allocation-failure paths were hardened so teardown cannot encounter stale ownership.
- Pugl tested content is green on Windows, Linux, macOS and WebAssembly. Repository automation sanitation rewrote commit identity while retaining the exact tree; the reachable sanitized `main` commit is the pin above.
- NativeUI CI #242 exposed one remaining old wrapper call during the first integration pass; it was corrected. CI #247 on the previous reviewed Pugl pin was green on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan, including the macOS Objective-C isolation and clipboard/multi-instance smokes.
- NativeUI CI #264 failed only during dependency checkout because it temporarily pinned the pre-sanitation merge SHA that was no longer reachable from a shallow clone. The branch has been repinned to reachable `0187a800...`; a fresh exact-head NativeUI matrix is required.
- The real macOS Finder → T018 drop smoke remains the final manual gate before #86 is marked Done.

## #86 background-window drop correction — 2026-09-09

- Pin: reachable `hemduf/pugl` `d12d63815b8cfe3f36293d3791a418e8f558ff1b`, including backend-child drag-destination forwarding. Main `b651583` is integrated without reverting T053 consumer-scoped bridges, T059 component availability or the IME/lifetime fixes.
- A further defect explained the user's no-op Finder drop: `Tree::dispatch` discarded all events after window deactivation. Drops now remain routable while mounted but inactive, without reactivating keyboard focus or IME. Normal input, clipping, disabled/hidden/collapsed targets and unmount guards remain covered.
- TDD RED: the new post-deactivation `nativeui_drop_tests` assertion failed before the routing fix. The T018 self-test also failed before file-content loading, and malformed/NUL file-URI coverage failed before decoder hardening. All targeted checks are GREEN after correction.
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` (legacy global runtime-prefix cache removed after T053), `cmake --build build -j 6`, and `ctest --test-dir build --output-on-failure`: **63/63 PASS** on local macOS. The build emitted no warnings.
- New `nativeui_smoke_macos_drop`: real OpenGL child destination, native focus loss, MIME and UTF-8 multi-file URLs, logical coordinates, no premature delivery, exactly one data event, rejection, cancellation/recovery, two embedded instances, sibling destruction and repeated create/drop/destroy. Registered in CTest and therefore the existing CI `macOS` job.
- Debug ASan+UBSan `nativeui_drop_tests` and `nativeui_smoke_macos_drop`: **2/2 PASS** using the same pinned local dependencies. No new cross-thread state; TSan is not applicable.
- `cmake -DSOURCE_DIR=<source> -DOUTER_BUILD=<build> -P tests/check_objc_two_consumers.cmake`: PASS (two final modules coexist). `nm` confirms the fixture and all four Pugl classes use `NUI_org_nativeui_test_macos_ffe172d0ef6b_`, derived from its unique T053 consumer identity; no generic runtime class was introduced.
- Existing CI also runs the core and T018 tests on Windows/MSVC and Linux X11, plus core ASan+UBSan on Linux. No workflow permissions or required checks were changed. Exact pushed-head CI results must still be recorded in PR #87.
- **Manual acceptance is not claimed:** direct Cocoa destination callbacks are not a real Finder drag session. `/tmp/hello.txt` was left untouched; the corrected `--trace-drops` example is available for Finder → panel → `Loaded hello.txt` plus visible contents. Keep #87 draft until this succeeds and CI is green.

## #86 initial image-drop crash correction — 2026-09-09 (f5d29fc)

- The user-provided macOS stack identifies `SkTypeface_Mac::onCharsToGlyphs` / `SkFont::measureText` below `resolve_text_layout` and Canvas painting. Synthetic JPEG header bytes reproduce the defect without retaining a user's image or metadata: decoded replacement scalars still left invalid original UTF-8 bytes in Skia's input.
- Baseline Release configure/build and CTest: **63/63 PASS** before this correction. TDD RED: `./build/nativeui_font_tests` aborts with exit 134 on four synthetic JPEG bytes; `./build/nativeui_example_t018_drop --self-test` exits 1 because binary `.txt` bytes were accepted. Both are GREEN after correction.
- `nativeui_font_tests` covers eight malformed UTF-8 cases (JPEG bytes, isolated continuations, overlong encodings, surrogates, out-of-range scalars and incomplete tails), finite/equivalent measurement, same-platform headless pixel equivalence to valid U+FFFD text, independent layout ownership and the valid-input no-repair path. Existing goldens are unchanged.
- The T018 self-test checks image and disguised binary `.txt` rejection, control-byte rejection, valid accented/emoji text, safe 120-byte/64 KiB cut points, error-state painting and recovery to valid file/plain-text drops. The synchronous file policy is example-only; unsupported image decoding is not a new feature. Diagnostic logging contains only acceptance and byte count.
- Release `cmake --build build -j 6` and `ctest --test-dir build --output-on-failure`: **63/63 PASS**, no compiler warnings. Debug ASan+UBSan targeted CTest regex `^(nativeui_font_tests|nativeui_drop_tests|nativeui_smoke_macos_drop|nativeui_example_t018_drop_self_test)$`: **4/4 PASS**. No new cross-thread state; TSan is not applicable.
- Existing `.github/workflows/ci.yml` jobs `macOS`, `Windows` and `Linux X11` run font/drop/T018 checks; `Linux ASan + UBSan` runs the core/font tests and compiles feature sources. The Cocoa drop test remains in the macOS CTest suite. Prior integrated head `caaf4c0` passed all four jobs; the image-crash corrective head needs its own CI result.
- The corrected diagnostic window is relaunched. Real Finder → image → text visual acceptance is still unconfirmed; automated callback/headless tests are not represented as a real drag gesture. PR #87 remains draft and #86 remains Doing.

## #86 extension-independent cleanup — 2026-09-09

- The user's review supersedes the preceding `.txt` rejection policy: extension-based reception was arbitrary, and the example duplicated the renderer's UTF-8 decoder. Neither is needed to fix the Skia crash.
- Baseline configure/build and local CTest: **63/63 PASS**. TDD RED: the T018 self-test failed with `text preview depends on a hardcoded filename extension` for valid text named `image.jpg`; GREEN after removing the allow-list. Identical contents are now checked under `.txt`, `.md`, `.jpg` and extensionless names. Binary content is received normally, with no text preview or stale preview data, followed by successful text recovery.
- `ui::text::utf8_prefix` reuses the existing decoder used by measurement/painting. Font tests cover invalid classes, all 256 single-byte values, every byte budget through mixed ASCII/accent/emoji text, uninspected suffix behavior, empty/NUL input, and borrowed-buffer identity. Existing headless malformed-text pixel-equivalence tests remain unchanged; the isolated text-header consumer compiles the helper.
- File reading remains bounded to 64 KiB plus three UTF-8 lookahead bytes. T018 checks all three ways a four-byte scalar can cross that limit, plus the 120-byte display boundary. No additional renderer allocation is introduced on valid text and no paint-time preview validation is needed.
- Final local `cmake --build build -j 6` and `ctest --test-dir build --output-on-failure`: **63/63 PASS**, no compiler warnings. Debug ASan+UBSan CTest regex `^(nativeui_font_tests|nativeui_drop_tests|nativeui_smoke_macos_drop|nativeui_example_t018_drop_self_test)$`: **4/4 PASS**. Whitespace checks pass; no golden baseline or unrelated assertion was changed.
- CI on prior head `f5d29fc` passed macOS, Linux X11 and Linux ASan+UBSan. Windows passed font/drop/T018 but hit the unchanged `nativeui_t047_attach_contract_tests` 30-second timeout in run `34353056094`; the matrix is not fully green. No timeout, assertion or workflow gate is weakened for this cleanup. Fresh corrective-head CI remains required.
- The existing three-platform CTest and Linux sanitizer jobs already cover the modified targets. No Pugl/Objective-C, dependency, workflow permission or native drag contract changes are introduced by this cleanup. Real Finder visual acceptance remains outstanding and is not replaced by the synthetic/headless tests.

## #86 merge acceptance — 2026-09-09

- The user explicitly confirmed the real Finder path: `/tmp/hello.txt` displays its contents in the UI, including after an image drop, and requested merging PR #87. This user-reported manual result closes the previously outstanding visual gate; the automated Cocoa callback test is still not described as a real Finder gesture.
- Exact cleanup head `696521fedda8488dd72f036c7c7c06c5d4812fbd` passed [CI 34355603379](https://github.com/hemduf/nativeui/actions/runs/34355603379) on macOS, Windows/MSVC, Linux X11 and Linux ASan+UBSan. The earlier Windows T047 timeout did not recur, with no timeout/assertion changes.
- Main `ed81a201ea459ea2443ae51f27dfcac7af5d7e63` is integrated with all T048 fixtures and CI gates preserved. The sole merge conflict was the roadmap execution snapshot; no production-code conflict resolution was needed. The exact final-head CI result, including relocated consumers, is recorded in PR #87 before merge.
- Native macOS smoke tests require access to the graphical session. A sandboxed baseline run passed 62/63 but could not create the Cocoa window (non-finite frame before drop dispatch); the identical native test passed immediately with authorized graphical-session access. Run the full platform suite in that environment, not a display-isolated sandbox.
- Main-integrated local Release build and full CTest in the graphical session: **63/63 PASS**, no compiler warnings. `nativeui_smoke_standalone` and the T048 external-consumer source contract also pass.

## T052 Pugl X11 empty-clipboard pin — 2026-09-09

- Current release-candidate Pugl pin: reachable `hemduf/pugl` `195f79b22644010c81a5e0c3231c591856787ec6`.
- The pin adds the reviewed X11 failed-selection guard: `SelectionNotify.property == None` is treated as a failed clipboard conversion instead of forwarding atom `None` to `XGetWindowProperty()`. The shared dependency integration remains independently tracked by #124 / PR #125.
- T052 RED evidence on head `66d741f944cd1fd85654fb0ac19b5c1b212147ce`, run `34399849848`: the only failed T052 job was `T052 release contract`, because release/dependency documentation still named the previous Pugl pin. The same head passed all three T052 clean-bootstrap jobs and its T051 comparative benchmark; normal CI `34399849892`, T042 lifecycle stress `34399850030` and T051 Release Benchmarks `34399849916` were also green.
- The release notes, third-party inventory, compact context and this validation record are synchronized to the new pin before the next exact-head qualification run. No runtime behavior, threshold, timeout, fixture or #64 Decision-B ownership rule is weakened by this documentation correction.
- The next exact source head must rerun T052, normal CI, T042 lifecycle stress and T051 Release Benchmarks before merge; prior green results remain diagnostic evidence only.
