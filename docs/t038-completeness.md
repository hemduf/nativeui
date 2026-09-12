# T038 completion matrix

This file is the requirement -> implementation -> deterministic evidence matrix for issue #38. It is deliberately conservative: a green CI run does not make a requirement complete when a review still finds an untested or incorrect behavior.

Current reconciled review head: `b884c95450aa76de496ed19355bdabd2563c0d0c`.

The branch is synchronized with current `main` and normal/path-scoped iterative validation is green on that executable head. Final completeness review `5187330530` nevertheless reopened state-aware invalidation requirements described below. T038 remains Draft and must not transition to Ready until those findings are corrected test-first.

## Fixed state model and resolver

| Requirement | Implementation | Test/evidence | Status |
| --- | --- | --- | --- |
| One shared `VisualState` with enabled/read-only/hovered/pressed/focused/selected/checked flags | `include/nativeui/style.hpp` | Public umbrella compile coverage plus family resolver tests | Complete |
| Interaction precedence `disabled > pressed > hovered > normal` | Shared `resolve_interaction_state()` in `style.hpp` | Button/choice/slider/TextInput/TextArea family tests plus `t038_widget_styles --self-test` | Complete |
| Focused/selected/checked/read-only remain orthogonal | Shared resolver plus typed family patches | Choice/Tabs/MenuItem/TextInput resolver coverage | Complete |
| T039-compatible inherited-base seam without scoped traversal | Typed `resolve_*_style(inherited, explicit, state)` APIs | `tests/headers/nativeui.cpp` and family resolver tests | Complete |

## Widget-family coverage

| Family required by #38 | Typed style/resolver | Widget consumption | Deterministic evidence | Status |
| --- | --- | --- | --- | --- |
| Button | `ButtonStyle` | integrated | `button_tests.cpp`, `t038_style_invariants --self-test`, T038 golden matrix | Complete |
| Checkbox / Radio | `CheckboxStyle` / `RadioStyle` | integrated | `checkbox_radio_tests.cpp`, T038 golden selection slice | **Invalidation review reopened** |
| Slider / RangeSlider | `SliderStyle` | integrated | `t032_slider_visual_tests.cpp`, invalidation regression, T038 golden matrix | Complete for current audited interaction paths |
| ProgressBar / Meter | `ProgressBarStyle` / `MeterStyle` | integrated | family tests/public compile coverage | Integrated; availability/layout audit remains part of final review |
| Toggle | `ToggleStyle` | integrated | existing golden plus `t038_toggle_invalidation --self-test` | Complete for current audited interaction paths |
| TextInput | `TextInputStyle` | integrated | existing behavior tests, `t038_text_input_invalidation --self-test`, T038 golden matrix | Complete for current audited interaction paths |
| TextArea | `TextAreaStyle` | integrated | TextArea behavior/candidate-position tests plus `t038_text_area_invalidation --self-test` | Complete for current audited interaction paths |
| ScrollView scrollbar | `ScrollbarStyle` | integrated in `detail/scroll_view.inc` | T034/T038 scrollbar regression coverage and resolver self-test | Complete |
| ComboBox / MenuItem | `ComboBoxStyle` / `MenuItemStyle` | integrated in `combo_popup.hpp` | T035 interaction contracts plus `t038_widget_styles --self-test` | **State-variant geometry/invalidation review reopened** |
| ListView row | `ListViewStyle` | integrated in retained and virtual ListView paths | T038/T067 visual and consumer contracts | Complete for audited presentation paths |
| Tabs header | `TabsStyle` | integrated in `detail/widgets_list_tabs.inc` | `t038_widget_styles --self-test` plus invariants | **Focused/hover invalidation review reopened** |

## Acceptance criteria

| Acceptance criterion | Deterministic evidence / remaining gap | Status |
| --- | --- | --- |
| Covered widgets obtain presentation through typed styles rather than hardcoded widget-specific state paint | Required-family consumption audit above | Complete |
| Resolver precedence is identical across families | Shared `resolve_interaction_state()` plus representative resolver assertions | Complete |
| Disabled wins over hover/pressed interaction presentation | Resolver precedence tests and disabled family state coverage | Complete |
| Focused + selected/checked/read-only combinations remain representable | Orthogonal typed patches exercised by representative families | Complete |
| Equal resolved style causes no invalidation | Button/Toggle/TextInput/TextArea have explicit no-op regressions, but Checkbox/Radio still use `PressActivationState` with unconditional visual invalidation; popup/Tabs hover paths also require final classification evidence | **Open — Important review finding** |
| Paint-only style/state changes cause no layout invalidation | Covered by corrected representative families; must remain true in the reopened Checkbox/Radio/popup/Tabs fixes | **Open until reopened families are proven** |
| Layout-affecting explicit/state style changes invalidate layout deterministically | Checkbox checked and Radio selected state changes currently notify only a paint invalidator; popup MenuItem state patches expose row/text geometry while highlight transitions are paint-only; Tabs focused patch can change container geometry without layout classification | **Open — Important review finding** |
| Default normal/hover/pressed/focused variants preserve geometry | `t038_style_invariants --self-test` covers Button, Slider, TextInput and Tabs | Complete on iterative CI |
| Two instances carry independent explicit overrides | `t038_style_invariants --self-test` measures/renders independently styled Button/UI instances | Complete on iterative CI |
| Public API is strongly typed and backend-neutral | Typed style headers + `tests/headers/nativeui.cpp`; no platform/backend types in public style surface | Complete |

## Required tests and artifacts

- [x] Pure style-resolution precedence coverage.
- [x] Orthogonal focused/selected/checked/read-only resolver coverage.
- [x] Disabled transition/state precedence coverage.
- [ ] Complete state-aware invalidation matrix for every family that exposes layout-affecting state patches. Current blockers: Checkbox/Radio checked/selected and interaction no-op classification, popup MenuItem highlighted/pressed geometry, Tabs focus/hover classification.
- [ ] Complete equal-resolved-style no-invalidation proof for the reopened families.
- [x] Default geometry-stability regression for representative Button/Slider/TextInput/Tabs families.
- [x] Two-instance explicit-style isolation regression.
- [x] Representative golden matrix for Button, Slider, TextInput and Checkbox selection state in `tests/golden/baselines/t038_widget_state_matrix.ppm`.
- [x] Dedicated `examples/features/t038_widget_styles.cpp` state/resolver/consumer demo with deterministic `--self-test`.
- [x] Reconcile the canonical branch with current `main`.
- [x] Re-run normal plus currently path-relevant iterative CI on reconciled executable head `b884c95450aa76de496ed19355bdabd2563c0d0c`.
- [ ] Correct findings from final-completeness review `5187330530` through RED -> GREEN -> REFACTOR and re-run affected normal/path-scoped validation.
- [ ] Perform the final `CODE_REVIEW.md` audit after those corrections with no Blocking/Important finding.
- [ ] Synchronize issue status, `CONTEXT.md` and `ROADMAP.md` in the completion cycle.
- [ ] Freeze the executable candidate, transition Draft -> Ready, then obtain T042/T052 qualification.

## Current review blockers

1. **Checkbox/Radio:** `PressActivationState` still performs raw interaction repainting for these components, so an equal resolved style can dirty paint. Their application-owned checked/selected state observers also use paint-only invalidation even though checked/selected patches can change measured geometry. The activation path compares layout before the state mutation, so user activation misses the same layout transition.
2. **Popup MenuItem state:** selected/hovered/pressed patches can override row height, padding and text metrics, but highlight transitions currently repaint only and base-only geometry drives measurement/hit testing. The typed contract and retained geometry must be made consistent.
3. **Tabs:** orthogonal focused/read-only/disabled patches can contain container layout fields such as header height/panel gap. Focus/hover transitions do not yet provide the same resolved-style invalidation classification required by #38.
4. **Lifetime safety:** fixes for application-owned state transitions must not solve the problem by capturing a raw component pointer in a long-lived `State` observer. `State::set()` copies listeners before dispatch, so callback-after-removal safety must remain explicit.

## Closure boundary

No new widget family, scoped inheritance, animation or styling feature should enter T038. The remaining scope is correction and proof of the already-declared #38 invalidation contract, followed by final review, completion metadata and final-candidate qualification. T039/T040 own later styling scope.
