# NativeUI compact recovery context

**Updated:** 2026-09-16

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

**T173 / #401 is Done.** PR #403 merged frozen exact head `afb577f6556a8ad36524c0d2db0013abd95783be` into main as merge commit `2d692a781537593fe686da19be330c0cdd24c4b6`. Exact-head normal CI, Package Contracts, T060, T065 and T066 passed; final-candidate T042 Lifecycle Stress `35102133059` and T052 v0.1 Release Gate `35102133276` also passed, including the exact-head T051 benchmark. Independent peer review `5223294153` recorded `REVIEW_PASS` with zero Blocking/Important findings. The public keyboard API now exposes every ASCII letter A-Z while preserving all pre-existing `Key` numeric values, existing Primary+A/C/V/X/Y/Z command semantics, the Primary+Q close path, plain/Shift+Q raw delivery, and text/IME separation. The dedicated T173 feature example and exhaustive A-Z mapping/KeyDown/KeyUp coverage are merged.

**T123 / #281 is Done.** PR #284 merged exact source head `f2c5cada877df82d8936f9dd545d796c0ab5156e` into main as merge commit `839a7b082f94e0bef3b688cc7bcc2074e6cbfb99`. The frozen candidate passed normal CI `34837857267`, T066 `34837857292`, T042 Lifecycle Stress `34841171641` and T052 v0.1 Release Gate `34841171493`. Independent final `CODE_REVIEW.md` review `5197372173` recorded zero Blocking/Important findings. The deterministic `State<T>` lifetime, reentrancy, pending-write and throwing-observer policy is now the baseline for dependent state work.

**T126 / #287 is Done.** PR #295 merged exact source head `4ae5dae65f86cc34bd3177f91ad3e6db7403b02d` as merge commit `be216d7a41999113472a0457827d43caad28cf15`. Exact-head CI `34844330034`, T064 `34844329931`, Package Contracts `34844329951`, T066 `34844330127`, T042 Lifecycle Stress `34846694493` and T052 v0.1 Release Gate `34846694353` all passed. Independent Integration review `5197980748` recorded zero Blocking/Important findings. DesktopServices completion is now terminal before fallible UI marshalling; AppKit/native/worker completion boundaries contain C++ exceptions, retained panel cleanup is deterministic, and queue rejection/allocation failure cannot strand request capacity or fall back to application code on a backend thread.

**T128 / #289 is Done.** PR #296 merged exact source head `ce58df7758e960c329f26394bad44ce4c7b4efc1` as merge commit `ca65087b503aff04133394c5ef200cd7cae75e12`. Exact-head CI `34848275511`, T051 `34848275318`, T064 `34848275472`, T065 Dispatcher `34848275470`, T065 Platform `34848275371`, T066 `34848275349`, T042 Lifecycle Stress `34853025047` and T052 v0.1 Release Gate `34853023976` all passed. Independent Integration review `5198631295` recorded zero Blocking/Important findings. Accepted Dispatcher work now survives neighboring callback failure without retrying the begun callback or allocating a recovery queue; animation failure terminalizes the failing entry while preserving sibling scheduling; and the native event-loop exception boundary is exercised on Linux, Windows and macOS.

**T138 / #303 is Done.** PR #343 merged the current-main-synchronized exact source head `268c2e4c00307e894ba02f56304174225ef49b50` as merge commit `1f63d9a4778773102f162618ad5ffd34f2bc6497`. Exact-head CI `34854738551`, Package Contracts `34854738531` and T066 `34854738635` passed. Final-candidate T042 Lifecycle Stress `34856470278` and T052 v0.1 Release Gate `34856470266` also passed after transient external dependency-download failures were classified as infrastructure and recovered with targeted reruns. Independent current-main Integration review `5198976168` recorded zero Blocking/Important findings. The core public `Binding<T>` value/lifetime contract is merged.

**T139 / #304 is Done.** PR #350 merged exact frozen head `f8e9729f5d967c8168335ae63e9800a0bb0b6ef4` into main as merge commit `61950a9f40145368dba709e755ee8f0c06ae411b`. Exact-head normal CI `34895438606`, Package Contracts `34895438615` and T066 Window Controls `34895438808` passed, then final-candidate T042 Lifecycle Stress `34901397463` and T052 v0.1 Release Gate `34901397478` passed. The mandatory second peer review `5203042503` on the exact review-fix head recorded `REVIEW_PASS`, zero Blocking/Important findings, and reviewer independence from current-head modifier W4. Representative Knob/Toggle/TextInput entry points now accept `Binding<T>` directly while preserving legacy `State<T>&` source compatibility; tests cover exactly-once mutation, targeted external invalidation, public/installed consumer compilation, and the mandatory T139 feature example. T140 and T141 subsequently completed the remaining Binding migration chain.

**T140 / #305 is Done.** PR #358 merged frozen exact head `5472ea1d0691724e78c01e1424ca8ad803a9fcbd` into main as merge commit `3e96491e1723418f17dea1eb173e1444d3006d0d`. Exact-head normal CI `34949901455` and T066 Window Controls `34949901516` passed, followed by final-candidate T042 Lifecycle Stress `34960645830` and T052 v0.1 Release Gate `34960645611`. Mandatory exact-head peer review `5208676715` recorded `REVIEW_PASS`, zero Blocking/Important findings, with reviewer W1 independent from source/current-head worker W4. Standard writable stateful widget families now retain canonical `Binding<T>` paths while legacy `State<T>&` source compatibility, invalidation classes, synchronous-destruction safety and two-UI isolation remain preserved.

**T141 / #306 is Done.** PR #372 merged frozen exact head `71635725e7929c5e43e00f7df19bee2fa56783d1` into main as merge commit `bf816a0318f0d0cd87819e8404c2425e11a537e2`. Exact-head CI `34978181978`, Package Contracts `34978181993`, T066 Window Controls `34978182056`, T042 Lifecycle Stress `34992041153` and T052 v0.1 Release Gate `34992041092` all passed. Mandatory exact-head peer review `5211779234` recorded `REVIEW_PASS`, zero Blocking/Important findings, with reviewer W1 independent from source/current-head worker W4. Dynamic `If`/`Switch`/`ForEach`, FocusScope and StyleScope now retain `Binding<T>` values while legacy State syntax remains source-compatible; T058 Structure behavior, focus/style invalidation, retained owner teardown safety, installed/relocated consumer compilation and two-UI isolation remain qualified. This completes the T138 -> T139 -> T140 -> T141 chain and the T124 integrated Binding qualification.

**T129 / #290 is Done.** PR #345 merged frozen exact head `b32a6433277a19f2e3f26394bad44ce4c7b4efc1` into main as merge commit `e400695e4f106a35093029e2465e0aa5ed12393d`. Exact-head CI `34953250223`, T044 Native Pointer Capture `34953250187`, T050 Inspector Contract `34953250193` and T066 Window Controls `34953250282` passed; final-candidate T042 Lifecycle Stress `34964143716` and T052 v0.1 Release Gate `34964143596` also passed. Mandatory exact-head peer review `5209343192` recorded `REVIEW_PASS`, zero Blocking/Important findings, with reviewer W1 independent from source/current-head worker W2. Long-lived retained invalidators now use a per-Tree weak lifetime generation plus stable `NodeId` resolution, terminal node removal retires identity before arbitrary teardown, explicit unmount/remount rotates the generation, stale callbacks after whole-Tree destruction are harmless, and stack-confined Input/Focus/Lifecycle paths remain on the immediate factories so the qualified hot paths stay allocation/performance neutral.

**T131 / #293 is Done.** PR #339 merged frozen exact head `bbdcf5d8cc58ac0f9a3386e48e4903c5737e9de0` into main as merge commit `bb5deb1f9a3c22adfa853338080ddc943ac1193d`. Exact-head CI `34947959869`, T050 Inspector Contract `34947959856` and T066 Window Controls `34947959862` passed; final-candidate T042 Lifecycle Stress `34961037609` and T052 v0.1 Release Gate `34961037565` also passed. Mandatory exact-head peer review `5209008696` recorded `REVIEW_PASS`, zero Blocking/Important findings, with reviewer W4 independent from source/current-head worker W1. Overlay show/close, Dialog completion/destructor, ComboBox/PopupMenu component commands and Tooltip presentation/hide now retain recoverable transaction ownership across injected structural failure, reentrancy and callback-driven owner destruction; begun application callbacks are never retried and recovery remains bounded and instance-owned.

**T132 / #294 is Done.** PR #342 merged exact frozen head `027e895ed8670158eead1d5c722c387f74a7448f` into main as merge commit `c2f381363d57e48df96f02d5d52b3e8a6571b478`. Exact-head CI `34908565263`, T060 `34908565154`, T064 `34908565205`, T065 Dispatcher `34908565174`, T065 Platform `34908565239`, T066 `34908565163` and Package Contracts `34908565192` passed; final-candidate T042 Lifecycle Stress `34911130032` and T052 v0.1 Release Gate `34911130054` also passed. The mandatory second peer review `5204033991` on the exact review-fix head recorded `REVIEW_PASS`, zero Blocking/Important findings, with reviewer independence from current-head modifier W1. Standalone close lifecycle control now remains deferred and owner-checkpointed when ordinary Dispatcher enqueue rejects or throws, including real component/input and native-close fault regressions, pending-destruction silence, exact-once completion and two-window isolation.

**T127 / #288 is Done.** PR #348 merged frozen exact head `c17cfca1dc53a921782c09d45e547482c854733b` into main as merge commit `9129d5e841bf969fecd215ba46bbd0b955f3e3f5`. Exact-head CI `35001985912`, T066 Window Controls `35001986036` and T067 Virtual List Contract `35001985913` passed; final-candidate T042 Lifecycle Stress `35010483243` and T052 v0.1 Release Gate `35010483184` also passed. Mandatory exact-head peer review `5214429062` recorded `REVIEW_PASS`, zero Blocking/Important findings, with reviewer W3 independent from source/current-head worker W2. ScrollState observer dispatch now has stable per-instance lifetime control, deterministic latest-write reentrancy and throwing-observer recovery; retained Scroll/ScrollView/scrollbar/VirtualList consumers gate borrowed-state use through lifetime tokens and callback boundaries; steady scrolling does not clone the full callback list. The post-T141 composition was reconciled and requalified before merge.

Current safety blocker set/frontier:

- **T124 / #282 — Done:** decomposed Binding umbrella T138 -> T139 -> T140 -> T141 is fully merged and final integrated T124 qualification is complete.
- **T138 / #303 — Done:** core public `Binding<T>` value/lifetime contract merged and fully qualified.
- **T139 / #304 — Done:** Binding-backed representative widget entry points, legacy `State<T>&` compatibility, exact invalidation and feature-example coverage merged and fully qualified.
- **T140 / #305 — Done:** standard stateful widget-family migration to Binding internals merged and fully qualified.
- **T141 / #306 — Done:** dynamic composition/style/focus Binding migration and final integrated T124 qualification merged and fully qualified.
- **T125 / #286 — Doing:** retained dispatch/reconciliation/cancellation exception safety.
- **T126 / #287 — Done:** DesktopServices completion/native exception boundaries.
- **T127 / #288 — Done:** `ScrollState` lifetime, retained-consumer ownership, deterministic reentrancy/exception semantics and post-T141 composition are merged and fully qualified as `9129d5e841bf969fecd215ba46bbd0b955f3e3f5`.
- **T128 / #289 — Done:** Dispatcher accepted-work recovery, Animation callback-exception invariants and cross-platform foreign-boundary exception smoke.
- **T129 / #290 — Done:** retained invalidation callback lifetime safety merged and fully qualified as `e400695e4f106a35093029e2465e0aa5ed12393d`.
- **T130 / #291 — Doing:** Component lifecycle/layout/paint exception safety plus native partial-construction and no-throw teardown.
- **T131 / #293 — Done:** transaction-safe Overlay/Dialog/popup/Tooltip failure recovery merged and fully qualified.
- **T132 / #294 — Done:** failure-safe standalone close deferral and lifecycle-control enqueue recovery merged and fully qualified.
- **T173 / #401 — Done:** complete public ASCII A-Z key exposure merged as `2d692a781537593fe686da19be330c0cdd24c4b6`, with exact-head peer review and full normal/final qualification green.

T069 / issue #81 remains **Blocked** until the complete pre-freeze blocker set is Done and synchronized on main. T124, T126, T127, T128, T129, T131, T132, T138, T139, T140, T141 and T173 are complete; remaining blockers are T125 and T130.

The immediate v1 frontier is therefore:

```text
T123(done) -> T138(done) -> T139(done) -> T140(done) -> T141(done) -> T124(done)

T123(done) ------------------------------------------------------------> T127(done)

T126(done)
T128(done)
T129(done)
T131(done)
T132(done)
T173(done)

parallel remaining P0: T125 + T130

T125 + T130 both Done
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
- T065 / PR #133: bounded UI-thread Dispatcher/timers; T128 hardens callback failure and T132 now hardens lifecycle-control enqueue failure.
- T066 / PR #237: standalone window controls and deterministic close lifecycle.
- T067 / PR #219 + PR #233: fixed-height virtualized ListView; T127 hardens ScrollState lifetime/reentrancy.
- T072 / PR #185: sole v1 Linux `libdbus-1` transport used by T064 and reserved for future T068 1.2 integration.
- T123 / PR #284: deterministic lifetime-safe `State<T>` notification, observer-exception and reentrancy contract; merged as `839a7b082f94e0bef3b688cc7bcc2074e6cbfb99` after complete normal/path/final qualification.
- T126 / PR #295: DesktopServices completion exception-boundary, terminal-drop and retained native cleanup hardening; merged as `be216d7a41999113472a0457827d43caad28cf15` after complete normal/path/final qualification.
- T127 / PR #348: ScrollState observer/lifetime and retained-consumer hardening; merged as `9129d5e841bf969fecd215ba46bbd0b955f3e3f5` after post-T141 reconciliation, exact-head CI/T066/T067, final T042/T052 qualification and independent peer review `5214429062`.
- T128 / PR #296: Dispatcher/Animation exceptional recovery and cross-platform foreign-boundary containment hardening; merged as `ca65087b503aff04133394c5ef200cd7cae75e12` after complete normal/path/final qualification.
- T129 / PR #345: retained invalidation lifetime safety with weak generation + stable NodeId resolution and stack-confined immediate-context preservation; merged as `e400695e4f106a35093029e2465e0aa5ed12393d` after exact-head CI/T044/T050/T066/T042/T052 qualification and independent peer review `5209343192`.
- T131 / PR #339: Overlay/Dialog/popup/Tooltip transaction-safety hardening; merged as `bb5deb1f9a3c22adfa853338080ddc943ac1193d` after exact-head CI/T050/T066/T042/T052 qualification and independent peer review `5209008696`.
- T132 / PR #342: failure-safe standalone close lifecycle-control deferral under queue rejection/throw; merged as `c2f381363d57e48df96f02d5d52b3e8a6571b478` after exact-head development/final qualification and independent second peer review.
- T138 / PR #343: core public `Binding<T>` value/lifetime contract; merged as `1f63d9a4778773102f162618ad5ffd34f2bc6497` after current-main normal/path/final qualification and clean Integration review.
- T139 / PR #350: representative Binding-backed Knob/Toggle/TextInput entry points plus legacy State compatibility and exact invalidation coverage; merged as `61950a9f40145368dba709e755ee8f0c06ae411b` after exact-head normal/path/final qualification and independent second peer review.
- T140 / PR #358: standard stateful widget-family Binding migration with legacy State compatibility, invalidation preservation and isolation coverage; merged as `3e96491e1723418f17dea1eb173e1444d3006d0d` after exact-head CI/T066/T042/T052 qualification and independent peer review `5208676715`.
- T141 / PR #372: retained dynamic composition/style/focus Binding migration plus final integrated T124 qualification; merged as `bf816a0318f0d0cd87819e8404c2425e11a537e2` after exact-head CI/Package/T066/T042/T052 qualification and independent peer review `5211779234`.
- T173 / PR #403: complete public ASCII letter-key exposure A-Z with ABI-preserving enum extension, shared constexpr/noexcept translation, command/Primary+Q regression coverage and dedicated retained feature example; merged as `2d692a781537593fe686da19be330c0cdd24c4b6` after exact-head normal CI, peer review `5223294153`, T042 and T052.

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

Scheduler/Delivery must treat explicit dependencies as hard gates. T124 is an umbrella after decomposition; the `T138 -> T139 -> T140 -> T141` chain is now complete and T124 is Done. T069 must remain blocked until T125 and T130 are both Done and synchronized on main; T127 and T173 are now complete.

Optimal parallelization before the freeze:

1. execute the Scheduler-authorized transactional canonical-stream replacements for T125 and T130 from current main, preserving unrelated changes and retiring composition-invalid Draft streams;
2. run fresh exact-head normal/path qualification, complete acceptance/tests/non-goals and self `CODE_REVIEW.md`, then obtain independent peer review handoffs before any final qualification;
3. after T125 and T130 are Done and synchronized on main, resume T069 whole-surface freeze;
4. then proceed T070 -> T071 on the exact frozen/RC baselines.

## Next actions

1. Complete the canonical-stream replacement/closeout for **T125 / #286** and **T130 / #291** according to the live Scheduler reservations.
2. Prioritize any resulting exact-head peer-review/qualification/merge handoff over widening source work.
3. Keep T069/#81 Blocked until **T125 and T130** are Done and synchronized on main.
4. Keep T068/PR #241 parked for NativeUI 1.2.