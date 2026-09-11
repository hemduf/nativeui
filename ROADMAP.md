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

Current `main` is `621a56e462590e47e7360e122f3d4031814d8947` and includes completed T067 / PR #219, T045 / PR #210, T037 / PR #151, T058 / PR #154, T036 / PR #155, T065 / PR #133, T034 / PR #135, T060/T042 lifecycle qualification, T052 v0.1 release qualification, the warning-free source-tree baseline, hover correction #212 / PR #213 and Tree paint-ownership correction #152 / PR #153.

The dynamic/overlay critical lane is completing **T061 / issue #73 / PR #216**. Its exact pre-documentation source head `d028d43c3ad95d229f02ff50e975f49a12f5b9e8` is synchronized with current main (`behind_by=0`) and is green across normal Linux X11/macOS/Windows/Linux ASan+UBSan CI plus T042, T045, T052, T060, T065 and T067 dedicated gates. Final source review `5174681221` reports no remaining Blocking/Important finding. The completion documentation changes the exact candidate head, so the documentation-complete head still requires fresh exact-head qualification before merge.

Current dependency frontier:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(done) -> T068
                                                   T058(done) ------------^

style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

dynamic/overlay:   T058(done) -> T061(completion PR #216)
                                      |-> T035 --------------------------> T068
                                      |-> T063 --------------------------> T068
                                      +-> T062

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052(done)

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

critical platform: T060(done) -> T065(done) -> T072 -> T064
                   T041(done) -> T043 -> T066
                                      |-> T068

release:            all explicit convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

T035 and T063 require T061 plus already-complete T034. T062 requires T061 plus already-complete T065. When those downstream owned tickets are simultaneously Ready, priority is T035, then T063, then T062 because T035/T063 directly unblock T068.

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

**T030–T034 and T036 complete; T035 waits for T061.**

- T030 Button — PR #94.
- T031 Checkbox/Radio — PR #95.
- T032 Slider/RangeSlider — PR #115.
- T033 ProgressBar/Meter — PR #123.
- T034 ScrollView — PR #135; `ScrollState` remains sole offset authority with nested wheel bubbling, pointer pan, retained scrollbars and `ensure_visible`.
- T036 ListView/Tabs — PR #155; stable logical selection, composite focus, T034 reveal, automatic tab activation and T059 availability semantics.
- #212 / PR #213 adds deterministic paint-only ListView/Tabs hover and retained pointer-leave lifetime without changing selection/activation semantics.
- T035 ComboBox/DropDown remains dependency-gated on T061 and T034; it must consume the generic T061 overlay layer rather than create popup infrastructure.

## Milestone 6 — Styling, theme and animation

**T037 complete; T038/T039/T040 remain.**

T037 / PR #151 provides typed per-UI Theme values and representative control theme binding. T038 owns typed widget variants, T039 scoped style inheritance, and T040 animation must reuse T065 instead of introducing another scheduler.

## Milestone 7 — Platform and embedded robustness

Delivered foundations include plug-in host isolation, standalone ownership Decision B, T042 lifecycle stress, T053 Objective-C runtime identity, T060 Application ownership, T065 Dispatcher/timers and T045 accessibility architecture.

Current platform order:

1. T072 / issue #84 — shared bounded Linux `libdbus-1` transport; unlocks T064 and Linux T068.
2. T043 / issue #43 / PR #142 — logical/physical resize-scale contract; unlocks T066 and T068.
3. T064 / issue #76 — DesktopServices after T072.
4. T066 / issue #78 — final standalone window controls after T043.

T044 / issue #44 / PR #145 remains a T071 release dependency but is not on the immediate T068/T069 convergence path.

## Milestone 8 — Packaging, virtualization, overlays and release convergence

Delivered foundations include T047/T048 package consumption, T051 performance qualification, T052 v0.1 release gate, T054 application helper, T056 binary data, T057 ResourceManager, T058 dynamic composition, T067 fixed-height virtualized ListView and the T045 semantic architecture it consumes.

### T061 — Generic overlay / portal layer

T061 / issue #73 / PR #216 is in its completion cycle.

Delivered contract:

- one per-UI overlay host/stack with creation-order z-index and monotonically increasing lifetime-safe handles;
- Modal/NonModal and Normal/Ignore policies with `Modal + Ignore` rejected;
- collision-safe public placement names `AnchorBelow`, `AnchorAbove`, `AnchorRight`, `AnchorLeft`, `Center`, `Auto`, preserving the issue's requested placement semantics while avoiding Xlib `Above`/`Below` macro collisions;
- requested/opposite fallback, Auto priority, deterministic equal-area tie handling, finite origin clamping and no automatic resize/scroll;
- Modal barrier preventing input to lower overlays/root while preserving overlays above it; Ignore overlays are pointer-transparent but remain paint-visible;
- outside-dismiss no-click-through and deterministic Escape ownership;
- retained `NodeId` anchoring with relayout tracking and auto-close for missing/Hidden/Collapsed/deactivated anchors;
- modal focus trapping/restoration with `NodeId` restore identity, stale-target fallback and prevention of pointer-focus escape into a later NonModal sibling;
- exact one-cancel lower-capture takeover when a new modal appears, including callback-driven modal creation; exact one-cancel captured-overlay teardown;
- T058 `DynamicChildrenSource` as the sole structural checkpoint/reconciliation mechanism, with reentrant show/close and pre-flush show+close coalescing;
- two-UI isolation, headless/golden coverage, standalone/EmbeddedView smoke parity and dedicated `t061_overlay_portal` interactive/self-test example;
- no native popup window, global overlay registry, automatic anchor styling, tooltip/menu/dialog policy, automatic scrolling/resizing or second mutation queue.

Exact source head `d028d43c3ad95d229f02ff50e975f49a12f5b9e8` passed normal CI `34554637275` (Linux X11/macOS/Windows/Linux ASan+UBSan), T052 `34554637335`, T042 `34554637304`, T045 `34554637284`, T060 `34554637332`, T065 `34554637322` and T067 `34554637270`. Linux X11 reported 93/93 CTests green, including generic focus, dynamic composition, headless, golden, T061 acceptance and the T061 feature self-test. Final source review `5174681221` found no remaining Blocking/Important issue.

This roadmap/context synchronization creates a documentation-complete head that must be requalified exactly before T061 is marked Done/merged. No code requirement is waived by the documentation-only completion step.

### T067 — Fixed-height virtualized ListView

**Complete in PR #219.** T067 provides finite-positive fixed-height virtualization, O(1) visible-range math, bounded visual materialization, stable keyed retained identity, T034/T036 behavior preservation, T058 safe keyed reconciliation and immutable T045 virtual semantic metadata that does not rebuild on ordinary scroll/selection/focus projection.

### T068 convergence

T068 starts only after **all** explicit issue #80 dependencies are Done. It implements T045 semantics through immutable per-view snapshots and native NSAccessibility/UIA/AT-SPI2 bridges, shares T067 virtual metadata, routes mutations through T065 and uses T072 as the sole Linux D-Bus transport.

### Final v1 release path

```text
T036(done) -> T045(done) -> T067(done) -------------------\
T058(done) -> T061 -> T035/T063 --------------------------+--> T068 -> T069 -> T070 -> T071 -> v1.0.0
T065(done) -> T072 -> T064 -------------------------------+
T043 -----------------> T066 ------------------------------/
other explicit T069 dependencies --------------------------/
```

T069 is the final v1 public API freeze and cannot start until every explicit issue #81 dependency is complete. T070 validates the reference application/Getting Started against the frozen API. T071 is validation/release-only on one exact RC SHA; defects found there return to their canonical fix ticket instead of being hidden in release work.

## Immediate cross-lane plan

1. Requalify the documentation-complete T061 exact head and merge PR #216 only at 100% evidenced conformity.
2. After T061, advance T035, then T063, then T062 as dependencies permit.
3. Keep T068 blocked until every explicit issue #80 dependency is Done.
4. Continue T072/T043 -> T064/T066 in the independent platform lane.
5. Continue T038 -> T039 and T040 in the style lane as capacity permits.
6. Keep T069/T070/T071 dependency-gated and do not freeze the v1 API early.

## Prioritization rule

```text
core correctness
  -> layout/input/render/text foundations
  -> widgets/styles/overlays/platform services
  -> accessibility convergence and public API freeze
  -> reference package/release
```

This is architectural progression, not unnecessary serialization. Independent tickets may progress concurrently only when explicit dependencies and the repository concurrency rules allow it.
