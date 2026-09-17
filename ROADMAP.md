# NativeUI roadmap

**Updated:** 2026-09-17

NativeUI is a reusable C++20 retained-mode UI toolkit: Pugl owns native views/events, Skia owns rendering, and NativeUI owns retained composition, layout, input/focus, widgets, styling, resources and packaging. GitHub Issues are the source of truth for exact ticket scope, status and dependencies.

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

**T130 / issue #291 / PR #408 merged as `e65e317d6584ae440f1f041b8187a63d18b2aa6a`.**

Frozen head `4c40b881fa13ea0f9839ae415059744f6107c62d` closed lifecycle/layout/paint/native construction and teardown exception safety. Normal/path CI, T042 and T052/T051 were green; final review reported zero Blocking/Important findings.

### T125 — Done

**T125 / issue #286 / PR #382 merged as `401d73983c2103fae1e48c1984a982277088e060`.**

Frozen head `da4386626837b8cedf9d7f17bc6e8b150aa99921` closed retained dispatch/reconciliation/cancellation exception safety and pending-work preservation on top of T130. Normal/path CI, T042 and T052/T051 were green; final self and independent reviews reported zero Blocking/Important findings.

### T174 — Done

**T174 / issue #409 / PR #410 merged as `1ef326494ce00d215c1211ad0cde2437e3ffadbb`.**

Frozen exact head `e6d747961a2fd39760f5703748c10440f8fb0efa` delivered the final pre-freeze raw-key fallback surface:

- `UI::set_key_down_handler(std::function<EventResult(const InputEvent&)>)`;
- per-`UI`/`Tree` ownership only, with no mutable global/singleton/`thread_local` state;
- framework KeyDown policy and Command normalization remain authoritative before the fallback;
- handled focused/ancestor routes suppress fallback and no sibling broadcast is introduced;
- only raw `KeyDown` is eligible; Command-mapped chords are never double-delivered;
- active callable lifetime survives re-entrant clear/replacement;
- non-empty installation allocates before publication, preserving the prior handler on allocation failure;
- fallback exceptions reuse T125's canonical dispatch unwind/reconciliation recovery without duplicate guards;
- feature example/self-test covers normal routing and throw -> catch -> later dynamic reconciliation.

Exact-head qualification is green for CI `35154523714`, T050 `35154524036` and T066 `35154523694`. Final T042 Lifecycle Stress `35156481983` passed Linux ASan+UBSan, Linux X11, macOS and Windows. T052 v0.1 Release Gate `35156481846` passed the release contract, clean Linux/macOS/Windows bootstraps and exact-head T051 performance/allocation benchmark. Self review `5228886765` and independent frozen-head review `5228890731` report zero Blocking/Important findings; the historical Blocking thread is resolved/outdated and privacy review is clean.

### T073 — Done (post-1.0 rendering foundation)

**T073 / issue #156 / PR #420 merged as `5c769749f280f18f60e8176ec5eadbc38881acc7`.**

Frozen exact head `63bc82c6e2fd3db0aa6aae06456e51c36da33bfc` introduces backend-neutral `ui::Brush` values for solid colors, linear gradients and radial gradients, with one shared Painter fill-materialization seam and Canvas/Painter Brush fill overloads.

Delivered contract includes:

- exact transparent-solid moved-from semantics, including self-move assignment;
- non-allocating/non-throwing Color construction, moves and destruction by representation contract;
- strong copy-assignment guarantee under deterministic allocation failure;
- owned gradient logical state with independent multi-UI lifetime behavior;
- preserved transform, clip, invalid-gradient, opacity and blend semantics;
- no mutable production global/singleton/`thread_local` state and no backend resource creation during Brush construction;
- isolated public-header coverage plus radial-circle and linear-Path golden coverage.

Exact-head normal/path qualification passed CI #1828, T044 #267 and T066 #324. Final T042 Lifecycle Stress #834 passed Linux ASan+UBSan, Linux X11, Windows and macOS. Final T052 v0.1 Release Gate #558 passed the release contract, clean Linux/macOS/Windows bootstraps and exact-head T051 benchmark. Final review `5233819886` reports zero remaining Blocking/Important findings.

T073 remains a NativeUI 1.1/post-1.0 rendering foundation and is not on the v1 critical path. T069 continues to own the v1 public-surface audit/freeze against the current `main` state.

### T075 — Done (post-1.0 rendering foundation)

**T075 / issue #158 / PR #421 merged as `7cd37ef7a2e23b2e3c69880174f60f9578edbbc1`.**

Frozen exact head `adf486a8aad6f7a94c1a512ed230ab40e7eec63f` extends the existing strict lexical `Painter::StateGuard` model with scoped Rect, rounded-Rect and `Path` clipping without adding a second public scope type or logical stack.

Delivered contract includes:

- `Painter::scoped_clip(Rect)`, `scoped_clip(Rect,float)` and `scoped_clip(const Path&)` all return the existing `StateGuard`;
- `StateGuard` is non-copyable and non-movable, with `noexcept` destruction and C++20 guaranteed-copy-elision factories;
- one Painter-private scope-frame begin/adopt/rollback path shares the existing save-depth/restore-floor invariant with `scoped_state()`;
- fallible Path/rounded preparation occurs before state publication and clip-application failure rolls back the just-entered frame;
- invalid, empty, inverted and non-finite clip geometry produces a balanced empty effective clip rather than unbounded drawing;
- rounded radius canonicalization is deterministic and capped at half the smallest rectangle dimension;
- transform capture/intersection semantics, legacy manual clipping interoperability, exception/early-return restoration and independent Painter isolation are covered deterministically;
- dedicated `t075_scoped_clipping` feature example/self-test is present.

Exact-head normal/path qualification passed CI #1836 plus T044 #275, T050 #156, T066 #332, T072 #351 and Package Contracts #265. Final T042 Lifecycle Stress #835 and T052 v0.1 Release Gate #559 both completed successfully on the frozen head, including Linux ASan+UBSan, Linux X11, Windows/macOS final qualification, clean package bootstraps and exact-head T051 benchmark. Final review `5233924873` reports zero remaining Blocking/Important findings and there are no unresolved review threads.

T075 is a NativeUI 1.1/post-1.0 rendering foundation and is not on the v1 critical path. T069 must nevertheless account for the current scoped-clipping public Painter surface when freezing the v1 API exposed by current `main`.

### Active pre-freeze work

- **T069 / #81 / PR #269 — Ready, P0:** final NativeUI v1 public API audit/freeze. All T123–T132 hard safety prerequisites are Done and T174's public input addition is resolved before the freeze. The audit must account for the current `main` public surface after the merged T073 and T075 rendering foundations.
- **T068 / #80 / PR #241 — deferred to 1.2:** native accessibility bridges remain outside the v1 critical path.

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

pre-freeze public additions:
  T173(done) -> T174(done)

v1 critical path:
  T069(ready) -> T070 -> T122/docs closeout -> T071 -> v1.0.0
  T049(done) -----------------------------------------> T071
  T044(done) -----------------------------------------> T071

post-1.0 / later-release work already landed:
  T073(done) -----------------------------------------> NativeUI 1.1 foundation
  T075(done) -----------------------------------------> NativeUI 1.1 foundation
  T068 ----------------------------------------------> 1.2
```

## Milestone status

### Milestone 0 — Baseline hardening

**Complete.** Core/state tests, retained lifecycle foundations, public-header split and invalidation foundations are established; unapproved NativeUI-owned compiler warnings are Blocking.

### Milestone 1 — Layout system

**Complete.** Constraints, alignment/distribution, flex, grid, scroll layout/state and clipping/overflow foundations are delivered. T130 hardens layout publication/recovery under exceptions.

### Milestone 2 — Input, focus and gestures

**Complete for the v1 input surface.** Event propagation, focus scopes/restoration, pointer capture, wheel normalization, gestures, commands and drag/drop are delivered. T173 completed public ASCII A-Z key exposure; T125 finalized exception-safe dispatch/reconciliation semantics; T174 now provides the per-UI fallback for otherwise-unhandled raw KeyDown shortcuts without changing Command/text/IME routing.

### Milestone 3 — Rendering and graphics

**Complete for v1.** Transforms, paths, gradients, images, SVG/resources, caches and deterministic rendering/golden tests are delivered. T130 guarantees Painter/SkCanvas unwind balance and failed-frame recovery. T073's generic Brush foundation and T075's strict lexical scoped-clipping API are also merged on `main` for the post-1.0/1.1 rendering line without changing the v1 critical path.

### Milestone 4 — Text system

**Complete.** Text editing, Label/fonts/fallback, TextArea, UTF-8 selection/navigation and platform IME composition bridges are delivered.

### Milestone 5 — Standard widget set

**Complete for the v1 standard-widget scope.** Button, Checkbox/Radio, Slider/RangeSlider, ProgressBar/Meter, ScrollView, ComboBox/PopupMenu and ListView/Tabs are delivered.

### Milestone 6 — Styling, theme and animation

**Complete.** T037–T040 provide typed Theme/style state, retained StyleScope inheritance and deterministic animation/reduced-motion behavior. T128 hardens exceptional animation progress/recovery.

### Milestone 7 — Platform and embedded robustness

**Complete for the current v1 platform surface.** T043 resize/scale, T044 pointer capture, T053 macOS Objective-C runtime identity, T060 Application ownership, T064 DesktopServices, T065 Dispatcher, T066 window controls and T072 Linux D-Bus are delivered. T125/T126/T128/T130/T132 close the relevant failure boundaries.

### Milestone 8 — Packaging, virtualization, overlays and release convergence

**Ready for the final API freeze.** Delivered foundations include T047/T048 packaging, T049 gallery, T050 inspector, T051/T052 qualification, T054/T056/T057 helpers/resources, T058 dynamic composition, T061/T062/T063 overlay/Tooltip/Dialog, T067 virtualized ListView and the completed safety series T123–T132 plus T138–T141. T174 is also resolved before freeze.

The remaining v1 sequence is:

1. resume T069 / #81 / PR #269 and complete the full public-surface audit/freeze against current `main`, including the merged T073/T075 Painter surface;
2. validate T070 reference application/Getting Started against the frozen surface;
3. complete explicitly scheduled v1 documentation closeout including T122 where applicable;
4. run T071 validation/release-only on one exact RC SHA.

## Completed safety and pre-freeze closeouts

- **T123 / #281:** deterministic lifetime-safe `State<T>` observer/reentrancy/throw semantics.
- **T124 / #282 + T138–T141:** stable `Binding<T>` lifetime/value contract and migration of retained/stateful consumers.
- **T125 / #286:** retained dispatch/reconciliation/cancellation exception safety and pending-work preservation.
- **T126 / #287:** DesktopServices exceptional completion/native boundaries.
- **T127 / #288:** `ScrollState` lifetime/reentrancy and retained-consumer safety.
- **T128 / #289:** Dispatcher accepted-work preservation and Animation exceptional recovery.
- **T129 / #290:** lifetime-safe retained invalidation callbacks.
- **T130 / #291:** lifecycle/layout/paint/native construction/teardown exception safety.
- **T131 / #293:** Overlay/Dialog/popup/Tooltip transaction failure safety.
- **T132 / #294:** failure-safe standalone close lifecycle-control deferral.
- **T173 / #401:** complete public ASCII A-Z key exposure and routing coverage.
- **T174 / #409:** per-UI fallback for unhandled raw KeyDown shortcuts on final T125 semantics.

## Completed post-1.0 foundations

- **T073 / #156:** backend-neutral generic Brush fill painting with deterministic move/copy/failure semantics and shared Painter materialization.
- **T075 / #158:** strict lexical scoped clipping for Rect, rounded Rect and Path through the existing `Painter::StateGuard` stack model.

## Release policy

T069 freezes only the backend-neutral v1 public API after every hard dependency and planned pre-freeze public addition is Done. That condition is now satisfied. T073 and T075 are merged as later-release rendering foundations and remain outside the v1 critical path; T069 must explicitly account for the current `main` surface when deciding the frozen v1 API. T070 validates the reference application against the frozen surface. T071 is validation/release-only: defects found there return to a focused canonical ticket, are merged first, and then a new exact release-candidate SHA is selected. T068 remains outside the v1 critical path and is targeted for NativeUI 1.2.
