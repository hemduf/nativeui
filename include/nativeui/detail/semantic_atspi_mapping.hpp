#pragma once

#include <nativeui/semantics.hpp>

#include <cstdint>
#include <initializer_list>
#include <optional>

namespace ui::detail {

enum class AtspiRole {
    PushButton,
    CheckBox,
    RadioButton,
    ToggleButton,
    Slider,
    ProgressBar,
    LevelBar,
    StaticText,
    Entry,
    Text,
    ComboBox,
    Menu,
    MenuItem,
    List,
    ListItem,
    PageTabList,
    PageTab,
    Panel,
    Dialog,
    Image,
};

enum class AtspiInterface : std::uint32_t {
    Action = 1u << 0u,
    Component = 1u << 1u,
    Value = 1u << 2u,
    Text = 1u << 3u,
    EditableText = 1u << 4u,
    Selection = 1u << 5u,
    Collection = 1u << 6u,
    Image = 1u << 7u,
};

using AtspiInterfaceMask = std::uint32_t;

[[nodiscard]] constexpr AtspiInterfaceMask
atspi_interface_mask(AtspiInterface interface) noexcept {
    return static_cast<AtspiInterfaceMask>(interface);
}

[[nodiscard]] constexpr bool
atspi_has_interface(AtspiInterfaceMask mask, AtspiInterface interface) noexcept {
    return (mask & atspi_interface_mask(interface)) != 0u;
}

struct AtspiRoleMapping final {
    AtspiRole role{AtspiRole::Panel};
    AtspiInterfaceMask interfaces{};
    bool action_from_advertised_semantics{};
    bool selection_state{};
    bool parent_selection_relation{};

    bool operator==(const AtspiRoleMapping&) const = default;
};

[[nodiscard]] constexpr AtspiInterfaceMask
atspi_interfaces(std::initializer_list<AtspiInterface> interfaces) noexcept {
    AtspiInterfaceMask mask{};
    for (const auto interface : interfaces) {
        mask |= atspi_interface_mask(interface);
    }
    return mask;
}

[[nodiscard]] constexpr std::optional<AtspiRoleMapping>
semantic_atspi_role_mapping(SemanticRole role) noexcept {
    using Interface = AtspiInterface;
    using Mapping = AtspiRoleMapping;
    using Role = AtspiRole;

    switch (role) {
        case SemanticRole::None:
            return std::nullopt;
        case SemanticRole::Button:
            return Mapping{
                .role = Role::PushButton,
                .interfaces = atspi_interfaces({Interface::Component}),
                .action_from_advertised_semantics = true};
        case SemanticRole::Checkbox:
            return Mapping{
                .role = Role::CheckBox,
                .interfaces = atspi_interfaces({Interface::Component}),
                .action_from_advertised_semantics = true};
        case SemanticRole::RadioButton:
            return Mapping{
                .role = Role::RadioButton,
                .interfaces = atspi_interfaces({Interface::Component}),
                .action_from_advertised_semantics = true,
                .selection_state = true,
                .parent_selection_relation = true};
        case SemanticRole::Toggle:
            return Mapping{
                .role = Role::ToggleButton,
                .interfaces = atspi_interfaces({Interface::Component}),
                .action_from_advertised_semantics = true};
        case SemanticRole::Slider:
        case SemanticRole::RangeSliderHandle:
            return Mapping{
                .role = Role::Slider,
                .interfaces =
                    atspi_interfaces({Interface::Value, Interface::Component})};
        case SemanticRole::ProgressBar:
            return Mapping{
                .role = Role::ProgressBar,
                .interfaces =
                    atspi_interfaces({Interface::Value, Interface::Component})};
        case SemanticRole::Meter:
            return Mapping{
                .role = Role::LevelBar,
                .interfaces =
                    atspi_interfaces({Interface::Value, Interface::Component})};
        case SemanticRole::Text:
            return Mapping{
                .role = Role::StaticText,
                .interfaces = atspi_interfaces({Interface::Text})};
        case SemanticRole::TextInput:
            return Mapping{
                .role = Role::Entry,
                .interfaces = atspi_interfaces(
                    {Interface::Text, Interface::EditableText,
                     Interface::Component})};
        case SemanticRole::TextArea:
            return Mapping{
                .role = Role::Text,
                .interfaces = atspi_interfaces(
                    {Interface::Text, Interface::EditableText,
                     Interface::Component})};
        case SemanticRole::ComboBox:
            return Mapping{
                .role = Role::ComboBox,
                .interfaces =
                    atspi_interfaces({Interface::Selection, Interface::Component}),
                .action_from_advertised_semantics = true};
        case SemanticRole::PopupMenu:
            return Mapping{
                .role = Role::Menu,
                .interfaces = atspi_interfaces({Interface::Component})};
        case SemanticRole::MenuItem:
            return Mapping{
                .role = Role::MenuItem,
                .interfaces = atspi_interfaces({Interface::Component}),
                .action_from_advertised_semantics = true};
        case SemanticRole::ListView:
            return Mapping{
                .role = Role::List,
                .interfaces = atspi_interfaces(
                    {Interface::Component, Interface::Selection,
                     Interface::Collection})};
        case SemanticRole::ListItem:
            return Mapping{
                .role = Role::ListItem,
                .interfaces = atspi_interfaces({Interface::Component}),
                .action_from_advertised_semantics = true,
                .selection_state = true};
        case SemanticRole::Tabs:
            return Mapping{
                .role = Role::PageTabList,
                .interfaces =
                    atspi_interfaces({Interface::Selection, Interface::Component})};
        case SemanticRole::Tab:
            return Mapping{
                .role = Role::PageTab,
                .interfaces = atspi_interfaces({Interface::Component}),
                .action_from_advertised_semantics = true,
                .selection_state = true};
        case SemanticRole::TabPanel:
        case SemanticRole::Group:
        case SemanticRole::Custom:
            return Mapping{
                .role = Role::Panel,
                .interfaces = atspi_interfaces({Interface::Component})};
        case SemanticRole::Dialog:
            return Mapping{
                .role = Role::Dialog,
                .interfaces = atspi_interfaces({Interface::Component})};
        case SemanticRole::Image:
            return Mapping{
                .role = Role::Image,
                .interfaces =
                    atspi_interfaces({Interface::Image, Interface::Component})};
    }

    return std::nullopt;
}

[[nodiscard]] constexpr bool
semantic_has_atspi_action(const SemanticInfo& info) noexcept {
    const auto mapping = semantic_atspi_role_mapping(info.role);
    if (!mapping || !mapping->action_from_advertised_semantics) {
        return false;
    }

    for (const auto action : info.actions) {
        if (action != SemanticAction::Focus) {
            return true;
        }
    }
    return false;
}

struct AtspiInterfaceEligibility final {
    AtspiRole role{AtspiRole::Panel};
    AtspiInterfaceMask interfaces{};
    bool selection_state{};
    bool parent_selection_relation{};

    bool operator==(const AtspiInterfaceEligibility&) const = default;
};

[[nodiscard]] constexpr std::optional<AtspiInterfaceEligibility>
semantic_atspi_interface_eligibility(const SemanticInfo& info) noexcept {
    const auto mapping = semantic_atspi_role_mapping(info.role);
    if (!mapping) {
        return std::nullopt;
    }

    auto interfaces = mapping->interfaces;
    if (semantic_has_atspi_action(info)) {
        interfaces |= atspi_interface_mask(AtspiInterface::Action);
    }

    return AtspiInterfaceEligibility{
        .role = mapping->role,
        .interfaces = interfaces,
        .selection_state = mapping->selection_state,
        .parent_selection_relation = mapping->parent_selection_relation};
}

} // namespace ui::detail
