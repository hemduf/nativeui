# NativeUI roadmap

**Updated:** 2026-09-16

NativeUI is a reusable C++20 desktop retained-mode UI toolkit: Pugl owns native views/events, Skia owns rendering, and NativeUI owns retained composition, layout, input/focus, widgets, styling, resources and packaging. GitHub Issues are the source of truth for exact ticket scope, status and dependencies.

## Execution rules

- explicit GitHub `Dependencies:` are hard gates;
- finish the current merge-near ticket before widening source work;
- behavior/configuration changes use local RED -> GREEN -> REFACTOR and coherent published batches;
- code-changing tickets require exact-head normal/path qualification and the complete `CODE_REVIEW.md` record with no Blocking/Important finding;
- keep implementation PRs Draft while executable source/tests/build/workflows change;
- Draft -> Ready is reserved for a frozen candidate and triggers T042/T052 final qualification;
- any executable source/test/build/workflow change after final qualification invalidates the candidate; pure completion/project-state documentation does not;
- NativeUI-owned targets compile with zero unapproved warnings and the default empty `NATIVEUI_ALLOWED_WARNINGS`;
- every completion cycle synchronizes the issue, `CONTEXT.md`, this roadmap and relevant automation status;
- never place personal information in tickets, source, tests, examples, fixtures or generated metadata.

## Current execution snapshot

### T130 — Done

**T130 / issue #291 / PR #408 is complete and merged as `e65e317d6584ae440f1f041b8187a63d18b2aa6a`.**

Frozen executable head `4c40b881fa13ea0f9839ae415059744f6107c62d` delivered the consolidated lifecycle/layout/paint/native exception-safety closeout:

- no-throw, best-effort retained and native teardown;
- exact-progress lifecycle transactions and rollback for mount/activate/deactivate/unmount;
- lifecycle publication barriers across retained Tree/UI measure/layout/paint/dispatch/inspector paths;
- transaction-safe dynamic insertion/removal with durable per-Tree retry state and staged focus-registry repair;
- terminal deactivation that does not construct pending desired children solely to tear them down;
- transactional layout rollback with dirty/repaint preservation;
- Painter/SkCanvas unwind balancing and deterministic failed-paint recovery;
- staged ViewCore partial-construction cleanup across native resource acquisition stages;
- deterministic T044 macOS native readiness/button-state synchronization without retrying semantic PointerDown/PointerUp;
- multi-instance isolation with no mutable process-global/singleton/`thread_local` recovery state.

Exact-head normal/path qualification is green for CI `35134851097`, Package Contracts `35134851013`, T044 `35134851103`, T050 `35134851018`, T060 `35134851051`, T064 `35134851039`, T065 `35134851008`, T066 `35134851040` and T072 `35134850948`. The initial Windows CI failure was an external Mesa download transport reset after a successful build; a targeted same-head rerun passed Mesa and Windows tests without source changes.

Final-candidate T042 Lifecycle Stress `35140156948` passed Linux ASan+UBSan, Linux X11, macOS and Windows. T052 v0.1 Release Gate `35140156810` passed release contracts, clean Linux/macOS/Windows bootstraps and the exact-head T051 performance/allocation benchmark. Review records `5226938758` and `5227314927` report zero Blocking/Important findings. Privacy review is clean.

### T125 — Done

**T125 / issue #286 / PR #382 is complete and merged as `401d73983c2103fae1e48c1984a982277088e060`.**

Frozen executable head `da4386626837b8cedf9d7f17bc6e8b150aa99921` closes the retained dispatch/reconciliation/cancellation exception-safety blocker while preserving T130 as the lifecycle/layout/paint transaction authority:

- `dispatch_depth_`, availability reconciliation, dynamic reconciliation and pointer-cancellation guards restore exact prior state across exception unwind;
- begun focus/hover callbacks are not automatically replayed solely because they threw; durable suffix/pending work resumes only at valid retained checkpoints;
- pending focus/hover semantic work is terminalized across mount/deactivate/unmount boundaries so stale requests cannot leak into later Tree reuse;
- dynamic reconciliation preserves captured, unprocessed and re-entrant owners after callback failure and recovers dirty-owner enqueue allocation failure through bounded per-Tree state;
- dynamic teardown quarantine blocks interaction publication into doomed subtrees while reusing T130's already-computed desired-key snapshot, avoiding duplicate user key callbacks;
- quarantine authority is structural-epoch scoped so a re-entrant structural reversal immediately retires provisional quarantine authority before T130 abandons the stale removal pass;
- throwing PointerDown/PointerUp/PointerCancel and nested cancellation cannot permanently wedge pointer interaction/capture state;
- transient Pugl drop offer/decision borrows restore exact nested prior state before existing foreign-ABI containment;
- recovery remains per-Tree or stack-local with no mutable process-global/singleton/`thread_local` state.

Exact-head normal/path qualification is green for CI `35150662534`, Package Contracts `35150662688`, T044 `35150662518`, T050 `35150662564`, T060 `35150662729`, T064 `35150662700`, T065 `35150662562`, T066 `35150662456` and T072 `35150662919`.

Final-candidate T042 Lifecycle Stress `35153051058` passed Linux ASan+UBSan, Linux X11, macOS and Windows. T052 v0.1 Release Gate `35153050940` passed the release contract, clean Linux/macOS/Windows bootstraps and the exact-head T051 performance/allocation benchmark. Self-review `5228579161` and independent review `5228585872` report zero Blocking/Important findings; no unresolved review thread remained and privacy review is clean.

### Active pre-freeze work

- **T174 / #409 / PR #410 — Ready, P1:** per-UI fallback for unhandled raw KeyDown shortcuts. T125 is now satisfied; retarget/reconcile the existing PR onto current `main`, requalify the exact resulting head and merge when green, or explicitly retarget T174 post-v1 before the API freeze.
- **T069 / #81 — Blocked by the T174 pre-freeze decision.** All T123–T132 hard safety prerequisites are now Done. T069 may start once T174 is either Done or explicitly moved post-v1 so no new public API straddles the freeze.
- **T068 / #80 / PR #241 — deferred to 1.2:** not a v1 blocker.

## Current dependency frontier

```text
state/safety:
  T123(done) -> T138(done) -> T139(done) -> T140(done) -> T141(done) -> T124(done)
  T123(done) -> T127(done)
  T125(done)
  T126(done)
  T128(done)
  T129(done)
  T130(done)
  T131(done)
  T132(done)

remaining pre-freeze public-surface decision:
  T174(ready; finish or retarget before freeze)
      |
      +----> T069(blocked until T174 resolved)

release:
  T174 resolved -> T069 -> T070 -> T122/docs closeout -> T071 -> v1.0.0
  T049(done) -------------------------------------------------------> T071
  T044(done) -------------------------------------------------------> T071

post-1.0:
  T068 ------------------------------------------------------------> 1.2
```

## Milestone status

### Milestone 0 — Baseline hardening

**Complete.** Core/state tests, retained lifecycle foundations, public-header split and invalidation foundations are established; unapproved NativeUI-owned compiler warnings are Blocking.

### Milestone 1 — Layout system

**Complete.** Constraints, alignment/distribution, flex, grid, scroll layout/state and clipping/overflow foundations are delivered. T130 hardens layout publication/recovery under exceptions.

### Milestone 2 — Input, focus and gestures

**Complete for the existing v1 input surface, with T174 pending a pre-freeze decision.** Event propagation, focus scopes/restoration, pointer capture, wheel normalization, gestures, commands and drag/drop are delivered. T173 completed public ASCII A-Z key exposure without changing pre-existing enum values. T125 now hardens dispatch/reconciliation/cancellation unwind behavior. T174 is the only remaining public input-surface addition that must either land or move post-v1 before the freeze.

### Milestone 3 — Rendering and graphics

**Complete.** Transforms, paths, gradients, images, SVG/resources, caches and deterministic rendering/golden tests are delivered. T130 guarantees framework-owned Painter/SkCanvas unwind balance and failed-frame recovery.

### Milestone 4 — Text system

**Complete.** Text editing, Label/fonts/fallback, TextArea, UTF-8 selection/navigation and platform IME composition bridges are delivered.

### Milestone 5 — Standard widget set

**Complete for the v1 standard-widget scope.** Button, Checkbox/Radio, Slider/RangeSlider, ProgressBar/Meter, ScrollView, ComboBox/PopupMenu and ListView/Tabs are delivered and remain covered by the safety/final qualification gates.

### Milestone 6 — Styling, theme and animation

**Complete.** T037–T040 provide typed Theme/style state, retained StyleScope inheritance and deterministic animation/reduced-motion behavior. T128 hardens exceptional animation progress/recovery.

### Milestone 7 — Platform and embedded robustness

**Complete for the current v1 platform surface.** T043 resize/scale, T044 pointer capture, T053 macOS Objective-C runtime identity, T060 Application ownership, T064 DesktopServices, T065 Dispatcher, T066 window controls and T072 Linux D-Bus are delivered. T125/T126/T128/T130/T132 close the relevant dispatch, completion, scheduler, teardown and deferred-lifecycle failure boundaries.

### Milestone 8 — Packaging, virtualization, overlays and release convergence

**In final pre-freeze convergence.** Delivered foundations include T047/T048 packaging, T049 gallery, T050 inspector, T051/T052 qualification, T054/T056/T057 helpers/resources, T058 dynamic composition, T061/T062/T063 overlay/Tooltip/Dialog, T067 virtualized ListView and the completed safety series T123/T124/T125/T126/T127/T128/T129/T130/T131/T132/T138/T139/T140/T141.

The remaining sequence is:

1. resume T174 / #409 / PR #410 on final T125 semantics and merge when green, or explicitly retarget it post-v1;
2. resume T069 / #81 for the whole public-surface audit and v1 freeze;
3. validate the T070 reference application/Getting Started against that frozen surface;
4. complete explicitly scheduled v1 documentation closeout including T122 where applicable;
5. run T071 validation/release-only on one exact RC SHA.

## Completed safety closeouts relevant to the v1 freeze

- **T123 / #281:** deterministic lifetime-safe `State<T>` observer/reentrancy/throw semantics.
- **T124 / #282 + T138–T141:** stable `Binding<T>` lifetime/value contract and migration of retained/stateful consumers.
- **T125 / #286:** retained dispatch/reconciliation/cancellation exception safety, pending-work preservation, dynamic enqueue failure recovery and transient native-borrow hardening, merged in PR #382.
- **T126 / #287:** DesktopServices exceptional completion/native boundaries.
- **T127 / #288:** `ScrollState` lifetime/reentrancy and retained-consumer safety.
- **T128 / #289:** Dispatcher accepted-work preservation and Animation exceptional recovery.
- **T129 / #290:** lifetime-safe retained invalidation callbacks.
- **T130 / #291:** lifecycle/layout/paint/native construction/teardown exception safety, merged in PR #408.
- **T131 / #293:** Overlay/Dialog/popup/Tooltip transaction failure safety.
- **T132 / #294:** failure-safe standalone close lifecycle-control deferral.
- **T173 / #401:** complete public ASCII A-Z key exposure and routing coverage.

## Release policy

T069 freezes only the backend-neutral v1 public API after every hard dependency is Done and any pending pre-freeze public feature such as T174 is resolved. T070 validates the reference application against the frozen surface. T071 is validation/release-only: defects found there return to a focused canonical ticket, are merged first, and then a new exact release-candidate SHA is selected. T068 remains outside the v1 critical path and is targeted for NativeUI 1.2.
