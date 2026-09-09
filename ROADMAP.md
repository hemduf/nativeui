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

The completed feature baseline is through T030, with T059 supplying the shared component-availability prerequisite; rendering resources T022/T023, platform safety issue #62, T053 and the #64 Decision B ownership diagnosis are also complete.

- **T053 — consumer-scoped macOS platform bridge / PR #88:** **Complete and merged** as `ad83ed05f1687fea31255bcc77329fae0f4efb69`. `NativeUI::Core` and portable Pugl C code remain generic; only the small macOS Objective-C Pugl/OpenGL/IME bridge is instantiated per final consumer. The frozen helper derives `NUI_<fragment>_<digest12>_` from exact UTF-8 `CONSUMER_ID`, rejects duplicate target/identity registration at configure time and introduces no runtime registry. The macOS acceptance path validates two final consumers plus class/metaclass symbol isolation.
- **#64 — macOS standalone PROGRAM-world ownership / PR #90:** **Complete and merged** as `b52d53eee65e5de91697e2c1f0685728b9646575`. Decision B is frozen: independent `PUGL_PROGRAM` worlds are not a valid NativeUI multi-window ownership model on macOS. Legacy `StandaloneWindow(UI&, ...)` therefore remains single-window/pre-v1 only. **T060 / issue #72 owns the replacement contract:** one explicit `ui::Application`, exactly one PROGRAM world that outlives all top-level windows, multiple `StandaloneWindow(Application&, ...)` views, and no hidden mutable global/singleton/`thread_local` application owner.
- **T059 — generic component availability / PR #89:** **Complete and merged** as `6d84bc6b7817b0dcaea4eefd81833d765ad7685d`. One retained-tree model resolves `Visible`/`Hidden`/`Collapsed`, inherited enabled/disabled and inherited read-only state. Hidden preserves layout while suppressing paint/input/focus; Collapsed removes layout contribution without unmounting; Disabled suppresses normal targeting/focus while remaining laid out/painted; ReadOnly preserves targeting/focus while widgets reject mutations. Capture/focus teardown occurs before suppression, reentrant reversal is bounded, and the mandatory-review regression preserves existing FocusScope restore semantics when an active scope becomes unavailable. No mutable process-global instance state is introduced.
- **T030 — Button / PR #94:** **Complete implementation / merge candidate.** `ui::Button` uses T059 central availability rather than a widget-local disabled policy; pointer press/capture/release, Space-on-release, Enter-on-key-down, focus/cancel teardown, ReadOnly action semantics and reentrant callback safety are covered by dedicated tests. Minimal normal/hover/pressed/focused/disabled rendering is deterministic in headless tests, and `t030_button` provides the mandatory interactive + `--self-test` example. Merging the completion branch makes T031 Ready and satisfies the Button-side dependency of T037.
- **T029 — advanced IME composition bridge / PR #85:** **Complete.** NativeUI has one shared platform-neutral composition model for `TextInput` and `TextArea`, transient underlined preedit rendering, UTF-8-safe offsets, single-transaction commit/cancel semantics, candidate geometry and private Cocoa/IMM32/XIM bridges.
- **Cross-cutting P0 safety gate — #62 / PR #63:** **Complete** and consumed by T053.
- **T023 — SVG/icon resources / PR #60:** **Complete.** Backend-neutral SVG resources and per-instance provider-backed caching are in the merged baseline.
- **Lifecycle lane frontier:** **T042** follows merged #64 Decision B. T042 must stress current supported ownership paths—one standalone PROGRAM application-owner lifetime and independent `EmbeddedView` / `PUGL_MODULE` instances—without inventing a hidden simultaneous-standalone workaround. Shared-Application multi-window stress belongs to T060.
- **Platform/package frontier:** **T047 is Ready** now that T053 is merged. T054 remains blocked until T047 is complete; T048/T056 also follow T047.
- **State/widget frontier:** **T030 / PR #94 is the completion branch** after merged T059. Once it lands, T031 becomes Ready; T032/T033/T034 remain independently Ready.

Parallel execution frontier:

```text
lifecycle:        #64(done) -> T042 -> T051 -> T052
platform/package: T053(done) -> T047 -> T048 -> T052
                              |       
                              +-> T056 -> T057
state/widgets:    T059(done) -> T030(#94) -> T031
                               |
                               +-> T037 (with T032)

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

T023 PR #60 completes SVG/icon resources with a backend-neutral `SvgIcon`, centered aspect-preserving contain rendering, provider-backed per-instance `SvgCache`, viewBox-only support and static/self-contained SVG resource semantics. The completion coverage includes path/transform/gradient rendering, malformed input, invalid rectangles, cache hit/failure reuse, two-cache same-ID isolation, deterministic path golden comparison, public-header compilation and `t023_svg_icons --self-test`.

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

**Status: T030 complete in PR #94; T031 becomes Ready on merge; T032/T033/T034 Ready**

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
T059(done) -> T030(#94) -> T031
T034 -> T035 / T036
```

T030's implementation uses one platform-neutral retained component, generic Tree focus/capture routing and T059 effective availability. Pointer activation requires release-inside after capture; Space activates on KeyUp; Enter activates once on KeyDown until its matching KeyUp; focus loss/cancel/deactivation and effective unavailability terminate pending interaction. ReadOnly intentionally leaves the action enabled. The activation callback is copied and invoked only after component/context mutation is complete, establishing a reentrancy-safe boundary for synchronous state changes and later T058 structural removal. Dedicated core tests plus `t030_button --self-test` cover the contract and deterministic visual states.

Completion of T030 has the highest immediate downstream unblock value because it makes T031 Ready and satisfies the Button-side dependency for T037, while T032 and T034 remain independently high-unblock-value and T033 remains independently Ready.

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

**Status: T041, #62 and #64 complete; T042/T043/T044/T046 remain independent work**

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

- standalone and embedded smoke tests pass on macOS/Windows/Linux X11 for the **currently supported ownership paths**;
- host lifecycle failures are reproducible in dedicated tests;
- multiple independent embedded/plugin views coexist safely;
- macOS runtime-visible Objective-C names are consumer/plugin-specific when the Pugl backend is statically embedded;
- no plugin API enters the core.

Tickets: `T041`–`T046`, plus cross-cutting safety issue #62 and standalone lifecycle issue #64.

The #62/PR #63 safety baseline removed unsafe wrapper move semantics, made process-shared font aliases immutable and established the consumer-specific Objective-C naming requirement. T053 has now made that requirement a per-final-consumer build invariant instead of a manual global prefix.

Issue #64 freezes the top-level macOS ownership boundary as **Decision B**. Multiple independent `PUGL_PROGRAM` worlds are not the supported multi-window architecture. The exact reproducer demonstrates failure after destroy-A/continue-B, matching Pugl's application-world ownership model. Until T060 implements one explicit shared `ui::Application` PROGRAM owner, normal platform validation must use one standalone PROGRAM owner lifetime. Independent `EmbeddedView` / `PUGL_MODULE` instances remain fully supported and are the current multi-instance host/plugin path.

T042 consumes that decision. Its stress helpers must remain separate from production API, exercise headless lifecycle and embedded A+B isolation/teardown heavily, and must not smuggle in a singleton or claim simultaneous legacy standalone support. Repeated/multi-window top-level stress under one PROGRAM owner becomes a T060 acceptance path once `ui::Application` exists.

## Milestone 8 — Packaging, tooling and v1 release

**Status: T053 complete; T047 Ready; T054/T056/T057 follow explicit dependencies; T055 Not planned**

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

T053 has frozen the lower-level macOS rule: `NativeUI::Core` and portable platform C code are generic, while Objective-C Pugl/OpenGL/IME code is instantiated per final consumer from stable identity. There is no normal global/manual `NATIVEUI_OBJC_RUNTIME_PREFIX` path. T047 must install/export this machinery relocatably and expose the single low-level v1 contract `NativeUI::Core + nativeui_attach_platform(TARGET ... CONSUMER_ID ...)`; it must not export/document `NativeUI::NativeUI` as a complete v1 package target.

Tickets: `T047`–`T054`, `T056`–`T057`. `T055` is closed as **Not planned** and deliberately excluded from the current v1 scope.

Release/package dependency chain:

```text
#62 complete -> T053 complete -> T047 -> T048 -------------------\
                                  |                               +-> T052
                                  +-> T056 -> T057               |
                                                                |
T042 -> T051 ----------------------------------------------------/

T047 + T053 -> T054
T055 nativeui_add_plugin: Not planned for current v1
```

The platform/package lane now proceeds with T047. Once T047 is complete, T048 and T056 become independently available and T054 is fully unblocked because T053 is already satisfied.

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
