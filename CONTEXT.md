# NativeUI compact recovery context

**Updated:** 2026-09-12

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, state, widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio and host parameter semantics remain out of scope.

Non-negotiable rules:

- widgets/layout stay platform-neutral and public geometry uses logical pixels;
- embedded polling is non-blocking;
- mutable instance-dependent production globals/singletons/`thread_local` state are forbidden;
- retained UI state is UI/main-thread confined unless explicitly documented otherwise;
- dependencies use CMake + CPM; Skia comes from pinned `skia-builder` binaries;
- macOS Objective-C runtime-visible platform classes are consumer-specific through the T053 identity contract;
- NativeUI-owned source-tree targets compile with zero unapproved warnings and an empty default `NATIVEUI_ALLOWED_WARNINGS`;
- behavior changes use RED -> GREEN -> REFACTOR and must not weaken tests or validation policy;
- `CODE_REVIEW.md`, `CI_POLICY.md`, a complete issue-to-code/test evidence matrix, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline and critical path

Current `main` includes T064 / PR #240 merged as `5e9798f6637af8d6275379002fa8116f167115f7`, plus completed T044, T072, T066, T062 and its post-merge completeness fix, T063, T035, T043, T061, T067, T045, T037, T058, T036, T065, T034 and T060 foundations.

T064 / issue #76 / PR #240 is complete and merged. Frozen current-main-synchronized executable head `ac1ebe13fe61e39bd5ba9bdb4cd5e75bfb795ca7` passed normal CI, T064 Desktop Services, T072 Linux D-Bus, T066 Window Controls, T065 Platform Dispatcher, T060 Application, T044 Native Pointer Capture, Package Contracts, T042 Lifecycle Stress and T052 v0.1 Release Gate. The final requirement -> implementation -> test matrix and current-main composition re-certification report no remaining Blocking/Important finding.

T072 / issue #84 / PR #185 is complete and merged. Its final current-main-synchronized executable candidate passed normal CI, T072 Linux D-Bus Contract, T060 Application Contract, T065 Platform Dispatcher, T067 Virtual List Contract, T042 Lifecycle Stress and T052 v0.1 Release Gate, with final `CODE_REVIEW.md` re-certification reporting no Blocking/Important finding.

T062 / issue #74 / PR #226 is complete and merged as `7269310995adad1e7b474b6cea319644fc8020f4`; post-merge completeness gaps were closed by PR #245 / `86c12e8472a82cd929a95c24705995dfa871bbf5`. Tooltip is no longer a convergence blocker.

T066 / issue #78 / PR #237 is complete and merged as `c0140e725ac33b0a3ca315124a3d20091b96a173`. Its frozen executable candidate passed normal/path-scoped validation plus final-candidate T042/T052 qualification, and its final requirement-to-implementation/test review reported no Blocking/Important finding.

T044 / issue #44 / PR #145 is complete and merged as `d4a61888c2d6bc096774de5a71dba8ad82cdff29`. Frozen implementation/test head `b16861f8db07e5292ebbfd40e5f21c00234b0f2a` passed exact-head normal/path validation and final-candidate T042/T052 qualification. Reviews `5185659103` and `5186332658` record the complete Outcome A/B, acceptance and `CODE_REVIEW.md` matrices with no Blocking/Important finding.

T050 / issue #50 / PR #238 delivers the opt-in debug inspector on top of the current T044/T064/T066 baseline. Frozen executable head `fc0cd452bc442e83ae89fac04d227922cfc4a450` is reconciled with current `main`, is zero commits behind, and passed exact-head normal CI, Package Contracts, T066 Window Controls, T072 Linux D-Bus and the dedicated inspector OFF/ON matrix. The inspector-ON configuration also compiled the full headless tree and passed all 65 registered tests.

Current convergence:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(done) -> T068
                                                   T058(done) ------------^

dynamic/overlay:   T058(done) -> T061(done)
                                      |-> T035(done) --------------------> T068
                                      |-> T063(done) --------------------> T068
                                      +-> T062(done) --------------------> T068

style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

critical platform: T065(done) -> T072(done) -> T064(done)
                   T041(done) -> T043(done) -> T066(done)
                                      |-------> T068

release:            T044(done) -------------------------------> T071
                    convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

The owned v1 platform prerequisite lane T065/T072/T043/T064/T066 is complete. T044 is also complete as an independent T071 release dependency.

## Completed foundations relevant to v1

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- #64 / PR #90: standalone PROGRAM-world ownership Decision B.
- T059 / PR #89: inherited visibility/enabled/read-only state.
- T030–T034: Button, Checkbox/Radio, Slider/RangeSlider, ProgressBar/Meter and ScrollView baseline.
- T035 / PR #225: ComboBox + PopupMenu on T061 with immutable open snapshots, exact keyboard/pointer/focus policy, T059 ReadOnly split, close-before-callback reentrancy, deterministic golden coverage and standalone/embedded native smoke.
- T036 / PR #155: retained non-virtualized ListView + Tabs with stable selection and composite focus.
- #212 / PR #213: deterministic ListView/Tabs hover presentation and retained pointer-leave lifetime.
- T037 / PR #151: typed per-UI theme tokens and representative widget theme binding.
- T043 / PR #142: resize/scale negotiation with logical public geometry, per-view scale state, authoritative configure snapshots and non-recursive native size requests.
- T044 / PR #145: evidence-gated native pointer capture qualification; macOS Outcome A, Windows Outcome B, Linux/X11 Outcome B with per-view ownership and no widget/platform leakage.
- T045 / PR #210: accessibility semantic architecture and immutable virtual collection contract.
- T047 / PR #92 + T048 / PR #99: relocatable package and external-consumer qualification.
- T050 / PR #238: build-default-OFF per-UI debug inspector with value snapshots, selected-node diagnostics, passive post-content rendering and deterministic headless qualification.
- T051 / PR #116 + T052 / PR #120: reproducible performance policy and v0.1 developer-preview release gate.
- T054 / PR #119, T056 / PR #111, T057 / PR #126: application/package/resource helpers.
- T058 / PR #154: bounded retained dynamic composition with one per-tree structural reconciliation queue.
- T060 / PR #118 + #139 / PR #140: explicit Application ownership and stress-qualified multi-window lifecycle.
- T061 / PR #216: generic per-UI overlay/portal layer with deterministic placement, modal focus/capture semantics, anchor tracking and retained reconciliation through T058.
- T062 / PR #226 + PR #245: text-only Tooltip decorator over T061 + T065 with hover/focus eligibility, deterministic dismissal/suppression, semantic help and completed post-merge qualification coverage.
- T063 / PR #227: modal Dialog policy over T061 with deterministic action/focus/scroll/reentrancy semantics and standalone/embedded qualification.
- T064 / PR #240: bounded callback-based DesktopServices with fixed macOS/Windows/Linux backends, T072-only Linux portal integration, owner-local request identity/capacity and native platform qualification.
- T065 / PR #133: bounded UI-thread Dispatcher/timer service with native wake integration.
- T066 / PR #237: standalone window min/max/title/show/hide/resize controls plus deterministic user/programmatic close, veto, exactly-once `on_closed`, destructor-silent teardown and two-window isolation across macOS/Windows/Linux.
- T067 / PR #219 + PR #233: fixed-height virtualized ListView with bounded materialization and qualified 100k startup behavior.
- #163 / PR #181: warning-free NativeUI-owned source-tree builds.
- #152 / PR #153: Tree no longer paints an implicit application background/help overlay.
- #234 / PR #235: canonical feature examples auto-discovered by root CMake with deterministic ordering and naming guards.

## T072 delivered contract

PR #185 provides the sole v1 Linux D-Bus transport for T064 portals and T068 AT-SPI2 integration:

- Linux-only internal system `libdbus-1`; macOS/Windows do not discover or link it;
- one ticket-authorized process-safe `dbus_threads_init_default()` initialization, with no process-global connection or instance registry;
- one private session-bus connection and one joinable bounded-wait I/O thread per transport;
- exact transport-wide 1,024 pending-call / 256 signal-subscription / 256 object-path limits;
- monotonic non-zero transport-local IDs, explicit registered clients, stale/cross-client rejection and capacity release before callbacks;
- async Reply / RemoteError / Timeout / Cancelled / Shutdown exactly-once completion;
- UI/client callbacks delivered only through T065 Dispatcher, with rejection/drop rather than I/O-thread fallback;
- queued-callback suppression and client teardown gates preventing callbacks after client/transport destruction;
- bounded signal subscribe/unsubscribe, object-path register/unregister and signal send;
- closed owned C++ value codec for the required scalar/array/dictionary/variant/struct set, with no libdbus type leakage;
- immutable provider-read boundary for T068 read-only queries; mutation/actions marshal through T065;
- one lazy shared transport per T060 Application and an independently ownable operations boundary for accessibility-enabled EmbeddedView use;
- Linux build/install/package integration and documented `libdbus-1` prerequisite.

The final requirement -> implementation -> test matrix and `CODE_REVIEW.md` audit are recorded in PR #185.

## T064 delivered contract

PR #240 provides the v1 cross-platform DesktopServices layer:

- one lazy view-owned `DesktopServices` facade per standalone window, with built-in EmbeddedView remaining side-effect-free/Unsupported unless an embedding owner explicitly injects a backend;
- callback-only open-file, open-files, save-file, select-directory and HTTP(S) URL operations, with accepted and immediate failures completed asynchronously through T065;
- exact one-active-chooser / sixteen-active-URL owner-local bounds, monotonic non-zero IDs, stale/cross-owner cancellation rejection, exactly-once completion and capacity release before application callback;
- exact filter, suggested-filename, result-cardinality, Unicode path and bounded HTTP(S) validation;
- distinct facade `Busy` and shared-T072 `ResourceLimit` semantics with no hidden overflow queue/transport;
- owner destruction cancels backend requests and suppresses not-yet-started application callbacks without invoking backend/user code under coordinator locks;
- macOS uses `NSOpenPanel` / `NSSavePanel` and `NSWorkspace`, without new NativeUI Objective-C runtime-visible classes/categories/swizzling/`+load`;
- Windows uses `IFileOpenDialog` / `IFileSaveDialog` on request-owned joinable STA workers, `ShellExecuteW` for URL launch and request-local same-STA cancellation with no GIT/process registry/raw cross-apartment dialog pointer;
- Linux/X11 uses XDG Desktop Portal `FileChooser` / `OpenURI` through T072 only, including parent-window routing, Request.Close cancellation, missing-portal `Unsupported`, hard-quota `ResourceLimit` and no GTK/Qt/zenity/shell fallback;
- deterministic fake-only `examples/features/t064_desktop_services.cpp --self-test`, native platform smokes and install/package integration.

The final acceptance matrix and `CODE_REVIEW.md` audit are recorded in PR #240; current-main synchronized head `ac1ebe13fe61e39bd5ba9bdb4cd5e75bfb795ca7` passed all required exact-head iterative and final-candidate gates before merge.

## T066 delivered contract

PR #237 completes only the v1 standalone-window control surface:

- optional logical `WindowDesc::min_size` / `max_size` with finite/order validation, initial clamp and atomic runtime updates;
- runtime UTF-8 title, idempotent show/hide, logical resize and min/max updates while preserving T043 native-authoritative configure semantics;
- `CloseDecision`, user/native veto, programmatic `request_close()`, `is_closed`, `on_close_request` and exactly-once `on_closed`;
- accepted close is deferred through the owned T065 Dispatcher so native/input callback stacks unwind before teardown;
- `request_close()` inside a veto callback wins over a returned Cancel; duplicate pending/native close requests cannot duplicate completion;
- direct C++ destruction is deliberately callback-silent, suppresses pending accepted-close completion and still updates T060 registration/quit policy exactly once;
- close callbacks may safely operate on the Application/other windows and are never invoked while internal teardown locks are held;
- per-window state only, with native two-window destroy-A/survivor-B qualification;
- macOS/Windows/Linux native min/max/title/show-hide/close qualification, pure deterministic state tests and `examples/features/t066_window_controls.cpp --self-test`.

## T044 delivered contract

PR #145 completes the evidence-gated pointer-capture qualification without changing widget APIs:

- macOS is Outcome A: pinned Pugl already preserves required outside-view motion/up; no native T044 capture extension is added;
- Windows is Outcome B: acquisition remains Pugl-owned, while retained cancellation releases only the concrete captured HWND; the native fixture uses real system cursor/input injection rather than direct HWND messages;
- Linux/X11 is Outcome B: retained cancellation releases the active X11 pointer grab and a narrow per-view focus proxy exposes the focus transitions consumed internally by pinned Pugl so capture cancellation on focus loss is observable;
- native capture mirrors only real retained none<->owner transitions and produces no duplicate PointerUp/PointerCancel;
- focus loss, destruction, capturing-subtree removal, repeated acquire/release and two-view isolation are qualified;
- bookkeeping is per concrete view/PlatformServices instance with no current-drag singleton, process registry or `thread_local` instance state;
- widgets/components remain platform-neutral and use only the existing `InputContext` capture API;
- Linux ASan+UBSan, normal CI and final T042/T052 qualification are green;
- no Objective-C runtime-visible class/category/swizzle/+load is introduced.

## T050 delivered contract

PR #238 adds a passive diagnostic consumer of the retained tree without creating a second runtime model:

- `NATIVEUI_ENABLE_INSPECTOR` defaults OFF; the runtime inspector activation path and per-UI inspector state exist only in inspector-enabled builds;
- enabled/selected state is owned by each `ui::UI`, with deterministic two-UI isolation and no process-global current inspector;
- snapshots are owned values only: NodeId, parent/depth/order, stable debug label, logical bounds, effective clip, layout/paint dirty state, focus/capture and T059 effective availability; no raw retained pointers escape;
- stale/destroyed NodeId queries return absent safely while previously copied snapshots remain valid;
- the selected NodeId only affects diagnostic emphasis; enabling/disabling or changing selection requests one paint invalidation and never dirties layout;
- normal root/application-overlay content is painted first, then a dedicated diagnostic pass draws node/clip/dirty/focus/capture information while preserving incoming Painter/SkCanvas state;
- the diagnostic pass is not a T061 application overlay, receives no hit testing/focus/input and does not consume overlay stack slots;
- static inspector state creates no timer/tick/continuous redraw; normal repaint naturally refreshes diagnostics;
- deterministic model/runtime/headless tests cover hierarchy/order, dirty regions, stale IDs, focus/capture/clip/availability, selected-node rendering, pointer/keyboard non-interception, canvas-state preservation, idle behavior and instance isolation;
- frozen executable head `fc0cd452bc442e83ae89fac04d227922cfc4a450` passed the inspector OFF/ON matrix, full 65-test inspector-ON headless suite, normal macOS/Windows/Linux CI, Linux ASan+UBSan, Package Contracts, T066 and T072 checks with no Blocking/Important review finding.

## Active platform work

The v1 platform prerequisite lane is complete: T065, T072, T043, T064 and T066 are Done, and T044 is also Done for the final release gate. No additional platform-lane implementation should start unless a later qualification run exposes a concrete regression or a new explicitly scoped dependency.

## Validation policy

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

During active development, keep code-changing PRs Draft and run normal CI plus only path-relevant dedicated checks. Once production source/tests/build/workflows are frozen, obtain green normal CI and the mandatory `CODE_REVIEW.md` review, then transition Draft -> Ready to launch heavyweight final-candidate T042/T052 qualification. A later executable/build/workflow change invalidates that candidate. Pure project-state completion documentation does not invalidate an otherwise green frozen executable candidate.

## Next actions

1. Platform prerequisite work is complete; do not open replacement platform streams unless a concrete regression requires one.
2. T068 remains governed only by its explicit issue #80 dependencies and can proceed independently of the now-complete T064 service lane.
3. Continue T038 -> T039 and T040 in the styling lane as capacity permits; T069/T070/T071 remain dependency-gated.
