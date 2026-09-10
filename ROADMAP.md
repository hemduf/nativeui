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

Current `main` is `c5270a1a971d1d409a53b3715df0340fc445fb33` and includes the completed T060 explicit Application/multi-window ownership model plus its completion-context synchronization. The active P0 lifecycle correction is #139 / PR #140, which upgrades T042's stale pre-T060 multi-window marker into real shared-Application stress for the ownership path T060 made current.

Recently completed foundations relevant to the dependency graph:

- **T053 / PR #88:** consumer-specific macOS Objective-C platform bridge.
- **#64 / PR #90:** standalone PROGRAM-world ownership Decision B.
- **T059 / PR #89:** generic component availability/read-only model.
- **T030 / PR #94:** Button.
- **T047 / PR #92:** relocatable low-level package with `NativeUI::Core` + `nativeui_attach_platform()`.
- **T048 / PR #99:** relocated external package consumers and macOS two-consumer isolation.
- **T031 / PR #95:** Checkbox + typed RadioGroup/RadioButton.
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
                                       +-> T032
                                       +-> T033
                                       +-> T034 -> T035 / T036
platform/event:    T060(done) -> T065(active PR #133) -> T072 -> T064
```

T052 / PR #120 is release-gated on #139 because release qualification must exercise every currently supported ownership path. T059/T030 and subsequent widget work remain in their parallel lane; T065 is an existing independent event-loop/dispatcher stream and must not be duplicated by lifecycle work.

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

T057 / issue #69 / PR #126 is merged.

Delivered behavior:

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

TDD/review corrections covered empty-resource provider semantics, exact generated-table ordering, allocation probes, extensible public-header contracts and deterministic SVG contain-fit sampling. Final exact head `20ec256241c9419b5a4d60f8f68968f4433d2855` passed CI #600 on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan plus package/relocation contracts. The final mandatory `CODE_REVIEW.md` pass reported no Blocking/Important finding. PR #126 merged as `c2cc83b35ee8cdf469df03d40a93ca2194e6923f`; issue #69 is closed Done.

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

## T057 completion protocol

- [x] RED validation/direct lookup/provider/cache/public-header contracts established before implementation;
- [x] immutable non-owning ResourceManager and explicit allocating provider adapter implemented;
- [x] generated T056 table integration covered in build-tree and relocated external consumer;
- [x] validation, zero-copy, allocation, copy/move, concurrency, multi-manager, provider and cache integration tests covered;
- [x] dedicated feature example + `--self-test` wired;
- [x] mandatory `CODE_REVIEW.md` passes report no Blocking/Important T057 finding;
- [x] exact final documentation-complete head passed Linux X11 / Windows / macOS / Linux ASan+UBSan CI #600;
- [x] final exact-head review clean;
- [x] candidate refreshed against then-current `main` immediately before merge;
- [x] PR #126 merged and #69 marked Done/closed;
- [x] completion status synchronized into `CONTEXT.md` and this roadmap.

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
