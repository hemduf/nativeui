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

Current `main` before T051 integration is `3a87070ae1b236a9d68f73e20f489ca5256da2ae`.

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

Current dependency frontier:

```text
lifecycle/release: #64(done) -> T042(done) -> T051(in review) -> T052
platform/package:  T053(done) -> T047(done) -> T048(done) --------^
                                       |
                                       +-> T054
                                       +-> T056(done) -> T057
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032
                                       +-> T033
                                       +-> T034 -> T035 / T036
```

T051 / PR #116 is the current benchmark/release-lane work. Once it merges, T052 is dependency-unblocked and becomes the next P0 release/lifecycle item.

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

**Status: T030/T031 complete; T032/T033/T034 Ready.** Button and Checkbox/Radio are merged. Slider/RangeSlider, ProgressBar/Meter and the next selection/container widgets remain dependency-driven work in the widget lane.

Current widget frontier:

```text
T059(done) -> T030(done) -> T031(done)
                              |
                              +-> T032
                              +-> T033
                              +-> T034 -> T035 / T036
```

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

**Status: package foundation complete; T051 in final review; T052 blocked only by T051.**

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

PR #116 implements the Release-only T051 microbenchmark harness consumed by T052/T071:

- exact **5 warmup + 30 measured** `steady_clock` protocol;
- fixed batch counts for layout, hit-testing, pointer/keyboard dispatch, text edit, headless paint and headless lifecycle construction;
- schema/workload metadata including compiler, OS/architecture, build type and exact NativeUI commit SHA;
- benchmark-only allocation count/bytes instrumentation, isolated from production NativeUI;
- exact JSON round-trip and immutable CI result artifacts;
- metadata compatibility is enforced before threshold comparison;
- timing blocker = median **>15%** and p95 **>20%**, reproduced by two complete independent candidate runs;
- allocation blocker = allocations/op or bytes/op **>10%**, also reproduced twice; explicit zero-recurring-allocation scenarios block on any recurring allocation;
- `idle_invalidation` = 1,000 deterministic 10 ms logical scheduler checkpoints / 10 seconds logical idle, requiring exactly zero framework invalidations;
- deterministic workload-shape and allocator-scope contract coverage;
- baseline policy documented in `docs/performance-benchmarks.md`: T052 selects the controlled v0.1 CI artifact; the benchmark never self-updates a baseline.

T051 completion still requires the exact final head to pass the T051 contract gate, Release benchmark run, normal platform/sanitizer CI and T042 lifecycle-stress workflow, followed by a final `CODE_REVIEW.md` record with no Blocking/Important finding.

### Remaining release frontier

```text
T024(done) + T042(done) -> T051(in review) -> T052
T047(done) + T048(done) ----------------------^

T047(done) + T053(done) -> T054
T056(done) + T022(done) -> T057
T055 nativeui_add_plugin: Not planned for current v1
```

When T051 merges, T052 becomes Ready and owns the aggregate v0.1 developer-preview exact-head CI/package/lifecycle/performance baseline gate. T071 remains the later full NativeUI 1.0 qualification gate.

## T051 completion protocol

- [x] frozen sampling/statistics and fixed workload/batch contracts implemented;
- [x] benchmark-only allocation interception and allocation regression rules implemented;
- [x] JSON schema/round-trip and environment metadata implemented;
- [x] metadata-safe timing/allocation two-run comparison entry point implemented;
- [x] deterministic workload shapes and allocating/non-allocating counter scopes covered;
- [x] idle-invalidation hard gate implemented;
- [x] current `main` synchronized into the T051 branch;
- [x] benchmark/baseline contract documented;
- [x] `CONTEXT.md` and `ROADMAP.md` synchronized in the completion candidate;
- [ ] exact final-head T051 Release benchmark + contract workflow green;
- [ ] exact final-head normal platform/sanitizer CI and T042 lifecycle stress green;
- [ ] final mandatory `CODE_REVIEW.md` pass clean;
- [ ] merge PR #116, mark #51 Done/closed and move T052 to Ready.

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
