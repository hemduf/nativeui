# NativeUI roadmap

**Updated:** 2026-09-09

This roadmap turns the current implementation into a reusable desktop UI toolkit while preserving the architecture: Pugl for native views/events, Skia for rendering, NativeUI for retained UI behavior. GitHub Issues are the source of truth for exact ticket status/dependencies. Milestone ordering is architectural progression, not a global execution lock.

## Execution rules

- explicit GitHub `Dependencies:` are the hard ticket-to-ticket gates;
- select Ready work by priority, then downstream unblock value / critical-path impact;
- resume existing work before creating another branch/PR;
- keep unrelated lanes moving while a PR waits on external CI;
- every behavior/configuration change uses test-first RED -> GREEN -> REFACTOR;
- every code-changing ticket requires exact-head validation plus a `CODE_REVIEW.md` record;
- every completion cycle synchronizes `CONTEXT.md` and this roadmap;
- feature tickets ship the required interactive + `--self-test` example; infrastructure tickets use their dedicated external/configuration fixtures.

## Current execution snapshot

Current `main` before the T032 integration merge is `3a87070ae1b236a9d68f73e20f489ca5256da2ae`.

Recently completed foundations:

- **T053 / PR #88:** consumer-specific macOS Objective-C platform bridge.
- **#64 / PR #90:** standalone PROGRAM-world ownership Decision B.
- **T059 / PR #89:** generic component availability/read-only model.
- **T030 / PR #94:** Button.
- **T031 / PR #95:** Checkbox + typed RadioGroup/RadioButton.
- **T042 / PR #93:** deterministic supported-path lifecycle stress.
- **T047 / PR #92:** relocatable low-level package with `NativeUI::Core` + `nativeui_attach_platform()`.
- **T048 / PR #99:** relocated external package consumers and macOS two-consumer isolation.
- **T056 / PR #111:** deterministic CMake binary-resource packaging, merged as current `main`.

Parallel dependency frontier with T032 completing in this merge:

```text
lifecycle:        #64(done) -> T042(done) -> T051 -> T052
platform/package: T053(done) -> T047(done) -> T048(done) -> T052
                                      |
                                      +-> T054 (Ready)
                                      +-> T056(done) -> T057 (Ready)
state/widgets:    T059(done) -> T030(done) -> T031(done)
                                      |
                                      +-> T032(done by this merge) -> T037 (Ready after merge)
                                      +-> T033 (Ready)
                                      +-> T034 (Ready) -> T035 / T036
```

## Milestone 0 — Baseline hardening

**Status: Complete (T001–T006).** Stronger core/input/state tests, headless foundations, public header split, event propagation, stable node identity/lifecycle and invalidation correctness are delivered.

## Milestone 1 — Layout system

**Status: Complete (T007–T012).** Constraints, alignment/distribution, flex growth/shrink, grid, scrollable layout, clipping/overflow and layout-vs-paint invalidation are delivered.

## Milestone 2 — Input, focus and gestures

**Status: Complete (T013–T018).** Event consumption, focus scopes, pointer hover/press/capture, wheel normalization, click/gesture helpers, command routing and drag/drop primitives are delivered. Later platform hardening keeps the generic API unchanged.

## Milestone 3 — Rendering and graphics

**Status: Complete (T019–T024).** Transforms, paths, clipping, gradients, image decoding/scaling, SVG/icon resources, caches and deterministic headless/golden tests are delivered.

## Milestone 4 — Text system

**Status: Complete (T025–T029).** Text-edit model, Label, font fallback, multiline TextArea, UTF-8-safe selection/navigation and advanced Cocoa/IMM32/XIM composition are delivered with private consumer-safe platform bridges.

## Milestone 5 — Standard widget set

**Status: T030/T031 complete; T032 completes by this merge; T033/T034 Ready.**

Delivered widgets:

- **T030 / PR #94:** Button.
- **T031 / PR #95:** Checkbox + typed `RadioGroup<T>` / `RadioButton<T>`.
- **T032 / PR #115:** `Slider` + `RangeSlider` with one shared deterministic numeric domain, horizontal/vertical input mapping, keyboard editing, T059 availability/read-only semantics, optional display-only Slider formatter and explicit six-state retained visuals.

### T032 — Slider / RangeSlider contract

T032 introduces no parallel parameter model or platform control. Bound values remain application-owned `State<float>` / `State<RangeValue>` and all user writes use the shared `SliderDomain` normalization contract.

Delivered behavior:

- finite `minimum < maximum`, finite non-negative optional step validation;
- user edits quantize then clamp, while externally supplied finite values are only clamped for display and NaN/Inf use deterministic safe fallbacks without rewriting bound state during mount/paint;
- horizontal and vertical pointer drag with pointer capture; Arrow/Home/End keyboard edits use the same normalized domain;
- `RangeSlider` selects the nearest thumb, resolves exact ties to the active thumb or lower thumb, retains the selected thumb until release and enforces `low <= high` for every user write;
- Disabled/Hidden/Collapsed pointer-capture teardown stays owned by T059/tree policy; ReadOnly consumes mutating input without changing state or retaining capture;
- synchronous State observers may change widget availability during a write without duplicate writes or use-after-reentrant-callback state;
- per-instance retained interaction state only; no mutable global/singleton/`thread_local` bookkeeping;
- `Slider::formatter(...)` is presentation-only and receives the effective displayed value;
- explicit visual states: Normal, Hover, Pressed, Focused, Disabled and ReadOnly; RangeSlider renders two thumbs and the selected interval with the same state contract;
- deterministic headless visual regressions cover state variants, focus/hover/press, both orientations and RangeSlider two-thumb geometry;
- `examples/features/t032_slider.cpp` supplies interactive controls plus deterministic `--self-test` coverage.

Completion coverage includes the original pointer/keyboard/T059 widget suite plus dedicated pure-domain, invalid-external-state, reentrancy, capture-cancellation and visual tests. The branch is refreshed from T056-complete `main` instead of overwriting the merged T031 umbrella header/package CMake state.

Current widget frontier after merge:

```text
T059(done) -> T030(done) -> T031(done)
                              |
                              +-> T032(done) -> T037(Ready)
                              +-> T033(Ready)
                              +-> T034(Ready) -> T035 / T036
```

## Milestone 6 — Styling, theme and animation

**Status: T037 becomes Ready when T032 merges.** Typed theme tokens, component styles, scoped inheritance, animation/tween helpers and reduced-motion support remain planned through their issue DAG.

Dependency chain:

```text
T030(done) + T032(done) -> T037(Ready) -> T038 -> T039 / T040
```

## Milestone 7 — Platform and embedded robustness

**Status: core lifecycle/consumer-safety baseline substantially complete; independent platform tickets remain.**

Delivered safety includes:

- #62 plug-in-host safety baseline;
- #64 explicit standalone ownership Decision B;
- T053 consumer-specific Objective-C bridge naming;
- #103 Linux/X11 Skia native GL integration;
- #105 constructor-time platform callback lifetime fix;
- #107 documented non-fatal standalone raise handling;
- T042 deterministic headless/embedded/standalone lifecycle stress.

T041, T042 and the above fixes provide the current supported-path qualification foundation. T043/T044/T046 and later application/platform tickets remain governed by their explicit dependencies.

## Milestone 8 — Packaging, tooling and release

**Status: T053/T047/T048/T056 complete; T054/T057 Ready; T055 Not planned.**

### Delivered package foundation

The low-level public package contract is:

```cmake
find_package(NativeUI CONFIG REQUIRED)

target_link_libraries(MyFinalTarget PRIVATE NativeUI::Core)
nativeui_attach_platform(
    TARGET MyFinalTarget
    CONSUMER_ID com.example.product
)
```

- **T053:** exact consumer-scoped macOS Objective-C runtime naming and bridge ownership.
- **T047:** install/export package exposing Core plus the platform-attach helper; no unsafe generic precompiled macOS Objective-C bridge.
- **T048:** independent relocated Core/standalone/embedded consumers on supported platforms, including macOS two-consumer symbol/runtime isolation.
- **T056 / PR #111:** deterministic CMake-only binary-data resource targets with immutable generated tables, exact SHA-256-derived internal symbols, semicolon-safe UTF-8 identity, incremental per-resource generation and build-tree/relocated install-tree consumers.

T056's final exact-head validation passed Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan plus the T042 lifecycle matrix. The final T047 change only raises its CTest budget from 30s to 60s after macOS demonstrated the unchanged contract needs about 41s.

### Remaining package/release frontier

```text
#62(done) -> T053(done) -> T047(done) -> T048(done) ----\
                                              |          +-> T052
                                              +-> T056(done) -> T057(Ready)

T047(done) + T053(done) -> T054(Ready)
T024(done) + T042(done) -> T051(Ready) -> T052
T055 nativeui_add_plugin: Not planned for current v1
```

## T032 completion protocol

- [x] shared numeric-domain contract implemented TDD-first;
- [x] Slider pointer/keyboard/orientation/T059 behavior implemented and previously validated;
- [x] RangeSlider nearest-thumb/tie/no-crossing/keyboard behavior implemented;
- [x] invalid external state and observer-reentrancy regressions added;
- [x] Hidden/Collapsed/Disabled capture ownership and ReadOnly semantics covered;
- [x] display-only formatter and six retained visual states implemented;
- [x] deterministic headless visual coverage for both orientations and two-thumb RangeSlider added;
- [x] dedicated `t032_slider` interactive + `--self-test` feature example registered;
- [x] branch refreshed from T056-complete current `main` while preserving T031/T042/T056 integration;
- [x] `CONTEXT.md` and `ROADMAP.md` synchronized in this completion candidate;
- [ ] exact final-head normal CI green on Linux X11/Windows/macOS/Linux ASan+UBSan;
- [ ] exact final-head T042 Lifecycle Stress green;
- [ ] mandatory `CODE_REVIEW.md` exact-head review has no Blocking/Important finding;
- [ ] PR #115 merged and #32 closed Done;
- [ ] T037 moved from Blocked to Ready.

## Prioritization rule

Preserve the architectural direction:

```text
core correctness
  -> layout
  -> input/focus
  -> rendering/text primitives
  -> widgets
  -> styling
  -> platform hardening
  -> packaging/release
```

This is an architectural progression, not a serialized work queue. Once explicit dependencies are satisfied, independent tickets may proceed concurrently. Prefer work that removes the most downstream blockers while keeping PRs small, reviewable and exact-head validated.
