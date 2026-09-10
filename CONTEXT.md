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

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline

`main` is `645679b3c375d54c68227753583c9cb0a74fd801`, which includes T034 / PR #135 in addition to the explicit T060 Application/multi-window ownership model, post-T060 T042 lifecycle qualification from #139, standard widgets through T034 and the T052 v0.1 developer-preview release gate.

Cross-cutting rendering regression #152 / PR #153 removes Tree-owned visual decoration: `Tree::paint()` no longer forces `colors::background` across the viewport and no longer draws the hard-coded keyboard/mouse help line. Generic retained-tree painting is therefore consumer/component-owned. The headless renderer now mirrors the GPU renderer's black framebuffer clear, and its regression verifies that an otherwise empty Tree adds no styled background or instructional overlay beyond that renderer-level clear.

Relevant completed foundations:

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- #64 / PR #90: standalone PROGRAM-world ownership Decision B.
- T059 / PR #89: inherited visibility/enabled/read-only state.
- T030 / PR #94: Button.
- T031 / PR #95: Checkbox + typed RadioGroup/RadioButton.
- T032 / PR #115: Slider + RangeSlider.
- T033 / PR #123: ProgressBar + Meter.
- T034 / PR #135: ScrollView with change-based wheel bubbling, optional pointer pan, retained overlay scrollbars, focus reveal and T059 availability integration.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform()`.
- T048 / PR #99: relocated external consumers and macOS two-consumer isolation.
- T051 / PR #116: reproducible Release benchmark and regression policy.
- T052 / PR #120: v0.1 developer-preview release gate with clean-cache bootstrap, exact package consumer, lifecycle, benchmark, idle-invalidation and platform qualification.
- T054 / PR #119: `nativeui_add_application()` high-level native application package helper.
- T056 / PR #111: deterministic `nativeui_add_binary_data()` packaging.
- T057 / PR #126: immutable non-owning `ResourceManager` plus allocating provider adapter.
- T060 / PR #118: one explicit `ui::Application` owns one standalone `PUGL_PROGRAM` world and multiple independent `StandaloneWindow(Application&, ...)` views.
- #139 / PR #140: T042 stress qualification of the supported T060 multi-window path while preserving #64 Decision B for the legacy independent-PROGRAM compatibility path.

## T065 platform/event lane — completion candidate

T065 / issue #77 / PR #133 is the active critical platform prerequisite. The branch is refreshed onto current `main` and is no longer behind T034.

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

TDD/review corrections already incorporated include stable mutable repeating callback state, root CMake/header/test registration, dedicated workflow path coverage, safe worker wake primitives, X11 portability fixes, the test-only self-post LSan cycle, and lock-free user-capture destruction boundaries.

The pre-refresh exact code head `d12064ab485aa9f21128eeb2059d40b1f8c3f969` passed normal CI, T065 Dispatcher Contract, T065 Platform Dispatcher, T060 Application Contract, T042 Lifecycle Stress and T052 Release Gate. PR #133 has since been cleanly refreshed onto `main` through merge candidate `e302b1e5f26ab4e078f80c3971c76d13cb7ed09f`; this documentation synchronization creates the final completion candidate and therefore requires a fresh exact-head validation pass plus the final exact-head `CODE_REVIEW.md` record before merge.

The legacy `StandaloneWindow(UI&, ...)` path remains pre-v1 compatibility only and is removed by T069. T065 does not introduce a hidden shared Application or a second supported PROGRAM-world ownership model.

## Current dependency frontier

```text
widgets/layout:    T059(done) -> T030(done) -> T031(done)
                                       |-> T032(done)
                                       |-> T033(done)
                                       +-> T034(done) -> T036
                                                      -> T035 only after T061

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052(done)

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

critical platform: T060(done) -> T065(completion PR #133) -> T072 -> T064
                   T041(done) -> T043(active PR #142) -> T066
                                              |-------> T068
                   T065 + T072 + T043 + other feature deps ------> T068 -> T069
```

T072 must not start until T065 is merged. T064 depends on T065 and T072. T066 depends on T060 and T043. T043 may progress independently while T065 is waiting only on CI. T044 is not on the T068/T069 critical path and remains lower priority until these prerequisites are complete.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

T065 completion additionally requires its dedicated core and platform dispatcher workflows, T060 Application Contract, T042 Lifecycle Stress, normal Linux X11/Windows/macOS/Linux ASan+UBSan CI and the current T052 release-regression gate on the exact final head.

## Next actions

1. Complete fresh exact-head qualification and final `CODE_REVIEW.md` review for T065 / PR #133 after this documentation synchronization; merge only if every required executed gate is green and no Blocking/Important finding remains.
2. Immediately after T065 merges, start/resume T072 / issue #84; it is the shared Linux D-Bus prerequisite for T064 and T068.
3. Advance existing T043 / PR #142 whenever T065 is waiting only on external CI; merge T043 before T066 and before T068 integration.
4. After T072 and T043 are complete, finish T064 and T066 according to their explicit dependencies and existing branch state.
5. Keep unrelated widget/style/dynamic/accessibility/release work in their own lanes and do not duplicate active PRs.
