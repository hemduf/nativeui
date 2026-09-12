# NativeUI compact recovery context

**Updated:** 2026-09-11

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

Current `main` is `07690842bab94f2d975685d28e0d2b0c1c0fc9d4` and includes T072 / PR #185 in addition to the completed T063, T035, T043, T061, T067, T045, T037, T058, T036, T065, T034 and T060 foundations.

T063 is complete. Its executable candidate `e907bd3119f59116522b1e672e690703c845f1e6` passed normal CI, and completion head `625bf6f13774652159781f39770e9c82ea3e2057` passed T042 Lifecycle Stress and T052 v0.1 Release Gate. Final `CODE_REVIEW.md` review reported no Blocking/Important finding.

T072 / issue #84 / PR #185 is merged on `main` as `07690842bab94f2d975685d28e0d2b0c1c0fc9d4`; T064 is now Ready. The exact current-main-synchronized candidate `b5658be1cc4af872642811790bd5821149a885f5` passed normal CI `34631977571`, T072 Linux D-Bus Contract `34631977574`, T060 Application Contract `34631977534`, T065 Platform Dispatcher `34631977512`, T067 Virtual List Contract `34631977511`, T042 Lifecycle Stress `34637714563` and T052 v0.1 Release Gate `34637714484`, with `CODE_REVIEW.md` re-certification `5182564487` reporting no Blocking/Important finding.

T062 / issue #74 / PR #226 is implementation-complete on `feat/t062-tooltip`, synchronized with `main` and validated locally: targeted/full Release CTest (106/106), full ASan/UBSan CTest (106/106), deterministic example self-test and real standalone + EmbeddedView platform smoke all pass. A platform gap discovered during that smoke was corrected in the same branch: `ViewCore` receives the private `StandaloneWindow::Impl`/`EmbeddedView::Impl` as its `PlatformServices`, so those impls now implement `DispatcherProvider` and expose the per-view T065 dispatcher to retained policies. Exact-head normal/path-scoped CI and the Ready-only T042/T052 candidate gates remain the last merge steps.

Current convergence:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(done) -> T068
                                                   T058(done) ------------^

dynamic/overlay:   T058(done) -> T061(done)
                                      |-> T035(done) --------------------> T068
                                      |-> T063(done) --------------------> T068
                                      +-> T062(complete, PR #226)

style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

critical platform: T065(done) -> T072(merged) -> T064
                   T041(done) -> T043(done) -> T066
                                      |-------> T068

release:            convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

T064 is now Ready on merged T072. T066 is independently unblocked by completed T043 and is active in PR #237; do not serialize it behind T072. T044 / issue #44 / PR #145 is an independent T071 release dependency and may be qualified opportunistically when the two primary platform streams are waiting on external gates.

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
- T045 / PR #210: accessibility semantic architecture and immutable virtual collection contract.
- T047 / PR #92 + T048 / PR #99: relocatable package and external-consumer qualification.
- T051 / PR #116 + T052 / PR #120: reproducible performance policy and v0.1 developer-preview release gate.
- T054 / PR #119, T056 / PR #111, T057 / PR #126: application/package/resource helpers.
- T058 / PR #154: bounded retained dynamic composition with one per-tree structural reconciliation queue.
- T060 / PR #118 + #139 / PR #140: explicit Application ownership and stress-qualified multi-window lifecycle.
- T061 / PR #216: generic per-UI overlay/portal layer with deterministic placement, modal focus/capture semantics, anchor tracking and retained reconciliation through T058.
- T062 / PR #226: text-only Tooltip decorator over T061 + T065 with hover/focus eligibility, deterministic dismissal/suppression and semantic help independent of the rendered overlay.
- T063 / PR #227: modal Dialog policy over T061 with deterministic action/focus/scroll/reentrancy semantics and standalone/embedded qualification.
- T065 / PR #133: bounded UI-thread Dispatcher/timer service with native wake integration.
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

The final requirement -> implementation -> test matrix and `CODE_REVIEW.md` audit are recorded in PR #185 reviews `5181043847` and `5182564487`.

## T062 delivered contract

PR #226 / issue #74 implements a text-only retained Tooltip decorator with no second timer, popup manager or placement algorithm:

- `ui::Tooltip{"Reset to default", child}.delay(std::chrono::milliseconds{500})`, plain owned UTF-8 text only, default delay exactly 500 ms, zero delay deferred to the next T065 checkpoint and never reentrant, default maximum text width 320 logical px with UTF-8-safe word wrapping;
- one `TooltipController` per decorated instance owning hover/focus eligibility, one T065 timer token and the visible flag; no warm-up, no `currentTooltip` and no cross-anchor delay reuse;
- hover and keyboard focus share the same delay; PointerMove inside the anchor does not restart it; leave/re-enter and focus loss/regain start a fresh full delay; moving A→B never reuses A's elapsed time; two UIs share no timing/state;
- relocation is a T061 `NonModal`/`Auto` overlay with `OverlayPointerPolicy::Ignore`, so the surface is non-focusable, non-hit-testable and never captures pointer/focus; T061 alone owns placement/clamping;
- dismissal on pointer leave/focus loss, any PointerDown, Escape, Hidden/Collapsed/Disabled, subtree removal, view deactivation and modal/overlay opening; PointerDown suppression persists until a new false→true eligibility transition, including a hover-only drag guard driven by the per-tree pointer-interaction flag;
- semantic help is published through the T045 `Component::semantics()` seam on the role-`None` decorator and remains available while no overlay is rendered; empty text contributes no description and never erases a child-supplied description;
- generic supporting seams: `MountContext::overlay_service()`, per-tree `TransientPresentation` dismissal, per-tree pointer-interaction state, `Tree::component_semantics()`, `UI::overlay_entries()`, and `DispatcherProvider` on the native window `Impl` objects actually handed to `ViewCore`;
- deterministic `tests/t062_tooltip_tests.cpp` coverage and `examples/features/t062_tooltip.cpp` with hover/focus demo, deterministic `--self-test` and real standalone + EmbeddedView `--platform-smoke` wired into Linux/macOS normal CI.

## Active platform work

- **T062 / PR #226:** implementation, local Release/ASan full suites, deterministic example self-test and native standalone/EmbeddedView platform smoke are green and synchronized with `main`. Remaining: exact-head normal/path-scoped CI, mandatory review record, then Draft -> Ready for the T042/T052 final-candidate gates and merge.
- **T064 / issue #76:** Ready after the T072 merge; start without waiting for T062.
- **T066 / PR #237:** independently active. Its dedicated T066 platform contract and T060/T065 regressions are green on the current branch; a normal Windows CI rerun is qualifying a previously observed T060 destroy-A/resize-surviving-B failure before any production correction is accepted.
- **T044 / PR #145:** tertiary release qualification. Dedicated workflow path filtering was narrowed to the actual capture/platform surfaces; pinned Pugl X11 focus delivery behavior is explicitly documented as the reason for the per-view focus proxy used by the focus-loss capture fixture.

## Validation policy

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

During active development, keep code-changing PRs Draft and run normal CI plus only path-relevant dedicated checks. Once production source/tests/build/workflows are frozen, obtain green normal CI and the mandatory `CODE_REVIEW.md` review, then transition Draft -> Ready to launch heavyweight final-candidate T042/T052 qualification. A later executable/build/workflow change invalidates that candidate. Pure project-state completion documentation does not invalidate an otherwise green frozen executable candidate.

## Next actions

1. Request exact-head normal CI plus the macOS/Linux T062 platform smoke for PR #226; record the mandatory review and transition Draft -> Ready for T042/T052 qualification, then merge T062.
2. Start T064 / issue #76 on merged T072 and keep T066 moving independently.
3. Keep T068 blocked until every explicit issue #80 dependency is Done (T062 is the last dynamic/overlay dependency and reaches Done with PR #226).
4. Continue T044 qualification only while T072/T066 are externally waiting and without exceeding the platform-lane concurrency budget.
5. T069/T070/T071 remain dependency-gated.
