# T038 completion matrix

This file is the requirement -> implementation -> deterministic evidence matrix for issue #38. T038 is now in closure-only mode: no additional widget family or styling scope belongs in this ticket.

Pre-reconciliation proof head: `d799c9a0141b7713734a061a9a11fa9cde79ac5d`.

## Fixed state model and resolver

| Requirement | Implementation | Test/evidence | Status |
| --- | --- | --- | --- |
| One shared `VisualState` with enabled/read-only/hovered/pressed/focused/selected/checked flags | `include/nativeui/style.hpp` | Public umbrella compile coverage plus family resolver tests | Complete before final `main` reconciliation |
| Interaction precedence `disabled > pressed > hovered > normal` | Shared `resolve_interaction_state()` in `style.hpp` | Button/choice/slider/TextInput/TextArea family tests plus `t038_widget_styles --self-test` | Complete before final `main` reconciliation |
| Focused/selected/checked/read-only remain orthogonal | Shared resolver plus typed family patches | Choice/Tabs/MenuItem/TextInput coverage and `t038_widget_styles --self-test` | Complete before final `main` reconciliation |
| T039-compatible inherited-base seam without scoped traversal | Typed `resolve_*_style(inherited, explicit, state)` APIs | `tests/headers/nativeui.cpp` and family resolver tests | Complete before final `main` reconciliation |

## Widget-family coverage

| Family required by #38 | Typed style/resolver | Widget consumption | Deterministic evidence | Status |
| --- | --- | --- | --- | --- |
| Button | `ButtonStyle` | integrated | `button_tests.cpp`, `t038_style_invariants --self-test`, T038 golden matrix | Complete |
| Checkbox / Radio | `CheckboxStyle` / `RadioStyle` | integrated | `checkbox_radio_tests.cpp`, T038 golden selection slice | Complete |
| Slider / RangeSlider | `SliderStyle` | integrated | `t032_slider_visual_tests.cpp`, invalidation regression, T038 golden matrix | Complete |
| ProgressBar / Meter | `ProgressBarStyle` / `MeterStyle` | integrated | family tests/public compile coverage | Complete |
| Toggle | `ToggleStyle` | integrated | existing golden plus `t038_toggle_invalidation --self-test` | Complete |
| TextInput | `TextInputStyle` | integrated | existing behavior tests, `t038_text_input_invalidation --self-test`, T038 golden matrix | Complete |
| TextArea | `TextAreaStyle` | integrated | TextArea behavior/candidate-position tests plus `t038_text_area_invalidation --self-test` | Complete |
| ScrollView scrollbar | `ScrollbarStyle` | integrated in `detail/scroll_view.inc` | T034/T038 scrollbar regression coverage and resolver self-test | Complete |
| ComboBox / MenuItem | `ComboBoxStyle` / `MenuItemStyle` | integrated in `combo_popup.hpp` | T035 interaction contracts plus `t038_widget_styles --self-test` | Complete |
| ListView row | `ListViewStyle` | integrated in `detail/widgets_list_tabs.inc` | `t038_widget_styles --self-test` retained consumer/render contract | Complete |
| Tabs header | `TabsStyle` | integrated in `detail/widgets_list_tabs.inc` | `t038_widget_styles --self-test` geometry/render contract plus invariants | Complete |

## Acceptance criteria

| Acceptance criterion | Deterministic evidence | Status |
| --- | --- | --- |
| Covered widgets obtain presentation through typed styles rather than hardcoded widget-specific state paint | Required-family consumption audit above | Complete |
| Resolver precedence is identical across families | Shared `resolve_interaction_state()` plus representative family resolver assertions | Complete |
| Disabled wins over hover/pressed interaction presentation | Resolver precedence tests and disabled family state coverage | Complete |
| Focused + selected/checked/read-only combinations remain representable | Orthogonal overlays exercised by choice/MenuItem/Tabs/TextInput resolver tests | Complete |
| Equal resolved style causes no invalidation | Button and Slider regressions plus Toggle/TextInput/TextArea dedicated invalidation self-tests | Complete |
| Paint-only changes do not invalidate layout | Button, Slider/RangeSlider, Checkbox/Radio, TextInput and TextArea regressions | Complete |
| Layout-affecting explicit/state style changes invalidate layout + paint | Same representative families exercise geometry-affecting variants | Complete |
| Default normal/hover/pressed/focused variants preserve geometry | `examples/features/t038_style_invariants.cpp --self-test` covers Button, Slider, TextInput and Tabs | Complete; exact-head normal CI was green at `90e1da69f2e3691178e36021281e9598d9552060` |
| Two instances carry independent explicit overrides | `t038_style_invariants --self-test` measures/renders two independently styled Button instances | Complete; exact-head normal CI was green at `90e1da69f2e3691178e36021281e9598d9552060` |
| Public API is strongly typed and backend-neutral | `style.hpp` + family typed style headers + `tests/headers/nativeui.cpp`; no platform/backend types in the style surface | Complete |

## Required tests and artifacts

- [x] Pure style-resolution precedence coverage.
- [x] Orthogonal focused/selected/checked/read-only coverage.
- [x] Disabled transition/state precedence coverage.
- [x] Paint-vs-layout invalidation coverage.
- [x] Equal-style no-invalidation coverage across representative families.
- [x] Default geometry-stability regression.
- [x] Two-instance explicit-style isolation regression.
- [x] Representative golden matrix for Button, Slider, TextInput and Checkbox selection state in `tests/golden/baselines/t038_widget_state_matrix.ppm`.
- [x] Dedicated `examples/features/t038_widget_styles.cpp` state/resolver/consumer demo with deterministic `--self-test`.
- [x] Pre-reconciliation normal CI on golden head `d799c9a0141b7713734a061a9a11fa9cde79ac5d`: CI run 1531 passed on macOS, Windows, Linux X11 and Linux ASan/UBSan.
- [ ] Reconcile the canonical branch with current `main`.
- [ ] Re-run normal plus path-relevant CI on the integrated candidate.
- [ ] Perform final `CODE_REVIEW.md` audit with no Blocking/Important finding.
- [ ] Synchronize issue status, `CONTEXT.md` and `ROADMAP.md` in the completion cycle.
- [ ] Freeze the executable candidate, transition Draft -> Ready, then obtain T042/T052 qualification.

## Closure boundary

Functional acceptance is closed on the pre-reconciliation branch. Remaining work is integration and release discipline only: current-main reconciliation, exact-head requalification, final review, status/docs synchronization, freeze, Ready/T042/T052, then merge. No new widget, state, styling capability, inheritance behavior or animation work is permitted in T038; T039/T040 own later styling scope.
