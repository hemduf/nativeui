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

Current `main` is `ce86c86e663ad5f8464224874039312143146300`, including merged T051 / PR #116.

Recently completed foundations:

- **T053 / PR #88:** consumer-specific macOS Objective-C platform bridge.
- **#64 / PR #90:** standalone PROGRAM-world ownership Decision B.
- **T059 / PR #89:** generic component availability/read-only model.
- **T030 / PR #94:** Button.
- **T047 / PR #92:** relocatable low-level package with `NativeUI::Core` + `nativeui_attach_platform()`.
- **T048 / PR #99:** relocated external package consumers and macOS two-consumer isolation.
- **T031 / PR #95:** Checkbox + typed RadioGroup/RadioButton.
- **T042 / PR #93:** deterministic supported-path lifecycle stress.
- **T056 / PR #111:** deterministic binary-resource packaging.
- **T051 / PR #116:** reproducible Release performance benchmark/regression harness.

Current dependency frontier:

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052(ready)
platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |
                                       +-> T054
                                       +-> T056(done) -> T057
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032(in review)
                                       +-> T033(completion candidate)
                                       +-> T034(ready) -> T035 / T036
```

The current widget lane has two existing implementation streams, T032 / PR #115 and T033 / PR #123. They must be resumed rather than duplicated. T034 is Ready but should not displace recoverable in-progress widget work.

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

**Status: T030/T031 complete; T032 and T033 in completion/review; T034 Ready.** Button and Checkbox/Radio are merged. Slider/RangeSlider and ProgressBar/Meter have active completion candidates. ScrollView is the next Ready widget item once existing streams are resolved.

Current widget frontier:

```text
T059(done) -> T030(done) -> T031(done)
                              |
                              +-> T032(in review)
                              +-> T033(completion candidate)
                              +-> T034(ready) -> T035 / T036
```

### T033 — ProgressBar and Meter completion contract

PR #123 delivers the display-only bounded-value widgets with one shared presentation domain:

- finite `minimum < maximum` range validation;
- finite values presentation-clamped, non-finite values shown at minimum, with no State writeback;
- horizontal left-to-right and vertical bottom-to-top fill;
- non-focusable, input-ignored behavior with no pointer capture, timer, hidden smoothing or animation source;
- paint-only invalidation from external State changes while geometry is stable;
- optional presentation formatter receiving the effective value;
- deterministic headless minimum/midpoint/maximum/out-of-range/NaN coverage for both orientations;
- renderer-reference visual assertions that avoid backend color-space byte assumptions;
- interactive + deterministic `examples/features/t033_progress_meter.cpp --self-test`.

T033 merges only after final documentation synchronization, exact-head normal platform/sanitizer CI, T042 lifecycle stress and a clean mandatory `CODE_REVIEW.md` record.

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

**Status: package and performance foundations complete; T052 is Ready.**

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
- **T056:** deterministic `nativeui_add_binary_data()` resources with stable IDs, exact bytes, package relocation and no runtime registry.

### T051 — reproducible performance regression contract

T051 / PR #116 is merged and provides the Release-only microbenchmark harness consumed by T052/T071:

- exact **5 warmup + 30 measured** `steady_clock` protocol;
- fixed batch counts for layout, hit-testing, pointer/keyboard dispatch, text edit, headless paint and headless lifecycle construction;
- schema/workload metadata including compiler, OS/architecture, build type and exact NativeUI commit SHA;
- benchmark-only allocation count/bytes instrumentation, isolated from production NativeUI;
- exact JSON round-trip and immutable CI result artifacts;
- metadata compatibility enforced before threshold comparison;
- timing blocker = median **>15%** and p95 **>20%**, reproduced by two complete independent candidate runs;
- allocation blocker = allocations/op or bytes/op **>10%**, also reproduced twice; explicit zero-recurring-allocation scenarios block on any recurring allocation;
- `idle_invalidation` = 1,000 deterministic 10 ms logical scheduler checkpoints / 10 seconds logical idle, requiring exactly zero framework invalidations;
- deterministic workload-shape and allocator-scope contract coverage;
- baseline policy in `docs/performance-benchmarks.md`: T052 selects the controlled v0.1 CI artifact; the benchmark never self-updates a baseline.

### Remaining release frontier

```text
T024(done) + T042(done) -> T051(done) -> T052(ready)
T047(done) + T048(done) -------------------^

T047(done) + T053(done) -> T054
T056(done) + T022(done) -> T057
T055 nativeui_add_plugin: Not planned for current v1
```

T052 now owns the aggregate v0.1 developer-preview exact-head CI/package/lifecycle/performance baseline gate. T071 remains the later full NativeUI 1.0 qualification gate.

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
