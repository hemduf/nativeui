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

Current `main` is `58f45ee02b32a1a3fcb139cc8345276ee334844c`. It includes the completed T057 ResourceManager and the reviewed #124 / PR #125 Pugl X11 failed-selection correction. T052 / PR #120 is the active P0 lifecycle/release candidate and is being synchronized with this exact baseline before final qualification.

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
- **T057 / PR #126:** embedded `ResourceManager` and explicit `ResourceManagerProvider` compatibility adapter.
- **#124 / PR #125:** Pugl X11 failed-selection correction, merged after exact-head normal CI and T042 lifecycle stress passed.

Current dependency frontier:

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052(in review)
platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |
                                       +-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032
                                       +-> T033
                                       +-> T034 -> T035 / T036
platform fix:       #124(done)
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

**Status: current supported lifecycle/consumer-safety baseline complete; later platform tickets remain dependency-driven.**

Delivered safety includes:

- #62 plug-in-host safety baseline;
- #64 explicit standalone ownership Decision B;
- T053 consumer-specific Objective-C bridge naming;
- #103 Linux/X11 Skia native GL integration;
- #105 constructor-time platform callback lifetime fix;
- #107 documented non-fatal standalone raise handling;
- T042 deterministic headless/embedded/standalone lifecycle stress;
- #124 reviewed Pugl X11 failed-selection guard, preventing `SelectionNotify.property == None` from reaching `XGetWindowProperty()` as atom `None` while leaving T042 unchanged.

The #124 completion head passed normal CI and T042 Lifecycle Stress with no Blocking/Important `CODE_REVIEW.md` finding before PR #125 merged to `main` as `58f45ee02b32a1a3fcb139cc8345276ee334844c`.

## Milestone 8 — Packaging, tooling and release

**Status: package/performance foundations complete; T052 is in final v0.1 developer-preview qualification.**

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
- **T057:** non-owning validated `ResourceManager`, zero-copy binary-search lookup and explicit allocating `ResourceManagerProvider` adapter for existing cache/provider APIs.
- **T051:** Release-only deterministic microbenchmark harness and relative regression policy consumed by T052/T071.

### T052 — v0.1 developer-preview release gate

PR #120 is the active aggregate qualification candidate and remains validation/release infrastructure only:

- exact candidate SHA and approved-base SHA are explicit inputs;
- clean-cache Linux/X11, Windows and macOS bootstraps verify pinned Pugl/Skia acquisition and fail-closed checksum behavior;
- the exact release-note low-level package CMake snippet is built against the installed package on all supported desktop platforms;
- normal CI supplies T047/T048 relocation, macOS two-consumer Objective-C namespace/runtime isolation, feature/headless tests and Linux ASan+UBSan;
- T042 stress independently requalifies supported lifecycle/multi-instance ownership paths while preserving #64 Decision B;
- T051 benchmark comparison is delegated to the canonical C++ two-run policy entry point, with exact baseline/candidate SHA validation and the zero `idle_invalidation` hard gate;
- release notes state v0.1 developer-preview semantics, known v1 gaps, pinned dependencies, legal/licensing notices and a reproducible exact-SHA tag procedure.

The candidate must be refreshed from current `main` immediately before qualification. The refresh preserves T054/T057/#124 state and the T052 release files; it does not absorb unrelated feature work. Any changed candidate SHA requires fresh T052, normal CI, T042 and T051 workflow evidence.

### Remaining release frontier

```text
T024(done) + T042(done) + T051(done) -> T052(in review)
T047(done) + T048(done) ---------------------^

T047(done) + T053(done) -> T054(done)
T056(done) + T022(done) -> T057(done)
T065 -> T072 -> T064
T055 nativeui_add_plugin: Not planned for current v1
```

T071 remains the later full NativeUI 1.0 qualification gate and is deliberately distinct from T052's v0.1 developer-preview baseline.

## T052 completion protocol

- [x] dependency frontier satisfied: T047/T048/T042/T051 merged;
- [x] exact candidate/release source contract implemented;
- [x] clean-cache bootstrap matrix and release-note installed-package consumer implemented;
- [x] canonical T051 C++ two-run benchmark policy gate integrated;
- [x] zero `idle_invalidation` hard gate preserved;
- [x] v0.1 developer-preview/release/tag documentation implemented;
- [x] licensing/legal payload contract and release-note references implemented;
- [x] #124/Pugl correction is merged on `main` and included in the synchronized release baseline;
- [ ] exact synchronized-head T052 release workflow green;
- [ ] exact synchronized-head normal platform/sanitizer CI, T042 lifecycle stress and T051 Release benchmark workflows green;
- [ ] aggregate mandatory `CODE_REVIEW.md` pass clean on the synchronized exact head;
- [ ] merge PR #120 without rewriting the validated candidate, mark #52 Done/closed and retain exact v0.1 qualification evidence.

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
