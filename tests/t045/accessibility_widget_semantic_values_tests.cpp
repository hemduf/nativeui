#include <nativeui/detail/semantic_widget_info.hpp>
#include <nativeui/semantics.hpp>

#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define ACCESSIBILITY_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

void label_value_contract() {
    const auto info = ui::detail::label_semantic_info("Status ready");
    ACCESSIBILITY_CHECK(info.role == ui::SemanticRole::Text);
    ACCESSIBILITY_CHECK(info.name == "Status ready");
    ACCESSIBILITY_CHECK(!info.focusable);
    ACCESSIBILITY_CHECK(info.actions.empty());
}

void checkbox_value_contract() {
    const auto checked = ui::detail::checkbox_semantic_info("Enabled", true);
    ACCESSIBILITY_CHECK(checked.role == ui::SemanticRole::Checkbox);
    ACCESSIBILITY_CHECK(checked.name == "Enabled");
    ACCESSIBILITY_CHECK(checked.checked == ui::SemanticCheckedState::Checked);
    ACCESSIBILITY_CHECK(checked.focusable);
    ACCESSIBILITY_CHECK(checked.supports(ui::SemanticAction::Toggle));
    ACCESSIBILITY_CHECK(checked.supports(ui::SemanticAction::Focus));
    ACCESSIBILITY_CHECK(!checked.supports(ui::SemanticAction::Activate));

    const auto unchecked = ui::detail::checkbox_semantic_info("Enabled", false);
    ACCESSIBILITY_CHECK(unchecked.checked == ui::SemanticCheckedState::Unchecked);
}

void radio_value_contract() {
    const auto selected = ui::detail::radio_button_semantic_info("Mode A", true);
    ACCESSIBILITY_CHECK(selected.role == ui::SemanticRole::RadioButton);
    ACCESSIBILITY_CHECK(selected.name == "Mode A");
    ACCESSIBILITY_CHECK(selected.selected);
    ACCESSIBILITY_CHECK(selected.checked == ui::SemanticCheckedState::Checked);
    ACCESSIBILITY_CHECK(selected.focusable);
    ACCESSIBILITY_CHECK(selected.supports(ui::SemanticAction::Select));
    ACCESSIBILITY_CHECK(selected.supports(ui::SemanticAction::Focus));
    ACCESSIBILITY_CHECK(!selected.supports(ui::SemanticAction::Toggle));

    const auto unselected = ui::detail::radio_button_semantic_info("Mode A", false);
    ACCESSIBILITY_CHECK(!unselected.selected);
    ACCESSIBILITY_CHECK(unselected.checked == ui::SemanticCheckedState::Unchecked);
}

void toggle_value_contract() {
    const auto checked = ui::detail::toggle_semantic_info("Bypass", true);
    ACCESSIBILITY_CHECK(checked.role == ui::SemanticRole::Toggle);
    ACCESSIBILITY_CHECK(checked.name == "Bypass");
    ACCESSIBILITY_CHECK(checked.checked == ui::SemanticCheckedState::Checked);
    ACCESSIBILITY_CHECK(checked.focusable);
    ACCESSIBILITY_CHECK(checked.supports(ui::SemanticAction::Toggle));
    ACCESSIBILITY_CHECK(checked.supports(ui::SemanticAction::Focus));
    ACCESSIBILITY_CHECK(!checked.supports(ui::SemanticAction::Activate));

    const auto unchecked = ui::detail::toggle_semantic_info("Bypass", false);
    ACCESSIBILITY_CHECK(unchecked.checked == ui::SemanticCheckedState::Unchecked);
}

void slider_value_contract() {
    const auto info = ui::detail::slider_semantic_info(0.25f, -1.0f, 1.0f, 0.25f);
    ACCESSIBILITY_CHECK(info.role == ui::SemanticRole::Slider);
    ACCESSIBILITY_CHECK(info.numeric_value.has_value());
    ACCESSIBILITY_CHECK(*info.numeric_value == 0.25);
    ACCESSIBILITY_CHECK(info.value_range.has_value());
    ACCESSIBILITY_CHECK(info.value_range->minimum == -1.0);
    ACCESSIBILITY_CHECK(info.value_range->maximum == 1.0);
    ACCESSIBILITY_CHECK(info.value_range->step == 0.25);
    ACCESSIBILITY_CHECK(info.focusable);
    ACCESSIBILITY_CHECK(info.supports(ui::SemanticAction::Increment));
    ACCESSIBILITY_CHECK(info.supports(ui::SemanticAction::Decrement));
    ACCESSIBILITY_CHECK(info.supports(ui::SemanticAction::SetValue));
    ACCESSIBILITY_CHECK(info.supports(ui::SemanticAction::Focus));
    ACCESSIBILITY_CHECK(!info.supports(ui::SemanticAction::Toggle));

    const auto clamped = ui::detail::slider_semantic_info(5.0f, -1.0f, 1.0f, 0.25f);
    ACCESSIBILITY_CHECK(clamped.numeric_value.has_value());
    ACCESSIBILITY_CHECK(*clamped.numeric_value == 1.0);
}

void bounded_display_value_contract() {
    const auto progress = ui::detail::bounded_display_semantic_info(
        0.25f, 0.0f, 1.0f, false);
    ACCESSIBILITY_CHECK(progress.role == ui::SemanticRole::ProgressBar);
    ACCESSIBILITY_CHECK(progress.numeric_value.has_value());
    ACCESSIBILITY_CHECK(*progress.numeric_value == 0.25);
    ACCESSIBILITY_CHECK(progress.value_range.has_value());
    ACCESSIBILITY_CHECK(progress.value_range->minimum == 0.0);
    ACCESSIBILITY_CHECK(progress.value_range->maximum == 1.0);
    ACCESSIBILITY_CHECK(progress.value_range->step == 0.0);
    ACCESSIBILITY_CHECK(!progress.focusable);
    ACCESSIBILITY_CHECK(progress.actions.empty());

    const auto meter = ui::detail::bounded_display_semantic_info(
        4.0f, -1.0f, 1.0f, true);
    ACCESSIBILITY_CHECK(meter.role == ui::SemanticRole::Meter);
    ACCESSIBILITY_CHECK(meter.numeric_value.has_value());
    ACCESSIBILITY_CHECK(*meter.numeric_value == 1.0);
    ACCESSIBILITY_CHECK(meter.value_range.has_value());
    ACCESSIBILITY_CHECK(meter.value_range->minimum == -1.0);
    ACCESSIBILITY_CHECK(meter.value_range->maximum == 1.0);
    ACCESSIBILITY_CHECK(meter.actions.empty());

    const auto non_finite = ui::detail::bounded_display_semantic_info(
        std::numeric_limits<float>::quiet_NaN(), -2.0f, 2.0f, false);
    ACCESSIBILITY_CHECK(non_finite.numeric_value.has_value());
    ACCESSIBILITY_CHECK(*non_finite.numeric_value == -2.0);
}

void text_edit_value_contract() {
    const auto single_line = ui::detail::text_edit_semantic_info(
        "Name", "Ada", false);
    ACCESSIBILITY_CHECK(single_line.role == ui::SemanticRole::TextInput);
    ACCESSIBILITY_CHECK(single_line.name == "Name");
    ACCESSIBILITY_CHECK(single_line.text_value.has_value());
    ACCESSIBILITY_CHECK(*single_line.text_value == "Ada");
    ACCESSIBILITY_CHECK(single_line.focusable);
    ACCESSIBILITY_CHECK(single_line.supports(ui::SemanticAction::SetValue));
    ACCESSIBILITY_CHECK(single_line.supports(ui::SemanticAction::Focus));

    const auto multiline = ui::detail::text_edit_semantic_info(
        "Notes", "line one\nline two", true);
    ACCESSIBILITY_CHECK(multiline.role == ui::SemanticRole::TextArea);
    ACCESSIBILITY_CHECK(multiline.name == "Notes");
    ACCESSIBILITY_CHECK(multiline.text_value.has_value());
    ACCESSIBILITY_CHECK(*multiline.text_value == "line one\nline two");
    ACCESSIBILITY_CHECK(multiline.focusable);
    ACCESSIBILITY_CHECK(multiline.supports(ui::SemanticAction::SetValue));
    ACCESSIBILITY_CHECK(multiline.supports(ui::SemanticAction::Focus));
}

void combo_box_value_contract() {
    const auto collapsed = ui::detail::combo_box_semantic_info("Mode A", false);
    ACCESSIBILITY_CHECK(collapsed.role == ui::SemanticRole::ComboBox);
    ACCESSIBILITY_CHECK(collapsed.text_value.has_value());
    ACCESSIBILITY_CHECK(*collapsed.text_value == "Mode A");
    ACCESSIBILITY_CHECK(collapsed.expanded == ui::SemanticExpandedState::Collapsed);
    ACCESSIBILITY_CHECK(collapsed.focusable);
    ACCESSIBILITY_CHECK(collapsed.supports(ui::SemanticAction::Expand));
    ACCESSIBILITY_CHECK(collapsed.supports(ui::SemanticAction::Collapse));
    ACCESSIBILITY_CHECK(collapsed.supports(ui::SemanticAction::Select));
    ACCESSIBILITY_CHECK(collapsed.supports(ui::SemanticAction::Focus));

    const auto expanded = ui::detail::combo_box_semantic_info("Mode B", true);
    ACCESSIBILITY_CHECK(expanded.text_value.has_value());
    ACCESSIBILITY_CHECK(*expanded.text_value == "Mode B");
    ACCESSIBILITY_CHECK(expanded.expanded == ui::SemanticExpandedState::Expanded);
}

void dialog_value_contract() {
    const auto info = ui::detail::dialog_semantic_info("Settings");
    ACCESSIBILITY_CHECK(info.role == ui::SemanticRole::Dialog);
    ACCESSIBILITY_CHECK(info.name == "Settings");
    ACCESSIBILITY_CHECK(info.focusable);
    ACCESSIBILITY_CHECK(info.supports(ui::SemanticAction::Focus));
    ACCESSIBILITY_CHECK(!info.supports(ui::SemanticAction::Activate));
}

} // namespace

int main() {
    try {
        label_value_contract();
        checkbox_value_contract();
        radio_value_contract();
        toggle_value_contract();
        slider_value_contract();
        bounded_display_value_contract();
        text_edit_value_contract();
        combo_box_value_contract();
        dialog_value_contract();
        std::cout << "PASS accessibility widget semantic values\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL accessibility widget semantic values: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
