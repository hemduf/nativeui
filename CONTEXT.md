# NativeUI compact recovery context

**Updated:** 2026-09-10

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio and host parameter semantics remain out of scope.

Non-negotiable rules:

- widgets/layout stay platform-neutral and public geometry uses logical pixels;
- embedded polling is non-blocking;
- mutable instance-dependent production globals/singletons/`thread_local` state are forbidden;
- retained UI state is UI/main-thread confined unless explicitly documented otherwise;
- dependencies use CMake + CPM; Skia comes from pinned `skia-builder` binaries;
- macOS Objective-C runtime-visible platform classes are consumer-specific through the T053 identity contract;
- NativeUI-owned source-tree targets compile with zero unapproved warnings; `NATIVEUI_ALLOWED_WARNINGS` is empty by default;
- `CODE_REVIEW.md`, exact-head validation, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline and critical path

The current implementation baseline includes completed T045 / PR #210 and the post-T036 hover correction #212 / PR #213 (squash merge `2d483ffe3585b9ab619a743a2131badb4709fa4f`) on top of T037, T058, T036, T065, T034, T060/T042 lifecycle, T052 developer-preview release gate and the warning-free source-tree baseline.

The UI/accessibility critical path is now:

```text
T034(done) -> T036(done) -> T045(done) -> T067(active PR #219) -> T068
                              T058(done) ---------------------------^
```

T045 freezes the accessibility semantics architecture that T067 and T068 consume. Its delivered contract provides:

- backend-neutral `SemanticRole`, `SemanticAction`, checked/expanded/value/range data and the closed semantic change categories;
- stable semantic identities independent of raw component addresses;
- immutable semantic node/tree snapshot value types suitable for native read-side publication;
- a concrete data-only `VirtualSemanticChildren` model for large ListView collections, with stable item tokens, shared immutable O(N) metadata and lazy selection/bounds projection;
- a 100k-item deterministic virtual collection fixture proving semantic lookup does not construct retained visual rows;
- exact NSAccessibility, Windows UIA and Linux AT-SPI2 mapping/lifetime/action-routing design in `docs/accessibility.md`;
- explicit native-proxy rule: weak bridge/root + semantic identity only, never a long-lived raw `Node*`/`Component*`;
- immutable native read snapshots and UI-thread action marshalling through T065 for T068;
- no process-global semantic/proxy registry;
- isolated warning-as-error T045 tests plus public-header isolation.

T067 is active after T045 completion and must preserve those identity/metadata rules while integrating fixed-height virtualization with T034/T036/T058.

## Completed foundations relevant to v1

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- #64 / PR #90: standalone PROGRAM-world ownership Decision B.
- T059 / PR #89: inherited visibility/enabled/read-only state.
- T030 / PR #94: Button.
- T031 / PR #95: Checkbox + typed RadioGroup/RadioButton.
- T032 / PR #115: Slider + RangeSlider.
- T033 / PR #123: ProgressBar + Meter.
- T034 / PR #135: ScrollView with change-based wheel bubbling, pointer pan, retained overlay scrollbars and focus reveal.
- T036 / PR #155: retained non-virtualized ListView + Tabs baseline with stable key/value selection and composite focus.
- #212 / PR #213: restores deterministic paint-only ListView/Tabs hover presentation, adds retained/native pointer-leave handling and makes hover lifetime safe across T058 dynamic subtree removal.
- T037 / PR #151: typed per-UI theme tokens and representative widget theme binding.
- T045 / PR #210: accessibility semantic architecture and virtual collection contract.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform()`.
- T048 / PR #99: relocated external consumers and macOS two-consumer isolation.
- T051 / PR #116: reproducible Release benchmark and regression policy.
- T052 / PR #120: v0.1 developer-preview release gate.
- T054 / PR #119: `nativeui_add_application()` high-level application package helper.
- T056 / PR #111: deterministic binary-data packaging.
- T057 / PR #126: immutable non-owning `ResourceManager` plus allocating provider adapter.
- T058 / PR #154: bounded retained dynamic composition with conditional, switch and keyed collection reconciliation.
- T060 / PR #118: one explicit `ui::Application` owns one standalone `PUGL_PROGRAM` world and multiple `StandaloneWindow(Application&, ...)` views.
- T065 / PR #133: bounded UI-thread Dispatcher/timer service with native wake integration.
- #139 / PR #140: T042 stress qualification of the supported T060 multi-window path.
- #163 / PR #181: warning-free NativeUI-owned builds and v1 Application ownership in examples/smokes.
- #152 / PR #153: Tree no longer paints an implicit application background/help overlay.

## T036 hover follow-up — complete

Issue #212 / PR #213 is complete and squash-merged as `2d483ffe3585b9ab619a743a2131badb4709fa4f`.

Delivered behavior:

- `ListView` enabled rows and enabled/unselected `Tabs` headers expose deterministic paint-only hover feedback;
- disabled entries never render hovered and hover transfer/leave does not mutate application selection or activation state;
- one per-Tree retained hover route tracks the authoritative pointer target without mutable process-global state;
- platform pointer crossing maps through Pugl `PUGL_POINTER_IN` / `PUGL_POINTER_OUT` to retained move/leave behavior;
- T058 dynamic reconciliation clears a hover route into a subtree before that subtree is deactivated, unmounted or destroyed;
- the existing T058 structural-epoch guard makes callback-driven structural mutation during `PointerLeave` abort/requeue the stale reconciliation snapshot;
- two-UI isolation and hovered-subtree removal lifetime regressions are covered.

Exact implementation head `711156e64dfe62200765c79e63c096e65b3456c4` passed normal CI on Linux ASan+UBSan, Linux X11, Windows and macOS plus T042, T045, T052, T060 and both T065 qualification workflows. Final `CODE_REVIEW.md` review found no remaining Blocking/Important finding.

## T045 completion state

T045 / issue #45 / PR #210 is complete. The design choices are frozen rather than deferred to T068:

- macOS uses NSAccessibility;
- Windows uses UI Automation (UIA), not MSAA as the primary v1 architecture;
- Linux/X11 uses AT-SPI2 through the shared T072 D-Bus transport;
- ordinary and virtual semantic identities are stable logical IDs/tokens, never raw retained object addresses;
- native readers consume immutable snapshots and never synchronously traverse the live retained tree from arbitrary native threads;
- actions are marshalled to the owning UI thread and revalidated against current availability before execution;
- virtualized ListView metadata is immutable and shared by generation, so scrolling/selection/focus does not rebuild O(N) semantic item metadata;
- T068 may create native virtual-item proxies lazily but may not eagerly materialize 100k visual/native objects.

`docs/accessibility.md` is the normative detailed T045 mapping/design artifact and is referenced by `DESIGN.md`.

## Other critical lanes

```text
style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

dynamic/overlay:   T058(done) -> T061 -> T035 ------------------------> T068
                                      +-> T063 ------------------------> T068

critical platform: T065(done) -> T072(active) -> T064
                   T041(done) -> T043(active PR #142) -> T066
                                              |-------> T068
                   T065 + T072 + T043 + remaining feature deps -> T068 -> T069
```

T044 remains a T071 release dependency but is not on the current T069 convergence path. T069/T070/T071 remain dependency-gated and must not start early.

## Validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The default build must use an empty `NATIVEUI_ALLOWED_WARNINGS`. A code-changing completion candidate must use its exact current head for normal CI plus every relevant dedicated workflow named by the ticket, followed by the mandatory `CODE_REVIEW.md` pass.

## Next actions

1. Continue T067 / PR #219 as the Critical UI lane now that T045 is complete.
2. Continue T061 -> T035/T063 in the independent overlay lane because those converge on T068.
3. Continue T072 and T043 -> T064/T066 in the independent platform lane because those converge on T068/T069.
4. Continue T038 -> T039 and then T040 in the style lane as capacity permits.
5. Do not start T068 until every explicit dependency in issue #80 is Done.
6. Keep T069/T070/T071 dependency-gated and do not freeze the v1 API early.
