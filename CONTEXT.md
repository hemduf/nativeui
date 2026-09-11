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

The current pre-T067-regression baseline is `main` `7d9fc61cc1453bc3557a6ea1908541dc6a701c8a`. It includes completed T043 / PR #142 on top of T061, T067 / PR #219, T045, T037, T058, T036, T065, T034, T060/T042 lifecycle qualification, the T052 developer-preview release gate, the warning-free source-tree baseline, post-T036 hover correction #212 and Tree paint-ownership correction #152.

T067 remains architecturally complete from PR #219. PR #233 carries a post-completion example startup regression closure: the 100k-item virtual ListView was inserted directly into a `Column` with the default `shrink = 0`, so its 2.8-million-pixel preferred content height became the initial viewport and almost the full dataset was materialized before the native window was shown. The corrected example constrains the virtual list with `Flex(grow=1, shrink=1)`, adds a production-composition startup materialization regression, executes the example `--self-test` in the dedicated T067 workflow and guards that CI wiring from the root integration contract.

Current convergence:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(done) -> T068
                                                   T058(done) ------------^

style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

dynamic/overlay:   T058(done) -> T061(done)
                                      |-> T035 --------------------------> T068
                                      |-> T063 --------------------------> T068
                                      +-> T062

critical platform: T065(done) -> T072 -> T064
                   T041(done) -> T043(done) -> T066
                                      |-------> T068

release:            convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

T035 and T063 require completed T061 plus already-complete T034. T062 requires completed T061 plus already-complete T065. T043 is complete and unlocks T066 while remaining an explicit completed T068 dependency.

## Completed foundations relevant to v1

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- #64 / PR #90: standalone PROGRAM-world ownership Decision B.
- T059 / PR #89: inherited visibility/enabled/read-only state.
- T030–T034: standard Button, Checkbox/Radio, Slider/RangeSlider, ProgressBar/Meter and ScrollView baseline.
- T036 / PR #155: retained non-virtualized ListView + Tabs baseline with stable selection and composite focus.
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
- T065 / PR #133: bounded UI-thread Dispatcher/timer service with native wake integration.
- T067 / PR #219, with startup regression closure in PR #233: fixed-height virtualized ListView with bounded visual materialization and immutable virtual semantic metadata; the required 100k example is constrained to the real viewport at startup and its production `--self-test` runs in dedicated CI.
- #163 / PR #181: warning-free NativeUI-owned source-tree builds.
- #152 / PR #153: Tree no longer paints an implicit application background/help overlay.

## T043 resize/scale completion state

T043 is complete in PR #142. Its platform-boundary contract is:

- `Size`, component/layout geometry, preferred sizes and invalidation rectangles stay logical; Pugl/native extents and Skia framebuffer extents stay physical;
- every view owns its own `last_valid_scale`, initialized to `1.0f`; only finite strictly-positive observations replace it;
- invalid scale reports retain the last valid value and publish the bounded existing platform diagnostic instead of dividing by zero or propagating NaN/Inf;
- public `set_size()` validates finite strictly-positive logical dimensions, emits at most one native request, records only an accepted pending request and never asserts the authoritative viewport before configure;
- one authoritative configure snapshot updates physical extent plus scale together and dispatches at most one resulting logical layout; zero/non-finite physical extents are transient non-renderable states;
- request echoes never call back into `set_size`, so successful resize negotiation has no recursive size-request loop;
- pointer/drop/text-input/invalidation conversion uses the same retained per-view scale exactly once, including fractional 1.25/1.5 scale;
- embedded child resizing never resizes the native parent; preferred-size notification remains a one-way advisory from `UI::measure().preferred`;
- preferred notifications use exact component epsilon `> 0.0001f`, coalesce to the latest safe value, reject recursive same-stack dispatch and keep the in-flight marker independent from owner lifetime;
- the T061 embedded-overlay smoke behavior is preserved while invalid-size, parent-authority and synchronous preferred-grant coverage remain qualified;
- `t043_resize_scale --self-test`, `nativeui_t043_view_geometry_tests`, registration checks and the platform smoke surface are wired into the root CMake.

The dedicated geometry suite includes direct synthetic resize-only, scale-only and combined configure snapshots and verifies exactly one layout dispatch per accepted configure. It also instruments public-request semantics so one logical request produces one native request, configure remains authoritative, an echoed configure emits no second request, invalid requests do not call the native boundary, and failed native requests do not mutate authoritative/pending geometry.

## Other critical lanes

- UI/accessibility: T061 and T067 are complete; T035/T063/T062 may advance as their dependencies permit, while T068 remains blocked until every explicit issue #80 dependency is Done.
- Platform: T043 is complete and T066 is unblocked; continue T072, then T064.
- T044 / issue #44 / PR #145 remains a T071 release dependency and requires its own current-main reconciliation and exact-head evidence before merge.
- Styling: T037 is complete; continue T038 -> T039 and T040 when dependencies permit.
- T069/T070/T071 remain dependency-gated; do not freeze the v1 API early.

## Validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The default build must use an empty `NATIVEUI_ALLOWED_WARNINGS`. A code-changing completion candidate must use its exact current head for normal CI plus every relevant dedicated workflow named by the ticket, followed by the mandatory `CODE_REVIEW.md` review. Documentation changes are part of the exact completion head and therefore require requalification before autonomous merge.

For the T067 startup regression closure, the exact completion head must keep normal CI green on Linux X11, Linux ASan+UBSan, macOS and Windows, plus T042, T045, T052, T060 and the dedicated T067 workflow including `nativeui_example_t067_virtual_list --self-test`.

## Next actions

1. Complete the exact-head requalification after the T067 regression completion-doc update and merge PR #233 only if every gate remains green.
2. Reconcile and qualify T044 / PR #145 independently; do not reuse T043 evidence.
3. Continue T035, T063 and T062 now that T061 is complete, respecting their explicit dependency/priority order.
4. Keep T068 blocked until all of its explicit dependencies are genuinely Done.
5. Continue T072 -> T064 and T066 in the platform lane.
6. Keep T069/T070/T071 dependency-gated and do not freeze the v1 API early.
