# NativeUI compact recovery context

**Updated:** 2026-09-09

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio, host parameter semantics and a custom native windowing stack remain out of scope.

Non-negotiable rules:

- widgets/layout stay platform-neutral and public geometry uses logical pixels;
- embedded polling is non-blocking;
- mutable instance-dependent production globals/singletons/`thread_local` state are forbidden;
- retained UI state is UI/main-thread confined unless explicitly documented otherwise;
- dependencies use CMake + CPM; Skia comes from pinned `skia-builder` binaries;
- macOS Objective-C runtime-visible platform classes are consumer-specific through the T053 identity contract;
- `CODE_REVIEW.md`, exact-head validation, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `d12d63815b8cfe3f36293d3791a418e8f558ff1b` on the T054 package candidate. The independent P0 platform regression #124 / PR #125 owns any later Pugl pin change and must not be folded into T054.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline

`main` `ce86c86e663ad5f8464224874039312143146300` contains T051 and the earlier lifecycle/package foundations. T054 / issue #66 / PR #119 is the current platform/package merge candidate and is refreshed from that baseline in the completion candidate.

Completed foundations relevant to the active lanes:

- #64 / PR #90: standalone ownership Decision B — one application-level `PUGL_PROGRAM` world.
- T042 / PR #93: deterministic supported-path lifecycle stress.
- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform()`.
- T048 / PR #99: relocated external consumers and macOS two-consumer isolation.
- T051 / PR #116: reproducible Release benchmark and regression policy.
- T056 / PR #111: deterministic binary-resource packaging.

## T054 application package helper — PR #119

T054 adds the public high-level CMake application helper while preserving T047/T053 as the only platform-attachment/runtime-identity authority:

```cmake
find_package(NativeUI CONFIG REQUIRED)

nativeui_add_application(MyApp
  PRODUCT_NAME "My App"
  BUNDLE_ID "com.example.myapp"
  VERSION "1.2.3"
  SOURCES src/main.cpp
  # MACOS_ICON path/to/icon.icns
  # WINDOWS_ICON path/to/icon.ico
)
```

Delivered contract:

- exact caller argument boundaries are preserved, including valid semicolons and keyword spellings inside one-value fields;
- `PRODUCT_NAME`, reverse-DNS `BUNDLE_ID`, exact decimal `MAJOR.MINOR.PATCH`, required sources and platform icon inputs are validated deterministically;
- final targets link `NativeUI::Core` and delegate platform ownership through `nativeui_attach_platform(TARGET ... CONSUMER_ID ...)`; T054 does not duplicate Pugl source lists or Objective-C prefix logic;
- macOS creates a real `.app` bundle with deterministic plist metadata and optional `.icns` resource; two T054 apps are audited against distinct T053 bridge archives/runtime prefixes;
- Windows creates a GUI-subsystem executable with one target-specific `.rc`/icon path; Linux creates a normal executable and ignores non-native icon options without side effects;
- build-tree and relocated install-tree packages expose the same helper and external consumers configure, build and execute;
- helper-created targets remain caller-owned and composable after creation;
- configuration, argument-boundary, platform package, icon/resource, relocation and isolation regressions are wired into normal CI.

Review/correction history includes the CMake macro argument-flattening bug, missing-target diagnostic interception, leaked PUBLIC C99 bridge requirement, uppercase source-name misclassification, real macOS icon-resource and two-app symbol-audit gaps, diagnostic line-wrap fragility, and pre-list reverse-DNS byte validation. Each behavior correction was locked by a RED regression before the GREEN change. The latest CODE_REVIEW.md pass reports no unresolved Blocking/Important T054 finding; one final pass is required on the exact refreshed completion head.

The T054 completion tree deliberately restores the current-main Pugl pin so #124 / PR #125 remains an independent dependency/platform change. T042 is likewise not modified by T054.

## Current DAG frontier

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052
platform/package:  T053(done) -> T047(done) -> T048(done) ----^
                                       |
                                       +-> T054 (complete by PR #119 merge)
                                       +-> T056(done) -> T057 (Ready)
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032 / T033 / T034 -> T035 / T036
```

After T054 merges, T057 is the next dependency-unblocked planned ticket in this platform/package lane (`T056 + T022` are already complete). #124 / PR #125 is an independent P0 platform dependency regression and should remain isolated from T054.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

T054 additionally runs its source/argument contracts and external package consumers from `.github/workflows/ci.yml` on Linux X11, Windows/MSVC and macOS, with the normal Linux ASan+UBSan lane retained. The macOS package consumer audits two application bridge archives with `tests/check_objc_runtime_prefix.cmake` and verifies the actual bundled icon bytes.

## Next actions

1. Validate the exact current-main-refreshed T054 completion head on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan.
2. Perform the final mandatory `CODE_REVIEW.md` pass on that exact head and fix any Blocking/Important finding through TDD.
3. Mark PR #119 ready and merge only when the required T054/platform/package gates are green.
4. Mark #66 Done/closed after merge, then continue with T057 in the platform/package lane. Keep #124/T042 and other excluded parallel lanes separate.
