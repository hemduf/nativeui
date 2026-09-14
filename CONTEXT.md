# NativeUI compact recovery context

**Updated:** 2026-09-14

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
- ordinary C++ callbacks may propagate exceptions only after NativeUI invariants are restored; foreign/native ABI callbacks contain C++ exceptions;
- destructor-driven retained/native teardown is no-throw and best-effort complete;
- retained invalidators intentionally allowed to outlive a component must remain safe after node and whole-UI destruction;
- accepted/deferred work must not be silently lost, duplicated or converted to unsafe synchronous execution by callback/enqueue failure;
- state machines that cross fallible work have explicit prepare/commit/recovery boundaries;
- partial native construction must leave no callback/native resource registered after failure;
- top-level `UI`/window/view destruction from an active callback is deferred unless the complete caller chain explicitly proves self-destruction safety;
- `CODE_REVIEW.md`, `CI_POLICY.md`, a complete issue-to-code/test evidence matrix, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline and critical path

The previously completed v1 feature/platform/styling foundation remains merged and qualified. The September 14 pre-freeze audit found correctness/lifetime/failure-path gaps that must be closed before T069 can freeze the public API.

**T123 / #281 is Done.** PR #284 merged exact source head `f2c5cada877df82d8936f9dd545d796c0ab5156e` into main as merge commit `839a7b082f94e0bef3b688cc7bcc2074e6cbfb99`. The frozen candidate passed normal CI `34837857267`, T066 `34837857292`, T042 Lifecycle Stress `34841171641` and T052 v0.1 Release Gate `34841171493`. Independent final `CODE_REVIEW.md` review `5197372173` recorded zero Blocking/Important findings. The deterministic `State<T>` lifetime, reentrancy, pending-write and throwing-observer policy is now the baseline for dependent state work.

Current safety blocker set/frontier:

- **T124 / #282 — Blocked umbrella/integration parent:** decomposed into T138 -> T139 -> T140 -> T141; T124 becomes Done only after all four children and final parent closeout.
- **T138 / #303 — Ready:** core public `Binding<T>` value/lifetime contract, now unblocked by T123.
- **T139 / #304 — Blocked on T138:** Binding-backed widget entry points and legacy `State<T>&` compatibility.
- **T140 / #305 — Blocked on T139:** migrate standard stateful widget families to Binding internals.
- **T141 / #306 — Blocked on T140:** dynamic composition/style/focus migration plus integrated T124 qualification.
- **T125 / #286:** retained dispatch/reconciliation/cancellation exception safety.
- **T126 / #287:** DesktopServices completion/native exception boundaries; current qualification still requires product-side closeout before merge.
- **T127 / #288 — Ready:** `ScrollState` lifetime, retained-consumer ownership, reentrant and throwing-observer semantics, now unblocked by T123.
- **T128 / #289:** Dispatcher accepted-work recovery and Animation scheduler callback-exception invariants; current candidate still requires the explicit cross-platform exceptional native-boundary smoke required by the ticket.
- **T129 / #290:** lifetime-safe retained invalidation callbacks after Node/Tree removal.
- **T130 / #291:** Component lifecycle/layout/paint exception safety plus native partial-construction and no-throw teardown.
- **T131 / #293:** transaction-safe Overlay/Dialog show/close/destructor behavior.
- **T132 / #294:** failure-safe deferred standalone close scheduling under Dispatcher rejection/throw.

T069 / issue #81 remains **Blocked** until the complete pre-freeze blocker set, including the T124 child chain and T127, is Done and synchronized on main.

The immediate v1 frontier is therefore:

```text
T123(done) -> T138(ready) -> T139(blocked) -> T140(blocked) -> T141(blocked)
                                                        |
                                                        v
                                                   T124 closeout

T123(done) -------------------------------------------> T127(ready)

parallel P0: T125 + T126 + T128 + T129 + T130 + T131 + T132

T124 + T125..T132 all Done
              |
              v
            T069 -> T070 -> T071 -> v1.0.0

T049(done) -------------------------------> T071
T044(done) -------------------------------> T071
T068 -------------------------------------> 1.2 only
```

T068 / issue #80 / PR #241 remains explicitly deferred to NativeUI 1.2. It does **not** block T069, T070, T071 or the NativeUI 1.0 release.

## Why the pre-freeze blockers exist

The audit found recurring classes of bugs that normal happy-path CI did not expose:

- guard/counter flags restored only on normal return;
- observer/callback engines without deterministic throwing-callback semantics;
- accepted Dispatcher snapshot work that can disappear when an earlier callback throws;
- animation work that can remain active with no future wake after callback failure;
- retained invalidators capturing raw `Tree*`/`Node&` beyond node/tree lifetime;
- lifecycle/layout/paint callbacks that can throw through destructor paths or leave partial state published;
- native resources acquired before user-controlled initialization without complete constructor-unwind ownership;
- Overlay/Dialog logical state published/cleared before fallible retained invalidation/close work is durably recoverable;
- standalone close deferral falling back to synchronous native teardown when Dispatcher enqueue fails;
- top-level self-destruction claims where later caller frames still access the destroyed owner.

`CODE_REVIEW.md` makes these mandatory review/fault-test domains.

## Completed foundations relevant to v1

- T030-T036: standard widget baseline through ListView/Tabs.
- T037 / PR #151: typed per-UI Theme values and representative theme binding.
- T038 / PR #218: typed widget visual-state/style resolution and invalidation contract, merged as `8d81a0803a9c7f9b191d1fd4232d975adb39bf36`.
- T039 / PR #260: typed retained lexical StyleScope inheritance, merged as `1b998491306ae3fff9771339bedca7e14007f355`.
- T040 / PR #259: deterministic per-context tween/spring animation layer over T065, merged as `df82860fd141a37140c67dc96e1326dbf9d87403`; T128 hardens exceptional callback paths.
- T043 / PR #142: logical/native resize and scale negotiation.
- T044 / PR #145: evidence-gated native pointer-capture qualification.
- T045 / PR #210: backend-neutral accessibility semantic architecture and virtual collection contract.
- T047/T048: relocatable package and external-consumer qualification.
- T049 / PR #258: single-window public component gallery, merged as `24d5b2265360917a37e1ab7d5846a0348b305485`.
- T050 / PR #238: opt-in per-UI retained-tree debug inspector.
- T051/T052: performance regression policy and v0.1 qualification gate.
- T053: consumer-scoped macOS Objective-C runtime identity.
- T054/T056/T057: application/package/resource helpers.
- T058 / PR #154: bounded retained dynamic composition; T125 hardens exceptional reconciliation/enqueue paths.
- T060: explicit Application ownership and multi-window lifecycle.
- T061 / PR #216: generic retained overlay/portal stack; T131 hardens transactional failure behavior.
- T062 / PR #226 + PR #245: Tooltip contract and post-merge completeness closure.
- T063 / PR #227: modal Dialog policy over T061.
- T064 / PR #240: bounded cross-platform DesktopServices using T072 on Linux; T126 hardens exceptional completion boundaries.
- T065 / PR #133: bounded UI-thread Dispatcher/timers; T128 hardens callback failure and T132 lifecycle-control enqueue failure.
- T066 / PR #237: standalone window controls and deterministic close lifecycle.
- T067 / PR #219 + PR #233: fixed-height virtualized ListView; T127 now hardens ScrollState lifetime/reentrancy.
- T072 / PR #185: sole v1 Linux `libdbus-1` transport used by T064 and reserved for future T068 1.2 integration.
- T123 / PR #284: deterministic lifetime-safe `State<T>` notification, observer-exception and reentrancy contract; merged as `839a7b082f94e0bef3b688cc7bcc2074e6cbfb99` after complete normal/path/final qualification.

## Validation policy

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

During active development, keep code-changing PRs Draft and run normal CI plus only path-relevant dedicated checks. Once production source/tests/build/workflows are frozen, obtain green normal/path CI and the mandatory `CODE_REVIEW.md` review, then transition Draft -> Ready to launch heavyweight T042/T052 final-candidate qualification. A later executable/build/workflow change invalidates that candidate. Pure project-state completion documentation does not invalidate an otherwise green frozen executable candidate.

Fault injection is mandatory where normal execution cannot deterministically reproduce the failure domain. Relevant tests include throwing callbacks, queue rejection/throw, allocation/enqueue failure, stale retained callbacks, partial native construction, destructor-time callback failure, failed layout recovery and failed paint recovery.

## Automation / integration recovery

GitHub live state is authoritative. `#250` is Scheduler-only control state; current `AUTOMATION CYCLE — GNNN` issue comments are the Worker -> Scheduler event bus. Delivery W1/W2/W3 are interchangeable with at most three source-changing lanes. Integration owns independent final review, exact-head CI diagnosis, qualification, merge and completion/unlock bookkeeping; it never implements product features. Reporter is read-only.

Scheduler/Delivery must treat explicit dependencies as hard gates. T124 is an umbrella after decomposition; do not assign a monolithic T124 implementation. Execute `T138 -> T139 -> T140 -> T141`, then close T124. T069 must remain blocked until the whole pre-freeze safety frontier is Done.

Optimal parallelization before the freeze:

1. start T138 and T127 from the now-merged T123 baseline when a compatible source lane is available;
2. continue independent T125/T126/T128/T129/T130/T131/T132 closeout according to conflict/PR availability;
3. advance the T124 child chain sequentially after each exact dependency merges;
4. after T124 and all remaining pre-freeze blockers are Done and synchronized on main, resume T069 whole-surface freeze;
5. then proceed T070 -> T071 on the exact frozen/RC baselines.

## Next actions

1. Schedule **T138 / #303** and **T127 / #288** as the two verified immediate unlocks from T123.
2. Close current product-side qualification/rework gaps on T126 and T128 before any Ready/final-gate transition for those PRs.
3. Continue independent P0 safety blockers in parallel where source overlap allows it.
4. Require every blocker PR to record transactional state, scheduling/queue failure, exception/unwind, partial construction, lifetime/reentrancy, performance/allocation, privacy and exact fault-test evidence where applicable.
5. Keep T069/#81 Blocked until the full pre-freeze safety blocker frontier is genuinely Done.
6. Keep T068/PR #241 parked for NativeUI 1.2.