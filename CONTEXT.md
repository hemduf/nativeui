# NativeUI compact recovery context

**Updated:** 2026-09-09

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio, host parameter semantics and a custom native windowing stack remain out of scope.

Non-negotiable rules: widgets/layout stay platform-neutral; public geometry is logical pixels; embedded polling is non-blocking; mutable instance-dependent process globals/singletons/thread-locals are forbidden; retained UI state is UI/main-thread confined; dependencies use CMake+CPM; Skia comes from pinned skia-builder binaries; macOS Objective-C runtime-visible platform classes must be consumer-specific.

## Pinned dependencies

- Pugl: `lv2/pugl` commit `b7637149ebe53124e5be90559e02a0185bbcbd73`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Merged baseline

The merged baseline is complete through T030 plus T023, #62, T053, #64 Decision B, T059 and T047.

- T053 / PR #88 merged as `ad83ed05f1687fea31255bcc77329fae0f4efb69`: `NativeUI::Core` and portable Pugl C remain generic; the Cocoa Pugl/OpenGL/IME bridge is compiled per final consumer. `cmake/NativeUIConsumerPlatform.cmake` derives `NUI_<fragment>_<digest12>_` from exact UTF-8 `CONSUMER_ID`, rejects duplicate target/identity registration at configure time and has no runtime registry.
- #64 / PR #90 merged as `b52d53eee65e5de91697e2c1f0685728b9646575`: independent overlapping `PUGL_PROGRAM` worlds are not a supported macOS multi-window ownership model. Legacy `StandaloneWindow(UI&, ...)` remains pre-v1/single-window; T060 owns one explicit `ui::Application` PROGRAM owner with multiple top-level windows.
- T059 / PR #89 merged as `6d84bc6b7817b0dcaea4eefd81833d765ad7685d`: the retained tree centrally resolves Visible/Hidden/Collapsed, inherited enabled/disabled and read-only state with focus/capture teardown, reentrancy handling and FocusScope restoration.
- T030 / PR #94 merged as `b32b09da473c70a857675e05f7fdc7c78e5f9361`: Button consumes the central T059 availability/focus/capture contract and is owned by the separate state/widget lane.
- T047 / PR #92 merged as `df569e874539aaafb8600960938465f733a46f19`: the installed/build-tree v1 low-level package exports only `NativeUI::Core` plus `nativeui_attach_platform(TARGET ... CONSUMER_ID ...)`, preserves T053 consumer-scoped macOS bridges, carries pinned Skia/Pugl implementation assets privately, validates relocation and missing-pinned-dependency failure, and does not export/document `NativeUI::NativeUI` as a complete package target.
- #97 / PR #98 merged as `864d2f35be1e7ae2976c14d2fac96ea4569f730f`: macOS Pugl MIN/MAX warning noise is removed from current main without changing the package/runtime contract.

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
- hosted Windows lacks a reliable interactive desktop and Linux/Xvfb native OpenGL execution proved nondeterministic, so those lanes retain deterministic package/runtime/link qualification rather than treating display availability as package correctness.

TDD/review history:

- RED `af34ed1c65409e054ba6c976d82cbdeff7234347` registered the source contract before fixtures existed and failed as intended;
- GREEN/correction cycles added relocation, all three consumer shapes, platform-link validation, graphical-session separation and stronger portable UI rendering self-tests;
- final code/review correction head `9904e2fe8f252a35e060bda128a82d18b9f5819a` passed CI #384 on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan, including the T048 source contract, relocated consumers and macOS native/runtime-isolation gates;
- CODE_REVIEW.md review is clean after correcting the earlier weak self-test and obsolete Linux diagnostic residue; no runtime globals, private package paths or Objective-C naming shortcuts were introduced;
- current main at the completion-doc refresh is `864d2f35be1e7ae2976c14d2fac96ea4569f730f`, which is already the PR base, so no source rebase is required before this docs-only final candidate.

The completion-doc commit changes `CONTEXT.md`/`ROADMAP.md`; its exact head must pass the full required matrix before PR #99 is marked ready and merged.

## Current DAG frontier

```text
lifecycle:        #64(done) -> T042 -> T051 -> T052
platform/package: T053(done) -> T047(done) -> T048(this merge) -> T052
                                      |
                                      +-> T054
                                      +-> T056 -> T057
state/widgets:    T059(done) -> T030(done) -> T031
```

After T048 merges, T054 and T056 are the dependency-unblocked platform/package items. T052 then has its T048 dependency satisfied but still requires T051 and T042. T057 remains dependent on T056. This lane must not take T059, T030, #64 or T042.

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

T048 additionally runs the source-contract script, relocated install-tree Core/standalone/embedded consumers, platform attachment/link tests, macOS native lifecycle and macOS two-consumer Objective-C namespace/runtime coexistence.

## Next platform/package action

1. Require the completion-doc exact T048 head CI green on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan.
2. Refresh current `main` immediately before merge; if it advanced, reconcile without dropping concurrent work and revalidate the new exact head.
3. Mark PR #99 ready and merge only with no blocking CODE_REVIEW.md finding.
4. Close/update issue #48 as Done.
5. Re-read T054/T056 priorities/dependencies and continue the highest-unblock-value Ready platform/package ticket; do not enter the lifecycle or state/widget lanes.
