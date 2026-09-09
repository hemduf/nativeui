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

Current `main` before T060 integration is `ce86c86e663ad5f8464224874039312143146300`.

Recently completed foundations:

- **T053 / PR #88:** consumer-specific macOS Objective-C platform bridge.
- **#64 / PR #90:** standalone PROGRAM-world ownership Decision B.
- **T059 / PR #89:** generic component availability/read-only model.
- **T030 / PR #94:** Button.
- **T047 / PR #92:** relocatable low-level package with `NativeUI::Core` + `nativeui_attach_platform()`.
- **T048 / PR #99:** relocated external package consumers and macOS two-consumer isolation.
- **T031 / PR #95:** Checkbox + typed RadioGroup/RadioButton.
- **T042 / PR #93:** deterministic supported-path lifecycle stress.
- **T051 / PR #116:** reproducible performance harness/regression policy.
- **T056 / PR #111:** deterministic binary-resource packaging.

Current dependency frontier:

```text
standalone lifecycle: #64(done) -> T042(done) -> T060(in review)
release baseline:     T042(done) + T047(done) + T048(done) + T051(done) -> T052(doing)
platform/package:     T053(done) -> T047(done) -> T048(done)
                                       |
                                       +-> T054
                                       +-> T056(done) -> T057
state/widgets:        T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032
                                       +-> T033
                                       +-> T034 -> T035 / T036
```

T060 / PR #118 is the current standalone-lifecycle implementation. T052 is independently in progress and intentionally does not require T060 for the v0.1 developer-preview gate.

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

**Status: supported-path lifecycle/consumer-safety baseline complete; explicit standalone Application ownership is in review.**

Delivered safety includes:

- #62 plug-in-host safety baseline;
- #64 explicit standalone ownership **Decision B**;
- T053 consumer-specific Objective-C bridge naming;
- #103 Linux/X11 Skia native GL integration;
- #105 constructor-time platform callback lifetime fix;
- #107 documented non-fatal standalone raise handling;
- T042 deterministic headless/embedded/standalone lifecycle stress.

### T060 — explicit Application and multi-window ownership

PR #118 implements the Decision-B standalone architecture:

```cpp
ui::Application application;
ui::StandaloneWindow a{application, ui_a, desc_a};
ui::StandaloneWindow b{application, ui_b, desc_b};
return application.run();
```

The frozen contract is:

- `Application` owns exactly one `PUGL_PROGRAM` world/event loop and is non-copyable/non-movable;
- explicit standalone windows borrow that world, register only after successful native creation and retain independent per-window UI/ViewCore state;
- Application-local bookkeeping uses monotonic IDs; no process-global/current-Application singleton, mutable registry or `thread_local` owner exists;
- `Application` must outlive every registered standalone window; violating this contract terminates deterministically rather than freeing a borrowed world and creating a UAF;
- `poll(timeout)` supports finite negative/zero/positive blocking semantics and treats NaN/Inf as terminal errors;
- default `QuitPolicy::OnLastWindowClosed` requests normal quit only after the last registered window closes; `ExplicitOnly` requires `request_quit()`;
- `request_quit()` is idempotent and does not destroy live windows, including when called reentrantly from a retained component callback;
- native close and toolkit quit-key paths feed the same Application bookkeeping;
- EmbeddedView remains the independent `PUGL_MODULE`, non-blocking host-owned lifecycle path;
- the legacy `StandaloneWindow(UI&, ...)` constructor remains temporarily deprecated as a single-window/pre-v1 compatibility path and never hides a shared Application; T069 owns final removal.

Dedicated acceptance coverage in `tests/t060` includes A+B shared-world creation, destroy-A/continue-B, repeated secondary-window stress, ExplicitOnly, direct/callback quit, negative/zero/positive polling, invalid timeout terminal state, rejected-window behavior and the Application-before-window lifetime diagnostic. `examples/features/t060_multi_window_application.cpp` provides the required interactive and `--self-test` example.

T060 completion requires final `DESIGN.md` synchronization, exact current-head Linux/X11, Windows, macOS and Linux ASan+UBSan T060 contract results, normal CI + T042 lifecycle-stress green, and a clean mandatory `CODE_REVIEW.md` review before merge.

## Milestone 8 — Packaging, tooling and release

**Status: package and T051 performance foundations complete; T052 v0.1 release gate is in progress.**

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

T051 / PR #116 is complete and supplies the Release-only microbenchmark contract consumed by T052/T071:

- exact **5 warmup + 30 measured** `steady_clock` protocol;
- fixed batch counts for layout, hit-testing, pointer/keyboard dispatch, text edit, headless paint and headless lifecycle construction;
- schema/workload metadata including compiler, OS/architecture, build type and exact NativeUI commit SHA;
- benchmark-only allocation count/bytes instrumentation, isolated from production NativeUI;
- exact JSON round-trip and immutable CI result artifacts;
- metadata compatibility enforced before threshold comparison;
- timing blocker = median **>15%** and p95 **>20%**, reproduced by two complete independent candidate runs;
- allocation blocker = allocations/op or bytes/op **>10%**, also reproduced twice; explicit zero-recurring-allocation scenarios block on any recurring allocation;
- `idle_invalidation` = 1,000 deterministic 10 ms logical scheduler checkpoints / 10 seconds logical idle, requiring exactly zero framework invalidations;
- baseline policy documented in `docs/performance-benchmarks.md`: T052 selects the controlled v0.1 CI artifact; the benchmark never self-updates a baseline.

### T052 — v0.1 developer-preview release gate

T052 is now in progress. It owns the aggregate exact-candidate-head platform/sanitizer matrix, clean-cache dependency/bootstrap checks, T047/T048 installed-package consumers, T042 lifecycle stress, T051 benchmark artifact/budget result, macOS Objective-C symbol isolation, existing feature self-tests and v0.1 developer-preview release documentation.

T052 deliberately does **not** require T060 or the rest of the future v1 application/widget surface unless those features have already merged into the candidate head. T071 remains the later full NativeUI 1.0 qualification gate.

### Remaining release frontier

```text
T024(done) + T042(done) + T051(done) -> T052(doing)
T047(done) + T048(done) ------------------^

T047(done) + T053(done) -> T054
T056(done) + T022(done) -> T057
T055 nativeui_add_plugin: Not planned for current v1
```

## T060 completion protocol

- [x] Decision B converted into explicit `ui::Application` ownership and explicit standalone constructor;
- [x] no hidden Application singleton/global/thread-local owner introduced;
- [x] per-Application registration and last-window/ExplicitOnly policy implemented;
- [x] finite timeout/error/terminal-state contract implemented;
- [x] simultaneous A+B, destroy-A/continue-B and repeated-secondary stress implemented;
- [x] direct and callback-requested quit acceptance coverage implemented;
- [x] deterministic Application-before-window lifetime violation coverage implemented;
- [x] required multi-window feature example and `--self-test` implemented;
- [x] dedicated Linux/X11, Windows, macOS and Linux ASan+UBSan T060 workflow implemented;
- [x] current `main` synchronized into the T060 branch before final validation cycle;
- [x] `CONTEXT.md` and `ROADMAP.md` synchronized in the completion candidate;
- [ ] `DESIGN.md` synchronized with the final explicit Application ownership architecture;
- [ ] exact final-head T060 contract workflow green;
- [ ] exact final-head normal platform/sanitizer CI and T042 lifecycle stress green;
- [ ] final mandatory `CODE_REVIEW.md` pass clean;
- [ ] merge PR #118 and mark #72 Done/closed.

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
