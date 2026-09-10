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

The pre-T058 merge baseline is `main` at `96dc57a2cc3395fcb00f5cd2c0398f3da60b4675`. It contains T034 and T065, the explicit T060 Application/multi-window ownership model, post-T060 T042 lifecycle qualification, standard widgets through T034, and the T052 v0.1 developer-preview release gate.

Cross-cutting rendering regression #152 / PR #153 removed Tree-owned visual decoration: `Tree::paint()` no longer forces a viewport background or draws a hard-coded keyboard/mouse help line. Generic retained-tree painting is consumer/component-owned; renderer-level framebuffer clear remains outside Tree.

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
- T052 / PR #120: v0.1 developer-preview release gate.
- T054 / PR #119: `nativeui_add_application()` high-level native application package helper.
- T056 / PR #111: deterministic `nativeui_add_binary_data()` packaging.
- T057 / PR #126: immutable non-owning `ResourceManager` plus allocating provider adapter.
- T060 / PR #118: one explicit `ui::Application` owns one standalone `PUGL_PROGRAM` world and multiple independent `StandaloneWindow(Application&, ...)` views.
- #139 / PR #140: T042 stress qualification of the supported T060 multi-window path while preserving #64 Decision B for the legacy independent-PROGRAM compatibility path.
- T065 / PR #133: bounded UI-thread dispatcher/timer service and native wake integration; issue #77 is Done.

## T058 dynamic composition lane

T058 / issue #70 / PR #154 introduces explicit retained dynamic containers without a global VDOM or reconciler:

- `ui::If`, `ui::Switch` and keyed `ui::ForEach` reconcile only their owned retained subtrees;
- State observers enqueue structural work and never splice/delete nodes synchronously inside callbacks;
- multiple writes coalesce to the latest source snapshot at the next structure-dependent checkpoint;
- unchanged keyed children retain component/NodeId identity across insert/remove/reorder; changing a key forces teardown and a fresh identity;
- duplicate-key snapshots are rejected atomically and preserve the previous valid retained subtree;
- removal cancels capture and repairs focus while nodes are still valid, then deactivates/unmounts/destroys before replacement construction;
- removed subscriptions/invalidators are detached before destruction, and NodeIds use a per-tree lifetime high-water mark so removed IDs are never recycled;
- lifecycle-triggered structural mutations are deferred to later passes;
- a top-level checkpoint performs at most exactly 32 reconciliation passes; remaining dirty work is preserved for a later checkpoint and exposes the bounded `structural reconciliation pass limit exceeded` diagnostic;
- focus rehoming honors the nearest surviving trapping FocusScope, including nested-scope removal cases;
- `examples/features/t058_dynamic_composition.cpp` provides interactive behavior and deterministic `--self-test` coverage.

The T058 branch is refreshed against the #153 Tree-paint ownership fix so dynamic checkpoint calls coexist with the consumer-owned painting contract. PR #154 remains the canonical T058 stream; exact-head CI and the final mandatory `CODE_REVIEW.md` audit are the merge gates.

## Current dependency frontier

```text
widgets/layout:    T059(done) -> T030(done) -> T031(done)
                                       |-> T032(done)
                                       |-> T033(done)
                                       +-> T034(done) -> T036
                                                      -> T035 after T061

dynamic/overlay:   T058(PR #154) -> T061 -> T035 -> T063
                                   |             ^
                                   +------------ T063 also requires T034(done)
                                   +-> T062 after T065(done)
                   T058 ----------> T067 / T068 dependency paths

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052(done)

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

critical platform: T065(done) -> T072 -> T064
                   T041(done) -> T043 -> T066
                                      |-> T068
                   dynamic/widget/platform convergence -> T068 -> T069 -> T070 -> T071
```

For the owned overlay chain, after T061 merges: T035 and T063 are enabled by already-complete T034, while T062 is enabled by already-complete T065. If those downstream tickets are simultaneously Ready, the critical-unblock priority is T035, then T063, then T062.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

T058 additionally requires its targeted dynamic composition and lifecycle coverage, feature `--self-test`, normal Linux X11/Windows/macOS CI, Linux ASan+UBSan, and applicable T042/T052/T060/T065 regression workflows on the exact final head.

## Next actions

1. Finish exact-head qualification and final `CODE_REVIEW.md` audit for T058 / PR #154; merge only when every required executed gate is green and no Blocking/Important finding remains.
2. Immediately start/resume T061 / issue #73 after T058 merges.
3. After T061, prioritize T035 / issue #35, then T063 / issue #75, then T062 / issue #74 as their explicit dependencies permit.
4. Keep T034/T036/T045/T067/T068, T043/T065/T072/T064/T066, and T037-T040/T049/T050 in their separate lanes; do not duplicate active work.
