# NativeUI compact recovery context

**Updated:** 2026-09-10

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio and host parameter semantics remain out of scope.

Non-negotiable rules:

- widgets/layout stay platform-neutral and public geometry uses logical pixels;
- embedded polling is non-blocking;
- mutable instance-dependent production globals/singletons/`thread_local` state are forbidden;
- retained UI state is UI/main-thread confined unless explicitly documented otherwise;
- dependencies use CMake + CPM; Skia comes from pinned `skia-builder` binaries;
- macOS Objective-C runtime-visible platform classes are consumer-specific through the T053 identity contract;
- `CODE_REVIEW.md`, exact-head validation, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`, including the reviewed X11 failed-selection guard from #124 / PR #125.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline

Current `main` is `ef10f9f8ece733b1f0f19d3326be9a3ced903e41`. It contains the completed T060 explicit Application/multi-window ownership model and the completed #139 post-T060 T042 lifecycle qualification on top of the reviewed Pugl X11 correction.

Completed foundations relevant to current release/platform work:

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform()`.
- T048 / PR #99: relocated external consumers and macOS two-consumer isolation.
- T051 / PR #116: reproducible Release benchmark and regression policy.
- T054 / PR #119: `nativeui_add_application()` high-level native application package helper.
- T056 / PR #111: deterministic `nativeui_add_binary_data()` packaging.
- T057 / PR #126: immutable non-owning `ResourceManager` plus explicit allocating provider adapter.
- #124 / PR #125: reviewed Pugl X11 `SelectionNotify.property == None` correction pinned into NativeUI.
- T060 / PR #118: one explicit `ui::Application` owns exactly one standalone `PUGL_PROGRAM` world; explicit `StandaloneWindow(Application&, ...)` instances borrow that world while retaining independent per-window UI/render/input state. `EmbeddedView` remains independent `PUGL_MODULE` ownership. The legacy standalone constructor remains pre-v1 only for later T069 removal.
- #139 / PR #140: T042 now directly stresses the supported T060 shared-Application A+B multi-window path while preserving #64 Decision B and the legacy process-isolated compatibility fixture.

T060 exact merge candidate `0e4cce56bd8874545794fdf1137d1c7ec5489dde` passed T060 Application Contract `34422787634`, T042 Lifecycle Stress `34422787683`, and normal CI `34422787695`. Final `CODE_REVIEW.md` review `5161881319` reported no Blocking/Important finding. PR #118 merged as `352bdf0e734df46e8edcb53a0a81a07c9e0d7d6d`; issue #72 is closed Done.

#139 final head `ee482da974222299bc94904ed8256511da1a256d` passed T042 Lifecycle Stress `34434737094`, T060 Application Contract `34434737109`, and normal CI `34434737107`, including Linux/X11, Windows, macOS and Linux ASan+UBSan coverage. Final `CODE_REVIEW.md` review `5162508225` reported no Blocking/Important finding. PR #140 merged as current `main` `ef10f9f8ece733b1f0f19d3326be9a3ced903e41`; issue #139 is closed Done.

## Active lifecycle/release work — T052 / issue #52 / PR #120

T052 is dependency-unblocked and is the active P0 v0.1 developer-preview release gate. It is aggregate validation/release infrastructure only and must not absorb unrelated feature work.

Current contract:

- exact candidate SHA and approved-base SHA are explicit release-gate inputs;
- exact-head normal Linux/X11, Windows/MSVC, macOS and Linux ASan+UBSan validation is mandatory;
- T042 supported-path lifecycle stress remains an independent exact-head gate and now includes #139's executable T060 shared-Application multi-window stress while preserving #64 Decision B;
- T060 Application Contract remains a separate exact-head ownership gate;
- clean dependency/bootstrap builds start from an empty CPM cache on Linux/X11, Windows and macOS and verify the exact Pugl/Skia pin/hash contract;
- the exact release-note `find_package(NativeUI CONFIG REQUIRED)` / `NativeUI::Core` / `nativeui_attach_platform()` snippet is materialized and built against the installed package on all supported desktop platforms;
- T047/T048 package relocation and macOS two-consumer Objective-C namespace/runtime isolation remain part of normal CI;
- one approved-base T051 benchmark run and two complete candidate runs are compared through T051's canonical C++ `compare_two_complete_runs()` policy rather than duplicated workflow thresholds;
- `idle_invalidation` remains an exact-zero timing/allocation hard gate;
- v0.1 release notes explicitly identify developer-preview semantics, Decision B/T060 ownership, known v1 gaps, pinned dependencies, reproducible exact-SHA tag procedure, and the `AGPL-3.0-only` / commercial dual-licensing model;
- T047's installed package legal-payload contract remains required for `LICENSE.md`, `NOTICE.md`, `THIRD_PARTY.md`, `EULA.md`, `PRIVACY.md`, `TERMS.md` and `LEGAL.md`.

The T052 branch was refreshed from current `main` after #139 without rewriting its TDD history. Merge commit `4f79d2e0e6b91e664b27854fa0c44b2846be54b7` has both the prior T052 head and `ef10f9f8ece733b1f0f19d3326be9a3ced903e41` as parents. This recovery snapshot is reapplied on top of that merge. The resulting documentation-complete head is a new exact release candidate and requires fresh T052 Release Gate, normal CI, T042 Lifecycle Stress, T051 Release Benchmarks and T060 Application Contract evidence before merge.

## Current dependency frontier

```text
lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(in review)
                   T042(done) -> T051(done) -----------------> T052(in review)
platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                     T056(done) + T022(done) -> T057(done)

T060(done) -> T065(active PR #133) -> T072 -> T064
                 |
                 +-> T066 (also depends on T043)

T043 ready
T044 ready
```

T052 owns only the release/lifecycle qualification slice. T065/T072/T064 and T043/T044 remain separate platform work; widget/state tickets remain in their own lane.

## Active platform work — T065 / issue #77 / PR #133

The existing T065 stream delivers the bounded Core dispatcher/timer engine and root integration. Its reviewed contract includes per-owner bounded queues/timers, deterministic fake time, FIFO ordering, finite drain fairness, owner shutdown semantics and no process-global dispatcher.

Remaining T065 work after T060 merge:

1. refresh PR #133 from current `main` without duplicating the branch or widening scope;
2. expose one dispatcher per `StandaloneWindow` and one independent dispatcher per `EmbeddedView` while preserving T060 shared-world ownership;
3. implement the smallest thread-safe standalone wake mechanism and preserve embedded host-driven non-blocking polling;
4. add native wake/multi-owner/lifecycle coverage and the required `t065_ui_dispatcher` feature example/self-test;
5. run targeted T065 tests, normal CI, T042 regression coverage and Linux ASan+UBSan on the exact final head;
6. perform the mandatory final `CODE_REVIEW.md` pass, synchronize issue/CONTEXT/ROADMAP, refresh from current main and merge only when exact-head gates are green.

T072 remains blocked by T065. T064 remains blocked by T065 and T072.

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

The normal CI matrix additionally validates Linux X11, Windows/MSVC, macOS, Linux ASan+UBSan, package/relocation contracts, macOS consumer isolation and feature/platform smokes. T042 lifecycle stress, T051 benchmarks, T052 release qualification and T060 Application ownership remain separate exact-head gates for the T052 candidate.

## Next actions

1. Run the fresh T052 v0.1 Release Gate, normal CI, T042 Lifecycle Stress, T051 Release Benchmarks and T060 Application Contract on the exact documentation-complete post-#139 candidate.
2. Perform the aggregate mandatory `CODE_REVIEW.md` pass against that exact head and fix any Blocking/Important finding before merge.
3. Re-check `main` immediately before final merge; any source advance requires another refresh and fresh exact-head qualification.
4. Merge PR #120 without rewriting the validated candidate, mark #52 Done/closed, and retain the exact v0.1 qualification evidence.
5. Leave T065 and unrelated platform/widget lanes to their existing owners.
