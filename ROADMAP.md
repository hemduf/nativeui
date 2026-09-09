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
- feature tickets ship the required interactive + `--self-test` example; infrastructure tickets use dedicated test/consumer/benchmark artifacts.

## Current execution snapshot

Current `main` before T032 integration is `ce86c86e663ad5f8464224874039312143146300`.

Recently completed foundations:

- **T053 / PR #88:** consumer-specific macOS Objective-C platform bridge.
- **#64 / PR #90:** standalone PROGRAM-world ownership Decision B.
- **T059 / PR #89:** generic component availability/read-only model.
- **T030 / PR #94:** Button.
- **T031 / PR #95:** Checkbox + typed RadioGroup/RadioButton.
- **T042 / PR #93:** deterministic supported-path lifecycle stress.
- **T047 / PR #92:** relocatable low-level package with `NativeUI::Core` + `nativeui_attach_platform()`.
- **T048 / PR #99:** relocated external package consumers and macOS two-consumer isolation.
- **T056 / PR #111:** deterministic binary-resource packaging.
- **T051 / PR #116:** reproducible Release benchmark harness and relative performance-budget policy.

Current dependency frontier:

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052(Doing)
platform/package:  T053(done) -> T047(done) -> T048(done) --------^
                                       |
                                       +-> T054
                                       +-> T056(done) -> T057
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032(current) -> T037
                                       +-> T033(Ready)
                                       +-> T034(Ready) -> T035 / T036
```

T052 is owned by the independent release lane. T032 is the current retained-state/widget completion candidate and must not be serialized behind T052.

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

**Status: T030/T031 complete; T032 in final integration; T033/T034 Ready.**

### T032 — Slider / RangeSlider — PR #115

T032 consumes T059 rather than introducing widget-local Disabled/ReadOnly policy. The completion candidate delivers:

- one shared numeric domain for finite range/step validation, quantize-then-clamp user writes and finite render-only fallback for external State;
- horizontal/vertical `Slider` pointer capture plus Arrow/Home/End keyboard editing;
- exact stepped keyboard increments and continuous range/100 versus Shift range/1000 behavior;
- `RangeSlider` with one `RangeValue`, deterministic nearest-thumb selection from the raw pointer position before quantization, stable active-thumb ownership and no crossing;
- Disabled/Hidden/Collapsed behavior owned by T059 and ReadOnly mutation consumption without State writes or new capture;
- interaction bookkeeping ordered before synchronous `State::set()` so observer reentrancy cannot produce duplicate writes or post-callback context access;
- deterministic visual states Normal/Hover/Pressed/Focused/Disabled/ReadOnly;
- dedicated domain/completion/headless visual tests plus `examples/features/t032_slider.cpp --self-test`.

Mandatory review found one Important nearest-thumb bug: step quantization could manufacture a false tie before hit selection. The regression is fixed by selecting the thumb from the raw continuous pointer value and applying quantization only to the subsequent State write. No Blocking/Important production finding remains in the reviewed diff.

The branch has been manually reconciled with current `main` while preserving T051. Exact-head normal CI and T042 Lifecycle Stress must rerun green after the synchronization/documentation commits. Merge only after a final exact-head `CODE_REVIEW.md` refresh.

Widget frontier after T032:

```text
T059(done) -> T030(done) -> T031(done)
                              |
                              +-> T032(current) -> T037
                              +-> T033(Ready)
                              +-> T034(Ready) -> T035 / T036
```

When T032 merges, T037 becomes Ready if all of its other explicit dependencies are already complete. T033 and T034 remain independent Ready work.

## Milestone 6 — Styling, theme and animation

**Status: blocked only by explicit widget/style dependencies.** Typed theme tokens, component styles, scoped inheritance, animation/tween helpers and reduced-motion support remain planned through their issue DAG.

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

T041/T042 and the above fixes provide the current supported-path qualification foundation. T043/T044/T046 and later application/platform tickets remain governed by their explicit dependencies.

## Milestone 8 — Packaging, tooling and release

**Status: low-level package/resource/performance foundations complete; T052 v0.1 aggregate gate is in progress.**

### Delivered package/release foundation

The low-level public package contract remains:

```cmake
find_package(NativeUI CONFIG REQUIRED)

target_link_libraries(MyFinalTarget PRIVATE NativeUI::Core)
nativeui_attach_platform(
    TARGET MyFinalTarget
    CONSUMER_ID com.example.product
)
```

Delivered infrastructure:

- **T053:** exact consumer-scoped macOS Objective-C runtime naming and bridge ownership.
- **T047:** install/export package exposing Core plus the platform-attach helper; no unsafe generic precompiled macOS Objective-C bridge.
- **T048:** independent relocated Core/standalone/embedded consumers on supported platforms, including macOS two-consumer symbol/runtime isolation.
- **T056:** deterministic `nativeui_add_binary_data()` resources with stable IDs, exact bytes, package relocation and no runtime registry.
- **T051:** Release-only deterministic microbenchmark harness with exact 5 warmup + 30 measured protocol, fixed workload metadata, benchmark-only allocation instrumentation, stable JSON artifacts, metadata-safe comparison, two-run regression confirmation and zero-idle-invalidation hard gate. Detailed protocol remains in `docs/performance-benchmarks.md`.

### Current release frontier

```text
T024(done) + T042(done) -> T051(done) -> T052(Doing)
T047(done) + T048(done) ---------------------^

T047(done) + T053(done) -> T054
T056(done) + T022(done) -> T057
T055 nativeui_add_plugin: Not planned for current v1
```

T052 owns the aggregate v0.1 developer-preview exact-head CI/package/lifecycle/performance baseline gate. It is intentionally distinct from the later full NativeUI 1.0 qualification gate T071 and does not block independent widget development.

## T032 completion protocol

- [x] shared Slider/RangeSlider numeric domain implemented test-first;
- [x] Slider pointer/keyboard/T059 behavior implemented and covered;
- [x] RangeSlider nearest/tie/active-thumb/no-crossing behavior implemented and covered;
- [x] invalid/non-finite external-state render-only behavior covered;
- [x] reentrant State-observer and T059 capture-cancellation regressions covered;
- [x] deterministic visual/headless tests and feature example registered;
- [x] Important raw-pointer nearest-thumb review finding corrected regression-first;
- [x] current `main` structurally synchronized without dropping T051 infrastructure;
- [x] `CONTEXT.md` and `ROADMAP.md` reconciled in the completion candidate;
- [ ] exact final-head normal CI green on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan;
- [ ] exact final-head T042 Lifecycle Stress green;
- [ ] final mandatory `CODE_REVIEW.md` refresh clean;
- [ ] merge PR #115, mark #32 Done/closed and update dependency frontier.

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