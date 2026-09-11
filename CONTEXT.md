# NativeUI compact recovery context

**Updated:** 2026-09-11

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio and host parameter semantics remain out of scope.

Non-negotiable rules:

- widgets/layout stay platform-neutral and public geometry uses logical pixels;
- embedded polling is non-blocking;
- mutable instance-dependent production globals/singletons/`thread_local` state are forbidden;
- retained UI state is UI/main-thread confined unless explicitly documented otherwise;
- dependencies use CMake + CPM; Skia comes from pinned `skia-builder` binaries;
- macOS Objective-C runtime-visible platform classes are consumer-specific through the T053 identity contract;
- NativeUI-owned source-tree targets compile with zero unapproved warnings and an empty default `NATIVEUI_ALLOWED_WARNINGS`;
- behavior changes use RED -> GREEN -> REFACTOR and must not weaken tests or validation policy;
- `CODE_REVIEW.md`, exact-head validation, a complete issue-to-code/test evidence matrix, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline and critical path

Current `main` is `621a56e462590e47e7360e122f3d4031814d8947`, which includes completed T067 / PR #219 in addition to T045, T037, T058, T036, T065, T034, T060/T042 lifecycle qualification, the T052 developer-preview release gate, the warning-free source-tree baseline, post-T036 hover correction #212 and Tree paint-ownership correction #152.

The active dynamic/overlay lane is completing T061 / issue #73 / PR #216 on top of that exact main. The pre-documentation exact source head is `d028d43c3ad95d229f02ff50e975f49a12f5b9e8`; compare against current main reports `behind_by=0`.

Current convergence:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(done) -> T068
                                                   T058(done) ------------^

style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

dynamic/overlay:   T058(done) -> T061(completion PR #216)
                                      |-> T035 --------------------------> T068
                                      |-> T063 --------------------------> T068
                                      +-> T062

critical platform: T065(done) -> T072 -> T064
                   T041(done) -> T043 -> T066
                                      |-> T068

release:            convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

T035 and T063 require T061 plus already-complete T034. T062 requires T061 plus already-complete T065. After T061, the owned critical order remains T035, then T063, then T062.

## Completed foundations relevant to v1

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- #64 / PR #90: standalone PROGRAM-world ownership Decision B.
- T059 / PR #89: inherited visibility/enabled/read-only state.
- T030–T034: standard Button, Checkbox/Radio, Slider/RangeSlider, ProgressBar/Meter and ScrollView baseline.
- T036 / PR #155: retained non-virtualized ListView + Tabs baseline with stable selection and composite focus.
- #212 / PR #213: deterministic ListView/Tabs hover presentation and retained pointer-leave lifetime.
- T037 / PR #151: typed per-UI theme tokens and representative widget theme binding.
- T045 / PR #210: accessibility semantic architecture and immutable virtual collection contract.
- T047 / PR #92 + T048 / PR #99: relocatable package and external-consumer qualification.
- T051 / PR #116 + T052 / PR #120: reproducible performance policy and v0.1 developer-preview release gate.
- T054 / PR #119, T056 / PR #111, T057 / PR #126: application/package/resource helpers.
- T058 / PR #154: bounded retained dynamic composition with one per-tree structural reconciliation queue.
- T060 / PR #118 + #139 / PR #140: explicit Application ownership and stress-qualified multi-window lifecycle.
- T065 / PR #133: bounded UI-thread Dispatcher/timer service with native wake integration.
- T067 / PR #219: fixed-height virtualized ListView with bounded visual materialization and immutable virtual semantic metadata.
- #163 / PR #181: warning-free NativeUI-owned source-tree builds.
- #152 / PR #153: Tree no longer paints an implicit application background/help overlay.

## T061 overlay/portal completion state

T061 / issue #73 / PR #216 adds one generic retained in-view overlay/portal layer per `UI`, implemented through the existing T058 structural checkpoint rather than a second reconciler.

Delivered contract:

- one per-UI creation-ordered overlay stack with monotonically increasing lifetime-safe handles; stale, already-closed and cross-UI handles are deterministic no-ops and IDs are never reused;
- public Modal/NonModal mode and Normal/Ignore pointer policy; invalid `Modal + Ignore` is rejected;
- X11-safe placement names `AnchorBelow`, `AnchorAbove`, `AnchorRight`, `AnchorLeft`, `Center`, `Auto` implement the issue's Below/Above/Right/Left/Center/Auto semantics without colliding with Xlib `Above`/`Below` macros;
- deterministic requested-side/opposite-side fallback, Auto priority Below -> Above -> Right -> Left, requested-side tie retention, finite origin clamping and natural-size preservation with no automatic overlay resize/scroll;
- creation-order paint/input z-order; Ignore overlays still paint but their entire subtree is pointer-transparent and excluded from ordinary focus traversal;
- retained full-viewport modal barriers block lower overlays/root while allowing overlays created above the modal to remain pointer-eligible;
- outside-pointer dismissal consumes the initiating PointerDown with no click-through; Escape honors topmost eligibility and modal ownership;
- anchors are retained `NodeId`s, reposition after authoritative layout and auto-close when missing, Hidden, Collapsed or when the owning UI deactivates;
- modal focus uses existing trapping FocusScope semantics; restoration candidates are stored as `NodeId` and re-resolved at restore time, with deterministic fallback for stale targets;
- a later focusable NonModal overlay may receive pointer input above a modal but cannot move keyboard focus out of the active modal trap; nested/new modal traps can still take focus;
- showing a new modal cancels any pre-existing lower pointer capture exactly once at a safe platform-bearing checkpoint, including modal creation from the current callback after that callback unwinds;
- closing a captured overlay routes exactly one PointerCancel while the component/context is valid before T058 teardown;
- show/close are reentrancy-safe; pre-flush show+close coalesces without mount/unmount; no global overlay registry and no second structural queue exists;
- root content remains consumer-owned, overlays paint outside ordinary root clipping, and the same generic API is used by standalone and `EmbeddedView` paths;
- dedicated `examples/features/t061_overlay_portal.cpp` demonstrates popup/modal/pointer-transparent behavior and supplies deterministic `--self-test` coverage.

### Exact source-head evidence before completion docs

Exact source head `d028d43c3ad95d229f02ff50e975f49a12f5b9e8` passed:

- normal CI `34554637275`: Linux X11, macOS, Windows and Linux ASan+UBSan all green;
- Linux X11 full CTest: 93/93 green, including generic focus, dynamic composition, headless, golden, dedicated T061 acceptance and `nativeui_example_t061_overlay_portal_self_test`;
- T052 v0.1 Release Gate `34554637335`;
- T042 Lifecycle Stress `34554637304`;
- T045 Accessibility Semantics `34554637284`;
- T060 Application Contract `34554637332`;
- T065 Dispatcher Contract `34554637322`;
- T067 Virtual List Contract `34554637270`.

Final `CODE_REVIEW.md` / anti-rush source review `5174681221` reports no remaining Blocking/Important finding after the modal-focus correction. The review explicitly covers per-instance ownership, globals/statics, threading, lifetime/reentrancy, focus/modality, capture, anchor/geometry, platform parity, warnings, performance gates and test completeness.

This completion documentation changes the branch head. Therefore **T061 is not merged yet**: the documentation-complete exact head must rerun the applicable exact-head gates before the issue can be marked Done and PR #216 merged.

## Other critical lanes

- UI/accessibility: T067 is complete; T068 remains blocked until every explicit issue #80 dependency is Done.
- Platform: finish T072, then T064; finish T043, then T066.
- Styling: T037 is complete; continue T038 -> T039 and T040 when dependencies permit.
- T044 remains a T071 release dependency but is not on the immediate T068/T069 convergence path.
- T069/T070/T071 remain dependency-gated; do not freeze the v1 API early.

## Validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The default build must use an empty `NATIVEUI_ALLOWED_WARNINGS`. A code-changing completion candidate must use its exact current head for normal CI plus every relevant dedicated workflow named by the ticket, followed by the mandatory `CODE_REVIEW.md` review. A documentation completion commit still requires exact-head qualification before autonomous merge.

## Next actions

1. Qualify the documentation-complete exact head of T061 / PR #216; merge only if all required executed gates remain green and no new Blocking/Important finding appears.
2. After T061 merges, immediately advance T035 / issue #35.
3. Then advance T063 / issue #75 and T062 / issue #74 as their explicit dependencies permit.
4. Keep T068 blocked until all of its explicit dependencies are genuinely Done.
5. Continue the independent platform/style lanes without duplicating their active work.
6. Keep T069/T070/T071 dependency-gated and do not freeze the v1 API early.
