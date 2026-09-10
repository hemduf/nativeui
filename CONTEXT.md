# NativeUI compact recovery context

**Updated:** 2026-09-10

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

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`. This reviewed pin includes the X11 failed-selection guard merged through #124 / PR #125 in addition to the established drag-and-drop fixes.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline

Current `main` is `58f45ee02b32a1a3fcb139cc8345276ee334844c`. It includes the reviewed Pugl X11 failed-selection correction from #124 / PR #125 and the completed T057 ResourceManager/platform-package foundations.

Completed foundations relevant to the lifecycle/release lane:

- #64 / PR #90: standalone ownership is Decision B — one explicit application-level `PUGL_PROGRAM` world for future multi-window ownership; no hidden singleton/shared-world workaround in the legacy constructor path.
- T042 / PR #93: deterministic headless/embedded/standalone lifecycle stress for all currently supported ownership paths.
- T024: deterministic headless raster/golden foundation.
- T051 / PR #116: reproducible Release benchmark harness, canonical two-run regression policy and immutable JSON result artifacts.
- T053 / T047 / T048: consumer-scoped macOS Objective-C bridge plus relocatable low-level package/external-consumer qualification.
- T054 / PR #119: high-level `nativeui_add_application()` package helper.
- T056 / PR #111 and T057 / PR #126: deterministic binary-resource packaging plus immutable embedded `ResourceManager`.
- #124 / PR #125: reviewed Pugl X11 failed-selection guard; exact-head normal CI and T042 lifecycle stress passed before merge.

## T052 v0.1 release gate — PR #120

T052 is dependency-unblocked and is the active P0 lifecycle/release completion candidate. It is an aggregate validation/release ticket, not a feature implementation ticket.

Current contract:

- exact candidate SHA and approved-base SHA are explicit release-gate inputs;
- exact-head normal Linux/X11, Windows/MSVC, macOS and Linux ASan+UBSan validation remains mandatory;
- T042 supported-path lifecycle stress remains an independent exact-head gate and preserves #64 Decision B;
- clean dependency/bootstrap builds start from an empty CPM cache on Linux/X11, Windows and macOS and verify the exact Pugl/Skia pin/hash contract;
- the exact release-note `find_package(NativeUI CONFIG REQUIRED)` / `NativeUI::Core` / `nativeui_attach_platform()` snippet is materialized and built against the installed package on all supported desktop platforms;
- T047/T048 package relocation and macOS two-consumer Objective-C namespace/runtime isolation remain part of normal CI;
- one approved-base T051 benchmark run and two complete candidate runs are compared through T051's canonical C++ `compare_two_complete_runs()` policy rather than duplicated workflow thresholds;
- `idle_invalidation` remains an exact zero timing/allocation hard gate;
- v0.1 release notes explicitly identify developer-preview semantics, Decision B, known v1 gaps, pinned dependencies, reproducible exact-SHA tag procedure, and the `AGPL-3.0-only` / commercial dual-licensing model;
- T047's installed package legal-payload contract remains required for `LICENSE.md`, `NOTICE.md`, `THIRD_PARTY.md`, `EULA.md`, `PRIVACY.md`, `TERMS.md` and `LEGAL.md`.

The former reverse sync PR #131 exposed documentation conflicts after #124 and later package/resource work advanced `main`. The T052 branch must resolve those conflicts by retaining current-main package/resource/platform documentation while preserving T052's release-gate files and semantics. Any synchronized head is a new release candidate and must rerun all exact-head gates before merge.

## Current DAG frontier

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052(in review)
platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |
                                       +-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032
                                       +-> T033
                                       +-> T034 -> T035 / T036
platform fix:       #124(done)
```

T052 is the current release/lifecycle item. T071 remains the later full NativeUI 1.0 qualification gate. Widget/platform implementation tickets remain owned by their separate lanes.

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

Normal CI also validates T047/T048/T054/T056 package contracts, relocated consumers and platform isolation. T042 lifecycle stress and T052 release qualification remain separate exact-head gates.

## Next actions

1. Synchronize the existing T052 branch with current `main` without dropping T054/T057/#124 state or widening T052 scope.
2. Require T052 v0.1 Release Gate, normal CI, T042 Lifecycle Stress and T051 Release Benchmarks to complete green on the exact synchronized head.
3. Repeat the mandatory aggregate `CODE_REVIEW.md` pass on that exact head and fix any Blocking/Important finding before merge.
4. Immediately before merge, verify the candidate is still based on current `main`; any source change creates a new candidate SHA and invalidates earlier exact-head evidence.
5. Merge PR #120 without rewriting the validated candidate, mark #52 Done/closed, and keep v0.1 developer-preview status distinct from T071.
