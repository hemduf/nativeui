# NativeUI roadmap

**Updated:** 2026-09-09

This roadmap turns the current POC into a reusable desktop UI toolkit while preserving the architecture: Pugl for native views/events, Skia for rendering, NativeUI for retained UI behavior.

GitHub Issues are the source of truth for ticket status and explicit dependencies. Milestone ordering describes architectural progression but is not a global execution lock; independent Ready work may proceed in parallel.

## Execution rules

- explicit GitHub `Dependencies:` are the only hard ticket-to-ticket gates;
- select Ready work by priority, then downstream unblock value / critical-path impact;
- keep unrelated lanes moving while another PR waits on CI/review/platform validation;
- every code-changing ticket requires exact-head validation plus a `CODE_REVIEW.md` record;
- every completion cycle synchronizes `CONTEXT.md` and this roadmap;
- every feature ticket ships a dedicated example executable with interactive and `--self-test` modes.

## Current execution snapshot — 2026-09-09

Current `main` before the T042 merge is `8da758242fa3b2c4fa2402c7a08a33b05619440a`.

Recently completed:

- **#86 / PR #87:** real drag/drop integration and Finder delivery correction; merged as `0c4778278c86d2b8946ece593e684f4203ba38db`.
- **#107 / PR #108:** accept only Pugl's documented non-fatal standalone raise failure while preserving all genuine show errors; merged as `e05ae703509a6971772b3bd5c53a3d7c71d7632a`.
- **T031 / PR #95:** Checkbox + typed RadioGroup/RadioButton; merged as `8da758242fa3b2c4fa2402c7a08a33b05619440a`.

**T042 / PR #93 — Complete by this merge.** The final stress-only candidate contains deterministic headless lifecycle, sequential embedded, two-live embedded destroy-A/continue-B, active capture/focus/text teardown, process-isolated standalone lifecycle, and an explicit #64 Decision-B contract test. Production defects exposed by the matrix (#103, #105, #107) are already merged independently.

Mandatory review `5155464378` has no Blocking/Important finding. Before the final T031 synchronization, exact head `acd86e2e678f3a3f3dc7596164f46b3b9902dac4` passed:

- normal CI `34362008979` / #435 on Linux X11, Windows, macOS and Linux ASan+UBSan after a macOS rerun;
- T042 Lifecycle Stress `34362009081` / #11 on the same four lanes.

The first macOS attempt of normal CI hit only the unchanged T047 CTest timeout at 30.17 s; the same T047 script passed directly in that job and the exact rerun passed without changing timeout/assertions. T031 merged while those gates were running, so PR #93 was synchronized again from current `main`. The post-sync exact head must pass both workflows before merge.

Parallel execution frontier after this T042 merge:

```text
lifecycle:        #64(done) -> #107(done) -> T042(done) -> T051 -> T052
platform/package: T053(done) -> T047(done) -> T048(done) -> T052
                                      |
                                      +-> T054
                                      +-> T056 -> T057
state/widgets:    T059(done) -> T030(done) -> T031(done)
                                      |
                                      +-> T032
                                      +-> T033
                                      +-> T034 -> T035 / T036
```

Consequences:

- T051 becomes Ready after T042 merges because T024 is already complete;
- T052 then waits only for T051; T047/T048/T042 are satisfied;
- T054 and T056 remain Ready and independent;
- T057 remains blocked by T056;
- T032, T033 and T034 remain independent widget work after T031.

## Milestone 0 — Baseline hardening

**Status: Complete (T001–T006)**

Goal: make the implementation safe to evolve without regressions.

Delivered scope:

- stronger core/input/state tests;
- headless test harness foundations;
- stable public header/module split;
- explicit event consumption/propagation;
- stable node identity and lifecycle hooks;
- robust dirty-region/invalidation behavior;
- focus, Canvas local input, embedded lifecycle and DPI regression coverage.

Exit gate: complete.

## Milestone 1 — Layout system

**Status: Complete (T007–T012)**

Delivered scope:

- min/max/preferred constraints;
- alignment/distribution;
- row/column flex growth/shrink;
- grid;
- scrollable layout;
- clipping/overflow;
- layout invalidation separated from paint invalidation.

Exit gate: complete.

## Milestone 2 — Input, focus and gestures

**Status: Complete (T013–T018)**

Delivered scope:

- event handled/propagation contract;
- focus scopes/default focus;
- pointer hover/press/capture lifecycle;
- wheel normalization;
- click helpers;
- command routing;
- drag/drop primitives;
- drag/value-edit gesture helpers.

#86 / PR #87 later hardened the native drag/drop platform path without changing this milestone's generic API boundary.

## Milestone 3 — Rendering and graphics

**Status: Complete (T019–T024)**

Delivered scope:

- transforms/save/restore;
- paths;
- nested clipping;
- gradients;
- image decoding/scaling;
- SVG/icon support;
- font/typeface/resource caching;
- deterministic headless raster/golden tests.

T023 / PR #60 provides backend-neutral SVG/icon resources. T024 provides the headless/golden foundation later consumed by T051.

## Milestone 4 — Text system

**Status: Complete (T025–T029)**

Delivered scope:

- reusable text editing model;
- Text/Label;
- font fallback/layout support;
- multiline TextArea;
- UTF-8-safe selection/caret/navigation;
- advanced IME composition bridge for Cocoa/IMM32/XIM;
- deterministic self-tests and rendering coverage.

T029 / PR #85 completes the text milestone with one platform-neutral composition model and private consumer-safe platform bridges.

## Milestone 5 — Standard widget set

**Status: T030 and T031 complete; T032/T033/T034 Ready**

Goal: cover common desktop/plugin controls without forcing Canvas implementations.

Initial widget set:

- Button;
- Checkbox/Radio;
- Slider;
- RangeSlider;
- ProgressBar/Meter;
- ComboBox;
- PopupMenu/Menu;
- ScrollView;
- ListView;
- Tabs;
- Image;
- Separator/Group/Panel helpers.

Progress:

- **T030 / PR #94:** Button complete and merged as `b32b09da473c70a857675e05f7fdc7c78e5f9361`.
- **T031 / PR #95:** Checkbox + typed RadioGroup/RadioButton complete and merged as `8da758242fa3b2c4fa2402c7a08a33b05619440a`. Radio groups own their identity/live-option bookkeeping, enforce unique simultaneously-live values, use one Tab stop, wrapped arrow navigation, and inherit T059 availability/read-only semantics without global state.

Dependency frontier:

```text
T059(done) -> T030(done) -> T031(done)
                              |
                              +-> T032
                              +-> T033
                              +-> T034 -> T035 / T036
```

## Milestone 6 — Styling, theme and animation

**Status: blocked only by explicit widget/style dependencies**

Planned scope:

- typed theme tokens;
- component style structs;
- normal/hover/pressed/focused/disabled variants;
- scoped theme/container/component inheritance;
- animation clock/tween helpers;
- reduced-motion hook;
- reusable design-system examples.

Dependency chain:

```text
T030(done) + T032 -> T037 -> T038 -> T039 / T040
```

## Milestone 7 — Platform and embedded robustness

**Status: T041, #62, #64, #103, #105 and #107 complete; T042 completes by this merge; T043/T044/T046 remain independent work**

Goal: make NativeUI dependable inside real hosts and standalone applications.

Required capabilities include:

- repeated attach/detach/open/close;
- multiple independent instances;
- resize/scale negotiation;
- clipboard/drop smoke tests;
- cursor/pointer capture evaluation;
- IME extensions;
- accessibility design;
- X11 v1 + explicit later Wayland strategy;
- plug-in-host coexistence and consumer-specific Objective-C runtime safety.

Key completed safety decisions/fixes:

- **#62 / PR #63:** plug-in-host safety baseline.
- **#64 / PR #90:** Decision B — independent overlapping `PUGL_PROGRAM` worlds are not the current multi-window model. T060 owns one explicit shared `ui::Application` PROGRAM owner for future top-level multi-window support.
- **T053 / PR #88:** consumer-specific Objective-C bridge naming.
- **#103 / PR #104:** Linux/X11 Skia native GL interface under the Pugl-owned GLX context.
- **#105 / PR #106:** constructor-time native callbacks are owned safely by private per-instance implementations.
- **#107 / PR #108:** documented non-fatal standalone raise result is no longer converted to a fatal constructor error.
- **T042 / PR #93:** deterministic supported-path lifecycle stress; no hidden singleton/Application workaround.

T042 acceptance matrix:

- `headless_tree_lifecycle_1000`;
- `embedded_sequential_100`;
- `embedded_two_live_50`;
- `embedded_capture_focus_teardown`;
- `standalone_sequential_50`;
- `standalone_supported_multi_instance` Decision-B assertion;
- Linux ASan+UBSan;
- native Linux/X11, Windows and macOS stress.

After T042, T051 becomes Ready and T052 waits only for T051.

## Milestone 8 — Packaging, tooling and v1 release

**Status: T053/T047/T048 complete; T054/T056 Ready; T057 follows T056; T055 Not planned; T051 becomes Ready after T042**

Goal: make the toolkit easy to consume, validate and release.

Delivered foundation:

- **T053:** consumer-scoped macOS Objective-C platform bridge.
- **T047:** install/export CMake package exposing `NativeUI::Core` + `nativeui_attach_platform(TARGET ... CONSUMER_ID ...)`.
- **T048:** independent relocated Core/standalone/embedded package consumers and macOS two-consumer runtime isolation.

Planned remaining scope includes:

- concise application target helper;
- real macOS `.app` packaging/bundle metadata;
- embedded binary-resource helper/runtime lookup;
- dependency diagnostics;
- examples gallery;
- inspector/debug overlay;
- benchmark suite/performance budgets;
- release checklist/semantic versioning.

Public low-level package contract:

```cmake
find_package(NativeUI CONFIG REQUIRED)

target_link_libraries(MyFinalTarget PRIVATE NativeUI::Core)
nativeui_attach_platform(
    TARGET MyFinalTarget
    CONSUMER_ID com.example.product
)
```

Release/package dependency chain:

```text
#62(done) -> T053(done) -> T047(done) -> T048(done) ----\
                                              |          +-> T052
                                              +-> T056 -> T057

T024(done) + T042(done) -> T051 ------------------------/

T047(done) + T053(done) -> T054
T055 nativeui_add_plugin: Not planned for current v1
```

T051 defines reproducible Release-mode microbenchmarks and relative same-environment performance budgets. T052 consumes T051 plus the completed lifecycle/package qualification chain.

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

## T042 completion protocol

Before PR #93 merges:

- [x] deterministic fixed stress matrix implemented;
- [x] #103/#105/#107 production defects split and merged independently;
- [x] mandatory CODE_REVIEW.md review completed with no Blocking/Important findings;
- [x] `CONTEXT.md` and `ROADMAP.md` synchronized with merged T031/current main;
- [ ] final post-T031 exact-head normal CI green;
- [ ] final post-T031 exact-head T042 Lifecycle Stress green;
- [ ] PR #93 merged and #42 closed Done;
- [ ] T051 moved from Blocked to Ready.
