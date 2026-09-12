#include <nativeui/detail/semantic_widget_info.hpp>
#include <nativeui/semantics.hpp>

#include <cstdlib>
#include <iostream>
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

} // namespace

int main() {
    try {
        checkbox_value_contract();
        radio_value_contract();
        toggle_value_contract();
        slider_value_contract();
        std::cout << "PASS t068 widget semantic values\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 widget semantic values: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
