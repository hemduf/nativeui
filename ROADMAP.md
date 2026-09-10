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

Current `main` is `c5270a1a971d1d409a53b3715df0340fc445fb33` and contains the completed T060 explicit Application/multi-window ownership model plus its recovery-context synchronization. T052 / PR #120 is the active P0 lifecycle/release candidate. T065 / PR #133 is an independent active platform stream and must not be folded into T052.

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
- **#124 / PR #125:** reviewed Pugl X11 failed-selection correction.
- **T060 / PR #118:** explicit one-Application/one-PROGRAM-world multi-window ownership model.

Current dependency frontier:

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052(in review)
platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                     T056(done) + T022(done) -> T057(done)

application/platform: #64(done) -> T060(done) -> T065(active PR #133) -> T072 -> T064
                                      |
                                      +-> T066 (also depends on T043)

state/widgets: T059(done) -> T030(done) -> T031(done)
                                    |-> T032
                                    |-> T033
                                    +-> T034 -> T035 / T036

platform hardening: #124(done), T043 ready, T044 ready
```

T052 is the current lifecycle/release lane item. T065 and other platform work remain independent; widget/state work remains in its own lanes.

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
                              |-> T032
                              |-> T033
                              +-> T034 -> T035 / T036
```

## Milestone 6 — Styling, theme and animation

**Status: blocked only by explicit widget/style dependencies.** Typed theme tokens, component styles, scoped inheritance, animation/tween helpers and reduced-motion support remain planned through their issue DAG.

## Milestone 7 — Platform and embedded robustness

**Status: core lifecycle/consumer-safety and explicit Application ownership are substantially complete; remaining platform work is dependency-driven.**

Delivered safety includes:

- #62 plug-in-host safety baseline;
- #64 explicit standalone ownership Decision B;
- T053 consumer-specific Objective-C bridge naming;
- #103 Linux/X11 Skia native GL integration;
- #105 constructor-time platform callback lifetime fix;
- #107 documented non-fatal standalone raise handling;
- T042 deterministic headless/embedded/standalone lifecycle stress;
- #124 reviewed Pugl X11 failed-selection guard;
- T060 explicit one-Application/one-PROGRAM-world multi-window ownership without a hidden singleton.

### #124 — Pugl X11 failed-selection correction

PR #125 pins reviewed Pugl commit `195f79b22644010c81a5e0c3231c591856787ec6`. A failed X11 selection conversion can report `SelectionNotify.property == None`; the old dependency path passed atom `None` to `XGetWindowProperty()` and terminated with `BadAtom`. The Pugl correction guards the failed conversion before the property read. NativeUI keeps the deterministic T042 clipboard/lifecycle fixture intact and does not add a local workaround.

The synchronized completion head passed normal CI and T042 Lifecycle Stress with no Blocking/Important `CODE_REVIEW.md` finding. PR #125 merged to `main` as `58f45ee02b32a1a3fcb139cc8345276ee334844c`; #124 is complete.

### T060 — explicit Application ownership

T060 / issue #72 / PR #118 is complete. One `ui::Application` owns exactly one standalone `PUGL_PROGRAM` world and outlives its `StandaloneWindow(Application&, ...)` views. Multiple top-level windows share only the Application/world/event-loop owner while retaining independent per-window UI, renderer, focus, capture and callback state. `EmbeddedView` remains an independent `PUGL_MODULE` ownership path. No mutable process-global or `thread_local` Application registry was introduced.

Exact candidate `0e4cce56bd8874545794fdf1137d1c7ec5489dde` passed T060 Application Contract `34422787634`, T042 Lifecycle Stress `34422787683`, and normal CI `34422787695`; final review `5161881319` had no Blocking/Important finding. PR #118 merged as `352bdf0e734df46e8edcb53a0a81a07c9e0d7d6d` and the follow-up context sync advanced `main` to `c5270a1a971d1d409a53b3715df0340fc445fb33`.

## Milestone 8 — Packaging, tooling and release

**Status: low-level packaging, relocated consumers, native application helper, benchmark harness, binary-data generation and ResourceManager are complete; T052 is in v0.1 developer-preview qualification.**

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

### T052 — v0.1 developer-preview release gate

PR #120 is the active aggregate qualification candidate and remains validation/release infrastructure only:

- exact candidate SHA and approved-base SHA are explicit workflow inputs;
- clean-cache Linux/X11, Windows and macOS bootstraps verify pinned Pugl/Skia acquisition and fail-closed checksum behavior;
- the exact release-note low-level package CMake snippet is built against the installed package on all supported desktop platforms;
- normal CI supplies T047/T048 relocation, macOS two-consumer Objective-C namespace/runtime isolation, feature/headless tests and Linux ASan+UBSan;
- T042 stress independently requalifies supported lifecycle/multi-instance ownership paths while preserving #64 Decision B and the T060 explicit Application ownership contract;
- T051 benchmark comparison is delegated to the canonical C++ two-run policy entry point, with exact baseline/candidate SHA validation and the zero `idle_invalidation` hard gate;
- release notes state v0.1 developer-preview semantics, known v1 gaps, pinned dependencies, legal/licensing notices and a reproducible exact-SHA tag procedure.

The branch was refreshed from exact current `main` via PR #138 without rewriting T052 history. Every source change after that refresh requires a fresh T052/CI/T042/T051 exact-head qualification.

### Remaining release/platform frontier

```text
T024(done) + T042(done) -> T051(done) -> T052(in review)
T047(done) + T048(done) -------------------^

T047(done) + T053(done) -> T054(done)
T056(done) + T022(done) -> T057(done)

#64(done) -> T060(done) -> T065(active PR #133) -> T072 -> T064
                     |
                     +-> T066 (also depends on T043)

T055 nativeui_add_plugin: Not planned for current v1
```

T052 belongs to the release/lifecycle lane. T065/T072/T064 and T043/T044 remain separate platform scopes.

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

## #124 completion protocol

- [x] root cause isolated to Pugl X11 failed-selection handling;
- [x] Pugl regression fixed and reviewed in the dependency repository;
- [x] NativeUI pin updated without weakening T042 or adding a local workaround;
- [x] `THIRD_PARTY.md`, `CONTEXT.md`, `ROADMAP.md` and `VALIDATION.md` included in the completion cycle;
- [x] final exact-head normal CI and T042 Lifecycle Stress green after synchronization with current `main`;
- [x] mandatory `CODE_REVIEW.md` pass records no Blocking/Important finding;
- [x] PR #125 merged and #124 closed Done.

## T052 completion protocol

- [x] dependency frontier satisfied: T047/T048/T042/T051 merged;
- [x] exact candidate/release source contract implemented;
- [x] clean-cache bootstrap matrix and release-note installed-package consumer implemented;
- [x] canonical T051 C++ two-run benchmark policy gate integrated;
- [x] zero `idle_invalidation` hard gate preserved;
- [x] v0.1 developer-preview/release/tag documentation implemented;
- [x] licensing/legal payload contract and release-note references implemented;
- [x] #124/Pugl correction and T060 ownership model are merged on `main` and included in the refreshed release baseline;
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
