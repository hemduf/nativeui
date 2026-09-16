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
- ordinary C++ callbacks may propagate only after NativeUI invariants are restored; foreign/native ABI callbacks contain C++ exceptions;
- destructor-driven retained/native teardown is no-throw and best-effort complete;
- retained invalidators intentionally allowed to outlive a component use lifetime-safe owner/identity semantics;
- accepted/deferred work is not silently lost, duplicated or converted to unsafe synchronous execution by failure;
- state machines crossing fallible work have explicit prepare/commit/recovery boundaries;
- partial native construction leaves no registered callback/native resource behind;
- top-level `UI`/window/view destruction from an active callback is deferred unless the complete caller chain proves self-destruction safety;
- `CODE_REVIEW.md`, `CI_POLICY.md`, exact-head tests, `CONTEXT.md` and `ROADMAP.md` are merge/Done gates for code-changing tickets;
- no personal information is placed in tickets, source, tests, examples, fixtures or generated metadata.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline and critical path

### T130 is Done

**T130 / #291 / PR #408 merged as `e65e317d6584ae440f1f041b8187a63d18b2aa6a`.** Frozen executable head `4c40b881fa13ea0f9839ae415059744f6107c62d` closes the retained Component lifecycle/layout/paint/native construction and teardown exception-safety blocker.

Delivered invariants:

- retained/native destruction is no-throw and best-effort complete;
- lifecycle transitions use exact-progress per-Tree transaction ownership and rollback;
- provisional lifecycle state cannot be observed/published through Tree/UI measure/layout/paint/dispatch/inspector paths;
- dynamic insertion/removal has coherent ownership publication, staged focus-registry repair and durable per-Tree retry state under callback/allocation failure;
- teardown does not consume pending desired dynamic structure solely to create-and-destroy it;
- failed layout rolls back geometry/publication state and preserves required layout/paint dirtiness;
- framework Painter/SkCanvas scopes remain balanced on exception unwind and failed paint remains dirty until success;
- ViewCore constructor stages have deterministic partial-construction cleanup and preserve shared Application-owned resources;
- native/window/view teardown continues after retained callback failure and releases owned resources exactly once;
- T044 semantic PointerDown/PointerUp remains exactly once while native readiness/button-release observation is deterministic;
- failure/recovery state is instance-owned with no mutable process-global/singleton/`thread_local` recovery state.

Exact-head normal/path workflows are all green: CI `35134851097`, Package Contracts `35134851013`, T044 `35134851103`, T050 `35134851018`, T060 `35134851051`, T064 `35134851039`, T065 `35134851008`, T066 `35134851040`, T072 `35134850948`. The only initial Windows red was an external Mesa download reset after a successful build; a same-head targeted rerun passed.

Final-candidate T042 Lifecycle Stress `35140156948` is green on Linux ASan+UBSan, Linux X11, macOS and Windows. T052 v0.1 Release Gate `35140156810` is green for release contracts, clean Linux/macOS/Windows bootstraps and the exact-head T051 performance/allocation benchmark. Review records `5226938758` and `5227314927` contain zero Blocking/Important findings. #291 is closed with `status:done`.

### Remaining v1 pre-freeze work

- **T125 / #286 — Doing, P0.** Retained dispatch/reconciliation/cancellation unwind safety is now the only non-Done hard safety prerequisite among T123–T132 for T069.
- **T174 / #409 — Blocked by T125, P1.** It adds a public per-UI unhandled-KeyDown fallback and is intentionally composed on final T125 dispatch semantics. After T125 merges, finish/requalify T174 or explicitly retarget it post-v1 before freezing public API.
- **T069 / #81 — Blocked.** T130 is satisfied; T125 remains a hard dependency. T174 must also be resolved or deliberately moved post-v1 before the final API freeze begins.
- **T068 / #80 / PR #241 — deferred to NativeUI 1.2.** It does not block 1.0.

Current path:

```text
T130(done)
T125(doing) -> T174(blocked; finish or retarget) -> T069 -> T070 -> T122/docs -> T071 -> v1.0.0
T068 ---------------------------------------------------------------> 1.2
```

## Completed safety foundations relevant to v1

- **T123 / #281:** deterministic lifetime-safe `State<T>` notifications, observer mutation/reentrancy and throwing-observer recovery.
- **T124 / #282 + T138/T139/T140/T141:** stable `Binding<T>` value/lifetime contract and completed migration of standard/retained state consumers.
- **T126 / #287:** DesktopServices completion/native exception boundaries and deterministic capacity cleanup.
- **T127 / #288:** `ScrollState` lifetime, observer reentrancy/exception semantics and retained-consumer lifetime gating.
- **T128 / #289:** Dispatcher accepted-work durability and Animation callback-exception progress/recovery.
- **T129 / #290:** retained invalidator lifetime safety using per-Tree lifetime generation + stable NodeId lookup.
- **T130 / #291:** lifecycle/layout/paint/native construction/teardown exception safety, merged in PR #408.
- **T131 / #293:** Overlay/Dialog/popup/Tooltip transaction-safe failure recovery.
- **T132 / #294:** standalone close lifecycle-control deferral under queue rejection/throw.
- **T173 / #401:** complete public ASCII A-Z key exposure while preserving pre-existing key numeric values and command/text/IME separation.

Other delivered v1 foundations include T030–T036 standard widgets, T037–T040 Theme/style/animation, T043 resize/scale, T044 pointer capture, T045 semantic accessibility architecture, T047/T048 packaging, T049 gallery, T050 inspector, T051/T052 qualification, T053 consumer-scoped macOS Objective-C runtime identity, T054/T056/T057 helpers/resources, T058 dynamic composition, T060 Application ownership, T061/T062/T063 overlay/Tooltip/Dialog, T064 DesktopServices, T065 Dispatcher, T066 window controls, T067 virtualized ListView and T072 Linux D-Bus.

## Validation policy

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

During active development:

1. keep source-changing PRs Draft;
2. use local TDD and coherent correction batches;
3. publish only qualification-worthy heads;
4. run normal CI plus path-relevant dedicated checks;
5. complete the full `CODE_REVIEW.md` record on the frozen head;
6. transition Draft -> Ready only for final T042/T052 qualification;
7. any executable/build/workflow change invalidates that candidate and requires Draft + requalification;
8. pure project-state completion documentation does not invalidate an otherwise green executable candidate.

Fault injection is mandatory where normal execution cannot deterministically reproduce allocation, callback, queue, partial-construction, layout, paint, teardown or native-boundary failure classes.

## Automation / integration recovery

GitHub live state is the only durable source of truth. The legacy Scheduler/Reporter control plane and W1–W4 persistent assignments are retired. Current automation is serialized:

```text
implementation/source-changing lanes: 1
independent review: on demand for a frozen candidate
reporting/watchdog: read-only
fallback work: disabled
```

Recovery sequence for any new session:

1. read `AGENTS.md`, `CODE_REVIEW.md`, `CI_POLICY.md`, `CONTEXT.md`, `ROADMAP.md` and `AUTOMATION.md`;
2. re-fetch live issues, canonical PRs, exact heads, checks, reviews and unresolved threads;
3. finish the current merge-near ticket before starting another source-changing ticket;
4. do not trust retired scheduler snapshots over current GitHub state.

## Next actions

1. Finish **T125 / #286 / PR #382** with one complete remaining failure-family audit/correction, exact-head qualification, review and merge.
2. After T125 merges, resume **T174 / #409 / PR #410** on final T125 semantics, or explicitly move it post-v1 before the freeze.
3. Resume **T069 / #81 / PR #269** only after every hard dependency is Done and T174 is resolved/retargeted.
4. Then execute T070 reference application/Getting Started, explicitly scheduled v1 documentation closeout including T122 where applicable, and T071 on one exact RC SHA.
5. Keep T068/PR #241 parked for NativeUI 1.2.
