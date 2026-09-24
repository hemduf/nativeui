#include <nativeui/detail/semantic_macos_mapping.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("check failed: " #condition);             \
        }                                                                       \
    } while (false)

using Mapping = ui::detail::MacOSAccessibilityRoleMapping;
using NativeRole = ui::detail::MacOSAccessibilityRole;
using Subrole = ui::detail::MacOSAccessibilitySubrole;

struct ExpectedMapping final {
    ui::SemanticRole semantic_role;
    Mapping native_mapping;
};

constexpr std::array expected_mappings{
    ExpectedMapping{ui::SemanticRole::Button, {NativeRole::Button}},
    ExpectedMapping{ui::SemanticRole::Checkbox, {NativeRole::CheckBox}},
    ExpectedMapping{ui::SemanticRole::RadioButton, {NativeRole::RadioButton}},
    ExpectedMapping{ui::SemanticRole::Toggle, {NativeRole::CheckBox}},
    ExpectedMapping{ui::SemanticRole::Slider, {NativeRole::Slider}},
    ExpectedMapping{ui::SemanticRole::RangeSliderHandle, {NativeRole::Slider}},
    ExpectedMapping{ui::SemanticRole::ProgressBar, {NativeRole::ProgressIndicator}},
    ExpectedMapping{ui::SemanticRole::Meter, {NativeRole::LevelIndicator}},
    ExpectedMapping{ui::SemanticRole::Text, {NativeRole::StaticText}},
    ExpectedMapping{ui::SemanticRole::TextInput, {NativeRole::TextField}},
    ExpectedMapping{ui::SemanticRole::TextArea, {NativeRole::TextArea}},
    ExpectedMapping{ui::SemanticRole::ComboBox, {NativeRole::ComboBox}},
    ExpectedMapping{ui::SemanticRole::PopupMenu, {NativeRole::Menu}},
    ExpectedMapping{ui::SemanticRole::MenuItem, {NativeRole::MenuItem}},
    ExpectedMapping{ui::SemanticRole::ListView, {NativeRole::List}},
    ExpectedMapping{ui::SemanticRole::ListItem, {NativeRole::Row}},
    ExpectedMapping{ui::SemanticRole::Tabs, {NativeRole::TabGroup}},
    ExpectedMapping{ui::SemanticRole::Tab,
                    {NativeRole::RadioButton, Subrole::TabButton}},
    ExpectedMapping{ui::SemanticRole::TabPanel, {NativeRole::Group}},
    ExpectedMapping{ui::SemanticRole::Dialog,
                    {NativeRole::Window, Subrole::Dialog}},
    ExpectedMapping{ui::SemanticRole::Group, {NativeRole::Group}},
    ExpectedMapping{ui::SemanticRole::Image, {NativeRole::Image}},
    ExpectedMapping{ui::SemanticRole::Custom, {NativeRole::Group}},
};

void fixed_role_mapping_is_complete() {
    CHECK(!ui::detail::macos_accessibility_role_mapping(
               ui::SemanticRole::None)
               .has_value());

    for (const auto& expected : expected_mappings) {
        const auto actual =
            ui::detail::macos_accessibility_role_mapping(expected.semantic_role);
        CHECK(actual.has_value());
        CHECK(*actual == expected.native_mapping);
    }
}

void only_roles_with_native_subroles_request_them() {
    for (const auto& expected : expected_mappings) {
        const auto actual =
            ui::detail::macos_accessibility_role_mapping(expected.semantic_role);
        CHECK(actual.has_value());

        const auto expected_subrole =
            expected.semantic_role == ui::SemanticRole::Tab
                ? Subrole::TabButton
                : expected.semantic_role == ui::SemanticRole::Dialog
                      ? Subrole::Dialog
                      : Subrole::None;
        CHECK(actual->subrole == expected_subrole);
    }
}

} // namespace

int main() {
    try {
        fixed_role_mapping_is_complete();
        only_roles_with_native_subroles_request_them();
        std::cout << "PASS semantic macOS mapping\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL semantic macOS mapping: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
