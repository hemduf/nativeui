# NativeUI roadmap

**Updated:** 2026-09-12

NativeUI is a reusable C++20 desktop retained-mode UI toolkit: Pugl owns native views/events, Skia owns rendering, and NativeUI owns retained composition, layout, input/focus, widgets, styling, resources and packaging. GitHub Issues are the source of truth for exact ticket scope, status and dependencies.

## Execution rules

- explicit GitHub `Dependencies:` are hard gates;
- resume existing canonical branches/PRs before creating work;
- among Ready work, prefer priority and downstream unblock value;
- behavior/configuration changes use RED -> GREEN -> REFACTOR;
- implementation progress and issue conformity are separate; Done/merge requires an explicit issue-to-code/test evidence matrix with no unchecked requirement;
- code-changing tickets require exact-head normal validation and a final `CODE_REVIEW.md` record with no Blocking/Important finding;
- use `CI_POLICY.md`: Draft for active implementation, then Draft -> Ready only for a frozen executable candidate to trigger T042/T052 qualification;
- NativeUI-owned targets must compile with zero unapproved warnings and the default empty `NATIVEUI_ALLOWED_WARNINGS`;
- unrelated lanes may continue only within repository concurrency rules while another PR waits exclusively on external CI;
- every completion cycle synchronizes issue status, `CONTEXT.md` and this roadmap;
- feature tickets ship an interactive example plus deterministic `--self-test`.

## Current execution snapshot

Current `main` includes T064 / PR #240 merged as `5e9798f6637af8d6275379002fa8116f167115f7`, plus completed T044, T072, T066, T062 with its post-merge completeness fix, T063, T035, T043, T061, T067, T045, T037, T058, T036, T065, T034 and T060.

T064 / issue #76 / PR #240 is complete and merged. Frozen current-main-synchronized executable head `ac1ebe13fe61e39bd5ba9bdb4cd5e75bfb795ca7` passed normal CI, T064 Desktop Services, T072 Linux D-Bus, T066 Window Controls, T065 Platform Dispatcher, T060 Application, T044 Native Pointer Capture, Package Contracts, T042 Lifecycle Stress and T052 v0.1 Release Gate. The final requirement-to-implementation/test review and current-main composition re-certification report no Blocking/Important finding.

T072 / issue #84 / PR #185 is complete and merged. Its final executable candidate passed normal CI, T072 Linux D-Bus Contract, T060 Application Contract, T065 Platform Dispatcher, T067 Virtual List Contract, T042 Lifecycle Stress and T052 v0.1 Release Gate with a clean final `CODE_REVIEW.md` review.

T062 / issue #74 / PR #226 is complete and merged as `7269310995adad1e7b474b6cea319644fc8020f4`; PR #245 / `86c12e8472a82cd929a95c24705995dfa871bbf5` closes its post-merge completeness gaps. It is no longer a T068 blocker.

T066 / issue #78 / PR #237 is complete and merged as `c0140e725ac33b0a3ca315124a3d20091b96a173`. Its frozen executable candidate passed normal/path-scoped validation plus final-candidate T042/T052 qualification, and the final requirement-to-implementation/test review recorded no Blocking/Important finding.

T044 / issue #44 / PR #145 is complete and merged as `d4a61888c2d6bc096774de5a71dba8ad82cdff29`. Frozen implementation/test head `b16861f8db07e5292ebbfd40e5f21c00234b0f2a` passed exact-head normal/path validation plus final-candidate T042/T052 qualification. Reviews `5185659103` and `5186332658` record the complete #44 Outcome A/B and `CODE_REVIEW.md` evidence matrix with no Blocking/Important finding.

T050 / issue #50 / PR #238 delivers the opt-in retained-tree debug inspector. Frozen executable head `fc0cd452bc442e83ae89fac04d227922cfc4a450` is synchronized with current `main`, preserves the T044/T064/T066 baseline, and passed normal macOS/Windows/Linux CI, Linux ASan+UBSan, Package Contracts, T066, T072 and the dedicated inspector OFF/ON matrix; the inspector-ON build also passed all 65 registered headless tests.

Current dependency frontier:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(done) -> T068
                                                   T058(done) ------------^

dynamic/overlay:   T058(done) -> T061(done)
                                      |-> T035(done) --------------------> T068
                                      |-> T063(done) --------------------> T068
                                      +-> T062(done) --------------------> T068

style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052(done)

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

critical platform: T060(done) -> T065(done) -> T072(done) -> T064(done)
                   T041(done) -> T043(done) -> T066(done)
                                      |-------> T068

release:            T044(done) -------------------------------> T071
                    all explicit convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

The v1 platform prerequisite lane is complete: T065, T072, T043, T064 and T066 are Done. T044 is also complete as an independent T071 release dependency.

## Milestone 0 — Baseline hardening

**Complete.** T001–T006 provide core/state tests, retained lifecycle, public-header split and invalidation foundations. #163 / PR #181 makes unapproved NativeUI-owned compiler warnings Blocking.

## Milestone 1 — Layout system

**Complete.** T007–T012 provide constraints, alignment/distribution, flex, grid, scroll state/layout and clipping/overflow foundations.

## Milestone 2 — Input, focus and gestures

**Complete baseline.** T013–T018 provide event propagation, focus scopes/restoration, pointer capture, wheel normalization, gestures, commands and drag/drop primitives.

## Milestone 3 — Rendering and graphics

**Complete.** T019–T024 provide transforms, paths, gradients, images, SVG/resources, caches and deterministic rendering/golden tests.

## Milestone 4 — Text system

**Complete.** T025–T029 provide text editing, Label/fonts/fallback, TextArea, UTF-8 selection/navigation and platform IME composition bridges.

## Milestone 5 — Standard widget set

**Complete for the v1 standard-widget scope.**

- T030 Button — PR #94.
- T031 Checkbox/Radio — PR #95.
- T032 Slider/RangeSlider — PR #115.
- T033 ProgressBar/Meter — PR #123.
- T034 ScrollView — PR #135.
- T035 ComboBox/PopupMenu — PR #225; policy over T061 with immutable open snapshots, exact keyboard/pointer/focus semantics, deterministic goldens and standalone/embedded smoke.
- T036 ListView/Tabs — PR #155; #212 / PR #213 adds deterministic hover presentation.

## Milestone 6 — Styling, theme and animation

**T037 complete; T038/T039/T040 remain.**

T037 / PR #151 provides typed per-UI Theme values and representative control theme binding. T038 owns typed widget variants, T039 scoped style inheritance, and T040 animation must reuse T065 instead of introducing another scheduler.

## Milestone 7 — Platform and embedded robustness

Delivered foundations include plug-in host isolation, standalone ownership Decision B, T042 lifecycle stress, T053 Objective-C runtime identity, T060 Application ownership, T065 Dispatcher/timers, T045 accessibility architecture and T043 resize/scale negotiation.

### T043 — Resize/scale negotiation

**Complete in PR #142.** Public/component geometry remains logical while native/framebuffer geometry is physical; per-view finite-positive scale, authoritative configure snapshots, exact request/echo behavior, fractional conversion, embedded parent authority and reentrancy-safe preferred-size notification are qualified.

### T044 — OS pointer capture qualification

**Complete in PR #145 / issue #44.** The evidence-gated decision procedure is finalized per platform:

- **macOS: Outcome A.** Pinned Pugl capture is sufficient; no T044 macOS capture backend was added. Native CGEvent outside-view motion/up, focus-loss, destruction and isolation evidence is green.
- **Windows: Outcome B.** Pinned Pugl owns acquisition; T044 records the concrete HWND at retained capture start and releases only that same HWND on retained cancellation. Outside-view evidence uses real `SetCursorPos` + `SendInput`, so delivery depends on native capture rather than direct window messaging.
- **Linux/X11: Outcome B.** X11 ButtonPress supplies the active grab; retained cancellation explicitly releases it with `XUngrabPointer`/`XSync`. A narrow per-view focus visibility seam exposes the FocusIn/FocusOut transitions consumed internally by the pinned Pugl XIC path so mandatory focus-loss cancellation remains deterministic.

All capture bookkeeping remains per view, widgets stay platform-neutral, exactly-once up/cancel and teardown/isolation paths are covered, Linux sanitizer coverage is green, and no Objective-C runtime-visible class/category/swizzle/+load was introduced. Final normal/path and T042/T052 gates are green with no Blocking/Important review finding.

### T066 — Standalone window controls

**Complete in PR #237 / issue #78.** V1 scope is limited to logical min/max constraints, runtime title/show/hide/size control and deterministic close lifecycle on T060 Application-owned windows:

- finite/order validation, initial clamp and atomic runtime constraint updates preserve T043 native-authoritative configure semantics;
- UTF-8 title and idempotent show/hide do not recreate or remount UI;
- user/native close supports veto, accepted close is deferred through T065, and programmatic `request_close()` bypasses veto;
- reentrant `request_close()` inside veto wins over Cancel and duplicate close requests cannot duplicate `on_closed`;
- direct C++ destruction is callback-silent, suppresses pending accepted-close completion and still unregisters exactly once through T060 quit policy;
- close callbacks run outside internal teardown/Application locks and per-window state remains isolated;
- native macOS/Windows/Linux control/close coverage, pure state tests, two-window destroy-A/survivor-B coverage and deterministic feature `--self-test` are qualified;
- final normal/path and T042/T052 gates are green with no Blocking/Important review finding.

## Milestone 8 — Packaging, virtualization, overlays and release convergence

Delivered foundations include T047/T048 package consumption, T050 debug inspector, T051 performance qualification, T052 v0.1 release gate, T054 application helper, T056 binary data, T057 ResourceManager, T058 dynamic composition, T061 overlay/portal infrastructure, T067 fixed-height virtualized ListView, T062 Tooltip, T063 Dialog, T064 DesktopServices, T072 Linux D-Bus transport and T045 semantic architecture.

### T050 — Debug inspector / overlay

PR #238 / issue #50 adds a passive, opt-in diagnostic layer rather than a second retained-tree system:

- build option `NATIVEUI_ENABLE_INSPECTOR` defaults OFF, with runtime activation and per-UI inspector state compiled only when enabled;
- each `ui::UI` owns enabled/selected state independently; no current-inspector singleton, mutable process registry or `thread_local` instance state is introduced;
- immutable value snapshots expose NodeId, parent/depth/order, stable debug label, logical bounds, effective clip, layout/paint dirty state, focus/capture and effective T059 availability without leaking raw runtime pointers;
- stale/destroyed NodeId queries are safe and previously copied snapshots remain self-contained;
- enabling/disabling or changing the selected NodeId requests one paint invalidation only and never creates layout dirtiness or a timer/tick loop;
- normal root and T061 application overlay content paint first; the diagnostic pass then draws node/clip/dirty/selection/focus/capture information and restores Painter/SkCanvas state;
- the inspector receives no hit testing, pointer/keyboard input or focus and does not consume T061 overlay slots;
- deterministic tests cover hierarchy/child order, dirty state/regions, stale IDs, focus/capture/clip/availability, selected-node emphasis, keyboard/pointer non-interception, canvas-state preservation, no-continuous-redraw behavior and two-UI isolation;
- infrastructure scope intentionally requires deterministic inspector/headless fixtures rather than a new normal feature API example;
- frozen executable head `fc0cd452bc442e83ae89fac04d227922cfc4a450` passed inspector OFF/ON, the full 65-test inspector-ON headless suite, normal CI on macOS/Windows/Linux, Linux ASan+UBSan, Package Contracts, T066 and T072 with no Blocking/Important review finding.

### Build/example registration hardening

**Complete in PR #235 / issue #234.** Canonical `examples/features/tNNN_<feature>.cpp` sources are auto-discovered by root CMake with deterministic ordering, `CONFIGURE_DEPENDS` and malformed-name rejection.

### T061 — Generic overlay / portal layer

**Complete in PR #216.** One generic retained in-view overlay/portal layer per UI using T058 structural reconciliation, including deterministic placement, modal focus/capture behavior, anchor tracking, no-click-through dismissal, reentrant show/close safety and standalone/EmbeddedView parity.

### T067 — Fixed-height virtualized ListView

**Complete in PR #219 with startup regression closure in PR #233.** The required 100k example is constrained to the real viewport, startup materialization remains bounded, and the production `--self-test` is executed by dedicated CI.

### T063 — Dialog

**Complete in PR #227 / issue #75.** Dialog remains policy over T061 rather than a second modal/window manager. It provides one active Dialog slot per UI, validated action/result IDs, Default/Cancel semantics, styleable backdrop, bounded centered sizing, fixed title/actions, T034 scrolling for overflow, no-click-through pointer behavior, trapped focus/restoration, child-first Enter behavior, Escape Cancel/Dismissed policy, exact-once completion, safe controller/UI teardown, per-UI isolation and close-before-callback reentrancy.

### T062 — Tooltip

**Complete in PR #226 / issue #74, with PR #245 closing post-merge completeness gaps.** A text-only retained decorator over T061/T065: exact 500 ms default delay with zero-delay checkpoint deferral, one per-instance timer/eligibility controller, no warm-up or cross-anchor reuse, shared hover/focus delay with full restart on new transitions, a `NonModal`/`Auto`/`Ignore` overlay that is non-focusable and non-hit-testable, deterministic dismissal and suppression, semantic help through T045 semantics, and native standalone/EmbeddedView qualification.

### T072 — Bounded Linux D-Bus transport

**Complete in PR #185 / issue #84.** It provides the sole Linux D-Bus layer for T064 and T068:

- system `libdbus-1` only, no public D-Bus API/type leakage;
- one private session connection + one joinable I/O thread per transport;
- only the explicit process-wide `dbus_threads_init_default()` once-initialization;
- exact 1,024/256/256 request/subscription/object-path limits with registered client ownership;
- exactly-once request terminal states and T065-only UI callback marshalling;
- teardown-safe queued callback suppression;
- bounded signal/object-path services and a closed owned C++ value codec;
- immutable T068 provider reads on the D-Bus thread, with mutation/actions marshalled to UI;
- one lazy shared transport per T060 Application and the independently owned EmbeddedView boundary required by T068;
- package/install integration and Linux prerequisite documentation.

### T064 — Cross-platform DesktopServices

**Complete in PR #240 / issue #76, merged as `5e9798f6637af8d6275379002fa8116f167115f7`.** The delivered bounded service contract is:

- callback-only open-file, open-files, save-file, select-directory and absolute HTTP(S) URL operations;
- callbacks always cross the owning T065 Dispatcher, including immediate Busy/ResourceLimit/Unsupported/InvalidArgument failures and accepted inline backend completion;
- exact one active chooser and sixteen active URL requests per DesktopServices owner, monotonic non-zero owner-local IDs, stale/cross-owner cancellation isolation, exactly-once completion and capacity release before application callback;
- exact filter/suggested-filename/cardinality/path/URL validation with no hidden request queue;
- `StandaloneWindow::desktop_services()` is lazy and view-owned; built-in EmbeddedView remains side-effect-free/Unsupported unless an embedding owner injects a backend;
- macOS uses `NSOpenPanel` / `NSSavePanel` + `NSWorkspace`, with no new NativeUI Objective-C runtime-visible class/category/swizzle/+load;
- Windows uses `IFileOpenDialog` / `IFileSaveDialog` on request-owned joinable STA workers, `ShellExecuteW` for URL launch and request-local same-STA cancellation without GIT/process registry/raw cross-apartment dialog state;
- Linux/X11 uses XDG Desktop Portal FileChooser/OpenURI through T072 only, preserves T072 hard-quota `ResourceLimit`, uses parent-window routing and Request.Close, and adds no GTK/Qt/zenity/shell fallback or overflow transport;
- native macOS/Windows/Linux qualification, package integration and `examples/features/t064_desktop_services.cpp --self-test` are complete.

Frozen current-main-synchronized head `ac1ebe13fe61e39bd5ba9bdb4cd5e75bfb795ca7` passed exact-head normal/path-scoped validation and final-candidate T042/T052. The final requirement-to-implementation/test matrix and `CODE_REVIEW.md` audit report zero remaining Blocking/Important findings.

### T068 convergence

T068 starts only after **all** explicit issue #80 dependencies are Done. It implements T045 semantics through immutable per-view snapshots and native NSAccessibility/UIA/AT-SPI2 bridges, shares T067 virtual metadata, routes mutations through T065 and uses T072 as the sole Linux D-Bus transport.

### Final v1 release path

```text
T036(done) -> T045(done) -> T067(done) ---------------------\
T058(done) -> T061(done) -> T035(done) -> T063(done) --------+--> T068 -> T069 -> T070 -> T071 -> v1.0.0
                         \-> T062(done) ---------------------/
T065(done) -> T072(done) -> T064(done) ---------------------+
T043(done) ---------------------> T066(done) ----------------/
T044(done) --------------------------------------------------> T071
other explicit T069 dependencies ---------------------------/
```

T069 is the final v1 public API freeze and cannot start until every explicit issue #81 dependency is complete. T070 validates the reference application/Getting Started against the frozen API. T071 is validation/release-only on one exact RC SHA; defects found there return to their canonical fix ticket.

## Immediate cross-lane plan

1. The platform prerequisite lane is complete; do not reopen T044/T064/T066/T072/T043/T065 unless a concrete regression appears.
2. Continue T038 -> T039 and T040 in the styling lane as capacity permits.
3. Continue T068 according to its own explicit issue #80 dependencies; T064 is no longer a platform blocker.
4. Keep T069/T070/T071 dependency-gated until their explicit prerequisite sets are genuinely Done.

## Prioritization rule

```text
core correctness
  -> layout/input/render/text foundations
  -> widgets/styles/overlays/platform services
  -> accessibility convergence and public API freeze
  -> reference package/release
```

Independent tickets may progress concurrently only when explicit dependencies, shared-file conflict risk and repository concurrency rules allow it.