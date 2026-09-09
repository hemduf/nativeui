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

- Pugl: `hemduf/pugl` commit `d12d63815b8cfe3f36293d3791a418e8f558ff1b`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline

Current `main` before the T051 benchmark integration is `3a87070ae1b236a9d68f73e20f489ca5256da2ae`.

Completed foundations relevant to the release/lifecycle lane:

- #64 / PR #90: standalone ownership frozen as Decision B — one application-level `PUGL_PROGRAM` world; no hidden singleton/shared-world workaround in the legacy constructor path.
- T042 / PR #93: deterministic headless/embedded/standalone lifecycle stress for every currently supported ownership path.
- T024: deterministic headless raster/golden foundation.
- T053 / T047 / T048: consumer-scoped macOS bridge plus relocatable low-level package/external-consumer qualification.
- T056 / PR #111: deterministic binary resource packaging.

## T051 performance harness — PR #116

T051 is the current lifecycle/release-lane merge candidate. It provides one Release-only deterministic microbenchmark suite and the relative regression policy consumed by T052/T071.

Delivered contract:

- fixed schema/workload versioning, compiler/OS/architecture/build/commit metadata and JSON round-trip support;
- exact 5 warmup + 30 measured `steady_clock` sampling, median indices 14/15 and p95 index 28;
- fixed batch counts for layout, deep hit testing, pointer/keyboard dispatch, short/multiline text editing, control/text headless paint and headless instance lifecycle;
- deterministic `idle_invalidation` correctness gate: 1,000 logical 10 ms checkpoints (10 seconds logical idle) after the settled frame, requiring exactly zero framework invalidations;
- benchmark-only allocation interception around measured operation scopes; it never changes `NativeUI::Core` or public allocator/lifetime APIs;
- workload-contract coverage for exact node/action shapes plus known allocating and non-allocating counter scopes;
- metadata-safe comparison entry point that rejects incompatible baseline/rerun environments before thresholds are evaluated;
- timing gate: median >15% **and** p95 >20%, reproduced by two complete independent runs;
- allocation gate: count or bytes/op >10% with two-run confirmation, or any recurring allocation for explicitly zero-allocation scenarios;
- CI-generated immutable JSON artifacts; T052 selects/records the controlled v0.1 baseline artifact rather than committing universal machine-specific nanosecond constants.

The benchmark and baseline contract is documented in `docs/performance-benchmarks.md`. Normal benchmark execution never rewrites baselines.

## Current DAG frontier

```text
lifecycle/release: #64(done) -> T042(done) -> T051(in review) -> T052
platform/package:  T053(done) -> T047(done) -> T048(done) ----^
                                       |
                                       +-> T054
                                       +-> T056(done) -> T057
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032 / T033 / T034 -> T035 / T036
```

When T051 merges cleanly, T052 becomes dependency-unblocked and is the next high-priority lifecycle/release ticket.

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

Linux CI retains X11/Xvfb/Mesa native smoke; macOS retains consumer-specific Objective-C symbol/isolation checks; sanitizer CI keeps the repository's current Skia/Fontconfig boundary policy. T042 lifecycle stress remains a separate gate.

## Next actions

1. Require exact-head T051 contract, Release benchmark, normal CI and T042 lifecycle-stress workflows to complete green.
2. Complete the mandatory `CODE_REVIEW.md` pass against that exact head and fix any Blocking/Important finding before merge.
3. Merge PR #116 only after current-main synchronization and exact-head validation are both satisfied.
4. Mark #51 Done/`status:done`, close it, then move T052 / #52 from Blocked to Ready and continue that release/lifecycle dependency chain.
