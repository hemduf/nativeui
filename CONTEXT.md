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

Current `main` is `1ddb6b89d9c1f49f4baff1a73cd21e69bfcae038` and includes T063 / PR #227 in addition to the completed T035, T043, T061, T067, T045, T037, T058, T036, T065, T034 and T060 foundations.

T063 is complete. Its executable candidate `e907bd3119f59116522b1e672e690703c845f1e6` passed normal CI, and completion head `625bf6f13774652159781f39770e9c82ea3e2057` passed T042 Lifecycle Stress and T052 v0.1 Release Gate. Final `CODE_REVIEW.md` review reported no Blocking/Important finding.

T072 / issue #84 / PR #185 is completion-ready on exact current-main-synchronized head `b5658be1cc4af872642811790bd5821149a885f5`. The current head is based directly on `main` `1ddb6b89d9c1f49f4baff1a73cd21e69bfcae038`; it passed normal CI `34631977571`, T072 Linux D-Bus Contract `34631977574`, T060 Application Contract `34631977534`, T065 Platform Dispatcher `34631977512`, T067 Virtual List Contract `34631977511`, T042 Lifecycle Stress `34637714563` and T052 v0.1 Release Gate `34637714484`. Exact-head `CODE_REVIEW.md` re-certification `5182564487` reports no Blocking/Important finding and confirms the issue #84 completeness matrix remains satisfied after current-main synchronization.

Current convergence:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(done) -> T068
                                                   T058(done) ------------^

dynamic/overlay:   T058(done) -> T061(done)
                                      |-> T035(done) --------------------> T068
                                      |-> T063(done) --------------------> T068
                                      +-> T062

style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

critical platform: T065(done) -> T072(completion) -> T064
                   T041(done) -> T043(done) -> T066
                                      |-------> T068

release:            convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

T064 remains hard-blocked until T072 is merged/Done. T066 is independently unblocked by completed T043 and is active in PR #237; do not serialize it behind T072. T044 / issue #44 / PR #145 is an independent T071 release dependency and may be qualified opportunistically when the two primary platform streams are waiting on external gates.

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

## Active platform work

- **T072 / PR #185:** all implementation, normal, dedicated and heavyweight final-candidate gates are green on `b5658be`; completion bookkeeping/merge is the only remaining step.
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

1. Merge T072 / PR #185 after completion metadata is synchronized; then mark issue #84 Done and immediately unlock/start T064 / issue #76.
2. Continue T066 / PR #237 independently; treat the exact Windows CI rerun as diagnostic evidence and correct production code only if the failure reproduces.
3. Keep T064 blocked until T072 is genuinely merged/Done.
4. Continue T044 qualification only while T072/T066 are externally waiting and without exceeding the platform-lane concurrency budget.
5. Keep T068 blocked until every explicit issue #80 dependency is Done; T069/T070/T071 remain dependency-gated.
