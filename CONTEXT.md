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

**T126 / #287 is Done.** PR #295 merged exact source head `4ae5dae65f86cc34bd3177f91ad3e6db7403b02d` as merge commit `be216d7a41999113472a0457827d43caad28cf15`. Exact-head CI `34844330034`, T064 `34844329931`, Package Contracts `34844329951`, T066 `34844330127`, T042 Lifecycle Stress `34846694493` and T052 v0.1 Release Gate `34846694353` all passed. Independent Integration review `5197980748` recorded zero Blocking/Important findings. DesktopServices completion is now terminal before fallible UI marshalling; AppKit/native/worker completion boundaries contain C++ exceptions, retained panel cleanup is deterministic, and queue rejection/allocation failure cannot strand request capacity or fall back to application code on a backend thread.

**T128 / #289 is Done.** PR #296 merged exact source head `ce58df7758e960c329f26394bad44ce4c7b4efc1` as merge commit `ca65087b503aff04133394c5ef200cd7cae75e12`. Exact-head CI `34848275511`, T051 `34848275318`, T064 `34848275472`, T065 Dispatcher `34848275470`, T065 Platform `34848275371`, T066 `34848275349`, T042 Lifecycle Stress `34853025047` and T052 v0.1 Release Gate `34853023976` all passed. Independent Integration review `5198631295` recorded zero Blocking/Important findings. Accepted Dispatcher work now survives neighboring callback failure without retrying the begun callback or allocating a recovery queue; animation failure terminalizes the failing entry while preserving sibling scheduling; and the native event-loop exception boundary is exercised on Linux, Windows and macOS.

**T138 / #303 is Done.** PR #343 merged the current-main-synchronized exact source head `268c2e4c00307e894ba02f56304174225ef49b50` as merge commit `1f63d9a4778773102f162618ad5ffd34f2bc6497`. Exact-head CI `34854738551`, Package Contracts `34854738531` and T066 `34854738635` passed. Final-candidate T042 Lifecycle Stress `34856470278` and T052 v0.1 Release Gate `34856470266` also passed after transient external dependency-download failures were classified as infrastructure and recovered with targeted reruns. Independent current-main Integration review `5198976168` recorded zero Blocking/Important findings. The core public `Binding<T>` value/lifetime contract is merged.

**T139 / #304 is Done.** PR #350 merged exact frozen head `f8e9729f5d967c8168335ae63e9800a0bb0b6ef4` into main as merge commit `61950a9f40145368dba709e755ee8f0c06ae411b`. Exact-head normal CI `34895438606`, Package Contracts `34895438615` and T066 Window Controls `34895438808` passed, then final-candidate T042 Lifecycle Stress `34901397463` and T052 v0.1 Release Gate `34901397478` passed. The mandatory second peer review `5203042503` on the exact review-fix head recorded `REVIEW_PASS`, zero Blocking/Important findings, and reviewer independence from current-head author W4. Representative Knob/Toggle/TextInput entry points now accept `Binding<T>` directly while preserving legacy `State<T>&` source compatibility; tests cover exactly-once mutation, targeted external invalidation, public/installed consumer compilation, and the mandatory T139 feature example. T140 is the verified immediate dependent and is now Ready.

Current safety blocker set/frontier:

- **T124 / #282 — Blocked umbrella/integration parent:** decomposed into T138 -> T139 -> T140 -> T141; T124 becomes Done only after all four children and final parent closeout. T138 and T139 are complete and checked in the parent.
- **T138 / #303 — Done:** core public `Binding<T>` value/lifetime contract merged and fully qualified.
- **T139 / #304 — Done:** Binding-backed representative widget entry points, legacy `State<T>&` compatibility, exact invalidation and feature-example coverage merged and fully qualified.
- **T140 / #305 — Ready:** standard stateful widget-family migration to Binding internals; dependency T139 is now satisfied.
- **T141 / #306 — Blocked on T140:** dynamic composition/style/focus migration plus integrated T124 qualification.
- **T125 / #286:** retained dispatch/reconciliation/cancellation exception safety.
- **T126 / #287 — Done:** DesktopServices completion/native exception boundaries.
- **T127 / #288 — Ready:** `ScrollState` lifetime, retained-consumer ownership, reentrant and throwing-observer semantics, now unblocked by T123.
- **T128 / #289 — Done:** Dispatcher accepted-work recovery, Animation callback-exception invariants and cross-platform foreign-boundary exception smoke.
- **T129 / #290:** lifetime-safe retained invalidation callbacks after Node/Tree removal; current review-fix head requires exact-head CI and an independent second peer review before Integration qualification.
- **T130 / #291:** Component lifecycle/layout/paint exception safety plus native partial-construction and no-throw teardown.
- **T131 / #293:** transaction-safe Overlay/Dialog show/close/destructor behavior; current source head remains product-red until the Dialog repair-state failure is corrected and requalified.
- **T132 / #294 — SOURCE_READY:** current exact-head normal/path matrix is green and source closeout reports complete acceptance/self-review; mandatory independent peer review is still required before any Ready/final-gate/merge transition.

T069 / issue #81 remains **Blocked** until the complete pre-freeze blocker set, including the T124 child chain and T127, is Done and synchronized on main. T126, T128, T138 and T139 are now complete independent/chain prerequisites, but T069 remains gated by the remaining AND prerequisites.

The immediate v1 frontier is therefore:

```text
T123(done) -> T138(done) -> T139(done) -> T140(ready) -> T141(blocked)
                                                          |
                                                          v
                                                     T124 closeout

T123(done) ------------------------------------------------------------> T127(ready)

T126(done)
T128(done)

parallel remaining P0: T125 + T129 + T130 + T131 + T132(source-ready/review pending)

T124 + T125 + T127 + T129 + T130 + T131 + T132 all Done
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
- T040 / PR #259: deterministic per-context tween/spring animation layer over T065, merged as `df82860fd141a37140c67dc96e1326dbf9d87403`; T128 now hardens exceptional callback paths.
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
- T064 / PR #240: bounded cross-platform DesktopServices using T072 on Linux.
- T065 / PR #133: bounded UI-thread Dispatcher/timers; T128 hardens callback failure and T132 lifecycle-control enqueue failure.
- T066 / PR #237: standalone window controls and deterministic close lifecycle.
- T067 / PR #219 + PR #233: fixed-height virtualized ListView; T127 now hardens ScrollState lifetime/reentrancy.
- T072 / PR #185: sole v1 Linux `libdbus-1` transport used by T064 and reserved for future T068 1.2 integration.
- T123 / PR #284: deterministic lifetime-safe `State<T>` notification, observer-exception and reentrancy contract; merged as `839a7b082f94e0bef3b688cc7bcc2074e6cbfb99` after complete normal/path/final qualification.
- T126 / PR #295: DesktopServices completion exception-boundary, terminal-drop and retained native cleanup hardening; merged as `be216d7a41999113472a0457827d43caad28cf15` after complete normal/path/final qualification.
- T128 / PR #296: Dispatcher/Animation exceptional recovery and cross-platform foreign-boundary containment hardening; merged as `ca65087b503aff04133394c5ef200cd7cae75e12` after complete normal/path/final qualification.
- T138 / PR #343: core public `Binding<T>` value/lifetime contract; merged as `1f63d9a4778773102f162618ad5ffd34f2bc6497` after current-main normal/path/final qualification and clean Integration review.
- T139 / PR #350: representative Binding-backed Knob/Toggle/TextInput entry points plus legacy State compatibility and exact invalidation coverage; merged as `61950a9f40145368dba709e755ee8f0c06ae411b` after exact-head normal/path/final qualification and independent second peer review.

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

GitHub live state is authoritative. `#250` is Scheduler-only control state; current `AUTOMATION CYCLE — GNNN` issue comments are the Worker -> Scheduler event bus. Delivery W1/W2/W3/W4 are interchangeable with at most four source-changing lanes under the current scheduler capacity override. Integration owns independent final review, exact-head CI diagnosis, qualification, merge and completion/unlock bookkeeping; it never implements product features. Reporter is read-only.

Scheduler/Delivery must treat explicit dependencies as hard gates. T124 is an umbrella after decomposition; do not assign a monolithic T124 implementation. Execute `T138 -> T139 -> T140 -> T141`, then close T124. T069 must remain blocked until the whole pre-freeze safety frontier is Done.

Optimal parallelization before the freeze:

1. advance T140 now that T139 is live-verified Done; continue the T124 child chain strictly one dependency at a time;
2. run T127 when a source lane is available and continue independent T125/T129/T130/T131/T132 closeout according to conflict/PR availability and peer-review gates;
3. send T132 through mandatory independent peer review before any Integration Ready/final qualification transition;
4. complete T129 exact-head CI after its reviewer fix, then require an independent second peer reviewer who did not author the fix head;
5. after T124 and all remaining pre-freeze blockers are Done and synchronized on main, resume T069 whole-surface freeze;
6. then proceed T070 -> T071 on the exact frozen/RC baselines.

## Next actions

1. Scheduler may claim/execute **T140 / #305** now; T139 is Done and its sole dependency is satisfied.
2. Assign an independent peer reviewer to **T132 / #294** exact SOURCE_READY head before any Integration qualification.
3. Finish exact-head CI and independent second-peer review for **T129 / #290** after the W3 review fix.
4. Continue source correction/closeout for T131 and independent T125/T127/T130 work within capacity/conflict limits.
5. Keep T069/#81 Blocked until the full pre-freeze safety blocker frontier is genuinely Done.
6. Keep T068/PR #241 parked for NativeUI 1.2.
