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

### Safety frontier complete

The full T123–T132 pre-freeze safety series is Done. The two most recent blockers are merged:

- **T130 / #291 / PR #408** — lifecycle/layout/paint/native construction/teardown exception safety, merged as `e65e317d6584ae440f1f041b8187a63d18b2aa6a` from frozen head `4c40b881fa13ea0f9839ae415059744f6107c62d`.
- **T125 / #286 / PR #382** — retained dispatch/reconciliation/cancellation exception safety, pending-work preservation, dynamic enqueue failure recovery and transient native-borrow hardening, merged as `401d73983c2103fae1e48c1984a982277088e060` from frozen head `da4386626837b8cedf9d7f17bc6e8b150aa99921`.

Both completed normal/path CI, T042 Lifecycle Stress and T052/T051 final qualification with zero remaining Blocking/Important review findings.

### T174 is Done

**T174 / #409 / PR #410 merged as `1ef326494ce00d215c1211ad0cde2437e3ffadbb`.** Frozen exact head `e6d747961a2fd39760f5703748c10440f8fb0efa` resolves the last planned public input addition before the v1 freeze.

Delivered contract:

- `UI::set_key_down_handler(std::function<EventResult(const InputEvent&)>)` is per UI/Tree only;
- framework KeyDown policy, Command normalization and ordinary focused/ancestor routing retain precedence;
- only otherwise-unhandled raw KeyDown can reach the fallback;
- Command-mapped shortcuts never double-deliver to the raw fallback;
- KeyUp/TextInput/Composition/Tick/pointer/wheel/drop remain excluded;
- active callable lifetime survives re-entrant clear/replacement through stable shared ownership;
- installation allocates before publication; dispatch itself adds no heap allocation or extra tree walk;
- callback exceptions use T125's canonical unwind path and later successful dispatch reaches normal outermost reconciliation;
- no new scheduler, queue, native resource or mutable global/TLS state.

Exact-head normal/path qualification is green for CI `35154523714`, T050 `35154524036` and T066 `35154523694`. Final T042 `35156481983` is green on Linux ASan+UBSan, Linux X11, macOS and Windows. Final T052 `35156481846` is green for release contract, clean Linux/macOS/Windows bootstraps and exact-head T051 benchmark. Reviews `5228886765` and `5228890731` are 0 Blocking / 0 Important; the historical Blocking thread is resolved/outdated and privacy review is clean.

### Remaining v1 work

- **T069 / #81 / PR #269 — Ready / P0.** All hard safety prerequisites are Done and T174 is resolved. Resume the existing canonical PR on current `main` and execute the complete public API inventory/cleanup/freeze.
- **T068 / #80 / PR #241 — deferred to NativeUI 1.2.** Native accessibility bridges do not block 1.0.

Current path:

```text
T123–T132(done) + T173(done) + T174(done)
                         |
                         v
T069(ready) -> T070 -> T122/docs -> T071 -> v1.0.0
T068 ---------------------------------------> 1.2
```

## Completed foundations relevant to v1

- **T123 / #281:** deterministic lifetime-safe `State<T>` notifications and observer exception/reentrancy semantics.
- **T124 / #282 + T138–T141:** stable `Binding<T>` contract and state-consumer migration.
- **T125 / #286:** retained dispatch/reconciliation/cancellation exception safety.
- **T126 / #287:** DesktopServices completion/native exception boundaries.
- **T127 / #288:** `ScrollState` lifetime and observer recovery semantics.
- **T128 / #289:** Dispatcher accepted-work durability and Animation exceptional recovery.
- **T129 / #290:** lifetime-safe retained invalidation callbacks.
- **T130 / #291:** lifecycle/layout/paint/native construction/teardown exception safety.
- **T131 / #293:** Overlay/Dialog/popup/Tooltip transaction-safe failure recovery.
- **T132 / #294:** standalone close lifecycle-control deferral under queue rejection/throw.
- **T173 / #401:** complete public ASCII A-Z key exposure.
- **T174 / #409:** per-UI unhandled raw KeyDown fallback on final T125 semantics.

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
7. executable/build/workflow changes invalidate that candidate and require Draft + requalification;
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

Recovery sequence:

1. read `AGENTS.md`, `CODE_REVIEW.md`, `CI_POLICY.md`, `CONTEXT.md`, `ROADMAP.md` and `AUTOMATION.md`;
2. re-fetch live issues, canonical PRs, exact heads, checks, reviews and unresolved threads;
3. finish the current merge-near ticket before starting another source-changing ticket;
4. do not trust retired scheduler snapshots over current GitHub state.

## Next actions

1. Resume **T069 / #81 / PR #269** on current `main` and complete the v1 public API inventory, breaking cleanup and freeze.
2. Execute T070 reference application/Getting Started against that frozen surface.
3. Complete explicitly scheduled v1 documentation closeout including T122 where applicable.
4. Run T071 on one exact release-candidate SHA.
5. Keep T068/PR #241 parked for NativeUI 1.2.
