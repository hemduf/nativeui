# NativeUI roadmap

**Updated:** 2026-09-09

This roadmap turns the current POC into a reusable desktop UI toolkit while preserving the simple architecture: Pugl for native views/events, Skia for rendering, NativeUI for UI behavior.

The nine milestones are also available in [GitHub](https://github.com/hemduf/nativeui/milestones?state=all); ticket details, explicit dependencies and status live in [GitHub Issues](https://github.com/hemduf/nativeui/issues?q=is%3Aissue).

## Execution rule

Milestones describe architectural progression; they are **not** global execution gates. GitHub `Dependencies:` are the only hard ticket-to-ticket blockers.

Ready work is selected by priority, then downstream unblock value / critical-path impact, with ticket number only as a tie-breaker. Independent tickets may progress in parallel in separate branches. A PR waiting on CI/platform validation does not block unrelated Ready work. See `AGENTS.md` for the operational rules and status semantics.

## Feature delivery rule

Feature examples are mandatory: every feature ticket ships a dedicated executable example with an interactive mode and a `--self-test` mode. T049 remains the later **gallery/aggregation** milestone, not the first point where examples are created.

## Current execution snapshot — 2026-09-09

The merged baseline is complete through T029, including T023. T053 is completing in this merge cycle on the platform/package lane.

- **T053 — consumer-scoped macOS platform bridge / PR #88:** **Complete in this merge cycle once the final documentation head revalidates.** `NativeUI::Core` remains generic and Pugl `common.c`/`internal.c` are compiled once on macOS; only `mac.m`, `mac_gl.m` and the Cocoa IME bridge compile per final consumer. One frozen CMake helper derives `NUI_<fragment>_<digest12>_` from exact UTF-8 `CONSUMER_ID` bytes. Duplicate target attachment and duplicate identity reuse fail at configure time with no runtime registry. The macOS acceptance fixture builds two final MODULE consumers in one configure, audits class+metaclass symbols with a future-proof unprefixed-`Pugl*` rejection pattern, and loads both modules in one Objective-C runtime. Exact implementation head `fad1d96a17902fa18ee25187ed965ac212b7c89b` passed CI #287 on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan; macOS also passed the two-consumer runtime-isolation and lifecycle smoke. Final docs remove the historical global/manual `NATIVEUI_OBJC_RUNTIME_PREFIX` contract; the final exact docs head must be green before merge.
- **T029 — advanced IME composition bridge / PR #85:** **Complete.** NativeUI has one shared platform-neutral composition model for `TextInput` and `TextArea`, transient underlined preedit rendering, UTF-8-safe offsets, single-transaction commit/cancel semantics, candidate geometry and private Cocoa/IMM32/XIM bridges.
- **Cross-cutting P0 safety gate — #62 / PR #63:** **Complete** and the safety baseline consumed by T053.
- **T023 — SVG/icon resources / PR #60:** **Complete.** Backend-neutral SVG resources and per-instance provider-backed caching are in the merged baseline.
- **#64 — multiple `StandaloneWindow` instances crash on macOS:** independent platform lane; it is not a T053 blocker because T053's required embedded/two-consumer coexistence gates are separate and green.
- **Platform/package lane frontier:** after T053 merges, **T047 becomes Ready** and is the next owned task. T054 remains blocked only until T047 is complete; T048/T056 also follow T047.
- Other independent lanes such as T059, T030, #64 and T042 are intentionally not part of the T053/T047 lane.

Platform/package execution:

```text
#62 complete -> T053 -> T047 -> T048 -> T052
                         |
                         +-> T056 -> T057

T047 + T053 -> T054
```

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

T029 PR #85 completes the text milestone with a neutral `CompositionEvent` stream shared by `TextInput`/`TextArea`, transient preedit rendering, candidate-window geometry after scrolling and scale conversion, single-transaction commit/cancel semantics, stale/duplicate commit protection, and private native bridges for Cocoa marked-text APIs, Win32 IMM32 and X11 XIM preedit callbacks. Platform implementation types remain private, composition ownership stays per editor/view, and the macOS helper preserves consumer-specific Objective-C runtime naming. The dedicated `t029_ime_composition --self-test` covers transient state, rendering, duplicate delivery and multi-view isolation.

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

**Status: T041 and cross-cutting #62 complete; T042/T043/T044/T046 and #64 remain independent work**

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

The #62/PR #63 safety baseline removed unsafe wrapper move semantics, made process-shared font aliases immutable and established the consumer-specific Objective-C naming requirement. T053 turns that requirement into a final-consumer build invariant instead of a manual global prefix.

## Milestone 8 — Packaging, tooling and v1 release

**Status: T053 completing; T047 is next and becomes Ready immediately after T053 merge; T054/T056/T057 follow explicit dependencies; T055 Not planned**

**Goal:** make the toolkit easy to consume and maintain.

Deliverables:

- `install()` / exported CMake package;
- exported generic `NativeUI::Core` plus a final-target platform attachment helper;
- JUCE-style `nativeui_add_application()` helper for concise application target creation;
- real macOS `.app` bundle generation with bundle metadata and automatic consumer identity handling;
- consumer-scoped macOS Pugl/Objective-C bridge so multiple final bundles never share one fixed runtime namespace;
- `nativeui_add_binary_data()` for deterministic resource packaging directly into binaries;
- public embedded `ResourceManager` with zero-copy immutable lookup and `ResourceProvider` compatibility;
- dependency lock/version diagnostics;
- examples gallery;
- component inspector/debug overlay;
- benchmark suite;
- CI build matrix;
- release checklist and semantic versioning policy.

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

T053 freezes the lower-level macOS rule: `NativeUI::Core` and portable platform C code are generic, while Objective-C Pugl/OpenGL/IME code is instantiated per final consumer from stable identity. There is no normal global/manual `NATIVEUI_OBJC_RUNTIME_PREFIX` path. T047 must install/export this machinery relocatably and expose the single low-level v1 contract `NativeUI::Core + nativeui_attach_platform(TARGET ... CONSUMER_ID ...)`; it must not export/document `NativeUI::NativeUI` as a complete v1 package target.

Tickets: `T047`–`T054`, `T056`–`T057`. `T055` is closed as **Not planned** and deliberately excluded from the current v1 scope.

Release/package dependency chain:

```text
#62 complete -> T053 -> T047 -> T048 -------------------\
                         |                               +-> T052
                         +-> T056 -> T057               |
                                                       |
T042 -> T051 -------------------------------------------/

T047 + T053 -> T054
T055 nativeui_add_plugin: Not planned for current v1
```

The platform/package lane proceeds T053 then T047. Once T047 is complete, T048 and T056 become independently available and T054 is fully unblocked because T053 is already satisfied.

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