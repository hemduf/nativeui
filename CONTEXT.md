# NativeUI compact recovery context

**Updated:** 2026-09-09

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio, host parameter semantics and a custom native windowing stack remain out of scope.

Non-negotiable rules: widgets/layout stay platform-neutral; public geometry is logical pixels; embedded polling is non-blocking; mutable instance-dependent process globals/singletons/thread-locals are forbidden; retained UI state is UI/main-thread confined; dependencies use CMake+CPM; Skia comes from pinned skia-builder binaries; macOS Objective-C runtime-visible platform classes must be consumer-specific.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `d12d63815b8cfe3f36293d3791a418e8f558ff1b`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Merged baseline

The merged baseline is complete through T030 plus T023, #62, T053, #64 Decision B, T059, T047 and T048.

- T053 / PR #88 merged as `ad83ed05f1687fea31255bcc77329fae0f4efb69`: `NativeUI::Core` and portable Pugl C remain generic; the Cocoa Pugl/OpenGL/IME bridge is compiled per final consumer. `cmake/NativeUIConsumerPlatform.cmake` derives `NUI_<fragment>_<digest12>_` from exact UTF-8 `CONSUMER_ID`, rejects duplicate target/identity registration at configure time and has no runtime registry.
- #64 / PR #90 merged as `b52d53eee65e5de91697e2c1f0685728b9646575`: independent overlapping `PUGL_PROGRAM` worlds are not a supported macOS multi-window ownership model. Legacy `StandaloneWindow(UI&, ...)` remains pre-v1/single-window; T060 owns one explicit `ui::Application` PROGRAM owner with multiple top-level windows.
- T059 / PR #89 merged as `6d84bc6b7817b0dcaea4eefd81833d765ad7685d`: the retained tree centrally resolves Visible/Hidden/Collapsed, inherited enabled/disabled and read-only state with focus/capture teardown, reentrancy handling and FocusScope restoration.
- T030 / PR #94 merged as `b32b09da473c70a857675e05f7fdc7c78e5f9361`: Button consumes the central T059 availability/focus/capture contract and is owned by the separate state/widget lane.
- T047 / PR #92 merged as `df569e874539aaafb8600960938465f733a46f19`: the installed/build-tree v1 low-level package exports only `NativeUI::Core` plus `nativeui_attach_platform(TARGET ... CONSUMER_ID ...)`, preserves T053 consumer-scoped macOS bridges, carries pinned Skia/Pugl implementation assets privately, validates relocation and missing-pinned-dependency failure, and does not export/document `NativeUI::NativeUI` as a complete package target.
- #97 / PR #98 merged as `864d2f35be1e7ae2976c14d2fac96ea4569f730f`: macOS Pugl MIN/MAX warning noise is removed from current main without changing the package/runtime contract.
- #105 / PR #106 merged as `b1dd7393b95adc4667224cd3da4cfb15dfc3f746`: constructor-time platform callbacks are owned by each private per-instance implementation, preventing callbacks from observing an unassigned public-wrapper `impl_`.
- #103 / PR #104 merged as `b6515836cd8bf571c66f5297a76fdb80d9924f83`: Linux/X11 Skia initialization uses the native GL interface under the Pugl-owned GLX context and the CI merge gate now includes a real Xvfb/Mesa llvmpipe renderer/lifecycle smoke.

## T048 external relocated consumers — PR #99

T048 validates the installed low-level package from genuinely independent out-of-tree consumer projects.

Delivered fixtures and gates:

- `core`: `find_package(NativeUI CONFIG REQUIRED)` + `NativeUI::Core` only, with deterministic public headless rendering;
- `standalone`: independent final executable using `NativeUI::Core` + `nativeui_attach_platform()` and stable reverse-DNS identity;
- `embedded`: SDK-neutral final executable using the same public package surface, with a tiny native parent seam for real lifecycle smoke;
- install to prefix A, copy to prefix B, delete A, then configure/build/run against B only;
- static source-contract checks reject repository-private includes/targets/override variables;
- every native fixture links its real attached platform symbols and runs deterministic relocated-Core `--self-test` on Linux/Windows/macOS;
- macOS additionally executes the real standalone/embedded `--native-smoke` lifecycle and the T053/T047 two-consumer Objective-C class/metaclass/runtime coexistence proof;
- hosted Windows lacks a reliable interactive desktop and Linux/Xvfb native external-window execution is not used as package acceptance, while the permanent root Linux renderer/lifecycle smoke still exercises the supported X11/GLX path.

TDD/review history:

- RED `af34ed1c65409e054ba6c976d82cbdeff7234347` registered the source contract before fixtures existed and failed as intended;
- GREEN/correction cycles added relocation, all three consumer shapes, platform-link validation, graphical-session separation and stronger portable UI rendering self-tests;
- final pre-refresh code/review correction head `9904e2fe8f252a35e060bda128a82d18b9f5819a` passed CI #384 on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan, including the T048 source contract, relocated consumers and macOS native/runtime-isolation gates;
- completion-doc head `841318d92c389c60ec48982cb0e7d002d56bccdf` passed CI #394 across the same required matrix;
- CODE_REVIEW.md review is clean after correcting the earlier weak self-test and obsolete Linux diagnostic residue; no runtime globals, private package paths or Objective-C naming shortcuts were introduced;
- before merge, PR #99 was refreshed onto current main `b6515836cd8bf571c66f5297a76fdb80d9924f83`, preserving #105 constructor-lifetime fixes, #103 Linux renderer integration and the permanent Linux X11 renderer smoke while retaining all T048 package gates.

PR #99 is merged as `ed81a201ea459ea2443ae51f27dfcac7af5d7e63`; issue #48 is closed as Done. Its delivered fixtures and CI gates are preserved by the #86 integration below.

## Current DAG frontier

```text
lifecycle:        #64(done) -> T042 -> T051 -> T052
platform/package: T053(done) -> T047(done) -> T048(done) -> T052
                                      |
                                      +-> T054
                                      +-> T056 -> T057
state/widgets:    T059(done) -> T030(done) -> T031
```

With T048 merged, T054 and T056 are the dependency-unblocked platform/package items. T052 has its T048 dependency satisfied but still requires T051 and T042. T057 remains dependent on T056. This lane must not take T059, T030, #64 or T042.

## Platform ownership reminders

- Normal standalone validation currently uses one `PUGL_PROGRAM` owner lifetime.
- Multiple independent `EmbeddedView` / `PUGL_MODULE` instances are supported and remain the current host/plugin coexistence path.
- T060 owns the future shared `Application` multi-window standalone model.
- No global/manual `NATIVEUI_OBJC_RUNTIME_PREFIX` is part of the normal package contract.

## Build / validation

Source-tree Release:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Offline dependency overrides:

```bash
cmake -S . -B build \
  -DNATIVEUI_PUGL_SOURCE=/path/to/pugl \
  -DNATIVEUI_SKIA_ROOT=/path/to/extracted/skia-builder
```

T048 additionally runs the source-contract script, relocated install-tree Core/standalone/embedded consumers, platform attachment/link tests, macOS native lifecycle and macOS two-consumer Objective-C namespace/runtime coexistence. Current Linux CI also retains the #103 Xvfb/Mesa llvmpipe renderer/lifecycle smoke.

## Next platform/package action

Re-read T054/T056 priorities/dependencies and continue the highest-unblock-value Ready platform/package ticket; do not enter the lifecycle or state/widget lanes. T048 / #48 completion is already recorded on GitHub.

## Lifecycle/stress lane update — #105

T042's deterministic lifecycle matrix exposed a constructor-time platform callback lifetime defect while exercising a focusable embedded view on macOS. Issue #105 / PR #106 fixes the defect without changing the public window/view API: each private `StandaloneWindow::Impl` and `EmbeddedView::Impl` is now its own per-instance `PlatformServices` bridge, so synchronous native callbacks during `ViewCore` construction never re-enter a public wrapper whose `impl_` has not yet been assigned.

- Exact PR #106 code head `bf8e2ff055b4fe5ed2c2bd415bb3818f925f366b` passed CI `34344755454` on Linux X11, Windows/MSVC, macOS including two-consumer Objective-C isolation and clipboard/multi-instance lifecycle smoke, and Linux ASan+UBSan.
- The stacked T042 validation head `97c2c19f09c60c537d538dc887660a0049fa35db` proves the original macOS `embedded_sequential_100` crash is GREEN; Linux X11 and Linux ASan+UBSan are also green.
- Windows reaches the standalone stress path and exposes a separate documented `puglShow(PUGL_SHOW_RAISE)` non-fatal status handling defect, tracked independently as #107. That defect is not folded into #105.
- #64 Decision B is unchanged: there is still no hidden application singleton, shared PROGRAM workaround, or mutable global/`thread_local` ownership state.

After #105 and the independent #107 status fix are merged, refresh T042/PR #93 from current `main`, rerun the exact-head lifecycle matrix on macOS/Windows/Linux X11 plus Linux ASan+UBSan, complete the mandatory review, then merge T042 only if all supported-path acceptance gates are green.

## Lifecycle/stress lane update — #103

T042 also exposed a production Linux/X11 renderer crash before stress-specific lifecycle behavior: the first native exposure under Xvfb/Mesa llvmpipe entered Skia's assembled GL extension discovery and crashed in `GrGLExtensions::init`. Issue #103 / PR #104 keeps the fix in the platform renderer rather than weakening T042.

- Linux now creates Skia's desktop-native GL interface from the Pugl-owned current GLX context. There is deliberately no Linux fallback to the assembled resolver that produced the crash; an unavailable interface returns through the existing controlled renderer-initialization failure path.
- macOS and Windows retain the existing assembled Pugl-proc interface path.
- CI `34343547392` passed the #103 code on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan. The Linux lane now includes a permanent Xvfb/Mesa llvmpipe real renderer/lifecycle smoke and passes it.
- A stacked T042 candidate reaches and completes Linux native lifecycle stress after this fix, confirming the stress harness is no longer stopped by first-exposure renderer initialization.
- No mutable GL/context global, singleton or `thread_local` state is introduced; #64 Decision B is unaffected.

After #103 merges, T042 still requires the independent #107 Windows show-status correction plus a fresh exact-head supported-path stress matrix before completion.

## Drag-and-drop completion — #86 / PR #87

Finder drops exposed a second integration defect after the Pugl backend-child destination fix: NativeUI discarded DropOffer/DropData whenever the window lost keyboard focus. The retained tree now routes drops while mounted but inactive, without reactivating focus or IME, preserving T059 availability and dispatch reconciliation. T018 now receives the first local regular file regardless of extension. Optional UTF-8 text preview is separate from successful file reception; a binary file shows its name and no preview. The read cap is 64 KiB plus three lookahead bytes; the displayed prefix is at most 120 complete-character bytes. Diagnostics log acceptance, drop size and preview size, never content.

The user's image drop reached painting but crashed in macOS Skia text-to-glyph conversion: fallback decoded U+FFFD but still measured/rendered the original invalid bytes. The shared text layout now owns repaired UTF-8 only when needed. The subsequent `.txt` allow-list/duplicate example decoder were rejected by the user and removed; `ui::text::utf8_prefix` reuses the rendering decoder, and no filename or extension decides whether a drop succeeds.

- TDD: the background-drop core test failed before the fix and passes after it; the malformed/NUL file-URI self-test likewise failed before hardening.
- Main `ed81a201ea459ea2443ae51f27dfcac7af5d7e63` (merged T048 / PR #99) is integrated, preserving external-consumer fixtures/CI. The macOS drop fixture uses T053 consumer identity `org.nativeui.test.macos-drop`, not a manual runtime prefix; native coverage includes two embedded instances, sibling destruction and repeated cycles.
- Image-crash TDD: four synthetic JPEG bytes reproduced SIGABRT (exit 134) in `nativeui_font_tests`, and T018's binary rejection test failed before correction. Headless pixel equivalence, malformed UTF-8 classes, byte-boundary truncation and image-to-text recovery now pass.
- Cleanup TDD: a valid-text `.jpg` fixture failed under the allow-list, then passed after removal. The same contents now pass as `.txt`, `.md`, `.jpg` and extensionless files. Shared UTF-8 prefix checks cover all 256 single bytes, malformed sequences, borrowed lifetime and byte-limit boundaries. Local Release CTest is 63/63 PASS and targeted font/drop/example/native ASan+UBSan is 4/4 PASS, with no compiler warnings.
- Cleanup head `696521fedda8488dd72f036c7c7c06c5d4812fbd` passed all four jobs in CI `34355603379`. The earlier Windows T047 timeout did not recur; no check was weakened. The exact main-integrated merge head must also pass the full matrix; final evidence is recorded in PR #87.
- On 2026-09-09 the user explicitly confirmed real Finder `/tmp/hello.txt` content display, including after an image drop, and requested merging #87. This is user-reported interactive evidence, not a claim that direct Cocoa callbacks reproduce Finder's drag session.

This merge completes #86 after the exact-head CI gate; no blocking code finding remains in the three-pass/CODE_REVIEW.md record in #87. The last independent completion is T048 / PR #99 (`ed81a20`). Next recommended platform/package work is the highest-priority Ready item among T054/T056 after checking current GitHub status; #86 changes neither that frontier nor milestone completion.

## State/widget lane update — T031 completion candidate

T031 / PR #95 is the active state/widget completion candidate after merged T030. Its reviewed implementation provides `Checkbox`, typed `RadioGroup<T>` / `RadioButton<T>`, one-Tab-stop radio groups, wrapped arrow navigation that skips unavailable options, T059 availability/read-only behavior, reentrancy-safe activation, and per-group weak duplicate-live-value bookkeeping with no global/singleton/`thread_local` state.

After current `main` advanced with #86 / PR #87, dependency synchronization PR #113 was conflict-resolved by preserving the complete current-main drag/drop, package and lifecycle state while overlaying only the T031 code/tests/CMake registrations. The synchronized merge head is `12e5945f6e50d9cb1250f08aa9a6c9ea9927c2fa`; current main `0c4778278c86d2b8946ece593e684f4203ba38db` is now an ancestor of the T031 branch.

The prior mandatory review is clean after the duplicate-live-value RED→GREEN correction. This completion-document update is part of the final T031 merge cycle; the resulting exact head must pass Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan before PR #95 is merged.
