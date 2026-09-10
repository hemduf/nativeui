# NativeUI roadmap

**Updated:** 2026-09-10

This roadmap turns NativeUI into a reusable desktop retained-mode UI toolkit while preserving the architecture: Pugl for native views/events, Skia for rendering, NativeUI for retained behavior/layout/input/widgets/resources. GitHub Issues remain the source of truth for exact ticket status and dependencies.

## Execution rules

- explicit GitHub `Dependencies:` are hard gates;
- resume existing work before creating another stream;
- among Ready work, prefer priority and downstream unblock value;
- behavior/configuration changes use test-first RED -> GREEN -> REFACTOR;
- code-changing tickets require exact-head validation and a `CODE_REVIEW.md` record;
- unrelated lanes may continue while another PR waits only on external CI;
- every completion cycle synchronizes `CONTEXT.md` and this roadmap;
- feature tickets ship an interactive example plus deterministic `--self-test`.

## Current execution snapshot

The pre-T058 merge baseline is `main` at `96dc57a2cc3395fcb00f5cd2c0398f3da60b4675`. It contains T034, T065, the supported T060 multi-window lifecycle model, post-T060 T042 stress qualification, and the T052 v0.1 developer-preview release/package gate.

Cross-cutting rendering regression #152 / PR #153 removed implicit Tree-level visual decoration: `Tree::paint()` no longer paints a default viewport background or hard-coded instruction line. Visual backgrounds and overlays belong to consumer/component composition; renderer framebuffer clear remains separate.

Current dependency frontier:

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
                   feature/platform convergence -> T068 -> T069 -> T070 -> T071
```

## Milestone 0 — Baseline hardening

**Complete.** T001–T006 provide core/state tests, retained lifecycle, public-header split and invalidation foundations.

## Milestone 1 — Layout system

**Complete.** T007–T012 provide constraints, alignment/distribution, flex, grid, scroll state/layout and clipping/overflow foundations.

## Milestone 2 — Input, focus and gestures

**Complete baseline.** T013–T018 provide event propagation, focus scopes, pointer capture, wheel normalization, gestures, commands and drag/drop primitives. Later widget work may add generic retained-tree seams only when required by those contracts; platform-specific widget input paths remain forbidden.

## Milestone 3 — Rendering and graphics

**Complete.** T019–T024 provide transforms, paths, gradients, images, SVG/resources, caches and deterministic rendering tests.

## Milestone 4 — Text system

**Complete.** T025–T029 provide the text-edit model, Label, fonts/fallback, TextArea, UTF-8 selection/navigation and platform IME composition bridges.

## Milestone 5 — Standard widget set

**Status: T030–T034 complete.**

### T030 — Button

Complete in PR #94. Pointer/keyboard activation, capture, disabled-state handling and reentrant callbacks use generic retained input/state semantics.

### T031 — Checkbox and Radio

Complete in PR #95. Checkbox and typed RadioGroup/RadioButton share deterministic value/state ownership and T059 availability semantics.

### T032 — Slider and RangeSlider

Complete in PR #115. Shared numeric-domain and track-axis helpers handle finite ranges, horizontal/vertical mapping, keyboard editing, nearest-thumb RangeSlider selection, no crossing, T059 read-only/disabled behavior and bounded reentrancy.

### T033 — ProgressBar and Meter

Complete in PR #123. The widgets are display-only, use finite presentation normalization without State writeback, support horizontal/vertical fill and optional formatting, and add no hidden timer or interaction state.

### T034 — ScrollView

Complete in PR #135.

Delivered contract:

- `ScrollState` remains the sole offset/metrics authority;
- wheel events are handled only when effective offset changes, allowing nested boundary bubbling;
- optional pointer panning uses toolkit capture with no inertia/timer;
- retained overlay scrollbars use fixed 8 logical px thickness and 18 px minimum thumb;
- public Nearest/Start/Center/End `ensure_visible` plus focused-descendant reveal;
- T059 Disabled/Hidden/Collapsed interaction suppression/cancellation while ReadOnly remains scrollable;
- generic pointer targetability is independent from keyboard focus.

T036 is an independent widget/layout continuation. T035 additionally waits for T061 and is owned by the dynamic/overlay critical chain.

## Milestone 6 — Styling, theme and animation

**In progress through independent dependencies.** T037 -> T038 -> T039 owns typed theme/style/scoped inheritance. T040 consumes the completed T065 dispatcher/timer service and remains in the style/animation lane.

## Milestone 7 — Platform and embedded robustness

Core lifecycle/consumer-safety baseline is delivered:

- #62 plug-in-host instance/runtime safety;
- #64 standalone ownership Decision B;
- T053 consumer-specific Objective-C bridge naming;
- T042 deterministic lifecycle stress;
- T060 explicit one-Application/one-PROGRAM-world multi-window ownership;
- #139 post-T060 T042 qualification;
- T065 bounded dispatcher/timer service with native wake integration.

### T065 — Dispatcher/timer service

T065 / issue #77 is Done. Its contract includes exact per-owner limits (65,536 pending tasks, 8,192 active timers, 1,024 callbacks per checkpoint), weak thread-safe Dispatcher handles, deterministic FIFO execution, one-shot/fixed-delay timers, fake monotonic time, callback destruction outside locks, native worker wake integration and host-driven non-blocking EmbeddedView dispatch.

### Critical downstream platform order

1. **T072 / issue #84** — shared bounded Linux `libdbus-1` transport; unlocks T064 and Linux T068.
2. **T043 / issue #43** — resize/scale contract; required by T066 and T068.
3. **T064 / issue #76** — DesktopServices, after T065 + T072.
4. **T066 / issue #78** — final standalone window controls, after T060 + T043.

T044 remains a T071 release dependency but is not on the T068/T069 critical path.

## Milestone 8 — Dynamic UI, packaging, tooling and release

Delivered package foundations include T047 low-level package export, T048 relocated external consumers, T051 performance-regression harness, T052 v0.1 developer-preview release gate, T054 native application helper, T056 deterministic binary-data generation and T057 ResourceManager.

### T058 — Dynamic subtree composition

T058 / issue #70 / PR #154 is the current dynamic/overlay critical-chain merge candidate.

Delivered scope:

- explicit `If`, `Switch` and keyed `ForEach` retained dynamic containers only; ordinary static `Spec` remains one-time construction data;
- per-tree dirty dynamic records with no global reconciler/registry;
- observers enqueue work rather than mutating retained structure reentrantly;
- unchanged keys preserve retained component/NodeId identity through insert/remove/reorder;
- duplicate-key snapshots reject atomically without partial teardown;
- capture/focus cleanup occurs while removed nodes remain valid, followed by deactivate/unmount/destruction before replacement construction;
- stale focus-restore targets and removed subscriptions/invalidators are purged safely;
- runtime NodeIds are monotonic for the Tree lifetime and are not recycled after removal;
- lifecycle-triggered writes are deferred to later reconciliation passes;
- exact 32-pass top-level checkpoint limit; pass-33 work remains queued for a later checkpoint and emits `structural reconciliation pass limit exceeded`;
- nested trapping FocusScope rehoming is deterministic;
- interactive `t058_dynamic_composition` example plus deterministic `--self-test`.

The final T058 candidate is refreshed against current `main`, preserving #153's consumer-owned Tree paint contract while retaining all dynamic checkpoint hooks. Merge requires the exact-head platform/sanitizer/regression matrix and final mandatory `CODE_REVIEW.md` audit to be green/clean.

### Owned v1 overlay critical chain

After T058 merges, execute strictly:

```text
T058 -> T061
          |-> T035 after T034(done)
          |-> T063 after T034(done)
          +-> T062 after T065(done)
```

When T035, T063 and T062 are simultaneously Ready, prioritize **T035 -> T063 -> T062** because that order maximizes T068/T069 unblock value.

- **T061 / #73**: generic per-UI overlay/portal stack, using T058's safe mutation checkpoints rather than a second reconciler.
- **T035 / #35**: ComboBox/PopupMenu on T061, with snapshot options, deterministic focus/dismissal and close-before-callback behavior.
- **T063 / #75**: one-modal-per-UI Dialog policy over T061/T034 with exact close/result/reentrancy rules.
- **T062 / #74**: text-only Tooltip over T061 + completed T065 timers, with deterministic hover/focus delays and pointer-transparent overlay behavior.

T058 also unlocks independent T067/T068 dependency paths, but those tickets remain owned by their separate lanes.

### T052 — v0.1 developer-preview release gate

T052 / issue #52 / PR #120 is complete. It establishes infrastructure/package release validation without claiming NativeUI 1.0 product completeness: pinned dependency bootstrap/checksums, relocated low-level package consumption, T053 Objective-C namespace isolation, lifecycle/performance/idle gates, and developer-preview release/legal documentation.

### Final v1 package/release frontier

```text
dynamic/widget/platform convergence ----\
T065 -> T072 -> T064 --------------------+-> T068 -> T069 -> T070 -> T071
T043 -> T066 ----------------------------/
other explicit T069 dependencies -------/
```

T069 is the final v1 public API freeze and must not start until every explicit dependency in issue #81 is complete. T070 validates the production reference application/Getting Started against that frozen API. T071 is validation/release-only on one exact RC SHA.

## Immediate cross-lane plan

1. Finish final exact-head qualification/review and merge T058 / PR #154.
2. Start T061 immediately after T058 merges.
3. After T061, advance T035 first, T063 second and T062 third whenever their explicit dependencies are satisfied.
4. In parallel lanes, continue T043 and T072/T064 plus the independent style/widget/accessibility chains without duplicate ownership.
5. Keep T069/T070/T071 dependency-gated; do not freeze the v1 API early.

## Prioritization rule

Preserve the architectural direction:

```text
core correctness
  -> layout/input/render/text foundations
  -> widgets and styles
  -> dynamic/overlay + platform/lifecycle services
  -> accessibility/public API freeze
  -> reference package/release
```

This is architectural progression, not serialization. Independent tickets may proceed concurrently once explicit dependencies are satisfied.
