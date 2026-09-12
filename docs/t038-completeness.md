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
| Checkbox / Radio | `CheckboxStyle` / `RadioStyle` | integrated | choice tests / public compile coverage / T038 paint-vs-layout interaction regression | Implemented; latest GREEN exact-head qualification pending |
| Slider / RangeSlider | `SliderStyle` | integrated | slider visual tests / public compile coverage / T038 equal-style + layout invalidation regression | Implemented; final qualification pending |
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
| Equal resolved style causes no invalidation | Dedicated invalidation regression | Slider interaction slice covered; broader representative proof pending |
| Paint-only changes do not invalidate layout | Dedicated invalidation regression | Button, Slider/RangeSlider, Checkbox/Radio, TextInput and TextArea interaction slices covered; final representative proof/evidence pending |
| Layout-affecting explicit style changes invalidate layout + paint | Dedicated invalidation regression | Button, Slider/RangeSlider, Checkbox/Radio, TextInput and TextArea interaction slices use allocation-free layout classification; final representative proof/evidence pending |
| Default normal/hover/pressed/focused variants preserve geometry | Cross-family geometry-stability matrix | `t038_style_invariants --self-test` now covers Button, Slider, TextInput and Tabs default layout fields; exact-head CI pending |
| Two instances can carry independent explicit overrides | Two-instance isolation regression | `t038_style_invariants --self-test` now measures two independently styled Button instances; exact-head CI pending |
| Public API is strongly typed and backend-neutral | Public-header compile audit across all families | In progress; final audit pending |

## Required completion artifacts

- [x] Integrate `ListViewStyle` into retained ListView row/surface presentation.
- [x] Integrate `TabsStyle` into retained Tabs header/panel presentation.
- [ ] Complete explicit equal-style/no-invalidation and paint-vs-layout invalidation tests across the representative families. Button, Slider/RangeSlider, Checkbox/Radio, TextInput and TextArea interaction slices are now present, but the criterion is not yet complete.
- [ ] Qualify the new default geometry-stability and two-instance explicit-style isolation regression on an exact head. Test source is present in `examples/features/t038_style_invariants.cpp` at `4f0ab3d08105e5e14ff6fb1f736d04b04f79a5b1`; CI evidence is still pending.
- [ ] Complete representative golden matrix for Button, Slider, TextInput and one selection widget.
- [ ] Finish `examples/features/t038_widget_styles.cpp` state-matrix demo and deterministic `--self-test` acceptance coverage.
- [ ] Reconcile the canonical branch with current `main` before final qualification.
- [ ] Run latest normal/path-relevant CI on the frozen executable candidate.
- [ ] Perform final `CODE_REVIEW.md` audit with no Blocking/Important finding.
- [ ] Transition Draft -> Ready and obtain required T042/T052 final-candidate qualification.
- [ ] Synchronize issue status, `CONTEXT.md` and `ROADMAP.md` in the completion cycle.

## Current recovery note

PR #218 remains Draft. All required standard widget families have typed resolver + concrete consumption on the branch, including virtualized ListView style propagation. The bounded cross-family invalidation phase now has explicit TDD slices for Button, Slider/RangeSlider, Checkbox/Radio, TextInput and TextArea. The latest exact source head before this matrix is `4f0ab3d08105e5e14ff6fb1f736d04b04f79a5b1`, which adds deterministic public-API acceptance coverage for default interaction geometry stability across Button/Slider/TextInput/Tabs plus two-instance explicit Button-style isolation. No exact-head CI result is claimed for that new source head yet.

The canonical branch still requires final reconciliation with current `main`. Broader equal-resolved-style proof, representative goldens, final example acceptance, final review and final qualification remain open. Do not merge from this state.
