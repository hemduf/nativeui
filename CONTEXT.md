# NativeUI compact recovery context

**Updated:** 2026-09-09

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio, host parameter semantics and a custom native windowing stack remain out of scope.

Non-negotiable rules: widgets/layout stay platform-neutral; public geometry is logical pixels; embedded polling is non-blocking; mutable instance-dependent process globals/singletons/thread-locals are forbidden; retained UI state is UI/main-thread confined; dependencies use CMake+CPM; Skia comes from pinned skia-builder binaries; macOS Objective-C runtime-visible platform classes must be consumer-specific.

## Pinned dependencies

- Pugl: `lv2/pugl` commit `b7637149ebe53124e5be90559e02a0185bbcbd73`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Merged baseline

The merged baseline is complete through T029 plus T059, including rendering resources T022/T023, platform safety issue #62, T053 and the #64 Decision B ownership diagnosis. TextInput/TextArea share the UTF-8 `TextEditModel`; T029 provides platform-neutral IME composition plus private Cocoa/IMM32/XIM bridges. `StandaloneWindow` uses `PUGL_PROGRAM`; `EmbeddedView` uses `PUGL_MODULE` and non-blocking polling. General clipboard text uses canonical `text/plain`.

T053 / PR #88 is merged as `ad83ed05f1687fea31255bcc77329fae0f4efb69`. `NativeUI::Core` and portable Pugl C code remain generic while the small macOS Objective-C Pugl/OpenGL/IME bridge is compiled per final consumer. `cmake/NativeUIConsumerPlatform.cmake` derives the frozen `NUI_<fragment>_<digest12>_` prefix from exact UTF-8 `CONSUMER_ID` bytes, rejects duplicate target/identity registration at configure time and introduces no runtime registry. The macOS acceptance fixture builds two final consumers, audits class+metaclass symbols and rejects any unprefixed `Pugl*` Objective-C runtime class/metaclass.

## T059 component availability — merged PR #89

T059 is merged as `6d84bc6b7817b0dcaea4eefd81833d765ad7685d` and provides one generic retained-tree availability model shared by standard and custom components:

- `VisibilityMode::{Visible, Hidden, Collapsed}`;
- monotonic inherited enabled/disabled state;
- monotonic inherited read-only state;
- `Visibility`, `Enabled` and `ReadOnly` composition wrappers;
- public effective-availability query/debug seam without platform headers.

The retained tree is the sole authority for availability. `Hidden` keeps layout contribution but suppresses paint, normal input and focus. `Collapsed` contributes no layout space and suppresses paint/input/focus without unmount/remount. Disabled subtrees stay laid out/painted but are excluded from normal input targeting and focus. Read-only stays targetable/focusable; TextInput/TextArea and existing Knob/Toggle controls reject value mutation while preserving applicable non-mutating interaction.

Availability transitions cancel pointer capture exactly once before suppression, deactivate/re-home focus before applying the new effective state and preserve retained component identity. Reconciliation is per-tree and UI-thread confined, supports reentrant availability reversal from teardown callbacks, and adds no mutable process-global/singleton/`thread_local` state.

Mandatory review found one FocusScope regression: clearing focus before the availability snapshot was applied caused an active trapping scope to lose its existing pre-scope `focus_restore` record when the whole scope became unavailable. RED head `8b3cdef553e680633039cf82ac896d18a8c416b5` exposed the incorrect global-first fallback; the corrected implementation retains only the transient per-tree availability teardown focus origin so normal FocusScope deactivation can restore the exact pre-scope target. Final T059 validation passed Linux ASan+UBSan, Linux X11, Windows and macOS including native lifecycle smoke.

## T030 Button — PR #94 completion candidate

T030 consumes T059 rather than introducing a widget-local disabled policy. The public `ui::Button` builder owns a visible label and activation callback and remains platform-neutral. The retained Button implementation:

- captures on PointerDown, tracks release-inside while captured and fires exactly once only for a valid inside PointerUp;
- cancels on PointerCancel, deactivation, focus loss and T059 availability teardown;
- activates Space on KeyUp and Enter on first KeyDown while suppressing duplicate held-key activation without requiring a platform repeat flag;
- treats inherited ReadOnly as non-blocking because Button is an action/command control, while inherited Disabled/Hidden/Collapsed remains centrally enforced by T059;
- copies the activation callback and completes all component/context mutation before invoking user code, with no `this` or `InputContext` access after the callback, so synchronous hide/disable is reentrancy-safe and later T058 structural removal can use the same boundary;
- exposes minimal normal/hover/pressed/focused/disabled visual states without adopting T038's future rich style/theme API.

TDD evidence begins with RED head `2800623fd00cf5ab7c0bfc6333afe8960944c7d2`, where the new Button contract intentionally failed compilation because `ui::Button` did not exist. Dedicated `nativeui_button_tests` now cover pointer inside/outside/cancel/capture, Space/Enter repeat guards, focus loss, disable/hide while armed, ReadOnly action behavior, deactivation and synchronous self-disable. Deterministic headless raster checks cover normal, hover, pressed, focused and disabled visual states. `examples/features/t030_button.cpp` provides the mandatory interactive mode and deterministic `--self-test`.

## Standalone ownership decision — issue #64 / PR #90

Issue #64 is complete and merged as `b52d53eee65e5de91697e2c1f0685728b9646575`, freezing **Decision B** for top-level standalone ownership.

Exact macOS RED evidence on diagnostic head `02910e4191d7aead61aee5f710f9a098d5eec600` reproduced:

```text
create A
create B while A is live
poll A + B
destroy A while B remains live
continue using B
destroy B
SIGSEGV
```

The native backtrace reaches AppKit `-[NSWindow(NSFullScreen) _inFullScreen]` through `ui::Tree::deactivate_focus`, `ui::ViewCore::~ViewCore()` and `ui::StandaloneWindow::~StandaloneWindow()`. Destroying one independent PROGRAM world while another remains live therefore invalidates the surviving world's later Cocoa focus/window teardown.

The pinned Pugl ownership model matches that evidence: `PuglWorld` is the application-level owner/event-loop context, `PUGL_PROGRAM` is the application world, and `PUGL_MODULE` is the embedded/plugin world. On macOS PROGRAM worlds share process-global `NSApplication` while retaining per-world platform lifetime resources. NativeUI must not model multiple top-level windows as independent PROGRAM worlds.

Decision B contract:

- legacy `StandaloneWindow(UI&, ...)` remains a single-window/pre-v1 compatibility path;
- independent PROGRAM-world overlap and repeated same-process PROGRAM reconstruction remain explicit diagnostic paths, not supported v1 lifecycle guarantees;
- the supported current multi-instance path is independent `EmbeddedView` / `PUGL_MODULE` views plus one standalone PROGRAM owner lifetime;
- T060 / issue #72 owns the replacement public architecture: one explicit `ui::Application` owns exactly one PROGRAM world/event loop, `StandaloneWindow(Application&, UI&, ...)` registers multiple top-level views, `Application` outlives every window, and no mutable global/singleton/`thread_local` current-application state is allowed.

PR #90 keeps `--issue64-simultaneous` and `--issue64-sequential` diagnostic modes with transition logging and macOS native backtrace support. The default standalone smoke validates only the current supported contract: one standalone owner, two simultaneous embedded instances, destroy-A/continue-B isolation, repeated embedded attach/detach, clipboard, resize, poll and close lifecycle.

## Current DAG frontier

- **T059 / issue #71 / PR #89:** complete and merged; this unblocked T030.
- **T030 / issue #30 / PR #94:** state/widget completion candidate; exact-head CI, mandatory review and documentation are the remaining merge gates. Once merged, T031 is unblocked and T037 has its Button-side dependency satisfied.
- **T042 / issue #42:** current lifecycle/stress lane after merged #64 Decision B; stress supported ownership paths without inventing hidden simultaneous standalone support before T060.
- **T053:** complete/merged. This unblocks T047 on the platform/package lane.
- **T047:** package/install lane; owned independently from lifecycle and state/widget work.

Important dependency chains include:

```text
T059(done) -> T030 -> T031
T030 + T032 -> T037 -> T038 -> T039 / T040
T034 -> T035 / T036
T030 + T031 + T032 + T036 -> T045
T030 + T032 + T034 + T035 + T036 -> T049
T042 -> T051 -> T052
T047 -> T048 -> T052
#62 -> T053 (complete)
T047 + T053 -> T054
T047 -> T056 -> T057
```

## T053 consumer-scoped bridge details

Architecture:

```text
NativeUI::Core                         generic, compiled once
Pugl common.c + internal.c            generic on macOS, compiled once
mac.m + mac_gl.m + Cocoa IME bridge   compiled per final consumer
```

Each final consumer has one stable non-empty `CONSUMER_ID`. Prefix derivation is exact UTF-8 identity -> SHA-256 -> first 12 lowercase hex plus an ASCII-only readable fragment up to 24 characters -> `NUI_<fragment>_<digest12>_`. Configure-time bookkeeping rejects duplicate target attachment and reuse of one identity by distinct final targets. Every consumer bridge renames `PuglWindow`, `PuglWindowDelegate`, `PuglWrapperView` and `PuglOpenGLView`; the pattern-based audit covers class and metaclass symbols so future pinned Pugl classes cannot silently escape prefixing.

## Packaging frontier

T047 / issue #47 now owns the installed/public low-level contract:

```cmake
find_package(NativeUI CONFIG REQUIRED)

nativeui_attach_platform(
    TARGET MyFinalTarget
    CONSUMER_ID com.example.product
)
```

T047 must export `NativeUI::Core`, install the T053 machinery relocatably, validate final target type + reverse-DNS identity, reject double attachment, preserve build-tree/install-tree parity, and prove external install-tree consumers on macOS/Windows/Linux. It must not export/document `NativeUI::NativeUI` as the complete v1 package target.

## Build / validation

Normal source-tree build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

No global `NATIVEUI_OBJC_RUNTIME_PREFIX` is required. NativeUI-owned source-tree final targets register consumer identities internally. Core-only builds use `-DNATIVEUI_BUILD_PLATFORM=OFF`.

Offline dependency overrides:

```bash
cmake -S . -B build \
  -DNATIVEUI_PUGL_SOURCE=/path/to/pugl \
  -DNATIVEUI_SKIA_ROOT=/path/to/extracted/skia-builder
```

## Current limitations

- Explicit shared-Application multi-window standalone support is not implemented yet; T060 owns it.
- Legacy independent `PUGL_PROGRAM` worlds are diagnostic-only on macOS after #64 Decision B.
- X11 advanced preedit callbacks depend on the installed XIM advertising `XIMPreeditCallbacks`; ordinary committed text remains available otherwise.
- Standard Slider/ComboBox/List/ScrollView/Tabs/Menu widgets remain incomplete; T030 Button is the current completion candidate.
- Dynamic structural self-removal is not available until T058; T030 already establishes the callback boundary needed for safe synchronous state changes and deferred future removal.
- Theme/style inheritance, accessibility, Wayland, install/export packaging and full host integration remain incomplete.

## Next state/widget action

1. Finish exact-head T030 validation and mandatory CODE_REVIEW.md review on PR #94.
2. Resolve every blocking finding, synchronize ROADMAP.md and merge T030 only from current `main`.
3. After T030 is merged, T031 becomes unblocked; this state/widget automation lane has completed the requested T059 -> T030 chain.
