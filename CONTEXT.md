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

The pre-T066 merge baseline is current `main` at `c6575cf8fe4301086af088642fc93ba21e61f5e5`. It includes completed T072, T062 (plus its post-merge completeness fix), T063, T035, T043, T061, T067, T045, T037, T058, T036, T065, T034 and T060 foundations. This completion cycle adds T066 / PR #237 without changing the already-qualified T066 executable candidate.

T072 / issue #84 / PR #185 is complete and merged. Its final current-main-synchronized executable candidate passed normal CI, T072 Linux D-Bus Contract, T060 Application Contract, T065 Platform Dispatcher, T067 Virtual List Contract, T042 Lifecycle Stress and T052 v0.1 Release Gate, with final `CODE_REVIEW.md` re-certification reporting no Blocking/Important finding.

T062 / issue #74 / PR #226 is complete and merged as `7269310995adad1e7b474b6cea319644fc8020f4`; post-merge completeness gaps were closed by PR #245 / `86c12e8472a82cd929a95c24705995dfa871bbf5`. Tooltip is no longer a convergence blocker.

T066 / issue #78 / PR #237 is completion-qualified. Frozen executable candidate `d6da7c472d7819269e87f0cdbccf6ac99ebfb174` is synchronized with `main` `c6575cf8fe4301086af088642fc93ba21e61f5e5`; current completion head `9822c79ff02e6cecddd42a2ecf0c0fa129492508` adds only release/document contract synchronization after the executable freeze. Exact-head normal/path-scoped validation is green (CI `34686088854`, T066 `34686088812`, T060 `34686088852`, T065 `34686088808`, T072 `34686088814`, Package Contracts `34686088902`) and final-candidate T042 Lifecycle Stress `34686364025` plus T052 v0.1 Release Gate `34686364059` are green. Final requirement-to-implementation/test review is recorded in PR reviews `5185850366` and `5186026531`, with no Blocking/Important finding.

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

critical platform: T065(done) -> T072(done) -> T064(active PR #240)
                   T041(done) -> T043(done) -> T066(done, PR #237)
                                      |-------> T068

release:            convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

T064 / issue #76 / PR #240 is the remaining primary platform prerequisite. T044 / issue #44 / PR #145 is an independent T071 release dependency and may be qualified opportunistically when T064 is waiting on external gates.

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
- T062 / PR #226 + PR #245: text-only Tooltip decorator over T061 + T065 with hover/focus eligibility, deterministic dismissal/suppression, semantic help and completed post-merge qualification coverage.
- T063 / PR #227: modal Dialog policy over T061 with deterministic action/focus/scroll/reentrancy semantics and standalone/embedded qualification.
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

## Active platform work

- **T064 / PR #240:** implementation scope is frozen across macOS/Windows/Linux. Exact-head normal/T060/T065/T072/package gates are green on the pre-smoke-recovery candidate. A concrete Windows native-dialog smoke timeout was isolated to the qualification fixture; exact head `0ce66349d84483c22ca4d9b1c7f98c47a8b76e6e` now uses a real owner HWND, owner-aware modal discovery and message pumping while preserving the four required chooser variants and HTTP(S) checks. New exact-head validation is running; keep Draft until it is green and the final completeness review is re-certified.
- **T044 / PR #145:** tertiary release qualification. The branch is synchronized with current main at `608de6c2fce6f960b92c6534699b599fbc318863`; the T044 capture implementation tree is unchanged by that synchronization. Exact-head normal/path-scoped requalification is running before a new Ready transition and heavyweight final-candidate gates.

## Validation policy

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

During active development, keep code-changing PRs Draft and run normal CI plus only path-relevant dedicated checks. Once production source/tests/build/workflows are frozen, obtain green normal CI and the mandatory `CODE_REVIEW.md` review, then transition Draft -> Ready to launch heavyweight final-candidate T042/T052 qualification. A later executable/build/workflow change invalidates that candidate. Pure project-state completion documentation does not invalidate an otherwise green frozen executable candidate.

## Next actions

1. Merge T066 / PR #237 after this same-cycle project-state synchronization; its normal/path and T042/T052 final-candidate gates are green with a complete review matrix.
2. Finish T064 / PR #240: validate the owner-aware Windows native smoke, re-run the final completeness/CODE_REVIEW pass, then Ready -> T042/T052 -> merge.
3. Re-qualify T044 / PR #145 on current main, then Ready -> T042/T052 and merge when its already-recorded Outcome A/B matrix is re-certified on the exact head.
4. T068 remains dependency-gated until every explicit issue #80 dependency is Done; T069/T070/T071 remain dependency-gated.
