#include <nativeui/detail/semantic_rules.hpp>
#include <nativeui/semantics.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

void read_only_state_filters_mutation_actions() {
    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Slider;
    info.name = "Read-only value";
    info.focusable = true;
    info.actions = {
        ui::SemanticAction::Increment,
        ui::SemanticAction::Decrement,
        ui::SemanticAction::SetValue,
        ui::SemanticAction::Focus,
    };

    const auto normalized = ui::detail::normalize_semantic_info(
        std::move(info), true, true, false);

    T068_CHECK(normalized.enabled);
    T068_CHECK(normalized.read_only);
    T068_CHECK(normalized.focusable);
    T068_CHECK(!normalized.focused);
    T068_CHECK(normalized.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});
}

void disabled_state_removes_all_interaction() {
    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Button;
    info.focusable = true;
    info.focused = true;
    info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};

    const auto normalized = ui::detail::normalize_semantic_info(
        std::move(info), false, false, true);

    T068_CHECK(!normalized.enabled);
    T068_CHECK(!normalized.focusable);
    T068_CHECK(!normalized.focused);
    T068_CHECK(normalized.actions.empty());
}

void suite() {
    read_only_state_filters_mutation_actions();
    disabled_state_removes_all_interaction();
}

} // namespace

int main() {
    try {
        suite();
        return 0;
    } catch (const std::exception&) {
        return 1;
    }
}
