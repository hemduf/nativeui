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

**#86 / PR #87 — Complete by this merge:** the reviewed Pugl fork restores native drop delivery, including into inactive windows. T018 receives files independently of their extension and previews valid text through the shared UTF-8 decoder; malformed text is repaired before Skia measurement/painting, preventing the image-drop crash. Automated coverage includes native delivery/lifecycle, renamed-file equivalence and malformed-text rendering. On 2026-09-09 the user confirmed that a real Finder drop of `/tmp/hello.txt` displays its contents, including after an image drop. This M7 regression correction does not complete another milestone or change the dependency frontier below.

The merged baseline is complete through T030, including T023 and T053, plus the merged #64 Decision B ownership diagnosis, T059 component-availability contract, T047 low-level install/export package and T048 relocated external-consumer acceptance layer. PR #87 preserves the T048 fixtures and CI gates from main `ed81a201ea459ea2443ae51f27dfcac7af5d7e63`.

- **T053 — consumer-scoped macOS platform bridge / PR #88:** **Complete and merged** as `ad83ed05f1687fea31255bcc77329fae0f4efb69`. `NativeUI::Core` and portable Pugl C code remain generic; only the small macOS Objective-C Pugl/OpenGL/IME bridge is instantiated per final consumer. The frozen helper derives `NUI_<fragment>_<digest12>_` from exact UTF-8 `CONSUMER_ID`, rejects duplicate target/identity registration at configure time and introduces no runtime registry. The macOS acceptance path validates two final consumers plus class/metaclass symbol isolation.
- **#64 — macOS standalone PROGRAM-world ownership / PR #90:** **Complete and merged** as `b52d53eee65e5de91697e2c1f0685728b9646575`. Decision B is frozen: independent `PUGL_PROGRAM` worlds are not a valid NativeUI multi-window ownership model on macOS. Legacy `StandaloneWindow(UI&, ...)` therefore remains single-window/pre-v1 only. **T060 / issue #72 owns the replacement contract:** one explicit `ui::Application`, exactly one PROGRAM world that outlives all top-level windows, multiple `StandaloneWindow(Application&, ...)` views, and no hidden mutable global/singleton/`thread_local` application owner.
- **T059 — generic component availability / PR #89:** **Complete and merged** as `6d84bc6b7817b0dcaea4eefd81833d765ad7685d`. One retained-tree model resolves `Visible`/`Hidden`/`Collapsed`, inherited enabled/disabled and inherited read-only state. Hidden preserves layout while suppressing paint/input/focus; Collapsed removes layout contribution without unmounting; Disabled suppresses normal targeting/focus while remaining laid out/painted; ReadOnly preserves targeting/focus while widgets reject mutations. Capture/focus teardown occurs before suppression, reentrant reversal is bounded, and FocusScope restore semantics are covered.
- **T030 — Button / PR #94:** **Complete and merged** as `b32b09da473c70a857675e05f7fdc7c78e5f9361`. Button consumes T059 effective enabled/read-only state and the central focus/capture policy, with platform-neutral pointer/keyboard activation and reentrancy-safe callbacks. T031 is now the next dependency-unblocked widget successor on its separate lane.
- **T047 — install/export CMake package / PR #92:** **Complete and merged** as `df569e874539aaafb8600960938465f733a46f19`. The installed/build-tree low-level v1 surface is `NativeUI::Core` plus `nativeui_attach_platform(TARGET <final-target> CONSUMER_ID <reverse-dns-id>)`. The helper rejects invalid/non-final/imported/alias targets, malformed identities and all double attachment; macOS delegates exact identity to T053 while Windows/Linux use the same public helper with generic platform implementation. The package carries pinned Skia assets and Pugl/platform source machinery privately, installs required legal payload, validates relocated install-tree consumers and never exports/documents `NativeUI::NativeUI` as a complete v1 target.
- **T048 — external relocated consumer smoke / PR #99:** **Complete by this merge.** Three independent projects validate the relocated installed package only through `find_package(NativeUI CONFIG REQUIRED)`: Core/headless, low-level standalone, and SDK-neutral embedded. All native fixtures link the real `nativeui_attach_platform()` implementation and run deterministic relocated-Core self-tests on Linux/Windows/macOS; macOS additionally runs real standalone/embedded native lifecycle plus the T053/T047 two-consumer Objective-C namespace/runtime coexistence proof. Static checks reject private/source-tree targets, includes and dependency override variables. Code/review head `9904e2fe8f252a35e060bda128a82d18b9f5819a` passed CI #384 on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan before this completion-doc refresh. Completion-doc head `841318d92c389c60ec48982cb0e7d002d56bccdf` also passed CI #394 before the required refresh over #105/#103.
- **T029 — advanced IME composition bridge / PR #85:** **Complete.** NativeUI has one shared platform-neutral composition model for `TextInput` and `TextArea`, transient underlined preedit rendering, UTF-8-safe offsets, single-transaction commit/cancel semantics, candidate geometry and private Cocoa/IMM32/XIM bridges.
- **Cross-cutting P0 safety gate — #62 / PR #63:** **Complete** and consumed by T053.
- **T023 — SVG/icon resources / PR #60:** **Complete.** Backend-neutral SVG resources and per-instance provider-backed caching are in the merged baseline.
- **Lifecycle lane frontier:** **T042** follows merged #64 Decision B. T042 must stress current supported ownership paths—one standalone PROGRAM application-owner lifetime and independent `EmbeddedView` / `PUGL_MODULE` instances—without inventing a hidden simultaneous-standalone workaround. Shared-Application multi-window stress belongs to T060.
- **Platform/package frontier:** **T054 and T056 are Ready after this T048 merge.** T057 remains dependent on T056. T052 has its T047/T048 package dependencies satisfied after this merge but still additionally requires T051 and T042.
- **State/widget frontier:** **T031 is Ready** after merged T030; later theme/accessibility/gallery work follows its declared dependencies.

Parallel execution frontier:

```text
lifecycle:        #64(done) -> T042 -> T051 -> T052
platform/package: T053(done) -> T047(done) -> T048(done) -> T052
                                    |
                                    +-> T056 -> T057
                                    +-> T054
state/widgets:    T059(done) -> T030(done) -> T031
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

**Status: T030 complete; T031/T032/T033/T034 Ready**

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
T059(done) -> T030(done) -> T031
T034 -> T035 / T036
```

T031 now follows the merged Button contract. T032 and T034 are independently Ready and high-unblock-value; T033 is independently Ready.

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
T030(done) + T032 -> T037 -> T038 -> T039 / T040
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

**Status: T053/T047/T048 complete; T054/T056 Ready; T057 follows T056; T055 Not planned**

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

T053 froze the lower-level macOS rule: `NativeUI::Core` and portable platform C code are generic, while Objective-C Pugl/OpenGL/IME code is instantiated per final consumer from stable identity. There is no normal global/manual `NATIVEUI_OBJC_RUNTIME_PREFIX` path.

T047 completes the relocatable low-level package. After `find_package(NativeUI CONFIG REQUIRED)`, consumers receive the exported generic `NativeUI::Core` and the single platform attachment contract:

```cmake
target_link_libraries(MyFinalTarget PRIVATE NativeUI::Core)
nativeui_attach_platform(
    TARGET MyFinalTarget
    CONSUMER_ID com.example.product
)
```

The helper accepts only existing final `EXECUTABLE`/`MODULE_LIBRARY`/`SHARED_LIBRARY` targets, requires one valid reverse-DNS identity on every platform and rejects every second attachment. macOS delegates exact identity to T053 consumer-specific bridges; Windows/Linux attach their generic Pugl/native implementation behind the same call. Installed CMake state is prefix-relative, carries pinned Skia/Pugl implementation assets privately, proves relocation and missing-pinned-asset failure, and does not export/document `NativeUI::NativeUI` as a complete v1 package target.

T048 adds permanent external acceptance projects around that contract. CI installs to one prefix, relocates the install tree, deletes the original prefix, and configures three independent projects against the relocated package only. Core/headless proves `NativeUI::Core` does not require platform attachment; standalone and embedded prove the documented final-target helper and public C++ lifecycle/link surface. All native lanes run deterministic relocated-Core rendering self-tests; macOS additionally runs the real native standalone/embedded lifecycle and two-consumer Objective-C namespace/runtime-isolation proof. Static fixture checks forbid private/detail/source-tree targets, paths and dependency override variables.

Tickets: `T047`–`T054`, `T056`–`T057`. `T055` is closed as **Not planned** and deliberately excluded from the current v1 scope.

Release/package dependency chain:

```text
#62 complete -> T053 complete -> T047 complete -> T048 complete ----\
                                           |                        +-> T052
                                           +-> T056 -> T057        |
                                                                   |
T042 -> T051 -------------------------------------------------------/

T047 complete + T053 complete -> T054
T055 nativeui_add_plugin: Not planned for current v1
```

The platform/package lane can now take T054 or T056 according to priority/unblock value. T052 has its package/external-consumer prerequisites satisfied after T048 but remains blocked by T051 and T042; T057 remains blocked by T056.

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

## Lifecycle completion note — #105

T042 lifecycle stress exposed #105, a constructor-time callback lifetime defect in the public wrapper-to-platform bridge. PR #106 keeps the public API intact while moving the `PlatformServices` bridge into each private per-instance `StandaloneWindow::Impl` / `EmbeddedView::Impl`, so synchronous native callbacks during `ViewCore` construction cannot observe an unassigned public-wrapper `impl_`.

The exact #105 code head `bf8e2ff055b4fe5ed2c2bd415bb3818f925f366b` passed Linux X11, Windows/MSVC, macOS (including Objective-C two-consumer and clipboard/multi-instance lifecycle validation), and Linux ASan+UBSan in CI `34344755454`. A stacked T042 validation candidate also turns the original macOS `embedded_sequential_100` crash GREEN. No global/singleton/`thread_local` ownership state or #64 Decision-B workaround is introduced.

T042 remains the lifecycle frontier after this focused fix. Its Windows stress subsequently exposed independent issue #107, where Pugl's documented `PUGL_SHOW_RAISE + PUGL_FAILURE` means the window was shown but could not be raised; that separate production integration defect must be resolved independently before the final T042 exact-head matrix and merge.

## Lifecycle completion note — #103

T042 also exposed #103 before the intended stress interaction: the first Linux/X11 real renderer exposure under Xvfb/Mesa llvmpipe could crash in `GrGLExtensions::init` while building Skia's assembled GL interface. PR #104 keeps the correction in production platform integration rather than suppressing exposure in the stress harness.

Linux now uses Skia's desktop-native GL interface while Pugl owns the current GLX context, and deliberately does not fall back to the assembled resolver that caused the crash. Interface/context/surface ownership remains per `SkiaGlRenderer`; macOS and Windows preserve the existing assembled Pugl-proc path. CI `34343547392` passed the code on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan, and the Linux merge gate now permanently exercises a real Xvfb/Mesa llvmpipe renderer/lifecycle smoke.

This fix does not change #64 Decision B or add mutable context globals. Once #103 and #107 are complete, T042 must be refreshed from current `main` and rerun as the exact supported-path lifecycle matrix before merge.
