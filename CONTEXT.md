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

The merged baseline is complete through T030 plus T023, #62, T053, #64 Decision B, T059, T047, T048, #86 and #107. T042 completes in the PR #93 merge after the final exact-head stress/CI gate.

- T053 / PR #88 merged as `ad83ed05f1687fea31255bcc77329fae0f4efb69`: `NativeUI::Core` and portable Pugl C remain generic; the Cocoa Pugl/OpenGL/IME bridge is compiled per final consumer. `cmake/NativeUIConsumerPlatform.cmake` derives `NUI_<fragment>_<digest12>_` from exact UTF-8 `CONSUMER_ID`, rejects duplicate target/identity registration at configure time and has no runtime registry.
- #64 / PR #90 merged as `b52d53eee65e5de91697e2c1f0685728b9646575`: independent overlapping `PUGL_PROGRAM` worlds are not a supported macOS multi-window ownership model. Legacy `StandaloneWindow(UI&, ...)` remains pre-v1/single-window; T060 owns one explicit `ui::Application` PROGRAM owner with multiple top-level windows.
- T059 / PR #89 merged as `6d84bc6b7817b0dcaea4eefd81833d765ad7685d`: the retained tree centrally resolves Visible/Hidden/Collapsed, inherited enabled/disabled and read-only state with focus/capture teardown, reentrancy handling and FocusScope restoration.
- T030 / PR #94 merged as `b32b09da473c70a857675e05f7fdc7c78e5f9361`: Button consumes the central T059 availability/focus/capture contract and is owned by the separate state/widget lane.
- T047 / PR #92 merged as `df569e874539aaafb8600960938465f733a46f19`: the installed/build-tree v1 low-level package exports only `NativeUI::Core` plus `nativeui_attach_platform(TARGET ... CONSUMER_ID ...)`, preserves T053 consumer-scoped macOS bridges, carries pinned Skia/Pugl implementation assets privately, validates relocation and missing-pinned-dependency failure, and does not export/document `NativeUI::NativeUI` as a complete package target.
- #97 / PR #98 merged as `864d2f35be1e7ae2976c14d2fac96ea4569f730f`: macOS Pugl MIN/MAX warning noise is removed from current main without changing the package/runtime contract.
- #105 / PR #106 merged as `b1dd7393b95adc4667224cd3da4cfb15dfc3f746`: constructor-time platform callbacks are owned by each private per-instance implementation, preventing callbacks from observing an unassigned public-wrapper `impl_`.
- #103 / PR #104 merged as `b6515836cd8bf571c66f5297a76fdb80d9924f83`: Linux/X11 Skia initialization uses the native GL interface under the Pugl-owned GLX context and the CI merge gate now includes a real Xvfb/Mesa llvmpipe renderer/lifecycle smoke.
- #86 / PR #87 merged as `0c4778278c86d2b8946ece593e684f4203ba38db`: reviewed Pugl drag/drop integration restores real Finder delivery, inactive-window drop routing and safe text preview/UTF-8 handling without extension-based acceptance.
- #107 / PR #108 merged as `e05ae703509a6971772b3bd5c53a3d7c71d7632a`: standalone `PUGL_SHOW_RAISE + PUGL_FAILURE` is treated as the documented non-fatal shown-but-not-raised case while genuine show errors remain fatal.

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

PR #99 is merged as `ed81a201ea459ea2443ae51f27dfcac7af5d7e63`; issue #48 is closed as Done. Its delivered fixtures and CI gates are preserved by the later #86/#107 integrations.

## Current DAG frontier

```text
lifecycle:        #64(done) -> #107(done) -> T042(this merge) -> T051 -> T052
platform/package: T053(done) -> T047(done) -> T048(done) -> T052
                                      |
                                      +-> T054
                                      +-> T056 -> T057
state/widgets:    T059(done) -> T030(done) -> T031
```

With T042 completing in PR #93, T051 becomes dependency-unblocked because T024 is already complete. T052 then remains blocked only by T051; its T047/T048/T042 prerequisites are satisfied. T054 and T056 remain Ready on the independent platform/package lane, T057 remains dependent on T056, and the state/widget lane remains independent.

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

Re-read T054/T056 priorities/dependencies and continue the highest-unblock-value Ready platform/package ticket; do not serialize it behind the lifecycle or state/widget lanes. T048 / #48 completion is already recorded on GitHub.

## Lifecycle/stress lane update — #105

T042's deterministic lifecycle matrix exposed a constructor-time platform callback lifetime defect while exercising a focusable embedded view on macOS. Issue #105 / PR #106 fixes the defect without changing the public window/view API: each private `StandaloneWindow::Impl` and `EmbeddedView::Impl` is now its own per-instance `PlatformServices` bridge, so synchronous native callbacks during `ViewCore` construction never re-enter a public wrapper whose `impl_` has not yet been assigned.

- Exact PR #106 code head `bf8e2ff055b4fe5ed2c2bd415bb3818f925f366b` passed CI `34344755454` on Linux X11, Windows/MSVC, macOS including two-consumer Objective-C isolation and clipboard/multi-instance lifecycle smoke, and Linux ASan+UBSan.
- The stacked T042 validation head `97c2c19f09c60c537d538dc887660a0049fa35db` proves the original macOS `embedded_sequential_100` crash is GREEN; Linux X11 and Linux ASan+UBSan are also green.
- Windows reaches the standalone stress path and exposes a separate documented `puglShow(PUGL_SHOW_RAISE)` non-fatal status handling defect, tracked independently as #107. That defect is not folded into #105.
- #64 Decision B is unchanged: there is still no hidden application singleton, shared PROGRAM workaround, or mutable global/`thread_local` ownership state.

The #105 and #107 blockers are now merged; T042's final stress-only completion is recorded below.

## Lifecycle/stress lane update — #103

T042 also exposed a production Linux/X11 renderer crash before stress-specific lifecycle behavior: the first native exposure under Xvfb/Mesa llvmpipe entered Skia's assembled GL extension discovery and crashed in `GrGLExtensions::init`. Issue #103 / PR #104 keeps the fix in the platform renderer rather than weakening T042.

- Linux now creates Skia's desktop-native GL interface from the Pugl-owned current GLX context. There is deliberately no Linux fallback to the assembled resolver that produced the crash; an unavailable interface returns through the existing controlled renderer-initialization failure path.
- macOS and Windows retain the existing assembled Pugl-proc interface path.
- CI `34343547392` passed the #103 code on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan. The Linux lane now includes a permanent Xvfb/Mesa llvmpipe real renderer/lifecycle smoke and passes it.
- A stacked T042 candidate reaches and completes Linux native lifecycle stress after this fix, confirming the stress harness is no longer stopped by first-exposure renderer initialization.
- No mutable GL/context global, singleton or `thread_local` state is introduced; #64 Decision B is unaffected.

The #103 and #107 production defects exposed by T042 are now resolved independently; PR #93 contains only the stress harness and workflow.

## Lifecycle/stress lane completion — #107 / PR #108

T042's Windows `standalone_sequential_50` fixture exposed a production integration bug rather than a stress-harness defect: pinned Pugl documents `PUGL_FAILURE` from `puglShow(..., PUGL_SHOW_RAISE)` as the non-fatal case where the view is shown but could not be raised. NativeUI previously treated every non-zero show status as fatal and destroyed that already-live standalone view during construction.

PR #108 introduces one translation-unit-private show-status policy. It normalizes only `PUGL_SHOW_RAISE + PUGL_FAILURE` to success; every other Pugl status remains unchanged and therefore follows the existing cleanup/throw path. Embedded `PUGL_SHOW_PASSIVE` is unchanged. The correction introduces no mutable global/singleton/registry/`thread_local` state and does not alter #64 Decision B.

- RED evidence: T042 stacked workflow `34344854224` failed Windows `standalone_sequential_50` at cycle 0 with `puglShow failed: Non-fatal failure` while the headless and embedded stress fixtures passed.
- GREEN evidence: exact code head `04dc15c3f8aff444f3cc29b377966c2a97ed25b5` passed NativeUI CI `34348272543`; the identical show-policy blob was also validated in the stacked T042 head.
- Full lifecycle evidence: T042 workflow `34348551067` is green on Windows, Linux/X11, macOS and Linux ASan+UBSan, with all 50 Windows process-isolated standalone lifetimes completing.
- Completion head `5735dc1ce5db64c28d7a18da1c2b314191177a95` passed exact-head CI `34359846143` on Linux X11, Windows, macOS and Linux ASan+UBSan.
- Mandatory CODE_REVIEW.md review `5154415827` found no blocking issue.

PR #108 is squash-merged as `e05ae703509a6971772b3bd5c53a3d7c71d7632a`; #107 is closed as Done.

## Lifecycle/stress lane completion — T042 / PR #93

After #108 merged, PR #93 was rebuilt as a true current-main merge candidate with the production show-status fix removed from its diff. The synchronized code head `27ec2411000c98060d52108db3436a0ab8004b90` changes only the five T042 stress/workflow files.

T042 provides deterministic `headless_tree_lifecycle_1000`, `embedded_sequential_100`, `embedded_two_live_50`, `embedded_capture_focus_teardown`, `standalone_sequential_50` and explicit `standalone_supported_multi_instance` Decision-B coverage. A+B fixtures prove destroy-A/continue-B isolation for state, focus, capture, invalidation and clipboard bookkeeping; weak sentinels reject stale callbacks. Standalone cycles are process-isolated and bounded, and simultaneous legacy standalone ownership remains explicitly deferred to T060.

Mandatory CODE_REVIEW.md review `5155464378` on the synchronized stress-only diff found no Blocking/Important issue: no mutable process-global/singleton/`thread_local` state, no production thread/RT change, no Objective-C runtime expansion and no hidden application-owner workaround. The completion documentation is part of the same PR; a fresh exact-head CI plus T042 lifecycle matrix is the final merge gate.

Once that final exact head is green and PR #93 merges, issue #42 closes as Done and T051 becomes Ready.

## Drag-and-drop completion — #86 / PR #87

Finder drops exposed a second integration defect after the Pugl backend-child destination fix: NativeUI discarded DropOffer/DropData whenever the window lost keyboard focus. The retained tree now routes drops while mounted but inactive, without reactivating focus or IME, preserving T059 availability and dispatch reconciliation. T018 now receives the first local regular file regardless of extension. Optional UTF-8 text preview is separate from successful file reception; a binary file shows its name and no preview. The read cap is 64 KiB plus three lookahead bytes; the displayed prefix is at most 120 complete-character bytes. Diagnostics log acceptance, drop size and preview size, never content.

The image-drop crash path was traced to malformed bytes reaching Skia text-to-glyph conversion. The shared text layout now owns repaired UTF-8 only when needed. The subsequent `.txt` allow-list/duplicate example decoder were removed; `ui::text::utf8_prefix` reuses the rendering decoder, and no filename or extension decides whether a drop succeeds.

- TDD: the background-drop core test failed before the fix and passes after it; the malformed/NUL file-URI self-test likewise failed before hardening.
- Main `ed81a201ea459ea2443ae51f27dfcac7af5d7e63` (merged T048 / PR #99) is integrated, preserving external-consumer fixtures/CI. The macOS drop fixture uses T053 consumer identity `org.nativeui.test.macos-drop`, not a manual runtime prefix; native coverage includes two embedded instances, sibling destruction and repeated cycles.
- Image-crash TDD: four synthetic JPEG bytes reproduced SIGABRT (exit 134) in `nativeui_font_tests`, and T018's binary rejection test failed before correction. Headless pixel equivalence, malformed UTF-8 classes, byte-boundary truncation and image-to-text recovery now pass.
- Cleanup TDD: a valid-text `.jpg` fixture failed under the allow-list, then passed after removal. The same contents now pass as `.txt`, `.md`, `.jpg` and extensionless files. Shared UTF-8 prefix checks cover all 256 single bytes, malformed sequences, borrowed lifetime and byte-limit boundaries. Local Release CTest is 63/63 PASS and targeted font/drop/example/native ASan+UBSan is 4/4 PASS, with no compiler warnings.
- Cleanup head `696521fedda8488dd72f036c7c7c06c5d4812fbd` passed all four jobs in CI `34355603379`; final merge evidence is recorded in PR #87.

PR #87 is merged as `0c4778278c86d2b8946ece593e684f4203ba38db` and #86 is Done. This changes neither the T042 ownership contract nor the independent platform/package frontier.