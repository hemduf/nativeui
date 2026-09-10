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

Current `main` is `2caea74200e67325b4ae4b34546d94ce784dd8c7`. It contains the completed T060 explicit Application/multi-window ownership model, #139's completed post-T060 T042 shared-Application lifecycle qualification, and the completed T032 Slider/RangeSlider widget set. T052 / PR #120 is the active P0 v0.1 developer-preview release candidate. T033 / PR #123 and T065 / PR #133 are independent active streams and must not be folded into T052.

Recently completed foundations relevant to the dependency graph:

- **T053 / PR #88:** consumer-specific macOS Objective-C platform bridge.
- **#64 / PR #90:** standalone PROGRAM-world ownership Decision B.
- **T059 / PR #89:** generic component availability/read-only model.
- **T030 / PR #94:** Button.
- **T031 / PR #95:** Checkbox + typed RadioGroup/RadioButton.
- **T032 / PR #115:** Slider + RangeSlider with shared numeric/track-axis mapping and T059 availability semantics.
- **T047 / PR #92:** relocatable low-level package with `NativeUI::Core` + `nativeui_attach_platform()`.
- **T048 / PR #99:** relocated external package consumers and macOS two-consumer isolation.
- **T042 / PR #93:** deterministic supported-path lifecycle stress baseline.
- **T051 / PR #116:** reproducible Release benchmark harness and regression policy.
- **T054 / PR #119:** high-level `nativeui_add_application()` package helper.
- **T056 / PR #111:** deterministic binary-resource packaging and sorted immutable generated tables.
- **T057 / PR #126:** embedded `ResourceManager` and explicit `ResourceManagerProvider` compatibility adapter.
- **#124 / PR #125:** reviewed Pugl/X11 failed-selection correction pinned into NativeUI.
- **T060 / PR #118:** one explicit `ui::Application` / one standalone `PUGL_PROGRAM` world with multiple independent `StandaloneWindow(Application&, ...)` instances.
- **#139 / PR #140:** T042 now executes deterministic simultaneous A+B lifecycle stress through the supported T060 shared-Application ownership path while preserving #64 Decision B.

Current dependency frontier:

```text
lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(in review)
                   T042(done) -> T051(done) -----------------> T052(in review)
platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |
                                       +-> T054(done)
                     T056(done) + T022(done) -> T057(done)
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032(done)
                                       +-> T033(active PR #123)
                                       +-> T034 -> T035 / T036
platform/event:    T060(done) -> T065(active PR #133) -> T072 -> T064
```

T052 is dependency-unblocked and must qualify the exact candidate that includes #139 and current `main`. T033 and later widget work remain in their parallel lane; T065 is an existing independent event-loop/dispatcher stream and must not be duplicated by release work.

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

**Status: T030/T031/T032 complete; T033 is the active existing widget stream.** Button, Checkbox/Radio and Slider/RangeSlider are merged. ProgressBar/Meter and subsequent selection/container widgets proceed through their explicit issue DAG.

Current widget frontier:

```text
T059(done) -> T030(done) -> T031(done)
                              |
                              +-> T032(done)
                              +-> T033(active PR #123)
                              +-> T034 -> T035 / T036
```

### T032 — Slider and RangeSlider

T032 / issue #32 / PR #115 is complete.

Delivered behavior:

- one shared finite numeric domain validates range/step configuration and normalizes user writes;
- accepted wide finite binary32 ranges use `double` intermediates so range spans, fractions, keyboard increments and thumb-distance comparisons cannot overflow merely because the endpoints are large;
- horizontal/vertical `Slider` uses toolkit pointer capture, Arrow/Home/End editing, exact stepped/continuous keyboard increments and an optional presentation-only formatter;
- `RangeSlider` chooses its nearest thumb from the raw pointer position before step quantization, keeps the chosen thumb for the interaction and enforces no crossing;
- one shared `SliderTrackAxis` is used for both paint geometry and pointer coordinate mapping, including vertical inversion and formatter-reserved geometry;
- external out-of-range/NaN/Inf state is made safe for rendering/hit testing without silent mount/paint writeback;
- T059 remains the sole Disabled/Hidden/Collapsed interaction authority; ReadOnly mutating input is consumed without state mutation or new capture;
- widget interaction and subscription state is per component; no mutable process-global/singleton/`thread_local` state is introduced;
- synchronous State observer reentrancy is bounded by completing local/context mutation before `State::set()`.

Final candidate `9b6022e8f4f1ea0c579c55813bdd209d6343a9b1` passed CI `34433500137`, T042 Lifecycle Stress `34433500199` and T060 Application Contract `34433500185`. Final mandatory `CODE_REVIEW.md` review `5162634478` found no Blocking/Important issue. PR #115 was squash-merged as `7228ea9e78ab244027337a0e34e73ccbbf33eac1`; issue #32 is Done. This completion advances the widget frontier to T033/T034.

### T033 — ProgressBar and Meter

T033 / issue #33 / PR #123 is the existing active widget stream. It remains non-interactive, platform-neutral display-only state with no hidden ticking/smoothing policy and is owned by the widget lane. T052 must not duplicate or absorb it.

## Milestone 6 — Styling, theme and animation

**Status: blocked only by explicit widget/style dependencies.** Typed theme tokens, component styles, scoped inheritance, animation/tween helpers and reduced-motion support remain planned through their issue DAG.

## Milestone 7 — Platform and embedded robustness

**Status: core lifecycle/consumer-safety and explicit Application ownership are qualified through the current T042/T060 contract.** Remaining platform work is dependency-driven.

Delivered safety includes:

- #62 plug-in-host safety baseline;
- #64 explicit standalone ownership Decision B;
- T053 consumer-specific Objective-C bridge naming;
- #103 Linux/X11 Skia native GL integration;
- #105 constructor-time platform callback lifetime fix;
- #107 documented non-fatal standalone raise handling;
- #124 reviewed Pugl/X11 failed-selection correction;
- T042 deterministic headless/embedded/legacy-standalone lifecycle stress;
- T060 explicit shared-Application multi-window ownership;
- #139 direct T042 qualification of the T060 shared-Application A+B path.

### #139 — completed T042 post-T060 Application lifecycle qualification

T042 originally completed before T060 existed, so `standalone_supported_multi_instance` only asserted #64 Decision B and reported that simultaneous top-level windows were deferred to T060. T060 later made one explicit `ui::Application` / one `PUGL_PROGRAM` world with multiple `StandaloneWindow(Application&, ...)` instances the supported ownership contract.

PR #140 replaces that stale decision-only gate with a dedicated platform-attached executable that keeps one explicit Application/world alive for 50 deterministic A+B cycles. It validates distinct native handles and independent resize/close behavior, destroys A while B remains live and continues polling/resizing, destroys B, and repeats under `QuitPolicy::ExplicitOnly`. It does not reintroduce simultaneous independent PROGRAM worlds or any hidden singleton/global/`thread_local` owner. The legacy process-isolated standalone constructor stress remains until T069, and embedded T042 fixtures remain unchanged.

The final exact head `ee482da974222299bc94904ed8256511da1a256d` passed T042 Lifecycle Stress `34434737094`, T060 Application Contract `34434737109`, and normal CI `34434737107`, including Linux/X11, Windows, macOS and Linux ASan+UBSan coverage. Final `CODE_REVIEW.md` review `5162508225` found no Blocking/Important issue. PR #140 merged as `ef10f9f8ece733b1f0f19d3326be9a3ced903e41`; #139 is Done.

### #124 — completed Pugl X11 failed-selection correction

PR #125 pins reviewed Pugl commit `195f79b22644010c81a5e0c3231c591856787ec6`. A failed X11 selection conversion can report `SelectionNotify.property == None`; the old dependency path passed atom `None` to `XGetWindowProperty()` and terminated with `BadAtom`. The Pugl correction guards the failed conversion before the property read. NativeUI keeps deterministic lifecycle/clipboard regression coverage and carries no local workaround.

## Milestone 8 — Packaging, tooling and release

**Status: low-level packaging, relocated consumers, native application helper, benchmark harness, binary-data generation and ResourceManager are complete; T052 is in v0.1 developer-preview qualification on the current post-#139 baseline.** Remaining M8 work proceeds through explicit dependencies and parallel lane ownership.

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

### T057 — embedded `ResourceManager`

T057 / issue #69 / PR #126 is merged. Constructor validation is allocation-free, IDs are sorted/unique, `find()` is allocation-free O(log N) zero-copy lookup, invalid managers fail atomically, managers have no shared mutable registry, and `ResourceManagerProvider` is the explicit allocating compatibility adapter. Final head `20ec256241c9419b5a4d60f8f68968f4433d2855` passed CI #600 across Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan plus package/relocation contracts; final review reported no Blocking/Important finding.

### T051 — reproducible performance regression contract

T051 / PR #116 is merged. It provides the Release-only microbenchmark harness and relative regression policy consumed by T052/T071, with its benchmark/baseline contract documented in `docs/performance-benchmarks.md`.

### T052 — v0.1 developer-preview release gate

PR #120 is the active aggregate qualification candidate and remains validation/release infrastructure only:

- exact candidate SHA and approved-base SHA are explicit workflow inputs;
- clean-cache Linux/X11, Windows and macOS bootstraps verify pinned Pugl/Skia acquisition and fail-closed checksum behavior;
- the exact release-note low-level package CMake snippet is built against the installed package on all supported desktop platforms;
- normal CI supplies T047/T048 relocation, macOS two-consumer Objective-C namespace/runtime isolation, feature/headless tests and Linux ASan+UBSan;
- T042 stress independently requalifies supported lifecycle/multi-instance ownership paths, including #139's T060 shared-Application A+B stress, while preserving #64 Decision B;
- T060 Application Contract remains an independent exact-head ownership gate;
- T051 benchmark comparison is delegated to the canonical C++ two-run policy entry point, with exact baseline/candidate SHA validation and the zero `idle_invalidation` hard gate;
- release notes state v0.1 developer-preview semantics, known v1 gaps, pinned dependencies, legal/licensing notices and a reproducible exact-SHA tag procedure.

The previous exact candidate `c6bb556f58668fc0aeb63bcda618078484b5a62b` passed T052 Release Gate `34436088259`, normal CI `34436088210`, T042 Lifecycle Stress `34436088216`, T051 Release Benchmarks `34436088267` and T060 Application Contract `34436088199`. Aggregate static review `5162574668` found no Blocking/Important issue. Because current `main` advanced afterward with T032 and its completion documentation, that evidence is historical and the refreshed merge candidate must rerun the complete exact-head qualification set.

### Remaining release/platform-package frontier

```text
T024(done) + T042(done) -> T051(done) ----------------> T052(in review)
T042(done) + T060(done) -> #139(done) -----------------> T052(in review)
T047(done) + T048(done) -------------------------------> T052(in review)

T047(done) + T053(done) -> T054(done)
T056(done) + T022(done) -> T057(done)
T060(done) -> T065 -> T072 -> T064
T055 nativeui_add_plugin: Not planned for current v1
```

T052 is the active release/lifecycle qualification item. T064 and T072 remain blocked by T065. T065 already has an active PR and is independent of T052. Independent platform-hardening tickets T043/T044 remain separate scopes and are selected only according to current lane ownership, explicit dependencies and conflict risk.

## #139 completion protocol

- [x] RED gate proves the old decision-only `standalone_supported_multi_instance` path no longer satisfies the current contract;
- [x] GREEN shared-Application A+B stress implemented without independent simultaneous PROGRAM worlds;
- [x] final exact-head T042 Lifecycle Stress, T060 Application Contract and normal CI green across required platforms/sanitizers;
- [x] mandatory review corrected stale pre-T060 diagnostics and added direct debugger coverage;
- [x] final exact-head `CODE_REVIEW.md` pass has no Blocking/Important finding;
- [x] `CONTEXT.md` and `ROADMAP.md` include the post-T060 lifecycle qualification contract;
- [x] PR #140 merged, #139 closed Done and T052 refresh unblocked.

## T052 completion protocol

- [x] dependency frontier satisfied: T047/T048/T042/T051/T060/#139 merged;
- [x] exact candidate/release source contract implemented;
- [x] clean-cache bootstrap matrix and release-note installed-package consumer implemented;
- [x] canonical T051 C++ two-run benchmark policy gate integrated;
- [x] zero `idle_invalidation` hard gate preserved;
- [x] v0.1 developer-preview/release/tag documentation implemented;
- [x] licensing/legal payload contract and release-note references implemented;
- [x] #124/Pugl correction, T060 ownership model and #139 T042 post-T060 lifecycle qualification are included in the release baseline;
- [ ] refreshed exact-head T052 Release Gate green;
- [ ] refreshed exact-head normal platform/sanitizer CI, T042 lifecycle stress, T051 Release benchmark and T060 Application workflows green;
- [ ] aggregate mandatory `CODE_REVIEW.md` pass clean on the refreshed exact head;
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
