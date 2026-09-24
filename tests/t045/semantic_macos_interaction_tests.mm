#include "../../src/detail/semantic_macos_interaction.hpp"

#import <objc/runtime.h>

#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("check failed: " #condition);             \
        }                                                                       \
    } while (false)

using AppKitKind = ui::detail::MacOSAccessibilityAppKitInteractionKind;
using Interaction = ui::detail::MacOSAccessibilityInteraction;

struct ExpectedInteraction final {
    ui::SemanticAction action;
    SEL selector;
    AppKitKind kind;
    std::optional<bool> boolean_value;
};

void semantic_actions_translate_to_exact_appkit_dispatch_forms() {
    const std::array<ExpectedInteraction, 9> expected{{
        {ui::SemanticAction::Activate,
         @selector(accessibilityPerformPress),
         AppKitKind::ActionMethod,
         std::nullopt},
        {ui::SemanticAction::Toggle,
         @selector(accessibilityPerformPress),
         AppKitKind::ActionMethod,
         std::nullopt},
        {ui::SemanticAction::Focus,
         @selector(setAccessibilityFocused:),
         AppKitKind::BooleanSetter,
         true},
        {ui::SemanticAction::Increment,
         @selector(accessibilityPerformIncrement),
         AppKitKind::ActionMethod,
         std::nullopt},
        {ui::SemanticAction::Decrement,
         @selector(accessibilityPerformDecrement),
         AppKitKind::ActionMethod,
         std::nullopt},
        {ui::SemanticAction::SetValue,
         @selector(setAccessibilityValue:),
         AppKitKind::ValueSetter,
         std::nullopt},
        {ui::SemanticAction::Select,
         @selector(setAccessibilitySelected:),
         AppKitKind::BooleanSetter,
         true},
        {ui::SemanticAction::Expand,
         @selector(setAccessibilityExpanded:),
         AppKitKind::BooleanSetter,
         true},
        {ui::SemanticAction::Collapse,
         @selector(setAccessibilityExpanded:),
         AppKitKind::BooleanSetter,
         false},
    }};

    for (const auto& expected_interaction : expected) {
        const auto mapped = ui::detail::macos_accessibility_interaction_mapping(
            expected_interaction.action);
        CHECK(mapped.has_value());

        const auto appkit =
            ui::detail::macos_accessibility_appkit_interaction(*mapped);
        CHECK(appkit.has_value());
        CHECK(sel_isEqual(appkit->selector, expected_interaction.selector));
        CHECK(appkit->kind == expected_interaction.kind);
        CHECK(appkit->boolean_value == expected_interaction.boolean_value);
    }
}

void malformed_interaction_values_fail_closed() {
    const auto missing_expanded_value =
        ui::detail::macos_accessibility_appkit_interaction(
            {Interaction::Expanded, std::nullopt});
    CHECK(!missing_expanded_value.has_value());

    const auto unexpected_press_value =
        ui::detail::macos_accessibility_appkit_interaction(
            {Interaction::Press, true});
    CHECK(!unexpected_press_value.has_value());

    const auto invalid_interaction =
        ui::detail::macos_accessibility_appkit_interaction(
            {static_cast<Interaction>(255), std::nullopt});
    CHECK(!invalid_interaction.has_value());
}

} // namespace

int main() {
    @autoreleasepool {
        try {
            semantic_actions_translate_to_exact_appkit_dispatch_forms();
            malformed_interaction_values_fail_closed();
            std::cout << "PASS semantic macOS interaction translation\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "FAIL semantic macOS interaction translation: "
                      << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}
