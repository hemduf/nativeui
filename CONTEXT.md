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
- NativeUI-owned source-tree targets compile with zero unapproved warnings; `NATIVEUI_ALLOWED_WARNINGS` is empty by default and is the only normal explicit CMake opt-in for a temporarily accepted diagnostic;
- `CODE_REVIEW.md`, exact-head validation, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline

The current `main` baseline is `c83595ea53a9212d9544da36400c2a01d2843f7e` and contains T058 / PR #154, #163 / PR #181 warning-free source-tree builds, T036 / PR #155, T065 / PR #133, T034 / PR #135, the explicit T060 Application/multi-window ownership model, post-T060 T042 lifecycle qualification from #139, the T052 v0.1 developer-preview release gate, and the Tree paint-ownership correction #152 / PR #153. T037 / PR #151 is the active style completion candidate refreshed on top of this baseline.

Cross-cutting rendering regression #152 / PR #153 removed Tree-owned visual decoration: `Tree::paint()` no longer forces `colors::background` across the viewport and no longer draws the hard-coded keyboard/mouse help line. Generic retained-tree painting is consumer/component-owned. The headless renderer mirrors the GPU renderer's black framebuffer clear, and its regression verifies that an otherwise empty Tree adds no styled background or instructional overlay beyond that renderer-level clear.

Relevant completed foundations:

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- #64 / PR #90: standalone PROGRAM-world ownership Decision B.
- T059 / PR #89: inherited visibility/enabled/read-only state.
- T030 / PR #94: Button.
- T031 / PR #95: Checkbox + typed RadioGroup/RadioButton.
- T032 / PR #115: Slider + RangeSlider.
- T033 / PR #123: ProgressBar + Meter.
- T034 / PR #135: ScrollView with change-based wheel bubbling, optional pointer pan, retained overlay scrollbars, focus reveal and T059 availability integration.
- T036 / PR #155: retained non-virtualized ListView + Tabs baseline with stable key/value selection, composite focus, keyboard/pointer interaction, ScrollView reveal, Collapsed tab panels, deterministic tests and feature example.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform()`.
- T048 / PR #99: relocated external consumers and macOS two-consumer isolation.
- T051 / PR #116: reproducible Release benchmark and regression policy.
- T052 / PR #120: v0.1 developer-preview release gate with clean-cache bootstrap, exact package consumer, lifecycle, benchmark, idle-invalidation and platform qualification.
- T054 / PR #119: `nativeui_add_application()` high-level native application package helper.
- T056 / PR #111: deterministic `nativeui_add_binary_data()` packaging.
- T057 / PR #126: immutable non-owning `ResourceManager` plus allocating provider adapter.
- T058 / PR #154: bounded retained dynamic composition with conditional, switch and keyed collection reconciliation plus structural diagnostics.
- T060 / PR #118: one explicit `ui::Application` owns one standalone `PUGL_PROGRAM` world and multiple independent `StandaloneWindow(Application&, ...)` views.
- T065 / PR #133: bounded UI-thread Dispatcher/timer service with native wake integration and deterministic timer/fairness semantics.
- #139 / PR #140: T042 stress qualification of the supported T060 multi-window path while preserving #64 Decision B for the legacy independent-PROGRAM compatibility path.
- #163 / PR #181: warning-free NativeUI-owned builds, v1 Application ownership in examples/smokes and blocking unapproved compiler diagnostics.

## T036 UI/accessibility lane — complete

T036 / issue #36 / PR #155 is complete on `main`.

Delivered behavior:

- fully retained, deliberately non-virtualized `ListView<T>` with stable logical keys and application-owned `State<std::optional<T>>` selection;
- deterministic duplicate-key rejection and missing-selection behavior without mount-time state rewrite;
- one composite ListView Tab stop; arbitrary focusable row presentation is excluded from global traversal through existing inactive focus-scope semantics;
- Up/Down/Home/End selection with disabled-item skipping, pointer selection and optional Enter/Space/pointer activation callback;
- T034 `ensure_visible` integration for both user-driven and application-originated selection changes;
- fully retained `Tabs<T>` with stable keys, automatic Left/Right/Home/End activation, disabled-tab skipping/wrap and pointer activation;
- inactive tab panels use T059 `Collapsed` semantics and therefore cannot retain layout, paint, hit testing or focus;
- T059 ReadOnly remains navigation-capable while Disabled suppresses normal targeting/focus;
- dedicated T036 tests, non-virtualized O(N) baseline, two-instance coverage, deterministic headless selected-state checks, and `examples/features/t036_list_tabs.cpp --self-test`.

T045 is the next UI/accessibility dependency item. T058 is now complete, so T067 is gated only by T045 before the T067/T068 convergence.

## T058 dynamic-composition lane — complete

T058 / issue #58 / PR #154 is complete on `main` as `c83595ea53a9212d9544da36400c2a01d2843f7e`.

Delivered behavior:

- retained `If`, `Switch` and keyed `ForEach` composition seams;
- bounded deferred reconciliation with deterministic failure diagnostics rather than unbounded reentrant mutation;
- keyed identity preservation and duplicate-key rejection without partial ambiguous commits;
- focus/lifecycle handling for removed/replaced retained subtrees, including trapping focus-scope rehome;
- monotonic NodeId ownership across dynamic removal/replacement;
- public structural diagnostics plus deterministic tests and `t058_dynamic_composition --self-test`.

T058 now unblocks its side of T067 and the dynamic/overlay chain toward T061/T035/T063/T068.

## T037 style lane — completion candidate

T037 / issue #37 / PR #151 delivers the first typed per-UI theme contract without introducing mutable process-global style state.

Delivered behavior:

- strongly typed `Theme` value covering palette, typography, spacing, radii and shared control metrics;
- deterministic `default_theme()` and explicit paint-only versus layout-affecting change classification;
- one Theme owned by each UI/retained tree with public `UI::theme()` and `UI::set_theme()` accessors;
- internal per-tree theme binding used by representative Button and Slider measurement/painting;
- deterministic per-UI isolation, invalidation and representative widget regressions;
- public `theme.hpp` isolated-header coverage, umbrella exposure and `t037_theme --self-test` feature example;
- Windows header compatibility regressions: theme spacing/radius tokens use `sm` rather than macro-prone `small`, and public geometry uses macro-safe `(std::min)` / `(std::max)` calls so `theme.hpp` remains includable after `windows.h` without requiring consumer-side `NOMINMAX`;
- T058 coexistence is preserved explicitly: dynamic composition remains in Tree/UI/umbrella/header tests while T037 adds theme ownership and accessors.

The TDD/review stream preserves the established 72x160 default formatted vertical Slider geometry. The branch is refreshed directly onto the T058 + warning-free baseline. Merge remains gated on fresh exact-head workflows and a final `CODE_REVIEW.md` pass. After merge, T038 is the direct style continuation, followed by T039; T040 must reuse T065 rather than creating another scheduler.

## T065 platform/event lane — complete

T065 / issue #77 / PR #133 is complete and merged.

Delivered behavior:

- public weak/copyable `Dispatcher` handles with per-owner FIFO task queues;
- exact limits of 65,536 pending tasks, 8,192 active timers and 1,024 callbacks per checkpoint snapshot;
- deterministic zero-delay one-shot and fixed-delay repeating timers with cancellation, queue-saturation retry and no catch-up bursts;
- injected clock and wake seams, including deterministic fake-time tests;
- per-window/view logical ownership with no process-global dispatcher/current-window registry;
- callback and callback-capture destruction outside dispatcher locks, including post rejection, timer cancellation and owner shutdown;
- callback-driven owner destruction without executing later callbacks from the captured snapshot;
- explicit-Application standalone windows share one Application-owned low-level wake backend while retaining independent task/timer namespaces;
- worker-originated standalone wake uses captured native primitives rather than concurrent Pugl calls: CFRunLoop source/wake on macOS, `PostMessageW` on Windows and `XSendEvent`/`XFlush` on X11;
- positive/indefinite Application waits are bounded by dispatcher timer deadlines and interruptible by worker posts without busy polling;
- `EmbeddedView` dispatch remains host-driven and non-blocking with no background polling thread;
- dedicated `examples/features/t065_ui_dispatcher.cpp` provides interactive worker/timer behavior plus deterministic `--self-test`.

The legacy `StandaloneWindow(UI&, ...)` path remains pre-v1 compatibility only and is removed by T069. T065 does not introduce a hidden shared Application or a second supported PROGRAM-world ownership model.

## #163 / PR #181 — warning-free source-tree builds — complete

#163 / PR #181 is merged on `main` as `1d0716a48b2294cc014b2988997eef4e4bb250a9`.

Delivered behavior:

- shared feature examples, the standalone demo, T041 and supported standalone/embedded/macOS drop smokes use the v1 `Application + StandaloneWindow(Application&, UI&, WindowDesc)` ownership path;
- NativeUI-owned source-tree builds enable strict warning levels and `CMAKE_COMPILE_WARNING_AS_ERROR=ON`;
- `NATIVEUI_ALLOWED_WARNINGS` is an explicit semicolon-separated CMake cache setting with an empty default; malformed identifiers fail configuration;
- historical #64 legacy diagnostics are excluded from the strict default build and are available only through explicit diagnostic opt-in;
- repository review rules classify unapproved NativeUI compiler warnings as Blocking and prohibit local warning suppression as a substitute for the explicit allowlist;
- existing deprecated-construction, shadowing, subobject-linkage and narrowing diagnostics exposed by the stricter build were fixed instead of whitelisted.

## Current dependency frontier

```text
critical UI:       T034(done) -> T036(done) -> T045(ready) -> T067 -> T068
                                                  T058(done) -----------^      ^

style:             T037(PR #151 completion) -> T038 -> T039
                                               +-> T040 with T065(done)

dynamic/overlay:   T058(done) -> T061 -> T035 ------------------------> T068
                                      +-> T063 ------------------------> T068

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052(done)

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

critical platform: T060(done) -> T065(done) -> T072(active) -> T064
                   T041(done) -> T043(active PR #142) -> T066
                                              |-------> T068
                   T065 + T072 + T043 + other feature deps ------> T068 -> T069
```

T045 is dependency-unblocked by T036/T058 completion. T072 is active after T065 completion. T064 depends on T065 and T072. T066 depends on T060 and T043. T043 may progress independently. T044 remains a T071 release dependency and its active PR #145 is being evidence-qualified independently.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The default build must use an empty `NATIVEUI_ALLOWED_WARNINGS`. A temporary approved Clang/GCC exception is explicitly configured, for example:

```bash
cmake -S . -B build -DNATIVEUI_ALLOWED_WARNINGS=deprecated-declarations
```

Do not add an exception when the warning can be fixed in NativeUI-owned code.

A code-changing completion candidate must use its exact current head for normal CI plus every relevant dedicated workflow named by the ticket, followed by the mandatory `CODE_REVIEW.md` pass. T037 requires fresh CI, T042 Lifecycle Stress, T060 Application Contract and T052 Release Gate on its final exact head; T065 dispatcher qualification is also expected because the refreshed baseline includes that lane. Normal CI also re-executes the merged T058 dynamic-composition tests on the combined head.

## Next actions

1. Finish exact-head qualification, final review and merge of T037 / PR #151; then continue T038 in the style lane.
2. Continue T043 / PR #142 independently and finish it before T066/T068.
3. Finish the evidence-gated T044 / PR #145 native outside-view capture qualification; add platform code only where the native smoke proves it is required.
4. Continue T045, then T067 now that T058 is complete; keep active T072/T064 progressing according to their explicit dependencies.
5. Continue T061, then T035/T063, without duplicating active work.
6. Keep T069/T070/T071 dependency-gated; do not freeze the v1 API early.
