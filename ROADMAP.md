# NativeUI roadmap

**Updated:** 2026-09-11

NativeUI is a reusable C++20 desktop retained-mode UI toolkit: Pugl owns native views/events, Skia owns rendering, and NativeUI owns retained composition, layout, input/focus, widgets, styling, resources and packaging. GitHub Issues are the source of truth for exact ticket scope, status and dependencies.

## Execution rules

- explicit GitHub `Dependencies:` are hard gates;
- resume existing canonical branches/PRs before creating work;
- among Ready work, prefer priority and downstream unblock value;
- behavior/configuration changes use RED -> GREEN -> REFACTOR;
- implementation progress and issue conformity are separate; Done/merge requires an explicit issue-to-code/test evidence matrix with no unchecked requirement;
- code-changing tickets require exact-head normal validation and a final `CODE_REVIEW.md` record with no Blocking/Important finding;
- use `CI_POLICY.md`: Draft for active implementation, then Draft -> Ready only for a frozen executable candidate to trigger T042/T052 qualification;
- NativeUI-owned targets must compile with zero unapproved warnings and the default empty `NATIVEUI_ALLOWED_WARNINGS`;
- unrelated lanes may continue only within repository concurrency rules while another PR waits exclusively on external CI;
- every completion cycle synchronizes issue status, `CONTEXT.md` and this roadmap;
- feature tickets ship an interactive example plus deterministic `--self-test`.

## Current execution snapshot

Current `main` before T035 merge is `f99f94b9aa029c877e6ef90346d4ad962fff2ee9`. The recent baseline includes T067 startup regression closure PR #233, feature-example auto-registration PR #235 and staged CI qualification policy PR #230 on top of the completed T043/T061/T067/T045/T037/T058/T036/T065/T034/T060 foundations.

T035 / issue #35 / PR #225 is the active final completion candidate. Production source/tests/build/workflows are frozen at executable head `882d29a6f6a4798c36336ed167555e33c6cc594a`. Normal CI run `34606092899` is green on Linux X11, Linux ASan+UBSan, Windows and macOS, including package contracts, Objective-C isolation and the T035 native standalone+embedded smoke. Final `CODE_REVIEW.md` is PASS with no Blocking/Important finding. The PR is Ready and only T042/T052 final-candidate qualification remains before merge.

Current dependency frontier:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(done) -> T068
                                                   T058(done) ------------^

dynamic/overlay:   T058(done) -> T061(done)
                                      |-> T035(final qualification) -----> T068
                                      |-> T063 --------------------------> T068
                                      +-> T062

style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052(done)

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

critical platform: T060(done) -> T065(done) -> T072 -> T064
                   T041(done) -> T043(done) -> T066
                                      |-------> T068

release:            all explicit convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

T035 and T063 consume completed T061 plus T034. T062 consumes completed T061 plus T065. T043 is complete, unlocks T066 and remains an explicit completed T068 dependency. T044 remains an explicit T071 dependency and is reconciled independently.

## Milestone 0 — Baseline hardening

**Complete.** T001–T006 provide core/state tests, retained lifecycle, public-header split and invalidation foundations. #163 / PR #181 makes unapproved NativeUI-owned compiler warnings Blocking.

## Milestone 1 — Layout system

**Complete.** T007–T012 provide constraints, alignment/distribution, flex, grid, scroll state/layout and clipping/overflow foundations.

## Milestone 2 — Input, focus and gestures

**Complete baseline.** T013–T018 provide event propagation, focus scopes/restoration, pointer capture, wheel normalization, gestures, commands and drag/drop primitives.

## Milestone 3 — Rendering and graphics

**Complete.** T019–T024 provide transforms, paths, gradients, images, SVG/resources, caches and deterministic rendering/golden tests.

## Milestone 4 — Text system

**Complete.** T025–T029 provide text editing, Label/fonts/fallback, TextArea, UTF-8 selection/navigation and platform IME composition bridges.

## Milestone 5 — Standard widget set

**T030–T034 and T036 complete; T035 is in final qualification in PR #225.**

- T030 Button — PR #94.
- T031 Checkbox/Radio — PR #95.
- T032 Slider/RangeSlider — PR #115.
- T033 ProgressBar/Meter — PR #123.
- T034 ScrollView — PR #135.
- T036 ListView/Tabs — PR #155; #212 / PR #213 adds deterministic paint-only hover behavior.
- T035 ComboBox/PopupMenu — PR #225 completion candidate; implemented entirely on the generic T061 overlay stack with immutable open snapshots, exact keyboard/pointer/focus policy, T059 ReadOnly split, close-before-callback reentrancy, deterministic golden coverage and standalone/embedded native smoke. Normal exact-head CI and final code review are green; merge is gated only by Ready-triggered T042/T052 qualification.

When PR #225 merges, Milestone 5 is complete for the v1 standard-widget scope defined by these tickets.

## Milestone 6 — Styling, theme and animation

**T037 complete; T038/T039/T040 remain.**

T037 / PR #151 provides typed per-UI Theme values and representative control theme binding. T038 owns typed widget variants, T039 scoped style inheritance, and T040 animation must reuse T065 instead of introducing another scheduler.

T035 also closes a dynamic-composition theme integration gap: retained nodes inserted after initial Tree mount now bind the owning Tree theme before lifecycle/paint, preventing T058/T061 popup subtrees from silently falling back to `default_theme()`.

## Milestone 7 — Platform and embedded robustness

Delivered foundations include plug-in host isolation, standalone ownership Decision B, T042 lifecycle stress, T053 Objective-C runtime identity, T060 Application ownership, T065 Dispatcher/timers, T045 accessibility architecture and T043 resize/scale negotiation.

### T043 — Resize/scale negotiation

**Complete in PR #142.** Public/component geometry remains logical while native/framebuffer geometry is physical; per-view finite-positive scale, authoritative configure snapshots, exact request/echo behavior, fractional conversion, embedded parent authority and reentrancy-safe preferred-size notification are qualified. T066 is unblocked.

T044 / issue #44 / PR #145 remains a T071 release dependency and requires its own current-main reconciliation, native capture evidence and exact-head qualification.

## Milestone 8 — Packaging, virtualization, overlays and release convergence

Delivered foundations include T047/T048 package consumption, T051 performance qualification, T052 v0.1 release gate, T054 application helper, T056 binary data, T057 ResourceManager, T058 dynamic composition, T061 overlay/portal infrastructure, T067 fixed-height virtualized ListView and T045 semantic architecture.

### Build/example registration hardening

**Complete in PR #235 / issue #234.** Canonical `examples/features/tNNN_<feature>.cpp` sources are auto-discovered by root CMake with deterministic ordering, `CONFIGURE_DEPENDS` and explicit malformed-name rejection. Adding a feature example no longer requires a second manual CMake list edit.

### T061 — Generic overlay / portal layer

**Complete in PR #216.** T061 provides one generic retained in-view overlay/portal layer per UI using T058 structural reconciliation, including deterministic placement, modal focus/capture behavior, anchor tracking, no-click-through dismissal, reentrant show/close safety and standalone/EmbeddedView parity.

### T067 — Fixed-height virtualized ListView

**Complete in PR #219 with startup regression closure in PR #233.** The required 100k example is constrained to the real viewport, startup materialization remains bounded, and the production `--self-test` is executed by dedicated CI.

### T035 — ComboBox and PopupMenu

**Completion candidate in PR #225; final executable qualification in progress.** The delivered scope is selection/action policy over T061 only: no native popup window and no second popup registry. The candidate covers immutable snapshots, keyboard/pointer contracts, exact-once commit/action, no click-through, focus/source teardown, T059 ReadOnly semantics, reentrant callback safety, per-view isolation, dynamic theme inheritance, golden states and standalone/embedded native smoke.

After T035 merge, reconcile T063 / PR #227 onto the new main before continuing because both branches touch `ui.hpp`; do not preserve stale/conflicting T063 integration code by force. Then continue T063 in bounded TDD slices and T062 according to the current UI convergence order.

### T068 convergence

T068 starts only after **all** explicit issue #80 dependencies are Done. It implements T045 semantics through immutable per-view snapshots and native NSAccessibility/UIA/AT-SPI2 bridges, shares T067 virtual metadata, routes mutations through T065 and uses T072 as the sole Linux D-Bus transport.

### Final v1 release path

```text
T036(done) -> T045(done) -> T067(done) -------------------\
T058(done) -> T061(done) -> T035/T063 --------------------+--> T068 -> T069 -> T070 -> T071 -> v1.0.0
T065(done) -> T072 -> T064 -------------------------------+
T043(done) -----------> T066 ------------------------------/
other explicit T069 dependencies --------------------------/
```

T069 is the final v1 public API freeze and cannot start until every explicit issue #81 dependency is complete. T070 validates the reference application/Getting Started against the frozen API. T071 is validation/release-only on one exact RC SHA; defects found there return to their canonical fix ticket.

## Immediate cross-lane plan

1. Finish T035 Ready qualification: require T042 + T052 green for executable head `882d29a6...`, then squash-merge PR #225 if no executable change occurred.
2. Close/update issue #35 and mark `status:done` in the same completion cycle.
3. Reconcile T063 / PR #227 onto post-T035 main, preserving the T035 overlay-command integration, then continue its missing Dialog policy/tests/example in TDD.
4. Continue T062 after T063 according to the current UI convergence plan.
5. Reconcile/qualify T044 / PR #145 independently; do not reuse T043 evidence.
6. Continue T072 -> T064 and T066 in the independent platform lane.
7. Continue T038 -> T039 and T040 in the styling lane as capacity permits.
8. Keep T068 blocked until every explicit issue #80 dependency is genuinely Done; keep T069/T070/T071 dependency-gated.

## Prioritization rule

```text
core correctness
  -> layout/input/render/text foundations
  -> widgets/styles/overlays/platform services
  -> accessibility convergence and public API freeze
  -> reference package/release
```

Independent tickets may progress concurrently only when explicit dependencies, shared-file conflict risk and repository concurrency rules allow it.
