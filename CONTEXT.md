# NativeUI compact recovery context

**Updated:** 2026-09-09

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio, host parameter semantics and a custom native windowing stack remain out of scope.

Non-negotiable rules:

- widgets/layout stay platform-neutral and public geometry uses logical pixels;
- embedded polling is non-blocking;
- mutable instance-dependent process globals/singletons/`thread_local` state are forbidden;
- retained UI state is UI/main-thread confined unless explicitly documented otherwise;
- dependencies use CMake + CPM; Skia comes from pinned `skia-builder` binaries;
- macOS Objective-C runtime-visible platform classes are consumer-specific through the T053 identity contract;
- `CODE_REVIEW.md`, exact-head validation, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `d12d63815b8cfe3f36293d3791a418e8f558ff1b`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline and platform/package lane

Current `main` before the T056 integration merge is `4cd904466d05e4b403eb2b61386868e7c498c719` and already contains T042 lifecycle stress plus the earlier platform/package foundation.

Completed dependencies relevant to this lane:

- T053 / PR #88: consumer-scoped macOS platform bridge.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform(TARGET ... CONSUMER_ID ...)`.
- T048 / PR #99: relocated external Core/standalone/embedded consumer qualification.
- T042 / PR #93: deterministic supported-path lifecycle stress, merged before this T056 refresh.

## T056 binary resources — PR #111

T056 is the current platform/package merge candidate. It adds:

- installed/build-tree `nativeui_add_binary_data()` and `NativeUIEmbedResource.cmake` helpers;
- backend-neutral `ui::EmbeddedResourceEntry` with immutable borrowed byte spans;
- one deterministic generated resource object per source and one target-specific generated table/header;
- exact SHA-256-derived payload symbols, sorted unique resource IDs, explicit aliases and symlink-aware BASE_DIR containment;
- exact binary/NUL/empty-file round trips, deterministic clean-build output, incremental rebuild coverage, two-target namespace/symbol isolation and relocated installed-package consumers;
- semicolon-safe handling for valid resource IDs and canonical source paths without relying on raw CMake list identity.

Review of pre-refresh head `4cc48ed083a493519b19e8481007a0a6315d0a71` completed all mandatory `CODE_REVIEW.md` categories. The review found one Important CMake-list identity defect for semicolon-containing IDs/paths; TDD regression coverage and the production correction are included in that head. Final pre-refresh review reports no remaining Blocking/Important finding.

Pre-refresh exact-head validation is green:

- normal CI run #451 (`34369151069`): Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan all passed, including T047/T048/T056 package/external-consumer checks;
- T042 Lifecycle Stress run #23 passed on the same T056 head.

The final T056 candidate is refreshed from current `main` rather than carrying the stale feature-branch `CMakeLists.txt`: it preserves merged T031 widget targets and the macOS drop smoke while applying only the T056 package/helper/header additions. `CONTEXT.md` and `ROADMAP.md` are synchronized in this same candidate. The refreshed exact head must rerun the required CI before merge; no earlier-SHA result substitutes for that gate.

## Current DAG frontier

```text
lifecycle:        #64(done) -> T042(done) -> T051 -> T052
platform/package: T053(done) -> T047(done) -> T048(done) -> T052
                                      |
                                      +-> T054 (Ready)
                                      +-> T056 (complete by this merge) -> T057
state/widgets:    T059(done) -> T030(done) -> T031(done)
                                      |
                                      +-> T032 / T033 / T034 -> T035 / T036
```

After T056 merges, T057 becomes Ready because T022 is already complete. T054 is independently Ready because T047 and T053 are complete. For the platform/package lane, select the next P1 by repository priority/dependency-unblock rules; with no existing in-progress platform/package PR, T054 is the next recommended ticket, then T057.

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

T047/T048/T056 package validation additionally exercises source/configure contracts, relocation, external consumers and deterministic generated-resource behavior. Linux CI retains X11/Xvfb/Mesa native smoke; macOS retains consumer-specific Objective-C symbol/isolation checks; sanitizer CI keeps the repository's current Skia/Fontconfig boundary policy.

## Next actions

1. Require all required workflows on the refreshed T056 PR #111 head to complete green.
2. Re-check the exact refreshed diff/review and merge PR #111 only if no Blocking/Important finding exists.
3. Mark #68 Done/`status:done` and close it only after merge.
4. Re-read the platform/package frontier; resume an existing platform/package PR if one appeared, otherwise take T054 / #66 next, with T057 / #69 Ready after T056.
