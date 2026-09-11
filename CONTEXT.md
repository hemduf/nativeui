# NativeUI compact recovery context

**Updated:** 2026-09-11

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
- `CODE_REVIEW.md`, `CI_POLICY.md`, a complete issue-to-code/test evidence matrix, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline and critical path

Current `main` includes T035 / PR #225 as squash commit `0412b9245a799fe873908d2659146ec4395f978e`, on top of the completed T067 startup regression closure in PR #233, generic feature-example auto-registration in PR #235, staged qualification policy in PR #230, and the already completed T043/T061/T067/T045/T037/T058/T036/T065/T034/T060 foundations.

T035 is complete. Its frozen executable candidate was `882d29a6f6a4798c36336ed167555e33c6cc594a`; normal CI run `34606092899`, T042 Lifecycle Stress run `34608497925` and T052 v0.1 Release Gate run `34608497792` all passed. The mandatory `CODE_REVIEW.md` review passed with no Blocking/Important finding, and issue #35 is closed with `status:done`.

T063 / issue #75 / PR #227 has a frozen executable candidate at `e907bd3119f59116522b1e672e690703c845f1e6`. Implementation, tests and CI wiring are complete; mandatory `CODE_REVIEW.md` review reports 0 Blocking, 0 Important and 0 Minor findings. Normal CI run `34618412866` is the exact-head qualification run for that executable candidate. Linux X11, Windows and Linux ASan/UBSan are green; macOS is the only remaining normal-CI job before the Draft -> Ready transition and final T042/T052 candidate gates.

Current convergence:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(done) -> T068
                                                   T058(done) ------------^

dynamic/overlay:   T058(done) -> T061(done)
                                      |-> T035(done) --------------------> T068
                                      |-> T063(frozen) ------------------> T068
                                      +-> T062

style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

critical platform: T065(done) -> T072 -> T064
                   T041(done) -> T043(done) -> T066
                                      |-------> T068

release:            convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

T063 consumes completed T061 plus T034. T062 consumes completed T061 plus T065 and is the next UI convergence item after T063 qualification/merge. T043 is complete, unlocks T066 and remains an explicit T068 dependency. T068 stays blocked until every dependency named by issue #80 is Done.

## Completed foundations relevant to v1

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- #64 / PR #90: standalone PROGRAM-world ownership Decision B.
- T059 / PR #89: inherited visibility/enabled/read-only state.
- T030–T034: Button, Checkbox/Radio, Slider/RangeSlider, ProgressBar/Meter and ScrollView baseline.
- T035 / PR #225: ComboBox + PopupMenu on T061 with immutable open snapshots, exact keyboard/pointer/focus policy, T059 ReadOnly split, close-before-callback reentrancy, deterministic golden coverage and standalone/embedded native smoke.
- T036 / PR #155: retained non-virtualized ListView + Tabs with stable selection and composite focus.
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
- T067 / PR #219 + PR #233: fixed-height virtualized ListView with bounded materialization; the 100k example is constrained to its real viewport and its production self-test is guarded by dedicated CI.
- #163 / PR #181: warning-free NativeUI-owned source-tree builds.
- #152 / PR #153: Tree no longer paints an implicit application background/help overlay.
- #234 / PR #235: canonical feature examples are auto-discovered by root CMake with deterministic ordering, `CONFIGURE_DEPENDS` and naming guards.

## T035 delivered contract

PR #225 implements ComboBox and PopupMenu as policy over the shared T061 in-view overlay stack; it introduces neither native popup windows nor a second popup manager.

Delivered behavior includes:

- immutable per-open ComboBox/PopupMenu snapshots;
- exact keyboard navigation/open/commit/Escape/Tab semantics with held-opener repeat suppression;
- pointer commit, disabled/separator handling and outside-click no-click-through;
- unmatched ComboBox state/placeholder behavior without implicit application-state rewrite;
- exact close/detach/focus-capture reconciliation before user state/callback invocation;
- stale-commit availability revalidation after Hidden/Collapsed/Disabled/destruction and T059 ReadOnly transitions;
- ReadOnly ComboBox cannot enter selection, while ReadOnly PopupMenu remains action-capable unless Disabled;
- per-anchor/per-UI state and two-UI isolation with no mutable global popup registry;
- reentrant callbacks that may remove their anchor or open another overlay safely;
- current-Tree theme binding for dynamically inserted T058/T061 subtrees;
- deterministic T035 golden probes and `t035_combo_popup --self-test`;
- real standalone + EmbeddedView smoke on the shared implementation.

## T063 frozen contract

PR #227 implements Dialog as a thin per-UI policy over T061; it does not introduce a second modal manager or a native dialog window.

Frozen behavior includes:

- one active T063 Dialog slot per `ui::UI`, with deterministic `Busy` rejection and independent simultaneous UIs;
- public non-template string action IDs/results with validation for missing body, empty/duplicate IDs and duplicate Default/Cancel roles;
- centered T061 modal presentation with styleable full-viewport backdrop, 560 px maximum width and 24 px viewport margins;
- fixed title/action chrome and mandatory T034 `ScrollView` for bounded body overflow;
- outside-pointer consumption with no dismissal/click-through;
- enabled Default initial focus, body-first fallback, trapped Tab/Shift+Tab and T061 deterministic focus restoration/fallback;
- Escape -> enabled Cancel action, otherwise Dismissed; focused TextInput/TextArea/custom child gets first refusal on Enter before Default fallback;
- exactly-once keyboard/pointer/programmatic completion and explicit live-controller destruction -> one Dismissed completion;
- UI deactivate/destruction abandons internal Dialog state without application completion or focus restoration into a deactivating UI;
- retained subtree detach/unmount and per-UI slot release complete before user callback; reentrant callback may open another Dialog or destroy the invoking UI;
- exception-safe `show()` acquisition so construction or overlay-show failure cannot strand the per-UI slot;
- deterministic `t063_dialog --self-test`, headless sizing/backdrop/scroll/fixed-chrome probes and real standalone + EmbeddedView native smoke wired into Linux/macOS CI.

## Other critical lanes

- T063 is implementation-complete on frozen candidate `e907bd3`; only normal macOS CI, Ready-only T042/T052 qualification, evidence sync and merge remain.
- T062 is the next UI convergence item after T063.
- T044 / issue #44 / PR #145 remains an independent T071 release dependency and needs current-main reconciliation plus its own exact-head evidence.
- Platform lane: continue T072, then T064; T066 is independently unblocked by completed T043.
- Styling lane: T038 -> T039; T040 consumes completed T065 rather than introducing another scheduler.
- T069/T070/T071 remain dependency-gated; do not freeze the v1 API early.

## Validation policy

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

During active development, keep code-changing PRs Draft and run normal CI plus only path-relevant dedicated checks. Once production source/tests/build/workflows are frozen, obtain green normal CI and the mandatory `CODE_REVIEW.md` review, then transition Draft -> Ready to launch the heavyweight final-candidate T042/T052 qualification. A later executable/build/workflow change invalidates that candidate. Pure project-state completion documentation does not invalidate an otherwise green frozen executable candidate.

## Next actions

1. Finish normal CI run `34618412866` for T063 candidate `e907bd3`; if macOS is green, transition PR #227 Draft -> Ready without executable changes.
2. Require exact frozen-candidate T042 Lifecycle Stress and T052 v0.1 Release Gate to pass, then publish the final issue-to-code/test evidence matrix and merge PR #227.
3. Continue T062 after T063 according to the current UI convergence plan.
4. Keep T068 blocked until all explicit issue #80 dependencies are Done.
5. Continue the independent T072 -> T064 / T066 platform work and T038 -> T039 / T040 styling work within concurrency limits.
6. Reconcile/qualify T044 / PR #145 independently; do not reuse T043 evidence.
