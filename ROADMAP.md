# NativeUI roadmap

This roadmap turns the current POC into a reusable desktop UI toolkit while preserving the simple architecture: Pugl for native views/events, Skia for rendering, NativeUI for UI behavior.

The nine milestones are also available in [GitHub](https://github.com/hemduf/nativeui/milestones?state=all); ticket details and status live in [GitHub Issues](https://github.com/hemduf/nativeui/issues?q=is%3Aissue).

## Feature delivery rule

Feature examples are mandatory: every feature ticket ships a dedicated executable example with an interactive mode and a `--self-test` mode. T049 remains the later **gallery/aggregation** milestone, not the first point where examples are created.

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

**Status: In progress (T019 and T024 complete; T020/T022 ready)**

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

## Milestone 4 — Text system

**Status: In progress (T025–T027 implementation/review/required matrix complete; T028 next)**

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

Progress: `T025` and `T026` are complete. `T027`'s implementation, deterministic embedded-font/fallback tests, feature example and review passes A/B/C are complete. GitHub Actions run #31 on code head `139f8a5` passed Linux X11, Windows/MSVC, Linux ASan+UBSan and macOS; PR #55 is now in its final recovery-document validation cycle. Strict sequencing advances to `T028` only after that final head is green, #55 is merged, #27 is closed `status:done`, and the T027 recovery ZIP is produced.

## Milestone 5 — Standard widget set

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

## Milestone 6 — Styling, theme and animation

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

## Milestone 7 — Platform and embedded robustness

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
- explicit Wayland strategy after X11 v1 is stable.

Exit gate:

- standalone and embedded smoke tests pass on macOS/Windows/Linux X11;
- host lifecycle failures are reproducible in dedicated tests;
- no plugin API enters the core.

Tickets: `T041`–`T046`.

## Milestone 8 — Packaging, tooling and v1 release

**Goal:** make the toolkit easy to consume and maintain.

Deliverables:

- `install()` / exported CMake package;
- `NativeUI::NativeUI` consumer target;
- dependency lock/version diagnostics;
- examples gallery;
- component inspector/debug overlay;
- benchmark suite;
- CI build matrix;
- release checklist and semantic versioning policy.

Exit gate:

```cmake
find_package(NativeUI CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE NativeUI::NativeUI)
```

works on supported platforms with documented prerequisites.

Tickets: `T047`–`T052`.

## Prioritization rule

Do not jump directly to a large widget catalog. The efficient sequence is:

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

This minimizes rewrites because widgets are built only after the generic primitives they need are stable.
