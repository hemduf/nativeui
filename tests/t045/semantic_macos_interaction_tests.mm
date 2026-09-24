#include "../../src/detail/semantic_macos_interaction.hpp"

#import <objc/runtime.h>

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

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

const std::array<ExpectedInteraction, 9> expected_interactions{{
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

ui::SemanticInfo info_for(ui::SemanticAction action) {
    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Custom;
    info.enabled = true;
    info.actions = {action};
    return info;
}

void semantic_actions_translate_to_exact_appkit_dispatch_forms() {
    for (const auto& expected : expected_interactions) {
        const auto mapped = ui::detail::macos_accessibility_interaction_mapping(
            expected.action);
        CHECK(mapped.has_value());

        const auto appkit =
            ui::detail::macos_accessibility_appkit_interaction(*mapped);
        CHECK(appkit.has_value());
        CHECK(sel_isEqual(appkit->selector, expected.selector));
        CHECK(appkit->kind == expected.kind);
        CHECK(appkit->boolean_value == expected.boolean_value);
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

void appkit_callbacks_reverse_to_the_single_advertised_action() {
    for (const auto& expected : expected_interactions) {
        auto info = info_for(expected.action);
        const auto action =
            ui::detail::macos_accessibility_semantic_action_for_appkit(
                info, expected.selector, expected.boolean_value);
        CHECK(action.has_value());
        CHECK(*action == expected.action);
    }

    auto ambiguous_press = info_for(ui::SemanticAction::Activate);
    ambiguous_press.actions.push_back(ui::SemanticAction::Toggle);
    CHECK(!ui::detail::macos_accessibility_semantic_action_for_appkit(
        ambiguous_press,
        @selector(accessibilityPerformPress),
        std::nullopt).has_value());

    auto focus = info_for(ui::SemanticAction::Focus);
    CHECK(!ui::detail::macos_accessibility_semantic_action_for_appkit(
        focus,
        @selector(setAccessibilityFocused:),
        false).has_value());

    auto selection = info_for(ui::SemanticAction::Select);
    CHECK(!ui::detail::macos_accessibility_semantic_action_for_appkit(
        selection,
        @selector(setAccessibilitySelected:),
        false).has_value());

    ui::SemanticInfo expansion;
    expansion.role = ui::SemanticRole::ComboBox;
    expansion.enabled = true;
    expansion.actions = {
        ui::SemanticAction::Expand,
        ui::SemanticAction::Collapse,
    };
    const auto expand = ui::detail::macos_accessibility_semantic_action_for_appkit(
        expansion,
        @selector(setAccessibilityExpanded:),
        true);
    const auto collapse = ui::detail::macos_accessibility_semantic_action_for_appkit(
        expansion,
        @selector(setAccessibilityExpanded:),
        false);
    CHECK(expand == ui::SemanticAction::Expand);
    CHECK(collapse == ui::SemanticAction::Collapse);
}

void argument_free_and_boolean_requests_preserve_action_identity() {
    for (const auto& expected : expected_interactions) {
        if (expected.action == ui::SemanticAction::SetValue) {
            continue;
        }

        auto info = info_for(expected.action);
        const auto request = ui::detail::macos_accessibility_appkit_action_request(
            info, expected.selector, expected.boolean_value);
        CHECK(request.has_value());
        CHECK(request->action == expected.action);
        CHECK(!request->numeric_value.has_value());
        CHECK(!request->text_value.has_value());
    }

    auto value = info_for(ui::SemanticAction::SetValue);
    value.numeric_value = 0.5;
    CHECK(!ui::detail::macos_accessibility_appkit_action_request(
        value,
        @selector(setAccessibilityValue:),
        std::nullopt).has_value());
}

void request_preparation_applies_current_eligibility() {
    auto disabled = info_for(ui::SemanticAction::Activate);
    disabled.enabled = false;
    CHECK(!ui::detail::macos_accessibility_appkit_action_request(
        disabled,
        @selector(accessibilityPerformPress),
        std::nullopt).has_value());

    ui::SemanticInfo read_only;
    read_only.role = ui::SemanticRole::Slider;
    read_only.enabled = true;
    read_only.read_only = true;
    read_only.numeric_value = 0.5;
    read_only.actions = {
        ui::SemanticAction::SetValue,
        ui::SemanticAction::Focus,
    };
    CHECK(!ui::detail::macos_accessibility_appkit_value_action_request(
        read_only,
        @selector(setAccessibilityValue:),
        @0.75).has_value());

    const auto focus = ui::detail::macos_accessibility_appkit_action_request(
        read_only,
        @selector(setAccessibilityFocused:),
        true);
    CHECK(focus.has_value());
    CHECK(focus->action == ui::SemanticAction::Focus);
}

void numeric_value_requests_are_typed_and_finite() {
    auto info = info_for(ui::SemanticAction::SetValue);
    info.role = ui::SemanticRole::Slider;
    info.numeric_value = 0.25;
    info.value_range = ui::SemanticValueRange{0.0, 1.0, 0.01};

    const auto request = ui::detail::macos_accessibility_appkit_value_action_request(
        info,
        @selector(setAccessibilityValue:),
        @0.75);
    CHECK(request.has_value());
    CHECK(request->action == ui::SemanticAction::SetValue);
    CHECK(request->numeric_value == 0.75);
    CHECK(!request->text_value.has_value());

    CHECK(!ui::detail::macos_accessibility_appkit_value_action_request(
        info,
        @selector(setAccessibilityValue:),
        @"0.75").has_value());

    NSNumber* non_finite = [NSNumber numberWithDouble:
        std::numeric_limits<double>::infinity()];
    CHECK(!ui::detail::macos_accessibility_appkit_value_action_request(
        info,
        @selector(setAccessibilityValue:),
        non_finite).has_value());
}

void text_value_requests_copy_utf8_and_reject_wrong_domains() {
    auto info = info_for(ui::SemanticAction::SetValue);
    info.role = ui::SemanticRole::TextInput;
    info.text_value = std::string{};

    const auto request = ui::detail::macos_accessibility_appkit_value_action_request(
        info,
        @selector(setAccessibilityValue:),
        @"updated value");
    CHECK(request.has_value());
    CHECK(request->action == ui::SemanticAction::SetValue);
    CHECK(!request->numeric_value.has_value());
    CHECK(request->text_value == std::optional<std::string>{"updated value"});

    CHECK(!ui::detail::macos_accessibility_appkit_value_action_request(
        info,
        @selector(setAccessibilityValue:),
        @42).has_value());

    info.numeric_value = 0.0;
    CHECK(!ui::detail::macos_accessibility_appkit_value_action_request(
        info,
        @selector(setAccessibilityValue:),
        @"ambiguous").has_value());
}

void malformed_reverse_requests_fail_closed() {
    auto activate = info_for(ui::SemanticAction::Activate);
    CHECK(!ui::detail::macos_accessibility_semantic_action_for_appkit(
        activate, nullptr, std::nullopt).has_value());
    CHECK(!ui::detail::macos_accessibility_appkit_action_request(
        activate,
        @selector(setAccessibilityExpanded:),
        true).has_value());

    auto value = info_for(ui::SemanticAction::SetValue);
    value.numeric_value = 0.0;
    CHECK(!ui::detail::macos_accessibility_appkit_value_action_request(
        value,
        @selector(setAccessibilityValue:),
        nil).has_value());
}

} // namespace

int main() {
    @autoreleasepool {
        try {
            semantic_actions_translate_to_exact_appkit_dispatch_forms();
            malformed_interaction_values_fail_closed();
            appkit_callbacks_reverse_to_the_single_advertised_action();
            argument_free_and_boolean_requests_preserve_action_identity();
            request_preparation_applies_current_eligibility();
            numeric_value_requests_are_typed_and_finite();
            text_value_requests_copy_utf8_and_reject_wrong_domains();
            malformed_reverse_requests_fail_closed();
            std::cout << "PASS semantic macOS interaction translation\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "FAIL semantic macOS interaction translation: "
                      << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}
