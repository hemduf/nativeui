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

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`. This reviewed pin includes the X11 `SelectionNotify.property == None` guard merged through #124 / PR #125 in addition to the established drag-and-drop fixes.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline

Current `main` is `58f45ee02b32a1a3fcb139cc8345276ee334844c`. It includes the completed T057 ResourceManager and the reviewed #124 / PR #125 Pugl X11 failed-selection correction.

Completed foundations relevant to this lane:

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform()`.
- T048 / PR #99: relocated external consumers and macOS two-consumer isolation.
- T051 / PR #116: reproducible Release benchmark and regression policy.
- T054 / PR #119: `nativeui_add_application()` high-level native application package helper.
- T056 / PR #111: deterministic `nativeui_add_binary_data()` packaging with sorted immutable generated tables.
- T057 / PR #126: immutable non-owning `ResourceManager` plus explicit allocating `ResourceManagerProvider` adapter.
- #64 / PR #90: standalone ownership is frozen as Decision B.
- T042 / PR #93: deterministic supported-path lifecycle/multi-instance stress.
- #124 / PR #125: Pugl X11 failed-selection correction; the NativeUI completion head passed normal CI and T042 Lifecycle Stress before merge.

## T057 completed state

T057 is merged and issue #69 is closed Done.

Delivered contract:

- `ui::ResourceManager` borrows `std::span<const EmbeddedResourceEntry>` and performs one allocation-free O(N) validation pass;
- valid IDs are non-empty and strictly ascending by exact unsigned-byte lexicographic comparison;
- invalid tables fail atomically: direct lookup/enumeration APIs behave empty/false and retain only a small enum-backed diagnostic;
- `find()` uses binary search, is allocation-free and returns `ResourceView` spans pointing to original storage;
- copy/move managers remain lightweight immutable views with no registry/cache/mutex; independent managers remain isolated and concurrent read-only lookup is safe for live immutable backing storage;
- `ResourceManagerProvider` is the explicit compatibility seam for `ResourceProvider`; successful non-empty loads copy into the owned vector and are intentionally not real-time safe;
- ImageCache and SvgCache consume the adapter without manager-specific decoding APIs;
- the actual T056 generated table is consumed by build-tree and relocated install-tree external consumers;
- `t057_embedded_resources --self-test` covers direct lookup, provider copy semantics and SVG integration; the public header has an isolated compile probe.

Final evidence: exact PR head `20ec256241c9419b5a4d60f8f68968f4433d2855`; CI #600 passed Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan plus package/relocation contracts; final `CODE_REVIEW.md` pass had no Blocking/Important finding; PR #126 merged as `c2cc83b35ee8cdf469df03d40a93ca2194e6923f` and #69 closed Done.

## #124 completed platform correction

PR #125 advanced the shared Pugl pin to `195f79b22644010c81a5e0c3231c591856787ec6`. The defect was in the X11 failed-selection path: `SelectionNotify.property == None` could reach `XGetWindowProperty()` as atom `None`, causing `BadAtom` during lifecycle/clipboard stress. The reviewed Pugl correction rejects that path before the X11 property read; NativeUI did not weaken the T042 fixture or introduce a local workaround.

The synchronized completion head passed normal CI and T042 Lifecycle Stress with a clean mandatory review before PR #125 merged to `main` as `58f45ee02b32a1a3fcb139cc8345276ee334844c`.

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

The branch is synchronized with current `main` using a real merge commit so the current baseline remains a parent of the candidate. Conflict resolution preserves the completed T054/T057/#124 documentation while retaining T052's release-only diff. Any synchronized head is a new release candidate and must rerun all exact-head gates.

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

T052 is the current P0 release/lifecycle item. T071 remains the later full NativeUI 1.0 qualification gate. Other implementation lanes remain independent.

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

The normal CI matrix additionally validates Linux X11, Windows/MSVC, macOS, Linux ASan+UBSan, T047/T048/T054/T056 package contracts, relocated consumers and platform isolation checks. T042 lifecycle stress and T052 release qualification remain separate exact-head gates.

## Next actions

1. Require T052 v0.1 Release Gate, normal CI, T042 Lifecycle Stress and T051 Release Benchmarks to complete green on the exact synchronized head.
2. Perform the mandatory aggregate `CODE_REVIEW.md` pass against that exact head and fix any Blocking/Important finding before merge.
3. Refresh from current `main` immediately before merge; any source change invalidates previous exact-head evidence.
4. Record exact run/SHA evidence, mark PR #120 ready, merge without rewriting the validated candidate, then mark #52 Done/closed.
5. Keep v0.1 developer-preview status distinct from the later T071 NativeUI 1.0 gate.
