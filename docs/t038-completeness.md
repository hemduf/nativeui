# T038 completion matrix

This file is the requirement -> implementation -> deterministic evidence matrix for issue #38. It is deliberately conservative: a green CI run does not make a requirement complete when a review still finds an untested or incorrect behavior.

Closeout reconciliation baseline: `e4b6a52d86e8a50f44438a5c28379e7ef1b8b8dd`. The current closeout batch adds exact checked/selected invalidation classification and deterministic regressions on top of that baseline; the PR review/event record owns the resulting exact qualification head because a file cannot embed the SHA of the commit that contains itself.

Normal CI plus the matching T044/T066/T067 iterative workflows were green on `e4b6a52d86e8a50f44438a5c28379e7ef1b8b8dd`. T038 remains Draft while the current correction head is qualified and independently reviewed.

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
| Checkbox / Radio | `CheckboxStyle` / `RadioStyle` | integrated | `checkbox_radio_tests.cpp`, `t038_choice_state_invalidation --self-test`, T038 golden selection slice | Corrected in current closeout batch; iterative CI pending |
| Slider / RangeSlider | `SliderStyle` | integrated | `t032_slider_visual_tests.cpp`, availability/invalidation regressions, T038 golden matrix | Complete |
| ProgressBar / Meter | `ProgressBarStyle` / `MeterStyle` | integrated | family tests plus availability closure coverage in `t038_closure_invalidation` | Complete |
| Toggle | `ToggleStyle` | integrated | existing golden plus `t038_toggle_invalidation --self-test` | Complete |
| TextInput | `TextInputStyle` | integrated | existing behavior tests, `t038_text_input_invalidation --self-test`, T038 golden matrix | Complete |
| TextArea | `TextAreaStyle` | integrated | TextArea behavior/candidate-position tests plus `t038_text_area_invalidation --self-test` | Complete |
| ScrollView scrollbar | `ScrollbarStyle` | integrated in `detail/scroll_view.inc` | T034/T038 scrollbar regression coverage and resolver self-test | Complete |
| ComboBox / MenuItem | `ComboBoxStyle` / `MenuItemStyle` | integrated in `combo_popup.hpp` | `t038_menu_item_invalidation --self-test` plus T035 interaction contracts | Complete on green `e4b6a52d...` evidence |
| ListView row | `ListViewStyle` | integrated in retained and virtual ListView paths | `t038_closure_invalidation --self-test` plus T067 contracts | Complete on green `e4b6a52d...` evidence |
| Tabs header | `TabsStyle` | integrated in `detail/widgets_list_tabs.inc` | equal-hover + layout-affecting hover cases in `t038_closure_invalidation --self-test` plus invariants | Complete on green `e4b6a52d...` evidence |

## Acceptance criteria

| Acceptance criterion | Deterministic evidence / remaining gap | Status |
| --- | --- | --- |
| Covered widgets obtain presentation through typed styles rather than hardcoded widget-specific state paint | Required-family consumption audit above | Complete |
| Resolver precedence is identical across families | Shared `resolve_interaction_state()` plus representative resolver assertions | Complete |
| Disabled wins over hover/pressed interaction presentation | Resolver precedence tests and disabled family state coverage | Complete |
| Focused + selected/checked/read-only combinations remain representable | Orthogonal typed patches exercised by representative families | Complete |
| Equal resolved style causes no invalidation | Explicit no-op regressions across Button/Toggle/TextInput/TextArea/ComboBox/MenuItem/Tabs/ListView plus current Checkbox/Radio checked/selected closeout cases | Corrected; current head CI pending |
| Paint-only style/state changes cause no layout invalidation | Representative interaction, availability, checked/selected and popup/list/tab cases classify paint separately from layout | Corrected; current head CI pending for final choice batch |
| Layout-affecting explicit/state style changes invalidate layout deterministically | Checkbox checked, Radio selected, MenuItem and Tabs geometry paths have deterministic layout regressions | Corrected; current head CI pending for final choice batch |
| Default normal/hover/pressed/focused variants preserve geometry | `t038_style_invariants --self-test` covers Button, Slider, TextInput and Tabs | Complete on iterative CI |
| Two instances carry independent explicit overrides | `t038_style_invariants --self-test` measures/renders independently styled Button/UI instances | Complete on iterative CI |
| Public API is strongly typed and backend-neutral | Typed style headers + `tests/headers/nativeui.cpp`; no platform/backend types in public style surface | Complete |

## Required tests and artifacts

- [x] Pure style-resolution precedence coverage.
- [x] Orthogonal focused/selected/checked/read-only resolver coverage.
- [x] Disabled transition/state precedence coverage.
- [x] State-aware invalidation matrix for layout-affecting Checkbox/Radio, popup MenuItem and Tabs state patches; the final Checkbox/Radio closeout cases are in the current qualification batch.
- [x] Equal-resolved-style no-invalidation fixtures for the audited families; current batch adds checked/selected choice coverage without raw component capture.
- [x] Default geometry-stability regression for representative Button/Slider/TextInput/Tabs families.
- [x] Two-instance explicit-style isolation regression.
- [x] Representative golden matrix for Button, Slider, TextInput and Checkbox selection state in `tests/golden/baselines/t038_widget_state_matrix.ppm`.
- [x] Dedicated `examples/features/t038_widget_styles.cpp` state/resolver/consumer demo with deterministic `--self-test`.
- [x] `e4b6a52d...` is based on the current main ancestry observed at closeout; final Integration sync/revalidation remains mandatory if main advances before merge.
- [x] Normal plus matching T044/T066/T067 iterative CI was green on `e4b6a52d...` before the current closeout correction.
- [ ] Re-run normal plus matching path-scoped iterative CI on the current closeout head.
- [ ] Perform independent final `CODE_REVIEW.md` audit on that exact head with no Blocking/Important finding.
- [ ] Synchronize issue status, `CONTEXT.md` and `ROADMAP.md` in the completion cycle.
- [ ] Freeze the executable candidate, transition Draft -> Ready, then obtain T042/T052 qualification.

## Reconciled prior review findings

1. **Checkbox/Radio interaction + lifetime:** interaction transitions already resolve before/after style instead of repainting blindly. Application-owned checked/selected observers now classify the actual resolved before/after choice state through a detached retained invalidation snapshot; the callback never captures the component and is deactivated on unmount. Intrinsic check/mark visibility is included so a real selection visual still repaints even when style fields are otherwise equal.
2. **Popup MenuItem:** retained row geometry follows the resolved highlighted/pressed/selected style and `t038_menu_item_invalidation --self-test` proves both geometry changes and an equal-resolved no-op.
3. **Tabs:** retained hover/focus/state paths classify resolved presentation/layout; `t038_closure_invalidation --self-test` proves equal hover is clean and a layout-affecting hover patch dirties layout + paint.
4. **Availability:** wrapper-only availability invalidation was removed; style-aware descendants classify effective before/after presentation. The executed `bc1f45a...` regression is addressed by `e4b6a52d...`, whose iterative CI is green.

## Final CODE_REVIEW status

Self-closeout has reconciled the known functional findings into deterministic fixtures and the current correction batch. Final independent PASS is intentionally **not** recorded here: it is exact-head evidence and must be produced only after the new iterative CI completes. Required audit areas remain instance isolation, globals/statics, UI-thread mutation, callback/reentrancy lifetime, backend neutrality, full acceptance coverage and zero Blocking/Important findings.

## Closure boundary

No new widget family, scoped inheritance, animation or styling feature belongs in T038. The remaining scope is exact-head qualification, independent final review, completion metadata and final-candidate T042/T052 gates. T039/T040 own later styling scope.
