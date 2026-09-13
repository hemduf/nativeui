#include <nativeui/detail/semantic_action.hpp>
#include <nativeui/detail/semantic_rules.hpp>
#include <nativeui/semantics.hpp>

#include <cstdlib>
#include <iostream>
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

void read_only_mutation_policy_is_single_and_exhaustive() {
    using ui::SemanticAction;

    T068_CHECK(!ui::detail::semantic_action_mutates_value(SemanticAction::Activate));
    T068_CHECK(ui::detail::semantic_action_mutates_value(SemanticAction::Toggle));
    T068_CHECK(!ui::detail::semantic_action_mutates_value(SemanticAction::Focus));
    T068_CHECK(ui::detail::semantic_action_mutates_value(SemanticAction::Increment));
    T068_CHECK(ui::detail::semantic_action_mutates_value(SemanticAction::Decrement));
    T068_CHECK(ui::detail::semantic_action_mutates_value(SemanticAction::SetValue));
    T068_CHECK(ui::detail::semantic_action_mutates_value(SemanticAction::Select));
    T068_CHECK(!ui::detail::semantic_action_mutates_value(SemanticAction::Expand));
    T068_CHECK(!ui::detail::semantic_action_mutates_value(SemanticAction::Collapse));

    ui::SemanticInfo info;
    info.enabled = true;
    info.read_only = true;
    info.focusable = true;
    info.actions = {
        SemanticAction::Activate,
        SemanticAction::Toggle,
        SemanticAction::Focus,
        SemanticAction::Increment,
        SemanticAction::Decrement,
        SemanticAction::SetValue,
        SemanticAction::Select,
        SemanticAction::Expand,
        SemanticAction::Collapse,
    };

    const auto normalized = ui::detail::normalize_semantic_info(
        info, true, true, false);
    const std::vector<SemanticAction> expected{
        SemanticAction::Activate,
        SemanticAction::Focus,
        SemanticAction::Expand,
        SemanticAction::Collapse,
    };
    T068_CHECK(normalized.actions == expected);

    for (const auto action : info.actions) {
        T068_CHECK(ui::detail::semantic_action_allowed(info, action) ==
                   !ui::detail::semantic_action_mutates_value(action));
    }
}

void disabled_state_rejects_every_advertised_action() {
    ui::SemanticInfo info;
    info.enabled = false;
    info.actions = {
        ui::SemanticAction::Activate,
        ui::SemanticAction::Focus,
        ui::SemanticAction::SetValue,
    };

    for (const auto action : info.actions) {
        T068_CHECK(!ui::detail::semantic_action_allowed(info, action));
    }
}

} // namespace

int main() {
    try {
        read_only_mutation_policy_is_single_and_exhaustive();
        disabled_state_rejects_every_advertised_action();
        std::cout << "PASS t068 semantic action policy\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic action policy: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
