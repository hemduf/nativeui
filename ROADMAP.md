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

Current `main` contains the completed T060 explicit Application/multi-window ownership model and the completed T032 Slider/RangeSlider widget set. T032 / PR #115 was squash-merged as `7228ea9e78ab244027337a0e34e73ccbbf33eac1`; the subsequent documentation synchronization is part of the same completion cycle. The active P0 lifecycle correction remains #139 / PR #140, which upgrades T042's stale pre-T060 multi-window marker into real shared-Application stress for the ownership path T060 made current.

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

Current dependency frontier:

```text
lifecycle/release: #64(done) -> T060(done) -> #139(active) -> T052
                   T042(done) -> T051(done) -----------------> T052
                       |             ^
                       +-> #139 -----+
platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |
                                       +-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032(done)
                                       +-> T033(active PR #123)
                                       +-> T034 -> T035 / T036
platform/event:    T060(done) -> T065(active PR #133) -> T072 -> T064
```

T052 / PR #120 is release-gated on #139 because release qualification must exercise every currently supported ownership path. T033 / PR #123 is the current existing widget stream and must be synchronized with post-T032 `main` before final exact-head qualification. T065 is an existing independent event-loop/dispatcher stream and must not be duplicated by widget work.

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

T033 / issue #33 / PR #123 is the existing active widget stream. Its implementation-complete pre-refresh head `7797e2e3cbeff15c23d5250fb27be21735e1d29c` passed normal CI, T042 Lifecycle Stress and T060 Application Contract. It remains non-interactive, platform-neutral display-only state with no hidden ticking/smoothing policy. Because T032 merged after that candidate, PR #123 must be synchronized conflict-aware with current `main`, preserving both widget sets, then rerun exact-head validation and final `CODE_REVIEW.md` review before merge. Do not create a duplicate T033 implementation stream.

## Milestone 6 — Styling, theme and animation

**Status: blocked only by explicit widget/style dependencies.** Typed theme tokens, component styles, scoped inheritance, animation/tween helpers and reduced-motion support remain planned through their issue DAG.

## Milestone 7 — Platform and embedded robustness

**Status: core lifecycle/consumer-safety baseline is delivered; #139 is the active P0 post-T060 qualification correction.**

Delivered safety includes:

- #62 plug-in-host safety baseline;
- #64 explicit standalone ownership Decision B;
- T053 consumer-specific Objective-C bridge naming;
- #103 Linux/X11 Skia native GL integration;
- #105 constructor-time platform callback lifetime fix;
- #107 documented non-fatal standalone raise handling;
- #124 reviewed Pugl/X11 failed-selection correction;
- T042 deterministic headless/embedded/legacy-standalone lifecycle stress;
- T060 explicit shared-Application multi-window ownership.

### #139 — T042 post-T060 Application lifecycle qualification

T042 originally completed before T060 existed, so `standalone_supported_multi_instance` only asserted #64 Decision B and reported that simultaneous top-level windows were deferred to T060. T060 is now merged, making that marker stale and leaving the current supported multi-window path outside T042's stress gate.

PR #140 replaces that registered gate with a dedicated platform-attached executable that keeps one explicit `ui::Application` / one `PUGL_PROGRAM` world alive for 50 deterministic A+B cycles. It validates distinct window handles and independent resize/close behavior, destroys A while B remains live and continues polling/resizing, destroys B, and repeats under `QuitPolicy::ExplicitOnly`. It does not reintroduce simultaneous independent PROGRAM worlds or any hidden singleton/global/`thread_local` owner. The legacy process-isolated standalone constructor stress remains until T069, and the embedded stress fixtures remain unchanged.

Initial GREEN head `1b917ec157a938b997e0afc6979454495b90effb` passed T060 Application Contract `34431532321`, T042 Lifecycle Stress `34431532386` on Linux/X11, Windows, macOS and Linux ASan+UBSan, and normal CI `34431532332`. Mandatory review then found stale pre-T060 diagnostic wording and that the workflow's explicit GDB/LLDB diagnostic phase did not directly run the new Application stress executable. The correction stream updates those diagnostics and requires a fresh exact-head T042/T060/normal-CI qualification before merge.

After #139 merges, T052 / PR #120 must refresh from the new `main` and rerun its exact release qualification; historical pre-#139 release runs are not sufficient.

### #124 — completed Pugl X11 failed-selection correction

PR #125 pins reviewed Pugl commit `195f79b22644010c81a5e0c3231c591856787ec6`. A failed X11 selection conversion can report `SelectionNotify.property == None`; the old dependency path passed atom `None` to `XGetWindowProperty()` and terminated with `BadAtom`. The Pugl correction guards the failed conversion before the property read. NativeUI keeps deterministic lifecycle/clipboard regression coverage and carries no local workaround.

## Milestone 8 — Packaging, tooling and release

**Status: low-level packaging, relocated consumers, native application helper, benchmark harness, binary-data generation and ResourceManager are complete. T052 release qualification is pending #139's post-T060 lifecycle gate.** Remaining M8 work proceeds through explicit dependencies and parallel lane ownership.

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

### Remaining release/platform-package frontier

```text
T024(done) + T042(done) -> T051(done) ---------> T052
T042(done) + T060(done) -> #139(active) -------> T052
T047(done) + T048(done) ------------------------> T052

T047(done) + T053(done) -> T054(done)
T056(done) + T022(done) -> T057(done)
T060(done) -> T065 -> T072 -> T064
T055 nativeui_add_plugin: Not planned for current v1
```

T052 belongs to the release/lifecycle qualification path and cannot complete from a candidate that predates #139. T064 and T072 remain blocked by T065. T065 already has an active PR and is independent of #139. Independent platform-hardening tickets T043/T044 remain separate scopes and are selected only according to current lane ownership, explicit dependencies and conflict risk.

## #139 completion protocol

- [x] RED gate proves the old decision-only `standalone_supported_multi_instance` path no longer satisfies the current contract;
- [x] GREEN shared-Application A+B stress implemented without independent simultaneous PROGRAM worlds;
- [x] initial exact-head Linux/X11, Windows, macOS, Linux ASan+UBSan, normal CI and T060 contract evidence green;
- [x] mandatory review identified stale pre-T060 diagnostic coverage and correction is implemented;
- [x] `CONTEXT.md` and `ROADMAP.md` include the post-T060 lifecycle qualification contract;
- [ ] final documentation/diagnostic-complete exact-head T042 Lifecycle Stress, T060 Application Contract and normal CI green;
- [ ] final exact-head `CODE_REVIEW.md` pass has no Blocking/Important finding;
- [ ] refresh against current `main` if it moves, merge PR #140, close #139 Done and unblock T052 refresh.

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
