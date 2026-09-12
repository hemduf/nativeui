# T038 completion matrix

This file is the live requirement -> implementation -> test/evidence matrix for issue #38. It is intentionally conservative: a row is complete only when the required widget/API is implemented, covered by deterministic tests, and represented in the final exact-head qualification evidence.

## Fixed state model and resolver

| Requirement | Implementation | Test/evidence | Status |
| --- | --- | --- | --- |
| One shared `VisualState` with enabled/read-only/hovered/pressed/focused/selected/checked flags | `include/nativeui/style.hpp` | Public umbrella compile coverage and family resolver tests in the T038 branch | Implemented; final exact-head evidence pending |
| Interaction precedence `disabled > pressed > hovered > normal` | Shared resolver in `style.hpp` | Existing family resolver coverage | Implemented; final exact-head evidence pending |
| Focused/selected/checked/read-only remain orthogonal | Shared resolver plus typed family patches | Existing family resolver coverage | Implemented; final exact-head evidence pending |
| T039-compatible inherited-base seam without scoped traversal | Typed `resolve_*_style(inherited, explicit, state)` APIs | Public-header compile coverage | Implemented; final exact-head evidence pending |

## Widget-family coverage

| Family required by #38 | Typed style/resolver | Widget consumption | Deterministic tests | Final status |
| --- | --- | --- | --- | --- |
| Button | `ButtonStyle` | integrated | button tests / public compile coverage / T038 feature invalidation slice | Implemented; final qualification pending |
| Checkbox / Radio | `CheckboxStyle` / `RadioStyle` | integrated | choice tests / public compile coverage | Implemented; final qualification pending |
| Slider / RangeSlider | `SliderStyle` | integrated | slider visual tests / public compile coverage | Implemented; final qualification pending |
| ProgressBar / Meter | `ProgressBarStyle` / `MeterStyle` | integrated | progress/meter branch coverage / public compile coverage | Implemented; final qualification pending |
| Toggle | `ToggleStyle` | integrated | existing Toggle golden/regression coverage | Implemented; final qualification pending |
| TextInput | `TextInputStyle` | integrated | existing TextInput behavior plus public compile coverage | Implemented; representative T038 golden still pending |
| TextArea | `TextAreaStyle` | integrated | TextArea behavior/candidate-position regressions plus public compile coverage | Implemented; final qualification pending |
| ScrollView scrollbar | `ScrollbarStyle` | integrated in `detail/scroll_view.inc` | T034/T038 scrollbar style regression coverage | Implemented on branch; exact-head qualification pending |
| ComboBox / MenuItem | `ComboBoxStyle` / `MenuItemStyle` | integrated in `combo_popup.hpp` | existing T035 interaction contracts plus T038 style coverage | Implemented on branch; exact-head qualification pending |
| ListView row | `ListViewStyle` | integrated in `detail/widgets_list_tabs.inc` | `t038_widget_styles --self-test` consumer/render contract | Implemented on branch; exact-head qualification pending |
| Tabs header | `TabsStyle` | integrated in `detail/widgets_list_tabs.inc` | `t038_widget_styles --self-test` geometry/render contract | Implemented on branch; exact-head qualification pending |

## Cross-family acceptance criteria

| Acceptance criterion | Evidence required before completion | Status |
| --- | --- | --- |
| Covered widgets obtain presentation through typed styles instead of hardcoded paint constants | Audit all required families on the integrated branch | Implemented families present; final audit pending |
| Resolver precedence identical across families | Pure precedence matrix covering representative family resolvers | Partially covered; final matrix pending |
| Disabled wins and pressed presentation is not retained misleadingly | Explicit disabled transition coverage | Partially covered; final integrated proof pending |
| Focused + selected/checked combinations are representable | Orthogonal-state matrix | Partially covered; final matrix pending |
| Equal resolved style causes no invalidation | Dedicated invalidation regression | Open |
| Paint-only changes do not invalidate layout | Dedicated invalidation regression | Button hover slice added; cross-family proof and exact-head evidence pending |
| Layout-affecting explicit style changes invalidate layout + paint | Dedicated invalidation regression | Button hover slice added with allocation-free layout-signature comparison; cross-family proof and exact-head evidence pending |
| Default normal/hover/pressed/focused variants preserve geometry | Cross-family geometry-stability matrix | Open |
| Two instances can carry independent explicit overrides | Two-instance isolation regression | Open |
| Public API is strongly typed and backend-neutral | Public-header compile audit across all families | In progress; final audit pending |

## Required completion artifacts

- [x] Integrate `ListViewStyle` into retained ListView row/surface presentation.
- [x] Integrate `TabsStyle` into retained Tabs header/panel presentation.
- [ ] Complete explicit equal-style/no-invalidation and paint-vs-layout invalidation tests across the representative families. The Button hover paint-vs-layout slice is now present, but the criterion is not yet complete.
- [ ] Add default geometry-stability and two-instance explicit-style isolation tests.
- [ ] Complete representative golden matrix for Button, Slider, TextInput and one selection widget.
- [ ] Finish `examples/features/t038_widget_styles.cpp` state-matrix demo and deterministic `--self-test` acceptance coverage.
- [ ] Reconcile the canonical branch with current `main` before final qualification.
- [ ] Run latest normal/path-relevant CI on the frozen executable candidate.
- [ ] Perform final `CODE_REVIEW.md` audit with no Blocking/Important finding.
- [ ] Transition Draft -> Ready and obtain required T042/T052 final-candidate qualification.
- [ ] Synchronize issue status, `CONTEXT.md` and `ROADMAP.md` in the completion cycle.

## Current recovery note

PR #218 remains Draft. All required standard widget families now have typed resolver + concrete consumption on the branch, including the virtualized ListView style propagation added after the retained ListView/Tabs slice. The current bounded cross-family phase has started with a Button invalidation contract: test-first commit `51ec9d350c3a68789b642232859e575838e1491a` requires paint-only hover variants to remain paint-only and geometry-changing hover variants to dirty layout; implementation `fb8a6cc965cf6d8838e6f10fa27db67dc3405525` added the first layout-aware transition, and review correction `ebffc44b65e9f2d73b477598b5536b8c7d519ab8` replaced allocation-capable resolved-style comparisons on the hot input/focus path with an allocation-free layout signature. No exact-head CI result is claimed for this slice: the canonical branch is still behind current `main` and the PR is presently non-mergeable, so final/iterative executable qualification must follow a safe reconciliation. Equal-resolved-style suppression, broader cross-family invalidation, geometry/isolation, representative goldens, final example acceptance, final review and final qualification remain open. Do not merge from this state.
