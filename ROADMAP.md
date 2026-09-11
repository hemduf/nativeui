# NativeUI roadmap

**Updated:** 2026-09-11

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

Current `main` includes completed T045 / PR #210 and the completed post-T036 hover correction #212 / PR #213, plus T037, T058, T036, T065, T034, T060/T042 lifecycle, T052 v0.1 developer-preview release gate, warning-free source-tree baseline and Tree paint-ownership correction.

The Critical UI lane is completing **T067 / PR #219**. The implementation and pre-documentation merge candidate are fully green across the T067 contract, T051 benchmarks, T045 semantics, T065 dispatcher, T060 application, T042 lifecycle, T052 release and normal CI/ASan matrices. The completion documentation advances the lane to T068 after this PR merges; T068 must still wait for every explicit issue #80 dependency.

Current dependency frontier:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(completion PR #219) -> T068
                                                   T058(done) -----------------------------^

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
- **T036 ListView/Tabs:** complete in PR #155 with stable key/value selection, composite focus, T034 reveal integration, automatic Tabs activation, T059 Collapsed inactive panels, deterministic tests/goldens and `t036_list_tabs --self-test`. Post-completion issue #212 / PR #213 restores deterministic paint-only hover feedback, native pointer-leave clearing and T058-safe hover lifetime without changing selection/activation semantics.

T036 deliberately remains the non-virtualized baseline; T067 adds the fixed-height production virtualization while preserving the same logical selection/focus model.

## Milestone 6 — Styling, theme and animation

**Status: T037 complete; T038/T039 next.**

T037 / PR #151 provides strongly typed per-UI Theme values, deterministic defaults, paint-vs-layout invalidation classification, representative Button/Slider theme binding, public-header isolation and a feature self-test. T038 owns typed widget style variants, T039 scoped inheritance, and T040 animation must reuse T065 rather than create a second scheduler.

## Milestone 7 — Platform and embedded robustness

Delivered lifecycle/platform foundations include:

- #62 plug-in-host instance/runtime safety;
- #64 standalone ownership Decision B;
- T042 deterministic lifecycle stress;
- T045 accessibility semantic architecture;
- T053 consumer-specific Objective-C bridge naming;
- T060 explicit one-Application/one-PROGRAM-world multi-window ownership;
- #139 post-T060 T042 multi-window stress qualification;
- T065 bounded UI-thread dispatcher/timer service.

### T045 — Accessibility semantic architecture

T045 / issue #45 / PR #210 is complete.

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

### Critical downstream platform order

1. T072 / issue #84: active shared bounded Linux `libdbus-1` transport; unlocks T064 and Linux T068.
2. T043 / issue #43 / PR #142: active resize/scale contract; required by T066 and T068.
3. T064 / issue #76: DesktopServices after T072.
4. T066 / issue #78: final standalone window controls after T043.

T044 / issue #44 / PR #145 remains a T071 release dependency and is evidence-qualified independently.

## Milestone 8 — Packaging, tooling and release

Delivered foundations include T047 low-level package export, T048 relocated external consumers, T051 performance-regression harness, T052 v0.1 developer-preview release gate, T054 native application helper, T056 deterministic binary-data generation and T057 ResourceManager.

### T067 — Fixed-height virtualized ListView

T067 / issue #79 / PR #219 is the completion candidate.

Delivered contract:

- finite-positive fixed row height and overflow-safe content geometry;
- O(1) visible-range math and exact default two-row overscan on each side;
- visual retained rows bounded to viewport/overscan plus at most one focused and one captured off-range exception;
- stable logical keys with no live Component rebinding across keys;
- `ScrollState` remains the only offset/viewport model and `scroll_to_index` / O(N) `scroll_to_key` use the shared exact `ScrollAlignment` semantics;
- T036 composite focus/navigation/activation semantics remain authoritative;
- T058 keyed reconciliation performs safe retained-row insertion/removal/reorder and invalid dataset updates are atomic;
- one immutable O(N) semantic metadata snapshot is created only for accepted dataset replacement and shared across ordinary scroll/selection/focus semantic projections;
- offscreen semantic lookup never calls the visual row factory and old/new immutable generations remain lifetime-safe for concurrent readers;
- T051 now benchmarks 1k/10k/100k datasets over deterministic repeated viewport changes and asserts bounded `factory_calls`, `max_materialized` and `metadata_rebuilds`;
- `examples/features/t067_virtual_list.cpp` demonstrates a 100k-item public API path and provides deterministic `--self-test` coverage.

Pre-documentation exact head `8cf1a4f900fbf8c5f55a343393fa38489632440f` passed T067 Virtual List Contract `34543202644`, T051 Release Benchmarks `34543202602`, T045 Accessibility Semantics `34543202669`, T065 Dispatcher Contract `34543202612`, T060 Application Contract `34543202653`, T042 Lifecycle Stress `34543202623`, T052 v0.1 Release Gate `34543202599` and normal CI `34543202607` including Linux ASan+UBSan. Review `5173364825` found no remaining Blocking/Important code issue. The completion-documentation head must rerun applicable exact-head validation before merge.

### T068 convergence

T068 starts only after **all** explicit issue #80 dependencies are Done. It implements T045 semantics through immutable per-view snapshots and native NSAccessibility/UIA/AT-SPI2 proxies, shares T067 virtual metadata rather than copying it, routes mutations through T065 and uses T072 as the sole Linux D-Bus transport.

After T067 merges, this lane waits for the remaining explicit T068 dependencies rather than taking work owned by the overlay/platform/style lanes.

### Final v1 release path

```text
T036(done) -> T045(done) -> T067(done after PR #219) --\
T058(done) ------------------------------------------+--> T068 -> T069 -> T070 -> T071 -> v1.0.0
T035/T063 -------------------------------------------+
T065(done) -> T072 -> T064 --------------------------+
T043 -----------------> T066 -------------------------/
other explicit T069 deps -----------------------------/
```

T069 is the final v1 public API freeze and must not start until every explicit dependency in issue #81 is complete. T070 validates the production reference application/Getting Started against that frozen API. T071 is validation/release-only on one exact RC SHA.

## Immediate cross-lane plan

1. Complete exact-head qualification and merge T067 / PR #219.
2. Keep this Critical UI lane waiting for T068 until every explicit issue #80 dependency is Done.
3. Continue T061 -> T035/T063 in the independent overlay lane because those converge on T068.
4. Continue T072/T043 -> T064/T066 in the independent platform lane because those converge on T068/T069.
5. Continue T038 -> T039 and then T040 in the style lane as capacity permits.
6. Keep T069/T070/T071 dependency-gated; do not freeze the v1 API early.

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
