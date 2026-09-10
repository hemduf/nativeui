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

`main` is `cbf68026fc0780f1e5d04e2120cae76b188ed780`, which includes T036 / PR #155, T065 / PR #133, T034 / PR #135, the explicit T060 Application/multi-window ownership model, post-T060 T042 lifecycle qualification from #139, the T052 v0.1 developer-preview release gate, and the merged Tree paint-ownership correction #152 / PR #153.

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
- T060 / PR #118: one explicit `ui::Application` owns one standalone `PUGL_PROGRAM` world and multiple independent `StandaloneWindow(Application&, ...)` views.
- T065 / PR #133: bounded UI-thread Dispatcher/timer service with native wake integration and deterministic timer/fairness semantics.
- #139 / PR #140: T042 stress qualification of the supported T060 multi-window path while preserving #64 Decision B for the legacy independent-PROGRAM compatibility path.

## T036 UI/accessibility lane — complete

T036 / issue #36 / PR #155 is complete and squash-merged to `main` as `cbf68026fc0780f1e5d04e2120cae76b188ed780`.

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

The completion review found and corrected one Important gap: application-originated ListView selection did not reveal an offscreen selected row. GREEN commit `fe51752e9093df0f23a68c4db0e1ef9532b16da7` reuses the per-row State subscription so the matching selected row applies T034 `ensure_visible`. Exact completion head `458b48ed60e187a1a89587b39735da3d387b0532` passed CI #861 on Linux ASan+UBSan, Linux X11, Windows and macOS plus T042 #433, T052 #178, T060 #256 and T065 #82. Final `CODE_REVIEW.md` review reports no remaining Blocking/Important finding.

T045 is now the next UI/accessibility dependency. T067 additionally waits for T045 and T058; T068 then converges the UI/accessibility chain with its explicit overlay/platform dependencies.

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

## Current dependency frontier

```text
critical UI:       T034(done) -> T036(done) -> T045 -> T067 -> T068
                                           T058(done required) ----^      ^

widgets/overlay:   T034(done) + T061 -> T035 ---------------------------> T068
                   T061 + T034 -> T063 -------------------------------> T068

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

T045 is dependency-unblocked by T036 completion and should move to Ready. T072 is already active after T065 completion. T064 depends on T065 and T072. T066 depends on T060 and T043. T043 may progress independently. T044 is not on the T068/T069 critical path and remains lower priority until these prerequisites are complete.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

T036 completion exact head `458b48ed60e187a1a89587b39735da3d387b0532` passed the dedicated T036 contract/tests/example self-test, normal Linux X11/Windows/macOS/Linux ASan+UBSan CI, T042 Lifecycle Stress, T060 Application Contract, T052 release gate and T065 dispatcher contract before merge.

## Next actions

1. Move T045 / issue #45 to Ready and start it as the next UI/accessibility critical-path item; it freezes the ordinary and virtual-collection accessibility semantics required before T067/T068.
2. Keep T067 dependency-gated until T045 + T058 are Done, then implement it before T068.
3. Continue active T072 in the independent platform lane; after it completes, T064 becomes available.
4. Continue T043 / PR #142 independently and finish it before T066/T068.
5. Keep unrelated style/dynamic/overlay/release work in their own lanes and do not duplicate active PRs.
