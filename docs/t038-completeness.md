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
| Button | `ButtonStyle` | integrated | button tests / public compile coverage | Implemented; final qualification pending |
| Checkbox / Radio | `CheckboxStyle` / `RadioStyle` | integrated | choice tests / public compile coverage | Implemented; final qualification pending |
| Slider / RangeSlider | `SliderStyle` | integrated | slider visual tests / public compile coverage | Implemented; final qualification pending |
| ProgressBar / Meter | `ProgressBarStyle` / `MeterStyle` | integrated | progress/meter branch coverage / public compile coverage | Implemented; final qualification pending |
| Toggle | `ToggleStyle` | integrated | existing Toggle golden/regression coverage | Implemented; final qualification pending |
| TextInput | `TextInputStyle` | integrated | existing TextInput behavior plus public compile coverage | Implemented; representative T038 golden still pending |
| TextArea | `TextAreaStyle` | integrated | TextArea behavior/candidate-position regressions plus public compile coverage | Implemented; final qualification pending |
| ScrollView scrollbar | `ScrollbarStyle` | integrated in `detail/scroll_view.inc` | T034/T038 scrollbar style regression coverage | Implemented on branch; exact-head qualification pending |
| ComboBox / MenuItem | `ComboBoxStyle` / `MenuItemStyle` | integrated in `combo_popup.hpp` | existing T035 interaction contracts plus T038 style coverage | Implemented on branch; exact-head qualification pending |
| ListView row | `ListViewStyle` | **not yet consumed by `widgets_list_tabs.inc`** | missing T038 integration tests | **Open** |
| Tabs header | `TabsStyle` | **not yet consumed by `widgets_list_tabs.inc`** | missing T038 integration tests | **Open** |

## Cross-family acceptance criteria

| Acceptance criterion | Evidence required before completion | Status |
| --- | --- | --- |
| Covered widgets obtain presentation through typed styles instead of hardcoded paint constants | Audit all required families after ListView/Tabs integration | Open |
| Resolver precedence identical across families | Pure precedence matrix covering representative family resolvers | Partially covered; final matrix pending |
| Disabled wins and pressed presentation is not retained misleadingly | Explicit disabled transition coverage | Partially covered; final integrated proof pending |
| Focused + selected/checked combinations are representable | Orthogonal-state matrix | Partially covered; final matrix pending |
| Equal resolved style causes no invalidation | Dedicated invalidation regression | Open |
| Paint-only changes do not invalidate layout | Dedicated invalidation regression | Open |
| Layout-affecting explicit style changes invalidate layout + paint | Dedicated invalidation regression | Open |
| Default normal/hover/pressed/focused variants preserve geometry | Cross-family geometry-stability matrix | Open |
| Two instances can carry independent explicit overrides | Two-instance isolation regression | Open |
| Public API is strongly typed and backend-neutral | Public-header compile audit across all families | In progress; final audit pending |

## Required completion artifacts

- [ ] Integrate `ListViewStyle` into retained ListView row/surface presentation.
- [ ] Integrate `TabsStyle` into retained Tabs header/panel presentation.
- [ ] Add explicit equal-style/no-invalidation and paint-vs-layout invalidation tests.
- [ ] Add default geometry-stability and two-instance explicit-style isolation tests.
- [ ] Complete representative golden matrix for Button, Slider, TextInput and one selection widget.
- [ ] Finish `examples/features/t038_widget_styles.cpp` state-matrix demo and deterministic `--self-test` acceptance coverage.
- [ ] Reconcile the canonical branch with current `main` before final qualification.
- [ ] Run latest normal/path-relevant CI on the frozen executable candidate.
- [ ] Perform final `CODE_REVIEW.md` audit with no Blocking/Important finding.
- [ ] Transition Draft -> Ready and obtain required T042/T052 final-candidate qualification.
- [ ] Synchronize issue status, `CONTEXT.md` and `ROADMAP.md` in the completion cycle.

## Current recovery note

At the checkpoint that created this matrix, PR #218 is still Draft. The current source head contains the ComboBox/PopupMenu hit-test exception-safety correction, while ListView/Tabs integration and the cross-family completion gates above remain intentionally open. Do not merge from this state.
