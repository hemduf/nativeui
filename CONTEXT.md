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

The previously completed v1 feature/platform/styling foundation remains merged and qualified. The pre-freeze audit performed on the September 14 main baseline found correctness/lifetime/failure-path gaps that must be fixed **before** T069 can freeze the public API.

Current safety blocker set:

- **T123 / #281 — Doing:** deterministic lifetime-safe `State<T>` observer/reentrancy/observer-exception semantics.
- **T124 / #282 — Blocked on T123:** stable v1 `Binding<T>` and migration of retained State consumers.
- **T125 / #286 — Ready:** retained dispatch/reconciliation/cancellation exception safety, including guard restoration and dynamic dirty-work/enqueue preservation.
- **T126 / #287 — Ready:** DesktopServices completion/native exception boundaries.
- **T127 / #288 — Blocked on T123:** `ScrollState` lifetime, retained consumer ownership, reentrant and throwing observer semantics.
- **T128 / #289 — Ready:** Dispatcher accepted-work recovery and Animation scheduler callback-exception invariants.
- **T129 / #290 — Ready:** lifetime-safe retained invalidation callbacks after Node/Tree removal.
- **T130 / #291 — Ready:** Component lifecycle/layout/paint exception safety plus native partial-construction and no-throw teardown.
- **T131 / #293 — Ready:** transaction-safe Overlay/Dialog show/close/destructor behavior.
- **T132 / #294 — Ready:** failure-safe deferred standalone close scheduling under Dispatcher rejection/throw.

T069 / issue #81 is **Blocked** on T123–T132. It must not become Ready or freeze any affected API family while one of those blockers is non-Done.

The immediate v1 frontier is therefore:

```text
T123(doing) -> T124(blocked)
      \------> T127(blocked)

parallel P0: T125 + T126 + T128 + T129 + T130 + T131 + T132

T123..T132 all Done
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

- guard/counter flags restored only on normal return (`dispatch_depth_`, availability/dynamic/capture cancellation state);
- observer/callback engines without deterministic throwing-callback semantics;
- accepted Dispatcher snapshot work that can disappear when an earlier callback throws;
- animation work that can remain active with no future wake after callback failure;
- retained invalidators capturing raw `Tree*`/`Node&` beyond node/tree lifetime;
- lifecycle/layout/paint callbacks that can throw through destructor paths or leave partial state published;
- native ViewCore resources acquired before user-controlled initial layout without complete constructor-unwind ownership;
- Overlay/Dialog logical state published/cleared before fallible retained invalidation/close work is durably recoverable;
- standalone close deferral falling back to synchronous native teardown when ordinary Dispatcher enqueue fails;
- top-level self-destruction claims where later caller frames still access the destroyed owner.

`CODE_REVIEW.md` has been strengthened so these patterns are now mandatory review/fault-test domains rather than one-off discoveries.

## Completed foundations relevant to v1

- T030–T036: standard widget baseline through ListView/Tabs.
- T037 / PR #151: typed per-UI Theme values and representative theme binding.
- T038 / PR #218: typed widget visual-state/style resolution and invalidation contract, merged as `8d81a0803a9c7f9b191d1fd4232d975adb39bf36`.
- T039 / PR #260: typed retained lexical StyleScope inheritance with structural removal/restoration and dynamic ancestry evidence, merged as `1b998491306ae3fff9771339bedca7e14007f355`.
- T040 / PR #259: deterministic per-context tween/spring animation layer over T065, merged as `df82860fd141a37140c67dc96e1326dbf9d87403`; T128 now hardens its exceptional callback paths.
- T043 / PR #142: logical/native resize and scale negotiation.
- T044 / PR #145: evidence-gated native pointer-capture qualification.
- T045 / PR #210: backend-neutral accessibility semantic architecture and virtual collection contract.
- T047/T048: relocatable package and external-consumer qualification.
- T049 / PR #258: single-window public component gallery, merged as `24d5b2265360917a37e1ab7d5846a0348b305485`.
- T050 / PR #238: opt-in per-UI retained-tree debug inspector.
- T051/T052: performance regression policy and v0.1 qualification gate.
- T053: consumer-scoped macOS Objective-C runtime identity.
- T054/T056/T057: application/package/resource helpers.
- T058 / PR #154: bounded retained dynamic composition; T125 now hardens exceptional reconciliation/enqueue paths.
- T060: explicit Application ownership and multi-window lifecycle.
- T061 / PR #216: generic retained overlay/portal stack; T131 now hardens transactional failure behavior.
- T062 / PR #226 + PR #245: Tooltip contract and post-merge completeness closure; T131 audits failure-state coherence.
- T063 / PR #227: modal Dialog policy over T061; T131 hardens close/destructor transactions.
- T064 / PR #240: bounded cross-platform DesktopServices using T072 on Linux; T126 hardens exceptional completion boundaries.
- T065 / PR #133: bounded UI-thread Dispatcher/timers; T128 hardens callback failure and T132 lifecycle-control enqueue failure.
- T066 / PR #237: standalone window controls and deterministic close lifecycle; T132 hardens deferral under enqueue rejection/throw.
- T067 / PR #219 + PR #233: fixed-height virtualized ListView with bounded 100k materialization behavior; T127 audits ScrollState lifetime.
- T072 / PR #185: sole v1 Linux `libdbus-1` transport used by T064 and reserved for future T068 1.2 integration.

## Previously qualified feature baselines

T038 / issue #38 / PR #218 is complete and squash-merged as `8d81a0803a9c7f9b191d1fd4232d975adb39bf36`. Frozen executable head `401ff95808b96636b3edb644ef8dd5902f0db749` passed normal CI plus T044/T066/T067 path-scoped contracts, then final-candidate T042 Lifecycle Stress `34758523505` and T052 v0.1 Release Gate `34758523484`.

T039 / issue #39 / PR #260 is complete and squash-merged as `1b998491306ae3fff9771339bedca7e14007f355`. Frozen executable head `3cf0c8e33cb472648cef880ece780dbda6b3fd38` passed normal CI `34778524773`, T066 Window Controls `34778524854`, final-candidate T042 Lifecycle Stress `34779287474` and T052 v0.1 Release Gate `34779287449`.

T049 / issue #49 / PR #258 is complete and squash-merged as `24d5b2265360917a37e1ab7d5846a0348b305485`. Frozen executable head `58bad47cdc7095f261a812cd34fb38e23e2c5af2` passed normal CI `34765491619`, T066 `34765491618`, final-candidate T042 Lifecycle Stress `34767252432` and T052 v0.1 Release Gate `34767252389`.

T040 / issue #40 / PR #259 is complete and squash-merged as `df82860fd141a37140c67dc96e1326dbf9d87403`. Frozen executable head `9bf6b9a49bf68ef4fe2bb38e1a602a0181aea39f` passed normal CI plus T051/T064/T065/T066 path-scoped contracts, then final-candidate T042 Lifecycle Stress `34770340801` and T052 v0.1 Release Gate `34770340828`.

Those successful qualifications remain evidence for their frozen candidates, but do not waive newly discovered v1 safety blockers on the current composed system.

## Validation policy

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

During active development, keep code-changing PRs Draft and run normal CI plus only path-relevant dedicated checks. Once production source/tests/build/workflows are frozen, obtain green normal CI and the mandatory `CODE_REVIEW.md` review, then transition Draft -> Ready to launch heavyweight final-candidate T042/T052 qualification. A later executable/build/workflow change invalidates that candidate. Pure project-state completion documentation does not invalidate an otherwise green frozen executable candidate.

For T123–T132, fault injection is mandatory where normal execution cannot deterministically reproduce the failure domain. Relevant tests include throwing callbacks, queue rejection/throw, allocation/enqueue failure, stale retained callbacks, partial native construction, destructor-time callback failure, failed layout recovery and failed paint recovery.

## Automation / integration recovery

GitHub live state is authoritative. `#250` is Scheduler-only control state; current `AUTOMATION CYCLE — GNNN` issue comments are the Worker → Scheduler event bus. Delivery W1/W2/W3 are interchangeable with at most three source-changing lanes. Integration owns independent final review, exact-head CI diagnosis, qualification, merge and completion/unlock bookkeeping; it never implements product features. Reporter is read-only.

Scheduler/Delivery must treat the explicit Dependencies in T069/#81 as hard gates. T069 must not be claimed merely because the old feature prerequisite lane is complete.

Optimal parallelization before the freeze:

1. keep T123 active; T124 and T127 wait for its observer/control-block semantics;
2. use available independent lanes for T125/T126/T128/T129/T130/T131/T132 according to conflict/PR availability;
3. after T123 merges, start/unblock T124 and T127 immediately;
4. after all T123–T132 are Done and synchronized on main, resume T069 whole-surface freeze;
5. then proceed T070 -> T071 on the exact frozen/RC baselines.

## Next actions

1. Finish T123 with the newly explicit throwing-observer contract; unblock T124/T127 only after that contract is frozen and tested.
2. Execute Ready independent P0 blockers T125, T126, T128, T129, T130, T131 and T132 in parallel where branch overlap allows it.
3. Require every blocker PR to use the strengthened `CODE_REVIEW.md` record: transactional state, scheduling/queue failure, exception/unwind, partial construction, lifetime/reentrancy, performance/allocation, privacy and exact fault tests.
4. Keep T069/#81 Blocked until T123–T132 are genuinely Done.
5. Keep T068/PR #241 parked for NativeUI 1.2.
6. After the safety blocker set closes, synchronize T069 with current main and perform the final v1 public API freeze audit before T070/T071.
