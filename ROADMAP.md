# NativeUI roadmap

**Updated:** 2026-09-08

This roadmap turns the current POC into a reusable desktop UI toolkit while preserving the simple architecture: Pugl for native views/events, Skia for rendering, NativeUI for UI behavior.

The nine milestones are also available in [GitHub](https://github.com/hemduf/nativeui/milestones?state=all); ticket details, explicit dependencies and status live in [GitHub Issues](https://github.com/hemduf/nativeui/issues?q=is%3Aissue).

## Execution rule

Milestones describe architectural progression; they are **not** global execution gates. GitHub `Dependencies:` are the only hard ticket-to-ticket blockers.

Ready work is selected by priority, then downstream unblock value / critical-path impact, with ticket number only as a tie-breaker. Independent tickets may progress in parallel in separate branches. A PR waiting on CI/platform validation does not block unrelated Ready work. See `AGENTS.md` for the operational rules and status semantics.

## Feature delivery rule

Feature examples are mandatory: every feature ticket ships a dedicated executable example with an interactive mode and a `--self-test` mode. T049 remains the later **gallery/aggregation** milestone, not the first point where examples are created.

## Current execution snapshot — 2026-09-08

The merged baseline is complete through T029, including T023.

- **T029 — advanced IME composition bridge / PR #85:** **Complete in this merge cycle**. NativeUI now has one shared platform-neutral composition model for `TextInput` and `TextArea`, transient underlined preedit rendering, UTF-8-safe preedit offsets, composition-start selection replacement with one undo transaction, deterministic cancel/stale-commit handling, one-shot duplicate committed-text suppression, logical candidate geometry that follows editor scrolling and scale changes, and private Cocoa/Win32 IMM32/X11 XIM bridges. Multi-view isolation, candidate geometry, duplicate delivery and both editor rendering paths are covered; `t029_ime_composition` provides interactive and deterministic `--self-test` modes. Final exact-head platform/sanitizer validation and the mandatory review record are attached to PR #85 before merge.
- **Cross-cutting P0 safety gate — #62 / PR #63:** **Complete**. Exact head `e197315c85dc6fb5213040f988833b0837c301d7` passed CI run #165 on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan, including the macOS two-consumer Objective-C runtime-isolation and multi-instance lifecycle smokes. PR #63 was squash-merged as `4922b85ae8ebb2f004611081f257946ac60e0fa1`; issue #62 is Done/closed.
- **T023 — SVG/icon resources / PR #60:** **Complete**. NativeUI now exposes backend-neutral `SvgIcon` handles, centered contain-fit SVG drawing, application-owned `ResourceProvider` loading and per-instance `SvgCache` reuse/failure caching. ViewBox-only SVGs, malformed input, invalid destinations, transforms/gradients, cache isolation, deterministic path golden coverage, isolated public-header compilation and `t023_svg_icons --self-test` are covered. The implementation preserves the #62/#63 plugin-host/runtime-prefix contracts.
- **#64 — multiple `StandaloneWindow` instances crash on macOS:** `Ready`, P1. This is a separate `PUGL_PROGRAM` application-world lifecycle defect; independent `EmbeddedView`/`PUGL_MODULE` multi-instance validation is green.
- **Ready P0 lanes:** **T042** multi-instance/attach-detach stress tests, **T047** install/export CMake package, and **T053** consumer-scoped macOS Pugl/Objective-C bridge (unblocked by #62 completion).
- **New M8 CMake/resource chain:** T053 is now `Ready`. T054 (`nativeui_add_application`) depends on T047 + T053. T056 (`nativeui_add_binary_data`) depends on T047, then T057 adds the embedded `ResourceManager`. T055 (`nativeui_add_plugin`) was deliberately closed as `Not planned` for now and is not part of the v1 release path.
- **Ready high-unblock widget lane:** T030 Button, T032 Slider/RangeSlider and T034 ScrollView. T033 ProgressBar/Meter remains independent Ready work.

Recommended near-term execution:

```text
T053           -> consumer-scoped macOS platform bridge

in parallel when branches do not conflict:
T042 -> T051
T047 -> T048
     -> T056 -> T057
T047 + T053 -> T054
T030 -> T031
T032
T034 -> T035 / T036
```

#64, T033, T043, T044 and T046 remain valid independent fallback work when a lane is free or another branch is waiting on external validation.

## Milestone 0 — Baseline hardening

**Status: Complete (T001–T006)**

**Goal:** make the current implementation safe to evolve without regressions.

Deliverables:

- stronger core/input/state tests;
- headless test harness foundations;
- split monolithic header into stable public modules without changing the API;
- explicit event consumption/propagation contract;
- stable node identity and lifecycle hooks;
- robust dirty-region/invalidation model;
- regression coverage for Tab/Shift+Tab, Canvas local input, embedded lifecycle and DPI conversions.

Exit gate:

- current demo behavior preserved;
- all core tests pass;
- no widget needs Pugl headers;
- baseline architecture documented and mechanically testable.

Tickets: `T001`–`T006`.

## Milestone 1 — Layout system

**Status: Complete (T007–T012)**

**Goal:** support production plugin/application layouts without adopting a CSS engine.

Deliverables:

- min/max/preferred constraints;
- alignment and distribution;
- row/column flex growth/shrink;
- grid;
- scrollable layout primitive;
- clipping and overflow behavior;
- layout invalidation separated from paint invalidation.

Exit gate:

- responsive resize works for realistic plugin panels;
- nested layout behavior is deterministic and unit tested;
- no widget-specific layout hacks.

Tickets: `T007`–`T012`.

## Milestone 2 — Input, focus and gestures

**Status: Complete (T013–T018)**

**Goal:** complete the generic interaction model before adding many widgets.

Deliverables:

- explicit event handled/propagation result;
- focus scopes and default focus;
- pointer hover/press/capture lifecycle;
- wheel and high-resolution scrolling normalization;
- double/triple-click helper;
- keyboard shortcut/command routing;
- drag-and-drop primitives;
- gesture helpers for drag/value editing.

Exit gate:

- widgets can implement complex input without touching platform code;
- nested interactive components route input predictably;
- keyboard-only navigation is reliable.

Tickets: `T013`–`T018`.

## Milestone 3 — Rendering and graphics

**Status: Complete (T019–T024)**

**Goal:** turn `Painter`/`CanvasContext2D` into a capable but compact 2D API over Skia.

Deliverables:

- paths;
- transforms and save/restore;
- nested clipping;
- gradients;
- shadows/blur where practical;
- images and image scaling;
- SVG/icon support;
- font/typeface/resource cache;
- headless raster + golden-image tests.

Exit gate:

- custom component authors rarely need direct `SkCanvas` access;
- rendering tests can run without a display;
- common resources are cached and reused.

Tickets: `T019`–`T024`.

Progress: T020 added backend-neutral path drawing and T021 added gradients/paint styles. T022 PR #59 added backend-neutral decoded `Image` handles, source-rectangle drawing, `Fill`/`Contain`/`Cover`, application-owned `ResourceProvider` loading, reusable decoded-image/failure caching, isolated `image.hpp` compilation, deterministic image scaling coverage and `t022_images --self-test`.

T023 PR #60 completes SVG/icon resources with a backend-neutral `SvgIcon`, centered aspect-preserving contain rendering, provider-backed per-instance `SvgCache`, viewBox-only support and static/self-contained SVG resource semantics. The completion coverage includes path/transform/gradient rendering, malformed SVGs, invalid rectangles, cache hit/failure reuse, two-cache same-ID isolation, deterministic path golden comparison, public-header compilation and `t023_svg_icons --self-test`.

## Milestone 4 — Text system

**Status: Complete (T025–T029)**

**Goal:** make text reliable enough for editors, forms and plugin UIs.

Deliverables:

- reusable `Text`/`Label` component;
- shaped text measurement/layout;
- font fallback abstraction;
- multiline text layout;
- `TextArea`;
- advanced IME composition bridge per platform where Pugl is insufficient;
- text selection/caret golden tests.

Exit gate:

- Unicode text rendering and editing are separated cleanly;
- single-line and multiline editing share a tested text model;
- IME limitations are explicit per platform.

Tickets: `T025`–`T029`.

Progress: T025–T027 are complete. T028 PR #56 added multiline `TextArea`, UTF-8-aware vertical navigation, cross-line selection, viewport scrolling, caret/selection painting and the mandatory feature self-test. Final head `4af63a380ad5c39afed79891242f2d64aa414dd8` passed Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan and was squash-merged as `ccf53d234cb81a4fb546acba2990bb1478351496`. The post-merge macOS clipboard regression was fixed by PR #61 and validated with real clipboard plus multi-`EmbeddedView` lifecycle coverage.

T029 PR #85 completes the text milestone with a neutral `CompositionEvent` stream shared by `TextInput`/`TextArea`, transient preedit rendering, candidate-window geometry after scrolling and scale conversion, single-transaction commit/cancel semantics, stale/duplicate commit protection, and private native bridges for Cocoa marked-text APIs, Win32 IMM32 and X11 XIM preedit callbacks. Platform implementation types remain private, composition ownership stays per editor/view, and the macOS helper preserves the existing consumer-specific Objective-C runtime namespace. The dedicated `t029_ime_composition --self-test` covers transient state, rendering, duplicate delivery and multi-view isolation.

## Milestone 5 — Standard widget set

**Status: Ready frontier available (T030, T032, T033, T034 Ready)**

**Goal:** cover most desktop/plugin UI needs without requiring Canvas implementations.

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

Exit gate:

- widgets share focus/input/style primitives;
- widgets are composable and do not create new platform dependencies;
- accessibility semantics can be attached later without API rewrites.

Tickets: `T030`–`T036`.

Dependency frontier:

```text
T030 -> T031
T034 -> T035 / T036
```

T030, T032 and T034 have the highest downstream unblock value because they also feed later theme/accessibility/gallery tickets. T033 is independently Ready.

## Milestone 6 — Styling, theme and animation

**Status: Blocked only by explicit widget/style dependencies**

**Goal:** style complete applications without CSS or per-widget callback boilerplate.

Deliverables:

- typed theme tokens;
- component style structs;
- state variants: normal/hover/pressed/focused/disabled;
- scoped inheritance: theme -> container -> component;
- animation clock/tween helpers;
- reduced-motion hook;
- reusable design-system examples.

Exit gate:

- a coherent visual theme can be replaced without editing widget internals;
- idle UIs do not redraw continuously;
- animation invalidation is bounded to active animations.

Tickets: `T037`–`T040`.

Dependency frontier:

```text
T030 + T032 -> T037 -> T038 -> T039 / T040
```

## Milestone 7 — Platform and embedded robustness

**Status: T041 and cross-cutting #62 complete; T042/T043/T044/T046 and #64 Ready**

**Goal:** make NativeUI dependable inside real hosts and standalone applications.

Deliverables:

- repeated attach/detach/open/close tests;
- multiple instance tests;
- resize/scale negotiation harness;
- clipboard/drop smoke tests;
- OS pointer capture evaluation;
- full cursor support;
- platform IME extensions;
- accessibility bridge design;
- explicit Wayland strategy after X11 v1 is stable;
- plugin-host process coexistence and Objective-C runtime collision safety on macOS.

Exit gate:

- standalone and embedded smoke tests pass on macOS/Windows/Linux X11;
- host lifecycle failures are reproducible in dedicated tests;
- multiple independent embedded/plugin views coexist safely;
- macOS runtime-visible Objective-C names are consumer/plugin-specific when the Pugl backend is statically embedded;
- no plugin API enters the core.

Tickets: `T041`–`T046`, plus cross-cutting safety issue #62 and standalone lifecycle issue #64.

T042 is P0 and feeds the benchmark/release path. T043, T044, T046 and #64 are independent Ready fallback work. T045 remains explicitly blocked on the standard-widget dependency chain.

The #62/PR #63 safety baseline is complete: unsafe wrapper move semantics are removed, process-shared font aliases are immutable, UI/resource thread contracts are explicit, and macOS Pugl Objective-C runtime classes require a consumer/plugin-specific prefix. This completed baseline unblocks T053 and is now the contract that subsequent platform/package work must preserve.

## Milestone 8 — Packaging, tooling and v1 release

**Status: T047 and T053 Ready; T054/T056/T057 follow explicit dependencies; T055 Not planned**

**Goal:** make the toolkit easy to consume and maintain.

Deliverables:

- `install()` / exported CMake package;
- `NativeUI::NativeUI` consumer target;
- JUCE-style `nativeui_add_application()` helper for concise application target creation;
- real macOS `.app` bundle generation with bundle metadata and automatic consumer-specific Objective-C runtime prefixing;
- consumer-scoped macOS Pugl/Objective-C bridge so multiple final bundles never share one fixed runtime namespace;
- `nativeui_add_binary_data()` for deterministic resource packaging directly into binaries;
- public embedded `ResourceManager` with zero-copy immutable lookup and `ResourceProvider` compatibility;
- dependency lock/version diagnostics;
- examples gallery;
- component inspector/debug overlay;
- benchmark suite;
- CI build matrix;
- release checklist and semantic versioning policy;
- macOS packaging that preserves a consumer/plugin-specific Objective-C runtime prefix for the statically linked Pugl bridge.

Exit gate:

```cmake
find_package(NativeUI CONFIG REQUIRED)

nativeui_add_application(MyApp
    PRODUCT_NAME "My App"
    BUNDLE_ID "com.example.myapp"
    VERSION "1.0.0"
    SOURCES src/main.cpp
)

nativeui_add_binary_data(MyResources
    SOURCES resources/logo.svg resources/theme.json
)

target_link_libraries(MyApp PRIVATE MyResources)
```

works on supported platforms with documented prerequisites, with resources available from the embedded runtime resource API and no required runtime filesystem lookup.

On macOS, packaging must not publish one generic precompiled static platform archive whose Objective-C runtime names collide when copied into unrelated application or plug-in bundles. `NativeUI::Core` may remain generic; the Pugl/Objective-C platform bridge must preserve per-consumer runtime naming. High-level CMake helpers must derive and apply the consumer-specific `NATIVEUI_OBJC_RUNTIME_PREFIX` automatically from stable final-target identity rather than requiring a global manual configure flag.

Tickets: `T047`–`T054`, `T056`–`T057`. `T055` is closed as **Not planned** and deliberately excluded from the current v1 scope.

Release/package dependency chain:

```text
#62 / PR #63 (complete) -> T053 -------\
                                      +-> T054
T047 ---------------------------------/
  |\
  | +-> T048 -------------------\
  |                              +-> T052
  +-> T056 -> T057               |
                                 |
T042 -> T051 -------------------/

T055 nativeui_add_plugin: Not planned for current v1
```

T047, T053 and T042 are now the P0/high-unblock-value release lanes. T056 can start after T047 independently of T053, and T054 starts when both T047 and T053 are complete. T057 follows T056. The plug-in target helper is explicitly deferred; NativeUI continues to support embedded views without owning plug-in target creation or plug-in SDK semantics.

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

This sequence minimizes rewrites, but it is an architectural roadmap rather than a serialized work queue. Once a ticket's explicit dependencies are satisfied, it may proceed independently. Prefer critical-path/unblock-value work and keep unrelated lanes moving while another PR waits on CI or platform validation.