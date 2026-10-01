#include <nativeui/detail/semantic_atspi_mapping.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("line " + std::to_string(__LINE__) +       \
                                     ": check failed: " #condition);            \
        }                                                                       \
    } while (false)

using ui::SemanticAction;
using ui::SemanticInfo;
using ui::SemanticRole;
using ui::detail::AtspiInterface;
using ui::detail::AtspiRole;
using ui::detail::atspi_has_interface;
using ui::detail::semantic_atspi_interface_eligibility;
using ui::detail::semantic_atspi_role_mapping;

struct ExpectedRole final {
    SemanticRole semantic_role;
    AtspiRole native_role;
};

constexpr std::array expected_roles{
    ExpectedRole{SemanticRole::Button, AtspiRole::PushButton},
    ExpectedRole{SemanticRole::Checkbox, AtspiRole::CheckBox},
    ExpectedRole{SemanticRole::RadioButton, AtspiRole::RadioButton},
    ExpectedRole{SemanticRole::Toggle, AtspiRole::ToggleButton},
    ExpectedRole{SemanticRole::Slider, AtspiRole::Slider},
    ExpectedRole{SemanticRole::RangeSliderHandle, AtspiRole::Slider},
    ExpectedRole{SemanticRole::ProgressBar, AtspiRole::ProgressBar},
    ExpectedRole{SemanticRole::Meter, AtspiRole::LevelBar},
    ExpectedRole{SemanticRole::Text, AtspiRole::StaticText},
    ExpectedRole{SemanticRole::TextInput, AtspiRole::Entry},
    ExpectedRole{SemanticRole::TextArea, AtspiRole::Text},
    ExpectedRole{SemanticRole::ComboBox, AtspiRole::ComboBox},
    ExpectedRole{SemanticRole::PopupMenu, AtspiRole::Menu},
    ExpectedRole{SemanticRole::MenuItem, AtspiRole::MenuItem},
    ExpectedRole{SemanticRole::ListView, AtspiRole::List},
    ExpectedRole{SemanticRole::ListItem, AtspiRole::ListItem},
    ExpectedRole{SemanticRole::Tabs, AtspiRole::PageTabList},
    ExpectedRole{SemanticRole::Tab, AtspiRole::PageTab},
    ExpectedRole{SemanticRole::TabPanel, AtspiRole::Panel},
    ExpectedRole{SemanticRole::Dialog, AtspiRole::Dialog},
    ExpectedRole{SemanticRole::Group, AtspiRole::Panel},
    ExpectedRole{SemanticRole::Image, AtspiRole::Image},
    ExpectedRole{SemanticRole::Custom, AtspiRole::Panel},
};

void role_mapping_is_exhaustive() {
    CHECK(!semantic_atspi_role_mapping(SemanticRole::None).has_value());
    for (const auto& expected : expected_roles) {
        const auto mapping = semantic_atspi_role_mapping(expected.semantic_role);
        CHECK(mapping.has_value());
        CHECK(mapping->role == expected.native_role);
    }
    CHECK(!semantic_atspi_role_mapping(static_cast<SemanticRole>(255)).has_value());
}

void role_interfaces_match_the_platform_contract() {
    const auto slider = semantic_atspi_role_mapping(SemanticRole::Slider);
    CHECK(slider.has_value());
    CHECK(atspi_has_interface(slider->interfaces, AtspiInterface::Value));
    CHECK(atspi_has_interface(slider->interfaces, AtspiInterface::Component));
    CHECK(!atspi_has_interface(slider->interfaces, AtspiInterface::Action));

    const auto text = semantic_atspi_role_mapping(SemanticRole::Text);
    CHECK(text.has_value());
    CHECK(atspi_has_interface(text->interfaces, AtspiInterface::Text));
    CHECK(!atspi_has_interface(text->interfaces, AtspiInterface::EditableText));

    const auto input = semantic_atspi_role_mapping(SemanticRole::TextInput);
    CHECK(input.has_value());
    CHECK(atspi_has_interface(input->interfaces, AtspiInterface::Text));
    CHECK(atspi_has_interface(input->interfaces, AtspiInterface::EditableText));
    CHECK(atspi_has_interface(input->interfaces, AtspiInterface::Component));

    const auto list = semantic_atspi_role_mapping(SemanticRole::ListView);
    CHECK(list.has_value());
    CHECK(atspi_has_interface(list->interfaces, AtspiInterface::Component));
    CHECK(atspi_has_interface(list->interfaces, AtspiInterface::Selection));
    CHECK(atspi_has_interface(list->interfaces, AtspiInterface::Collection));

    const auto image = semantic_atspi_role_mapping(SemanticRole::Image);
    CHECK(image.has_value());
    CHECK(atspi_has_interface(image->interfaces, AtspiInterface::Image));
    CHECK(atspi_has_interface(image->interfaces, AtspiInterface::Component));
}

void action_interface_follows_advertised_semantics() {
    SemanticInfo button;
    button.role = SemanticRole::Button;
    button.actions = {SemanticAction::Focus};

    auto eligibility = semantic_atspi_interface_eligibility(button);
    CHECK(eligibility.has_value());
    CHECK(!atspi_has_interface(eligibility->interfaces, AtspiInterface::Action));

    button.actions.push_back(SemanticAction::Activate);
    eligibility = semantic_atspi_interface_eligibility(button);
    CHECK(eligibility.has_value());
    CHECK(atspi_has_interface(eligibility->interfaces, AtspiInterface::Action));

    SemanticInfo item;
    item.role = SemanticRole::ListItem;
    item.actions = {SemanticAction::Select, SemanticAction::Focus};
    const auto item_eligibility = semantic_atspi_interface_eligibility(item);
    CHECK(item_eligibility.has_value());
    CHECK(atspi_has_interface(item_eligibility->interfaces,
                              AtspiInterface::Action));

    SemanticInfo custom;
    custom.role = SemanticRole::Custom;
    custom.actions = {SemanticAction::Activate};
    const auto custom_eligibility = semantic_atspi_interface_eligibility(custom);
    CHECK(custom_eligibility.has_value());
    CHECK(!atspi_has_interface(custom_eligibility->interfaces,
                               AtspiInterface::Action));
}

void selection_metadata_is_preserved() {
    const auto radio =
        semantic_atspi_interface_eligibility(SemanticInfo{
            .role = SemanticRole::RadioButton,
            .actions = {SemanticAction::Select, SemanticAction::Focus}});
    CHECK(radio.has_value());
    CHECK(radio->selection_state);
    CHECK(radio->parent_selection_relation);

    const auto item =
        semantic_atspi_interface_eligibility(SemanticInfo{
            .role = SemanticRole::ListItem,
            .actions = {SemanticAction::Select, SemanticAction::Focus}});
    CHECK(item.has_value());
    CHECK(item->selection_state);
    CHECK(!item->parent_selection_relation);

    const auto tab =
        semantic_atspi_interface_eligibility(SemanticInfo{
            .role = SemanticRole::Tab,
            .actions = {SemanticAction::Select, SemanticAction::Focus}});
    CHECK(tab.has_value());
    CHECK(tab->selection_state);
    CHECK(!tab->parent_selection_relation);
}

} // namespace

int main() {
    try {
        role_mapping_is_exhaustive();
        role_interfaces_match_the_platform_contract();
        action_interface_follows_advertised_semantics();
        selection_metadata_is_preserved();
        std::cout << "PASS semantic AT-SPI mapping\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL semantic AT-SPI mapping: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
