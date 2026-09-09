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

Current `main` baseline for the T054 completion candidate is `ce86c86e663ad5f8464224874039312143146300`, which includes T051.

Recently completed foundations:

- **T053 / PR #88:** consumer-specific macOS Objective-C platform bridge.
- **#64 / PR #90:** standalone PROGRAM-world ownership Decision B.
- **T059 / PR #89:** generic component availability/read-only model.
- **T030 / PR #94:** Button.
- **T047 / PR #92:** relocatable low-level package with `NativeUI::Core` + `nativeui_attach_platform()`.
- **T048 / PR #99:** relocated external package consumers and macOS two-consumer isolation.
- **T031 / PR #95:** Checkbox + typed RadioGroup/RadioButton.
- **T042 / PR #93:** deterministic supported-path lifecycle stress.
- **T051 / PR #116:** reproducible Release benchmark harness and regression policy.
- **T056 / PR #111:** deterministic binary-resource packaging.

Current dependency frontier:

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052
platform/package:  T053(done) -> T047(done) -> T048(done) --------^
                                       |
                                       +-> T054 (complete by PR #119 merge)
                                       +-> T056(done) -> T057 (Ready)
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032
                                       +-> T033
                                       +-> T034 -> T035 / T036
```

T054 / issue #66 / PR #119 is the active platform/package completion candidate. T057 is already dependency-unblocked by T056 + T022 and is the next planned ticket in this lane after T054. The independent P0 Pugl/X11 regression #124 / PR #125 and T042 remain separate work and must not be folded into T054.

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

**Status: T030/T031 complete; remaining widgets proceed through their explicit issue DAG.** Button and Checkbox/Radio are merged. Slider/RangeSlider, ProgressBar/Meter and subsequent selection/container widgets belong to the parallel widget lane.

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

The P0 Pugl/X11 empty-clipboard regression is tracked independently as #124 / PR #125. It owns any shared Pugl pin change and associated dependency documentation; T054 does not absorb that change.

## Milestone 8 — Packaging, tooling and release

**Status: low-level package, external-consumer qualification, benchmark harness and binary resources are complete; T054 application helper completes the high-level native application package surface in PR #119.**

### Delivered low-level package foundation

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

### T054 — `nativeui_add_application()`

PR #119 adds the frozen high-level native application helper:

```cmake
nativeui_add_application(MyApp
  PRODUCT_NAME "My App"
  BUNDLE_ID "com.example.myapp"
  VERSION "1.2.3"
  SOURCES src/main.cpp
  # MACOS_ICON path/to/icon.icns
  # WINDOWS_ICON path/to/icon.ico
)
```

The helper:

- validates portable product names, exact reverse-DNS consumer IDs, decimal `MAJOR.MINOR.PATCH`, required caller sources and platform-specific icon inputs;
- preserves original CMake argument boundaries, including semicolons/keyword spellings inside one-value arguments;
- creates a macOS `.app`, Windows GUI executable or normal Linux executable as appropriate;
- links only `NativeUI::Core` directly and delegates platform attachment to T047 with the exact `BUNDLE_ID` as `CONSUMER_ID`;
- leaves the target caller-owned and composable after helper creation;
- exposes identical behavior from build-tree and relocated install-tree packages;
- verifies real macOS plist/icon bundle output and two-application T053 Objective-C archive/prefix isolation;
- verifies target-specific Windows RC/icon generation and Linux absence of non-native packaging side effects;
- keeps the independent #124 Pugl pin change out of the T054 completion diff.

The implementation has completed multiple RED -> GREEN correction cycles for CMake argument flattening, missing-target diagnostics, private C-language bridge requirements, SOURCES parsing, bundle-resource coverage, Objective-C two-app audit coverage, portable UTF-8/control-byte validation, diagnostic formatting and consumer-ID grammar validation. Mandatory CODE_REVIEW.md passes currently report no unresolved Blocking/Important finding. The exact current-main-refreshed completion head still requires its final platform/package matrix and one final exact-head review before merge.

Once PR #119 reaches `main` through those gates, T054 is complete and issue #66 can be marked Done/closed.

### T051 — reproducible performance regression contract

T051 / PR #116 is merged. It provides the Release-only microbenchmark harness and relative regression policy consumed by T052/T071:

- exact **5 warmup + 30 measured** `steady_clock` protocol;
- fixed workload/batch contracts for layout, hit-testing, input dispatch, text editing, headless paint and lifecycle construction;
- schema/workload metadata including compiler, OS/architecture, build type and exact NativeUI commit SHA;
- benchmark-only allocation count/bytes instrumentation isolated from production NativeUI;
- exact JSON round-trip and immutable CI result artifacts;
- metadata compatibility before threshold comparison;
- timing blocker = median **>15%** and p95 **>20%**, reproduced by two complete independent runs;
- allocation blocker = allocations/op or bytes/op **>10%**, reproduced twice; explicit zero-recurring-allocation scenarios block on any recurring allocation;
- deterministic `idle_invalidation` hard gate at zero framework invalidations.

The benchmark/baseline contract remains documented in `docs/performance-benchmarks.md`.

### Remaining release frontier

```text
T024(done) + T042(done) -> T051(done) -> T052
T047(done) + T048(done) -------------------^

T047(done) + T053(done) -> T054 (complete by PR #119 merge)
T056(done) + T022(done) -> T057 (Ready)
T055 nativeui_add_plugin: Not planned for current v1
```

T052 is dependency-unblocked by T051 and belongs to the release/lifecycle lane. T057 is the next dependency-unblocked planned ticket in the platform/package lane after T054.

## T054 completion protocol

- [x] frozen helper signature and platform semantics implemented;
- [x] source/configuration contracts established RED before implementation;
- [x] build-tree and relocated install-tree consumers covered;
- [x] macOS real bundle/icon output and two-application T053 runtime namespace audits covered;
- [x] Windows GUI/RC and Linux normal-executable behavior covered;
- [x] caller target composability covered;
- [x] review findings corrected through regression-first cycles;
- [x] implementation refreshed against current `main` for the completion candidate;
- [x] `CONTEXT.md` and `ROADMAP.md` reconciled with the concurrent lanes in the same completion candidate;
- [ ] exact final-head Linux X11 / Windows / macOS / Linux ASan+UBSan T054/platform-package matrix green;
- [ ] final exact-head `CODE_REVIEW.md` pass clean;
- [ ] merge PR #119 and mark #66 Done/closed.

The unchecked items are pre-merge gates. Once this candidate reaches `main`, they have necessarily been satisfied by the repository merge rules and T054 is complete.

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
