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

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

The current Pugl pin includes the reviewed X11 failed-selection guard so an empty/unavailable clipboard conversion (`SelectionNotify.property == None`) is handled as failure rather than passed to `XGetWindowProperty()` as atom `None`. The shared dependency integration is tracked independently by #124 / PR #125; T052 consumes the same reviewed pin for exact release qualification.

## Current baseline

Current `main` is `ce86c86e663ad5f8464224874039312143146300`, including merged T051 / PR #116.

Completed foundations relevant to the release/lifecycle lane:

- #64 / PR #90: standalone ownership frozen as Decision B — one application-level `PUGL_PROGRAM` world; no hidden singleton/shared-world workaround in the legacy constructor path.
- T042 / PR #93: deterministic headless/embedded/standalone lifecycle stress for every currently supported ownership path.
- T024: deterministic headless raster/golden foundation.
- T053 / T047 / T048: consumer-scoped macOS bridge plus relocatable low-level package/external-consumer qualification.
- T056 / PR #111: deterministic binary resource packaging.
- T051 / PR #116: Release-only deterministic benchmark harness, canonical two-run regression policy and immutable JSON result artifacts.

## T052 v0.1 release gate — PR #120

T052 is dependency-unblocked and is the current lifecycle/release-lane completion candidate. It is an aggregate validation/release ticket, not a feature implementation ticket.

Current contract:

- exact candidate SHA carried explicitly by the release-gate workflow;
- normal Linux/X11, Windows/MSVC, macOS and Linux ASan+UBSan validation remains mandatory on the same head;
- T042 supported-path lifecycle stress remains an independent exact-head gate;
- clean dependency/bootstrap builds start from an empty CPM cache on Linux/X11, Windows and macOS and verify the exact Pugl/Skia pin/hash contract;
- the exact release-note `find_package(NativeUI CONFIG REQUIRED)` / `NativeUI::Core` / `nativeui_attach_platform()` snippet is materialized and built against the installed package on all supported desktop platforms;
- T047/T048 package relocation and macOS two-consumer Objective-C namespace/runtime isolation remain part of normal CI;
- one approved-base T051 benchmark run and two complete candidate runs are compared through T051's canonical C++ `compare_two_complete_runs()` policy rather than duplicated workflow thresholds;
- `idle_invalidation` remains an exact zero timing/allocation hard gate;
- v0.1 release notes explicitly identify developer-preview semantics, Decision B, known v1 gaps, pinned dependencies, reproducible exact-SHA tag procedure, and the `AGPL-3.0-only` / commercial dual-licensing model;
- T047's installed package legal-payload contract remains required for `LICENSE.md`, `NOTICE.md`, `THIRD_PARTY.md`, `EULA.md`, `PRIVACY.md`, `TERMS.md` and `LEGAL.md`.

T052 must not merge until one exact final head passes all of the above and the aggregate `CODE_REVIEW.md` audit has no Blocking/Important finding.

## Current DAG frontier

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052(in review)
platform/package:  T053(done) -> T047(done) -> T048(done) -----------^
                                       |
                                       +-> T054
                                       +-> T056(done) -> T057
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032 / T033 / T034 -> T035 / T036
```

T052 is the current P0 release/lifecycle item. T071 remains the later full NativeUI 1.0 qualification gate.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

T051 Release benchmark validation:

```bash
cmake -S tests/t051 -B build-t051 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DNATIVEUI_SOURCE_DIR="$PWD" \
  -DNATIVEUI_BENCHMARK_COMMIT_SHA="$(git rev-parse HEAD)"
cmake --build build-t051 --parallel
ctest --test-dir build-t051 --output-on-failure
./build-t051/nativeui_benchmarks --self-test
./build-t051/nativeui_benchmarks --json t051-results.json
```

Linux CI retains X11/Xvfb/Mesa native smoke; macOS retains consumer-specific Objective-C symbol/isolation checks; sanitizer CI keeps the repository's current Skia/Fontconfig boundary policy. T042 lifecycle stress and T052 release qualification remain separate exact-head gates.

## Next actions

1. Require exact-head T052 release contract/bootstrap/benchmark workflow, normal CI, T042 lifecycle stress and T051 Release benchmark workflow to complete green on the same candidate.
2. Perform the mandatory aggregate `CODE_REVIEW.md` pass against that exact head and fix any Blocking/Important finding before merge.
3. Refresh from current `main` immediately before merge; any source change invalidates previous exact-head evidence.
4. Record exact run/SHA evidence, mark PR #120 ready, merge without rewriting the validated release candidate, then mark #52 Done/closed.
5. Keep v0.1 developer-preview status distinct from the later T071 NativeUI 1.0 gate.
