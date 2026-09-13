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

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

void label_value_contract() {
    const auto info = ui::detail::label_semantic_info("Status ready");
    T068_CHECK(info.role == ui::SemanticRole::Text);
    T068_CHECK(info.name == "Status ready");
    T068_CHECK(!info.focusable);
    T068_CHECK(info.actions.empty());
}

void checkbox_value_contract() {
    const auto checked = ui::detail::checkbox_semantic_info("Enabled", true);
    T068_CHECK(checked.role == ui::SemanticRole::Checkbox);
    T068_CHECK(checked.name == "Enabled");
    T068_CHECK(checked.checked == ui::SemanticCheckedState::Checked);
    T068_CHECK(checked.focusable);
    T068_CHECK(checked.supports(ui::SemanticAction::Toggle));
    T068_CHECK(checked.supports(ui::SemanticAction::Focus));
    T068_CHECK(!checked.supports(ui::SemanticAction::Activate));

    const auto unchecked = ui::detail::checkbox_semantic_info("Enabled", false);
    T068_CHECK(unchecked.checked == ui::SemanticCheckedState::Unchecked);
}

void radio_value_contract() {
    const auto selected = ui::detail::radio_button_semantic_info("Mode A", true);
    T068_CHECK(selected.role == ui::SemanticRole::RadioButton);
    T068_CHECK(selected.name == "Mode A");
    T068_CHECK(selected.selected);
    T068_CHECK(selected.checked == ui::SemanticCheckedState::Checked);
    T068_CHECK(selected.focusable);
    T068_CHECK(selected.supports(ui::SemanticAction::Select));
    T068_CHECK(selected.supports(ui::SemanticAction::Focus));
    T068_CHECK(!selected.supports(ui::SemanticAction::Toggle));

    const auto unselected = ui::detail::radio_button_semantic_info("Mode A", false);
    T068_CHECK(!unselected.selected);
    T068_CHECK(unselected.checked == ui::SemanticCheckedState::Unchecked);
}

void toggle_value_contract() {
    const auto checked = ui::detail::toggle_semantic_info("Bypass", true);
    T068_CHECK(checked.role == ui::SemanticRole::Toggle);
    T068_CHECK(checked.name == "Bypass");
    T068_CHECK(checked.checked == ui::SemanticCheckedState::Checked);
    T068_CHECK(checked.focusable);
    T068_CHECK(checked.supports(ui::SemanticAction::Toggle));
    T068_CHECK(checked.supports(ui::SemanticAction::Focus));
    T068_CHECK(!checked.supports(ui::SemanticAction::Activate));

    const auto unchecked = ui::detail::toggle_semantic_info("Bypass", false);
    T068_CHECK(unchecked.checked == ui::SemanticCheckedState::Unchecked);
}

void slider_value_contract() {
    const auto info = ui::detail::slider_semantic_info(0.25f, -1.0f, 1.0f, 0.25f);
    T068_CHECK(info.role == ui::SemanticRole::Slider);
    T068_CHECK(info.numeric_value.has_value());
    T068_CHECK(*info.numeric_value == 0.25);
    T068_CHECK(info.value_range.has_value());
    T068_CHECK(info.value_range->minimum == -1.0);
    T068_CHECK(info.value_range->maximum == 1.0);
    T068_CHECK(info.value_range->step == 0.25);
    T068_CHECK(info.focusable);
    T068_CHECK(info.supports(ui::SemanticAction::Increment));
    T068_CHECK(info.supports(ui::SemanticAction::Decrement));
    T068_CHECK(info.supports(ui::SemanticAction::SetValue));
    T068_CHECK(info.supports(ui::SemanticAction::Focus));
    T068_CHECK(!info.supports(ui::SemanticAction::Toggle));

    const auto clamped = ui::detail::slider_semantic_info(5.0f, -1.0f, 1.0f, 0.25f);
    T068_CHECK(clamped.numeric_value.has_value());
    T068_CHECK(*clamped.numeric_value == 1.0);
}

void bounded_display_value_contract() {
    const auto progress = ui::detail::bounded_display_semantic_info(
        0.25f, 0.0f, 1.0f, false);
    T068_CHECK(progress.role == ui::SemanticRole::ProgressBar);
    T068_CHECK(progress.numeric_value.has_value());
    T068_CHECK(*progress.numeric_value == 0.25);
    T068_CHECK(progress.value_range.has_value());
    T068_CHECK(progress.value_range->minimum == 0.0);
    T068_CHECK(progress.value_range->maximum == 1.0);
    T068_CHECK(progress.value_range->step == 0.0);
    T068_CHECK(!progress.focusable);
    T068_CHECK(progress.actions.empty());

    const auto meter = ui::detail::bounded_display_semantic_info(
        4.0f, -1.0f, 1.0f, true);
    T068_CHECK(meter.role == ui::SemanticRole::Meter);
    T068_CHECK(meter.numeric_value.has_value());
    T068_CHECK(*meter.numeric_value == 1.0);
    T068_CHECK(meter.value_range.has_value());
    T068_CHECK(meter.value_range->minimum == -1.0);
    T068_CHECK(meter.value_range->maximum == 1.0);
    T068_CHECK(meter.actions.empty());

    const auto non_finite = ui::detail::bounded_display_semantic_info(
        std::numeric_limits<float>::quiet_NaN(), -2.0f, 2.0f, false);
    T068_CHECK(non_finite.numeric_value.has_value());
    T068_CHECK(*non_finite.numeric_value == -2.0);
}

void text_edit_value_contract() {
    const auto single_line = ui::detail::text_edit_semantic_info(
        "Name", "Ada", false);
    T068_CHECK(single_line.role == ui::SemanticRole::TextInput);
    T068_CHECK(single_line.name == "Name");
    T068_CHECK(single_line.text_value.has_value());
    T068_CHECK(*single_line.text_value == "Ada");
    T068_CHECK(single_line.focusable);
    T068_CHECK(single_line.supports(ui::SemanticAction::SetValue));
    T068_CHECK(single_line.supports(ui::SemanticAction::Focus));

    const auto multiline = ui::detail::text_edit_semantic_info(
        "Notes", "line one\nline two", true);
    T068_CHECK(multiline.role == ui::SemanticRole::TextArea);
    T068_CHECK(multiline.name == "Notes");
    T068_CHECK(multiline.text_value.has_value());
    T068_CHECK(*multiline.text_value == "line one\nline two");
    T068_CHECK(multiline.focusable);
    T068_CHECK(multiline.supports(ui::SemanticAction::SetValue));
    T068_CHECK(multiline.supports(ui::SemanticAction::Focus));
}

void combo_box_value_contract() {
    const auto collapsed = ui::detail::combo_box_semantic_info("Mode A", false);
    T068_CHECK(collapsed.role == ui::SemanticRole::ComboBox);
    T068_CHECK(collapsed.text_value.has_value());
    T068_CHECK(*collapsed.text_value == "Mode A");
    T068_CHECK(collapsed.expanded == ui::SemanticExpandedState::Collapsed);
    T068_CHECK(collapsed.focusable);
    T068_CHECK(collapsed.supports(ui::SemanticAction::Expand));
    T068_CHECK(collapsed.supports(ui::SemanticAction::Collapse));
    T068_CHECK(collapsed.supports(ui::SemanticAction::Select));
    T068_CHECK(collapsed.supports(ui::SemanticAction::Focus));

    const auto expanded = ui::detail::combo_box_semantic_info("Mode B", true);
    T068_CHECK(expanded.text_value.has_value());
    T068_CHECK(*expanded.text_value == "Mode B");
    T068_CHECK(expanded.expanded == ui::SemanticExpandedState::Expanded);
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
        std::cout << "PASS t068 widget semantic values\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 widget semantic values: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}