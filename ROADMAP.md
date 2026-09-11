# NativeUI roadmap

**Updated:** 2026-09-11

NativeUI is a reusable C++20 desktop retained-mode UI toolkit: Pugl owns native views/events, Skia owns rendering, and NativeUI owns retained composition, layout, input/focus, widgets, styling, resources and packaging. GitHub Issues are the source of truth for exact ticket scope, status and dependencies.

## Execution rules

- explicit GitHub `Dependencies:` are hard gates;
- resume existing canonical branches/PRs before creating work;
- among Ready work, prefer priority and downstream unblock value;
- behavior/configuration changes use RED -> GREEN -> REFACTOR;
- implementation progress and issue conformity are separate; Done/merge requires an explicit issue-to-code/test evidence matrix with no unchecked requirement;
- code-changing tickets require exact-head validation and a final `CODE_REVIEW.md` record with no Blocking/Important finding;
- NativeUI-owned targets must compile with zero unapproved warnings and the default empty `NATIVEUI_ALLOWED_WARNINGS`;
- unrelated lanes may continue only within the repository concurrency rules while another PR waits exclusively on external CI;
- every completion cycle synchronizes issue status, `CONTEXT.md` and this roadmap;
- feature tickets ship an interactive example plus deterministic `--self-test`.

## Current execution snapshot

The pre-T043 completion baseline is `main` `fc15cbf4798b5071570aac5c8b0819c14ddd56c9`. It includes completed T061 / PR #216, T067 / PR #219, T045 / PR #210, T037 / PR #151, T058 / PR #154, T036 / PR #155, T065 / PR #133, T034 / PR #135, T060/T042 lifecycle qualification, T052 v0.1 release qualification, the warning-free source-tree baseline, hover correction #212 / PR #213 and Tree paint-ownership correction #152 / PR #153.

T043 / issue #43 / PR #142 is now a mergeable completion candidate rebased onto that exact baseline. The current candidate preserves T061 overlay/pointer-leave behavior, restores root CMake registration on the current feature/test set, and adds direct deterministic proofs for the two previously missing acceptance edges: one resulting layout per configure snapshot and one native request with no recursive echo request.

Current dependency frontier:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(done) -> T068
                                                   T058(done) ------------^

style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

dynamic/overlay:   T058(done) -> T061(done)
                                      |-> T035 --------------------------> T068
                                      |-> T063 --------------------------> T068
                                      +-> T062

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052(done)

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

critical platform: T060(done) -> T065(done) -> T072 -> T064
                   T041(done) -> T043(completion PR #142) -> T066
                                                   |-------> T068

release:            all explicit convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

T035 and T063 require completed T061 plus already-complete T034. T062 requires completed T061 plus already-complete T065. T043 unlocks T066 and is an explicit T068 dependency. T044 remains an explicit T071 dependency and is being reconciled independently rather than hidden inside T043.

## Milestone 0 — Baseline hardening

**Complete.** T001–T006 provide core/state tests, retained lifecycle, public-header split and invalidation foundations. #163 / PR #181 makes unapproved NativeUI-owned compiler warnings Blocking.

## Milestone 1 — Layout system

**Complete.** T007–T012 provide constraints, alignment/distribution, flex, grid, scroll state/layout and clipping/overflow foundations.

## Milestone 2 — Input, focus and gestures

**Complete baseline.** T013–T018 provide event propagation, focus scopes/restoration, pointer capture, wheel normalization, gestures, commands and drag/drop primitives. Later tickets extend only generic retained seams required by explicit contracts.

## Milestone 3 — Rendering and graphics

**Complete.** T019–T024 provide transforms, paths, gradients, images, SVG/resources, caches and deterministic rendering/golden tests.

## Milestone 4 — Text system

**Complete.** T025–T029 provide text editing, Label/fonts/fallback, TextArea, UTF-8 selection/navigation and platform IME composition bridges.

## Milestone 5 — Standard widget set

**T030–T034 and T036 complete; T035 is unblocked by completed T061.**

- T030 Button — PR #94.
- T031 Checkbox/Radio — PR #95.
- T032 Slider/RangeSlider — PR #115.
- T033 ProgressBar/Meter — PR #123.
- T034 ScrollView — PR #135; `ScrollState` remains sole offset authority with nested wheel bubbling, pointer pan, retained scrollbars and `ensure_visible`.
- T036 ListView/Tabs — PR #155; stable logical selection, composite focus, T034 reveal, automatic tab activation and T059 availability semantics.
- #212 / PR #213 adds deterministic paint-only ListView/Tabs hover and retained pointer-leave lifetime without changing selection/activation semantics.
- T035 ComboBox/DropDown must consume the generic T061 overlay layer rather than create popup infrastructure.

## Milestone 6 — Styling, theme and animation

**T037 complete; T038/T039/T040 remain.**

T037 / PR #151 provides typed per-UI Theme values and representative control theme binding. T038 owns typed widget variants, T039 scoped style inheritance, and T040 animation must reuse T065 instead of introducing another scheduler.

## Milestone 7 — Platform and embedded robustness

Delivered foundations include plug-in host isolation, standalone ownership Decision B, T042 lifecycle stress, T053 Objective-C runtime identity, T060 Application ownership, T065 Dispatcher/timers and T045 accessibility architecture.

### T043 — Resize/scale negotiation

T043 / issue #43 / PR #142 is in its final qualification cycle.

Completion-candidate contract:

- public size/layout/preferred/invalidation geometry is logical; native view/framebuffer geometry is physical;
- scale is per view, initialized to `1.0f`, replaced only by finite strictly-positive observations and never stored globally;
- invalid platform scale observations retain the exact previous valid scale and produce a bounded diagnostic instead of invalid layout/math;
- valid logical `set_size` converts exactly once to a covering native extent, emits at most one native request and records a pending request only after native acceptance;
- native configure is authoritative for the actual viewport, updates physical size plus scale as one snapshot, and dispatches at most one resulting logical layout;
- configure echoes never recursively call `set_size`; zero/non-finite physical configure extents are transient non-renderable states;
- fractional input/dirty/text/drop conversion uses the same retained scale exactly once;
- embedded child resize does not resize its parent, and preferred-size notifications are advisory, epsilon-coalesced and reentrancy/lifetime safe;
- two-view scale state remains isolated;
- dedicated deterministic conversion/sequencing tests, embedded platform smoke, registration contract and `t043_resize_scale --self-test` are registered on the current root build.

The final deterministic gap closure adds synthetic resize-only, scale-only and combined configure snapshots with exactly one layout callback per accepted snapshot, plus an instrumented request boundary proving one logical request -> one native request -> authoritative configure with no echo recursion. Invalid/failed requests are also proven not to mutate authoritative geometry incorrectly.

Merge remains gated on exact-head normal CI (Linux X11/macOS/Windows/Linux ASan+UBSan), relevant dedicated lifecycle/release/application/dispatcher/accessibility/virtual-list workflows, the complete issue-to-code/test matrix and a final `CODE_REVIEW.md` review with no Blocking/Important finding. Historical stale-head success is not sufficient.

After T043 completes, T066 / issue #78 becomes the remaining direct window-control successor. T072 / issue #84 independently unlocks T064.

T044 / issue #44 / PR #145 remains a T071 release dependency and requires its own current-main reconciliation, native capture evidence and exact-head qualification.

## Milestone 8 — Packaging, virtualization, overlays and release convergence

Delivered foundations include T047/T048 package consumption, T051 performance qualification, T052 v0.1 release gate, T054 application helper, T056 binary data, T057 ResourceManager, T058 dynamic composition, T061 overlay/portal infrastructure, T067 fixed-height virtualized ListView and the T045 semantic architecture it consumes.

### T061 — Generic overlay / portal layer

**Complete in PR #216.** T061 provides one generic retained in-view overlay/portal layer per UI using T058 structural reconciliation. It includes creation-order z-order, modal/non-modal and pointer-transparent policies, deterministic placement, retained anchor tracking, modal focus trapping/restoration, lower-capture cancellation, no-click-through dismissal, reentrant show/close safety, standalone/EmbeddedView parity and the dedicated `t061_overlay_portal` example/self-test.

### T067 — Fixed-height virtualized ListView

**Complete in PR #219.** T067 provides finite-positive fixed-height virtualization, O(1) visible-range math, bounded visual materialization, stable keyed retained identity, T034/T036 behavior preservation, T058 safe keyed reconciliation and immutable T045 virtual semantic metadata that does not rebuild on ordinary scroll/selection/focus projection.

### T068 convergence

T068 starts only after **all** explicit issue #80 dependencies are Done. It implements T045 semantics through immutable per-view snapshots and native NSAccessibility/UIA/AT-SPI2 bridges, shares T067 virtual metadata, routes mutations through T065 and uses T072 as the sole Linux D-Bus transport.

### Final v1 release path

```text
T036(done) -> T045(done) -> T067(done) -------------------\
T058(done) -> T061(done) -> T035/T063 --------------------+--> T068 -> T069 -> T070 -> T071 -> v1.0.0
T065(done) -> T072 -> T064 -------------------------------+
T043(completion) -----> T066 ------------------------------/
other explicit T069 dependencies --------------------------/
```

T069 is the final v1 public API freeze and cannot start until every explicit issue #81 dependency is complete. T070 validates the reference application/Getting Started against the frozen API. T071 is validation/release-only on one exact RC SHA; defects found there return to their canonical fix ticket instead of being hidden in release work.

## Immediate cross-lane plan

1. Finish exact-head qualification/review for T043 / PR #142 and merge only at 100% evidenced conformity.
2. Reconcile, qualify and merge T044 / PR #145 independently; do not reuse T043 platform evidence.
3. Advance T035, then T063, then T062 as dependencies and current active work permit.
4. Continue T072 -> T064 and, after T043, T066 in the independent platform lane.
5. Keep T068 blocked until every explicit issue #80 dependency is Done.
6. Continue T038 -> T039 and T040 in the style lane as capacity permits.
7. Keep T069/T070/T071 dependency-gated and do not freeze the v1 API early.

## Prioritization rule

```text
core correctness
  -> layout/input/render/text foundations
  -> widgets/styles/overlays/platform services
  -> accessibility convergence and public API freeze
  -> reference package/release
```

This is architectural progression, not unnecessary serialization. Independent tickets may progress concurrently only when explicit dependencies and the repository concurrency rules allow it.
