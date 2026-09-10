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

The T045 branch is synchronized with `main` `19c14970918105ebef059b41db8cdcfcb4b5de64`, which includes T037 / PR #151 on top of the completed T058, T036, T065, T034, T060/T042 lifecycle, T052 developer-preview release gate and warning-free source-tree baseline.

The UI/accessibility critical path is now:

```text
T034(done) -> T036(done) -> T045(completion PR #210) -> T067 -> T068
                              T058(done) -----------------^
```

T045 freezes the accessibility semantics architecture that T067 and T068 consume. Its implementation/design candidate provides:

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

The previous exact implementation head passed T045, normal CI, T060, T065, T042 and T052 workflows with a clean mandatory review. The branch was then refreshed from current main to preserve T037 and now requires completion-doc exact-head requalification before merge.

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
- T037 / PR #151: typed per-UI theme tokens and representative widget theme binding.
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

## T045 completion state

T045 / issue #45 / PR #210 is the active Critical UI lane item. The design choices are frozen rather than deferred to T068:

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

For T045, final completion requires exact-head T045 Accessibility Semantics, normal CI, T060 Application Contract, T065 dispatcher/platform qualification, T042 Lifecycle Stress and T052 Release Gate after the current-main/doc synchronization. No native accessibility smoke is claimed by T045; T068 owns production NSAccessibility/UIA/AT-SPI2 implementation and native qualification.

## Next actions

1. Finish T045 completion bookkeeping, exact-head qualification and final review on PR #210; merge autonomously when green.
2. Immediately start/resume T067 after T045 merges, because T034/T036/T058 are already complete.
3. Do not start T068 until every explicit dependency in issue #80 is Done; continue T067 while the independent overlay/platform lanes close their prerequisites.
4. Keep the Critical UI lane scoped to T045 -> T067 -> T068 and do not duplicate T035/T061/T063/T043/T065/T072 work owned by other lanes.
