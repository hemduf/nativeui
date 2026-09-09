# NativeUI roadmap

**Updated:** 2026-09-10

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

Current `main` is `005a207237a98726573160b8f58c9a9ebff262f9` and includes the completed T054 native application package helper.

Recently completed foundations relevant to the dependency graph:

- **T053 / PR #88:** consumer-specific macOS Objective-C platform bridge.
- **#64 / PR #90:** standalone PROGRAM-world ownership Decision B.
- **T059 / PR #89:** generic component availability/read-only model.
- **T030 / PR #94:** Button.
- **T047 / PR #92:** relocatable low-level package with `NativeUI::Core` + `nativeui_attach_platform()`.
- **T048 / PR #99:** relocated external package consumers and macOS two-consumer isolation.
- **T031 / PR #95:** Checkbox + typed RadioGroup/RadioButton.
- **T042 / PR #93:** deterministic supported-path lifecycle stress.
- **T051 / PR #116:** reproducible Release benchmark harness and regression policy.
- **T054 / PR #119:** high-level `nativeui_add_application()` package helper.
- **T056 / PR #111:** deterministic binary-resource packaging and sorted immutable generated tables.

Current dependency frontier:

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052
platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |
                                       +-> T054(done)
                                       +-> T056(done) + T022(done) -> T057 (PR #126)
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032
                                       +-> T033
                                       +-> T034 -> T035 / T036
```

T057 / issue #69 / PR #126 is the active platform/package ticket. The independent P0 Pugl/X11 regression #124 / PR #125 and T042 are separate work; T059, T030 and #64 also belong to other lanes and must not be folded into T057.

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

The P0 Pugl/X11 regression is tracked independently as #124 / PR #125. It owns any shared Pugl pin change and associated dependency documentation; the platform/package resource lane does not absorb that change.

## Milestone 8 — Packaging, tooling and release

**Status: low-level packaging, external consumers, native application helper, benchmark harness and binary-data generation are complete; T057 ResourceManager is the active platform/package resource ticket.**

### Delivered package foundation

Low-level final-target attachment:

```cmake
find_package(NativeUI CONFIG REQUIRED)

target_link_libraries(MyFinalTarget PRIVATE NativeUI::Core)
nativeui_attach_platform(
    TARGET MyFinalTarget
    CONSUMER_ID com.example.product
)
```

High-level application packaging:

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

Delivered contracts:

- **T053:** exact consumer-scoped macOS Objective-C runtime naming and bridge ownership.
- **T047:** install/export package exposing Core plus the platform-attach helper; no unsafe generic precompiled macOS Objective-C bridge.
- **T048:** independent relocated Core/standalone/embedded consumers on supported platforms, including macOS two-consumer symbol/runtime isolation.
- **T054:** validated build-tree/relocated `nativeui_add_application()` with macOS app bundles, Windows GUI resources and normal Linux executables, delegating platform attachment to T047/T053.
- **T056:** deterministic `nativeui_add_binary_data()` resources with stable IDs, exact bytes, package relocation, sorted immutable generated tables and no runtime registry.

### T057 — embedded `ResourceManager`

PR #126 adds the resource lookup/adapter layer over T056 tables:

- constructor performs one allocation-free O(N) validation pass over borrowed entries;
- valid IDs are non-empty, unique and strictly ascending by exact unsigned-byte lexicographic order;
- invalid managers fail atomically and direct APIs behave empty/false;
- `find()` performs allocation-free O(log N) binary search and returns borrowed zero-copy `ResourceView` spans;
- `resources()` returns the original validated table and copies/moves retain the same borrowed identity;
- independent managers have no shared mutable registry/cache state and immutable concurrent reads require no lock;
- `ResourceManagerProvider` is the explicit allocating compatibility seam for `ResourceProvider`; successful non-empty loads copy exact bytes and are documented not real-time safe;
- ImageCache/SvgCache consume the adapter without ResourceManager-specific decoding;
- the real T056 generated table is consumed by build-tree and relocated install-tree external consumers;
- the dedicated `t057_embedded_resources` example supports interactive use and deterministic `--self-test`.

TDD/review corrections cover empty-resource provider semantics, exact generated-table ordering, allocation probes, extensible public-header contracts and SVG contain-fit sampling. The mandatory CODE_REVIEW.md pass on code head `2860f5ab7d1daa1e0aa2553f988a90b0f6b94ac9` reports no Blocking/Important T057 finding.

### T051 — reproducible performance regression contract

T051 / PR #116 is merged. It provides the Release-only microbenchmark harness and relative regression policy consumed by T052/T071, with its benchmark/baseline contract documented in `docs/performance-benchmarks.md`.

### Remaining release/platform-package frontier

```text
T024(done) + T042(done) -> T051(done) -> T052
T047(done) + T048(done) -------------------^

T047(done) + T053(done) -> T054(done)
T056(done) + T022(done) -> T057 (PR #126)
T055 nativeui_add_plugin: Not planned for current v1
```

T052 belongs to the separate release/lifecycle lane. After T057 merges, live GitHub issue dependencies must be re-read before selecting any next platform/package ticket; readiness must not be inferred from milestone numbering.

## T057 completion protocol

- [x] RED validation/direct lookup/provider/cache/public-header contracts established before implementation;
- [x] immutable non-owning ResourceManager and explicit allocating provider adapter implemented;
- [x] generated T056 table integration covered in build-tree and relocated external consumer;
- [x] validation, zero-copy, allocation, copy/move, concurrency, multi-manager, provider and cache integration tests covered;
- [x] dedicated feature example + `--self-test` wired;
- [x] first mandatory CODE_REVIEW.md pass on the exact code candidate has no Blocking/Important T057 finding;
- [x] `CONTEXT.md` and `ROADMAP.md` reconciled with merged T054 and the active parallel lanes in the completion candidate;
- [ ] exact final documentation-complete head Linux X11 / Windows / macOS / Linux ASan+UBSan matrix green;
- [ ] final exact-head `CODE_REVIEW.md` pass clean;
- [ ] refresh against current `main`, merge PR #126 and mark #69 Done/closed.

The unchecked items are pre-merge gates. The unrelated T042/#124 workflow state is not a T057 acceptance gate unless repository policy explicitly makes it a required check for this PR.

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
