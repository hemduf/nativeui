# NativeUI roadmap

**Updated:** 2026-09-07

This roadmap turns the current POC into a reusable desktop UI toolkit while preserving the simple architecture: Pugl for native views/events, Skia for rendering, NativeUI for UI behavior.

The nine milestones are also available in [GitHub](https://github.com/hemduf/nativeui/milestones?state=all); ticket details, explicit dependencies and status live in [GitHub Issues](https://github.com/hemduf/nativeui/issues?q=is%3Aissue).

## Execution rule

Milestones describe architectural progression; they are **not** global execution gates. GitHub `Dependencies:` are the only hard ticket-to-ticket blockers.

Ready work is selected by priority, then downstream unblock value / critical-path impact, with ticket number only as a tie-breaker. Independent tickets may progress in parallel in separate branches. A PR waiting on CI/platform validation does not block unrelated Ready work. See `AGENTS.md` for the operational rules and status semantics.

## Feature delivery rule

Feature examples are mandatory: every feature ticket ships a dedicated executable example with an interactive mode and a `--self-test` mode. T049 remains the later **gallery/aggregation** milestone, not the first point where examples are created.

## Current execution snapshot — 2026-09-07

The merged baseline is complete through T028 except for T023, which is still active.

- **Cross-cutting P0 safety gate — #62 / PR #63:** `Doing`. The implementation fixes unsafe movable platform wrappers, makes process-shared embedded-font aliases immutable, documents UI/resource thread contracts, and requires a consumer/plugin-specific `NATIVEUI_OBJC_RUNTIME_PREFIX` for the statically linked macOS Pugl backend. PR #63 is non-draft, mergeable, and its current exact-head CI matrix is green. Completion still requires merge/issue bookkeeping.
- **T023 — SVG/icon resources / PR #60:** `Doing`. The current head has a green Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan CI matrix. After #63 lands, it must be synchronized with the new `main` safety/runtime baseline and then rerun the exact final-head matrix before merge.
- **#64 — multiple `StandaloneWindow` instances crash on macOS:** `Ready`, P1. This is a separate `PUGL_PROGRAM` application-world lifecycle defect; independent `EmbeddedView`/`PUGL_MODULE` multi-instance validation is green.
- **Ready P0 lanes after/alongside the safety merge:** T042 multi-instance/attach-detach stress tests and T047 install/export CMake package.
- **New M8 CMake/resource chain:** T053 is `Blocked` on #62 and will make the macOS Objective-C/Pugl bridge consumer-scoped. T054 (`nativeui_add_application`) depends on T047 + T053. T056 (`nativeui_add_binary_data`) depends on T047, then T057 adds the embedded `ResourceManager`. T055 (`nativeui_add_plugin`) was deliberately closed as `Not planned` for now and is not part of the v1 release path.
- **Ready high-unblock widget lane:** T030 Button, T032 Slider/RangeSlider and T034 ScrollView. T033 ProgressBar/Meter and T029 advanced IME remain independent Ready work.

Recommended near-term execution:

```text
#62 / PR #63  -> merge safety/runtime baseline -> T053
T023 / PR #60 -> sync with main -> exact-head CI -> merge

in parallel when branches do not conflict:
T042 -> T051
T047 -> T048
     -> T056 -> T057
T047 + T053 -> T054
T030 -> T031
T032
T034 -> T035 / T036
```

#64, T029, T033, T043, T044 and T046 remain valid independent fallback work when a lane is free or another branch is waiting on external validation.

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

**Status: In progress (T019–T022 and T024 complete; T023 Doing)**

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

T023 PR #60 now implements the backend-neutral `SvgIcon`, arbitrary logical-size SVG rendering, provider-backed `SvgCache`, headless/cache/public-header coverage and `t023_svg_icons --self-test`. Its current CI matrix is green, but the branch predates the latest plugin-host safety branch; T023 remains `Doing` until #63 is merged, PR #60 is synchronized with the new `main` baseline, and the exact final head is green.

## Milestone 4 — Text system

**Status: In progress (T025–T028 complete; T029 Ready)**

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

Progress: T025–T027 are complete. T028 PR #56 added multiline `TextArea`, UTF-8-aware vertical navigation, cross-line selection, viewport scrolling, caret/selection painting and the mandatory feature self-test. Final head `4af63a380ad5c39afed79891242f2d64aa414dd8` passed Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan and was squash-merged as `ccf53d234cb81a4fb546acba2990bb1478351496`. The post-merge macOS clipboard regression was fixed by PR #61 and validated with real clipboard plus multi-`EmbeddedView` lifecycle coverage. With T028 complete, T029 is Ready.

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

**Status: T041 complete; T042/T043/T044/T046 and #64 Ready; cross-cutting #62 Doing**

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

The #62/PR #63 safety work is a baseline gate rather than a new numbered milestone ticket: it removes unsafe wrapper move semantics, hardens process-shared font registration, makes UI/resource thread contracts explicit and introduces the macOS consumer-specific Objective-C runtime-prefix requirement. It should land before platform/package work that depends on those contracts.

## Milestone 8 — Packaging, tooling and v1 release

**Status: T047 Ready; T053/T054/T056/T057 added; downstream work follows explicit dependencies; T055 Not planned**

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
#62 / PR #63 -> T053 -----------\
                                  +-> T054
T047 ---------------------------/
  |\
  | +-> T048 -------------------\
  |                              +-> T052
  +-> T056 -> T057               |
                                 |
T042 -> T051 -------------------/

T055 nativeui_add_plugin: Not planned for current v1
```

T047 and T042 remain the existing P0/high-unblock-value release lanes. Once #62 is complete, T053 becomes a P0 packaging/runtime prerequisite. T056 can start after T047 independently of T053, and T054 starts when both T047 and T053 are complete. T057 follows T056. The plug-in target helper is explicitly deferred; NativeUI continues to support embedded views without owning plug-in target creation or plug-in SDK semantics.

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
