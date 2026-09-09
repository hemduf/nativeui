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
- feature tickets ship the required interactive + `--self-test` example; infrastructure tickets use their dedicated external/configuration fixtures.

## Current execution snapshot

Current `main` before the T056 integration merge is `4cd904466d05e4b403eb2b61386868e7c498c719`.

Recently completed foundations:

- **T053 / PR #88:** consumer-specific macOS Objective-C platform bridge.
- **#64 / PR #90:** standalone PROGRAM-world ownership Decision B.
- **T059 / PR #89:** generic component availability/read-only model.
- **T030 / PR #94:** Button.
- **T047 / PR #92:** relocatable low-level package with `NativeUI::Core` + `nativeui_attach_platform()`.
- **T048 / PR #99:** relocated external package consumers and macOS two-consumer isolation.
- **T031 / PR #95:** Checkbox + typed RadioGroup/RadioButton.
- **T042 / PR #93:** deterministic supported-path lifecycle stress, merged as current `main`.

Parallel dependency frontier with T056 completing in this merge:

```text
lifecycle:        #64(done) -> T042(done) -> T051 -> T052
platform/package: T053(done) -> T047(done) -> T048(done) -> T052
                                      |
                                      +-> T054 (Ready)
                                      +-> T056 (done by this merge) -> T057 (Ready after merge)
state/widgets:    T059(done) -> T030(done) -> T031(done)
                                      |
                                      +-> T032
                                      +-> T033
                                      +-> T034 -> T035 / T036
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

T041, T042 and the above fixes provide the current supported-path qualification foundation. T043/T044/T046 and later application/platform tickets remain governed by their explicit dependencies.

## Milestone 8 — Packaging, tooling and release

**Status: T053/T047/T048 complete; T056 completes by this merge; T054 Ready; T057 becomes Ready after this merge; T055 Not planned.**

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

### T056 — deterministic binary resources

**Complete by PR #111 merge.** The final contract is:

```cmake
nativeui_add_binary_data(MyResources
    NAMESPACE myapp::resources
    [BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}]
    SOURCES ...
    [ALIASES "source=logical/id" ...]
)
```

Delivered scope:

- one normal STATIC resource target per helper call;
- public backend-neutral `ui::EmbeddedResourceEntry` containing immutable non-owning `id`/byte spans;
- deterministic generated header `nativeui_binary_data/<Target>/resources.hpp` exposing one namespaced `table()`;
- namespace, source/base containment, alias and duplicate-ID validation;
- exact byte-lexicographic sorted unique IDs, including supported punctuation/Unicode text without CMake-list corruption;
- one CMake-script-generated payload `.cpp` per input for bounded incremental rebuilds;
- exact internal symbol form `nativeui_bd_<target SHA256 first12>_<ID SHA256 first16>`;
- exact text/binary/NUL/empty payload preservation with immutable static lifetime and no runtime registry/startup copy;
- deterministic clean-build generated output containing no timestamp, host/user identity or absolute source/build paths;
- independent same-ID/different-bytes targets/namespaces;
- build-tree and relocated install-tree consumers using only CMake/package API.

TDD/review correction: the first implementation used raw CMake list storage in places where valid semicolon-containing resource IDs/source paths could split list elements. Regression tests were added first, then final IDs were moved to exact UTF-8 hex ordering/storage and canonical source identity to exact hex-keyed variables. Final pre-refresh `CODE_REVIEW.md` review on head `4cc48ed083a493519b19e8481007a0a6315d0a71` has no remaining Blocking/Important finding.

Pre-refresh exact-head evidence:

- CI #451 (`34369151069`) passed Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan, including T047/T048/T056 package/external-consumer contracts;
- T042 Lifecycle Stress #23 passed on the same head.

Before merge, PR #111 is refreshed from current `main`, preserving merged T031/T042/platform work while applying only the T056 package additions and these documentation updates. The refreshed exact head must rerun all required gates; earlier-SHA green results do not substitute for that final validation.

### Remaining package/release frontier

```text
#62(done) -> T053(done) -> T047(done) -> T048(done) ----\
                                              |          +-> T052
                                              +-> T056(done) -> T057

T047(done) + T053(done) -> T054
T024(done) + T042(done) -> T051 -> T052
T055 nativeui_add_plugin: Not planned for current v1
```

After T056 merges:

- **T054 / #66** is Ready and remains the next recommended platform/package ticket when no existing platform/package PR supersedes it;
- **T057 / #69** becomes Ready because T056 and T022 are complete;
- **T051** is Ready because T024/T042 are complete; it belongs to the benchmark/release lane rather than this platform/package lane;
- **T052** remains blocked on T051 only with its package/lifecycle dependencies satisfied.

## T056 completion protocol

- [x] strict TDD/configuration-contract coverage implemented;
- [x] deterministic binary/resource/package fixtures implemented;
- [x] semicolon identity review finding reproduced RED and corrected GREEN;
- [x] full pre-refresh four-platform + sanitizer CI green;
- [x] mandatory `CODE_REVIEW.md` review complete with no remaining Blocking/Important finding;
- [x] refreshed merge candidate preserves current-main CMake/platform/widget additions;
- [x] `CONTEXT.md` and `ROADMAP.md` synchronized in the merge candidate;
- final merge requires the refreshed exact-head required workflows to be green and an exact-head review refresh;
- after merge, #68 is marked Done/`status:done` and closed; T057 becomes Ready.

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
