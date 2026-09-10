# NativeUI roadmap

**Updated:** 2026-09-10

This roadmap turns NativeUI into a reusable desktop retained-mode UI toolkit while preserving the architecture: Pugl for native views/events, Skia for rendering, NativeUI for retained behavior/layout/input/widgets/resources. GitHub Issues remain the source of truth for exact ticket status and dependencies.

## Execution rules

- explicit GitHub `Dependencies:` are hard gates;
- resume existing work before creating another stream;
- among Ready work, prefer priority and downstream unblock value;
- behavior/configuration changes use test-first RED -> GREEN -> REFACTOR;
- code-changing tickets require exact-head validation and a `CODE_REVIEW.md` record;
- NativeUI-owned targets must compile with zero unapproved warnings and the default empty `NATIVEUI_ALLOWED_WARNINGS`;
- unrelated lanes may continue while another PR waits only on external CI;
- every completion cycle synchronizes `CONTEXT.md` and this roadmap;
- feature tickets ship an interactive example plus deterministic `--self-test`.

## Current execution snapshot

The T045 completion branch is synchronized with `main` `19c14970918105ebef059b41db8cdcfcb4b5de64`. Main contains T037 / PR #151 plus the completed T058, T036, T065, T034, T060/T042 lifecycle, T052 v0.1 developer-preview release gate, warning-free source-tree baseline and Tree paint-ownership correction.

The Critical UI lane has advanced from T034/T036 to **T045 / PR #210**. T045's accessibility semantic types, 100k virtual-collection contract/tests and complete NSAccessibility/UIA/AT-SPI2 mapping design are implemented and previously exact-head green. The current-main refresh and completion documentation require one fresh exact-head qualification before merge. Once merged, T067 becomes ready because T034, T036 and T058 are already complete.

Current dependency frontier:

```text
critical UI:       T034(done) -> T036(done) -> T045(PR #210) -> T067 -> T068
                                                   T058(done) -----------^

style:             T037(done) -> T038 -> T039
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
                   T065 + T072 + T043 + remaining feature deps -> T068 -> T069
```

## Milestone 0 — Baseline hardening

**Complete.** T001–T006 provide core/state tests, retained lifecycle, public-header split and invalidation foundations. #163 / PR #181 establishes warning-free NativeUI-owned source-tree builds as a project-wide quality gate.

## Milestone 1 — Layout system

**Complete.** T007–T012 provide constraints, alignment/distribution, flex, grid, scroll state/layout and clipping/overflow foundations.

## Milestone 2 — Input, focus and gestures

**Complete baseline.** T013–T018 provide event propagation, focus scopes, pointer capture, wheel normalization, gestures, commands and drag/drop primitives.

## Milestone 3 — Rendering and graphics

**Complete.** T019–T024 provide transforms, paths, gradients, images, SVG/resources, caches and deterministic rendering tests.

## Milestone 4 — Text system

**Complete.** T025–T029 provide the text-edit model, Label, fonts/fallback, TextArea, UTF-8 selection/navigation and platform IME composition bridges.

## Milestone 5 — Standard widget set

**Status: T030–T034 and T036 complete; T035 remains downstream of T061.**

- **T030 Button:** complete in PR #94.
- **T031 Checkbox/Radio:** complete in PR #95.
- **T032 Slider/RangeSlider:** complete in PR #115.
- **T033 ProgressBar/Meter:** complete in PR #123.
- **T034 ScrollView:** complete in PR #135 with `ScrollState` as sole offset authority, change-based wheel bubbling, optional pointer pan, retained overlay scrollbars, deterministic `ensure_visible` and focus reveal.
- **T036 ListView/Tabs:** complete in PR #155 with stable key/value selection, composite focus, T034 reveal integration, automatic Tabs activation, T059 Collapsed inactive panels, deterministic tests/goldens and `t036_list_tabs --self-test`.

T036 deliberately remains non-virtualized; T067 owns fixed-height virtualization after T045 and T058.

## Milestone 6 — Styling, theme and animation

**Status: T037 complete; T038/T039 next.**

T037 / PR #151 provides strongly typed per-UI Theme values, deterministic defaults, paint-vs-layout invalidation classification, representative Button/Slider theme binding, public-header isolation and a feature self-test. T038 owns typed widget style variants, T039 scoped inheritance, and T040 animation must reuse T065 rather than create a second scheduler.

## Milestone 7 — Platform and embedded robustness

Delivered lifecycle/platform foundations include:

- #62 plug-in-host instance/runtime safety;
- #64 standalone ownership Decision B;
- T042 deterministic lifecycle stress;
- T053 consumer-specific Objective-C bridge naming;
- T060 explicit one-Application/one-PROGRAM-world multi-window ownership;
- #139 post-T060 T042 multi-window stress qualification;
- T065 bounded UI-thread dispatcher/timer service.

### T045 — Accessibility semantic architecture

T045 / issue #45 / PR #210 is the active Critical UI completion candidate.

Delivered design/API contract:

- backend-neutral closed `SemanticRole`/`SemanticAction` sets and semantic state/value/range/change-category data;
- stable `SemanticId` logical identity and immutable semantic snapshot/value model;
- concrete data-only virtual collection semantics suitable for 100k-item lists without visual row materialization;
- virtual item identity derived from owning semantic collection plus stable logical item token, not visual `NodeId` or pointer address;
- shared immutable O(N) virtual metadata generation, with lazy selected-state and bounds projection so scroll/focus/selection do not rebuild the collection metadata;
- native proxy lifetime model based on semantic identity + weak bridge/root, never long-lived raw retained object pointers;
- native read-side access through immutable snapshots and mutation/action routing back through the owning UI thread;
- fixed backends: macOS NSAccessibility, Windows UIA, Linux/X11 AT-SPI2;
- exact role/action/state/notification/virtual-collection platform mappings and T068 implementation order in `docs/accessibility.md`;
- no mutable process-global semantic/proxy registry.

`docs/accessibility.md` is the normative detailed mapping/design artifact and `DESIGN.md` references it. T045 does not ship native accessibility bridges; T068 owns their implementation and native smokes.

The prior implementation head passed T045 Accessibility Semantics, normal CI, T060 Application Contract, T065 qualification, T042 Lifecycle Stress and T052 Release Gate with no Blocking/Important review finding. The synchronized completion head must rerun those exact-head gates before merge.

### Critical downstream platform order

1. T072 / issue #84: active shared bounded Linux `libdbus-1` transport; unlocks T064 and Linux T068.
2. T043 / issue #43 / PR #142: resize/scale contract; required by T066 and T068.
3. T064 / issue #76: DesktopServices after T072.
4. T066 / issue #78: final standalone window controls after T043.

T044 / issue #44 / PR #145 remains a T071 release dependency and is evidence-qualified independently.

## Milestone 8 — Packaging, tooling and release

Delivered foundations include T047 low-level package export, T048 relocated external consumers, T051 performance-regression harness, T052 v0.1 developer-preview release gate, T054 native application helper, T056 deterministic binary-data generation and T057 ResourceManager.

### T067/T068 convergence

After T045 merges, T067 becomes the next Critical UI ticket because T034, T036 and T058 are already complete. T067 must implement the fixed-height virtualized ListView and immutable semantic metadata generation defined by T045. It is not allowed to materialize full visual collections for accessibility or rebuild O(N) semantic metadata on ordinary scroll/selection/focus changes.

T068 starts only after **all** explicit issue #80 dependencies are Done. It implements T045 semantics through immutable per-view snapshots and native NSAccessibility/UIA/AT-SPI2 proxies, shares T067 virtual metadata rather than copying it, routes mutations through T065 and uses T072 as the sole Linux D-Bus transport.

### Final v1 release path

```text
T036(done) -> T045 -> T067 ----\
T058(done) --------------------+--> T068 -> T069 -> T070 -> T071 -> v1.0.0
T035/T063 ---------------------+
T065(done) -> T072 -> T064 ----+
T043 -----------> T066 --------/
other explicit T069 deps -------/
```

T069 is the final v1 public API freeze and must not start until every explicit dependency in issue #81 is complete. T070 validates the production reference application/Getting Started against that frozen API. T071 is validation/release-only on one exact RC SHA.

## Immediate cross-lane plan

1. Finish T045 completion bookkeeping, exact-head qualification and final review on PR #210; merge as soon as all gates are green.
2. Immediately start/resume T067 after T045 merges.
3. Continue T061 -> T035/T063 in the independent overlay lane because those converge on T068.
4. Continue T072/T043 -> T064/T066 in the independent platform lane because those converge on T068/T069.
5. Continue T038 -> T039 and then T040 in the style lane as capacity permits.
6. Start T068 only when every explicit issue #80 dependency is Done.
7. Keep T069/T070/T071 dependency-gated; do not freeze the v1 API early.

## Prioritization rule

Preserve the architectural direction:

```text
core correctness
  -> layout/input/render/text foundations
  -> widgets and styles
  -> platform/lifecycle services
  -> accessibility/public API freeze
  -> reference package/release
```

This is architectural progression, not serialization. Independent tickets may proceed concurrently once explicit dependencies are satisfied.
