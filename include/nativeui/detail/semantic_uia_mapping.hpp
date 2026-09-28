#pragma once

#include <nativeui/semantics.hpp>

#include <optional>

namespace ui::detail {

/// Backend-neutral UIA control-type tokens for the frozen T045 §8 mapping.
///
/// The Win32 adapter translates these tokens to the concrete
/// `UIA_*ControlTypeId` constants; no UIAutomationCore type crosses this header
/// or an immutable semantic snapshot.
enum class UiaControlType {
    Button,
    CheckBox,
    RadioButton,
    Slider,
    Thumb,
    ProgressBar,
    Text,
    Edit,
    ComboBox,
    Menu,
    MenuItem,
    List,
    ListItem,
    Tab,
    TabItem,
    Group,
    Window,
    Image,
};

/// Closed set of UIA pattern providers this bridge can expose.
///
/// `Invoke`, `Toggle`, `SelectionItem`, `RangeValue`, `ExpandCollapse` and
/// `Value` are action-driven. `Text`, the read-only `RangeValue` of
/// ProgressBar/Meter, and `Selection`/`ItemContainer` on ListView are
/// role-mandated read patterns from §8: they carry no mutating T045 action but
/// are required so assistive technology can read the frozen semantic content.
enum class UiaPattern {
    Invoke,
    Toggle,
    SelectionItem,
    RangeValue,
    ExpandCollapse,
    Value,
    Text,
    Selection,
    ItemContainer,
    VirtualizedItem,
};

struct UiaRoleMapping final {
    UiaControlType control_type{UiaControlType::Group};

    /// §8: ProgressBar/Meter expose a read-only RangeValue and never a mutating
    /// action. The adapter still reports `IsReadOnly = TRUE` and rejects
    /// `SetValue`.
    bool read_only_range_value{false};

    /// §8: ListView is a Selection container; ComboBox owns item selection.
    bool container_selection{false};

    /// §8: ListView exposes ItemContainerPattern for lazy logical children.
    bool item_container{false};

    /// §8: Text/TextInput/TextArea expose text access where the platform asks.
    bool text_access{false};

    bool operator==(const UiaRoleMapping&) const = default;
};

/// Map the closed NativeUI semantic role set to the fixed Windows accessibility
/// contract. `SemanticRole::None` is intentionally absent because flattened
/// layout wrappers never receive a native UIA provider of their own.
[[nodiscard]] constexpr std::optional<UiaRoleMapping>
semantic_uia_role_mapping(SemanticRole role) noexcept {
    using Mapping = UiaRoleMapping;
    using Control = UiaControlType;

    switch (role) {
        case SemanticRole::None:
            return std::nullopt;
        case SemanticRole::Button:
            return Mapping{.control_type = Control::Button};
        case SemanticRole::Checkbox:
            return Mapping{.control_type = Control::CheckBox};
        case SemanticRole::RadioButton:
            return Mapping{.control_type = Control::RadioButton};
        case SemanticRole::Toggle:
            return Mapping{.control_type = Control::CheckBox};
        case SemanticRole::Slider:
            return Mapping{.control_type = Control::Slider};
        case SemanticRole::RangeSliderHandle:
            return Mapping{.control_type = Control::Thumb};
        case SemanticRole::ProgressBar:
            return Mapping{.control_type = Control::ProgressBar,
                           .read_only_range_value = true};
        case SemanticRole::Meter:
            return Mapping{.control_type = Control::ProgressBar,
                           .read_only_range_value = true};
        case SemanticRole::Text:
            return Mapping{.control_type = Control::Text, .text_access = true};
        case SemanticRole::TextInput:
            return Mapping{.control_type = Control::Edit, .text_access = true};
        case SemanticRole::TextArea:
            return Mapping{.control_type = Control::Edit, .text_access = true};
        case SemanticRole::ComboBox:
            return Mapping{.control_type = Control::ComboBox,
                           .container_selection = true};
        case SemanticRole::PopupMenu:
            return Mapping{.control_type = Control::Menu};
        case SemanticRole::MenuItem:
            return Mapping{.control_type = Control::MenuItem};
        case SemanticRole::ListView:
            return Mapping{.control_type = Control::List,
                           .container_selection = true,
                           .item_container = true};
        case SemanticRole::ListItem:
            return Mapping{.control_type = Control::ListItem};
        case SemanticRole::Tabs:
            return Mapping{.control_type = Control::Tab};
        case SemanticRole::Tab:
            return Mapping{.control_type = Control::TabItem};
        case SemanticRole::TabPanel:
            return Mapping{.control_type = Control::Group};
        case SemanticRole::Dialog:
            // §8: one in-view fragment element; never a second native HWND.
            return Mapping{.control_type = Control::Window};
        case SemanticRole::Group:
            return Mapping{.control_type = Control::Group};
        case SemanticRole::Image:
            return Mapping{.control_type = Control::Image};
        case SemanticRole::Custom:
            return Mapping{.control_type = Control::Group};
    }

    return std::nullopt;
}

/// Pattern eligibility derived from one semantic role plus the actions that
/// role currently advertises.
///
/// Eligibility follows `SemanticInfo::actions` (the frozen T045 advertisement)
/// rather than the transient `enabled`/`read_only` state: a disabled control
/// keeps its discoverable pattern and reports `IsEnabled = FALSE`, and every
/// mutation is revalidated on the UI thread by the existing semantic action
/// router. `range_value_writable` is the one capability bit a platform reader
/// needs up front (`IRangeValueProvider::get_IsReadOnly`); the adapter still
/// folds the live `read_only` state into the reported value.
struct UiaPatternEligibility final {
    bool invoke{};
    bool toggle{};
    bool selection_item{};
    bool range_value{};
    bool range_value_writable{};
    bool expand_collapse{};
    bool value{};
    bool text{};
    bool selection{};
    bool item_container{};
    bool virtualized_item{};

    bool operator==(const UiaPatternEligibility&) const = default;
};

[[nodiscard]] constexpr bool
uia_has_pattern(const UiaPatternEligibility& eligibility,
                UiaPattern pattern) noexcept {
    switch (pattern) {
        case UiaPattern::Invoke:
            return eligibility.invoke;
        case UiaPattern::Toggle:
            return eligibility.toggle;
        case UiaPattern::SelectionItem:
            return eligibility.selection_item;
        case UiaPattern::RangeValue:
            return eligibility.range_value;
        case UiaPattern::ExpandCollapse:
            return eligibility.expand_collapse;
        case UiaPattern::Value:
            return eligibility.value;
        case UiaPattern::Text:
            return eligibility.text;
        case UiaPattern::Selection:
            return eligibility.selection;
        case UiaPattern::ItemContainer:
            return eligibility.item_container;
        case UiaPattern::VirtualizedItem:
            return eligibility.virtualized_item;
    }
    return false;
}

/// Project one semantic node's advertised action set onto the closed UIA
/// pattern set. `virtual_item` marks the lazily materialized logical row of a
/// T067 ListView, which additionally exposes `VirtualizedItemPattern` without
/// ever being materialized as a visual row.
[[nodiscard]] constexpr UiaPatternEligibility
semantic_uia_pattern_eligibility(const SemanticInfo& info,
                                 bool virtual_item = false) noexcept {
    const auto role_mapping = semantic_uia_role_mapping(info.role);
    if (!role_mapping) {
        return {};
    }

    UiaPatternEligibility eligibility;
    switch (info.role) {
        case SemanticRole::Button:
            eligibility.invoke = info.supports(SemanticAction::Activate);
            break;
        case SemanticRole::Checkbox:
        case SemanticRole::Toggle:
            eligibility.toggle = info.supports(SemanticAction::Toggle);
            break;
        case SemanticRole::RadioButton:
        case SemanticRole::Tab:
        case SemanticRole::ListItem:
            // T045/T059: read-only nodes reject the mutating Select action.
            // Ordinary nodes already lose it during normalization; virtual
            // T067 ListItems are projected from app-declared immutable metadata
            // before that normalization, so reflect the live read-only
            // eligibility here as well and never advertise SelectionItem for a
            // row whose every Select() would fail closed.
            eligibility.selection_item =
                info.supports(SemanticAction::Select) && !info.read_only;
            break;
        case SemanticRole::MenuItem:
            eligibility.invoke = info.supports(SemanticAction::Activate);
            eligibility.selection_item = info.supports(SemanticAction::Select);
            break;
        case SemanticRole::Slider:
        case SemanticRole::RangeSliderHandle:
            eligibility.range_value =
                info.supports(SemanticAction::Increment) ||
                info.supports(SemanticAction::Decrement) ||
                info.supports(SemanticAction::SetValue);
            eligibility.range_value_writable =
                eligibility.range_value && !info.read_only;
            break;
        case SemanticRole::ProgressBar:
        case SemanticRole::Meter:
            eligibility.range_value = true;
            eligibility.range_value_writable = false;
            break;
        case SemanticRole::ComboBox:
            eligibility.expand_collapse =
                info.supports(SemanticAction::Expand) ||
                info.supports(SemanticAction::Collapse);
            eligibility.selection = info.supports(SemanticAction::Select);
            break;
        case SemanticRole::TextInput:
        case SemanticRole::TextArea:
            eligibility.value = info.supports(SemanticAction::SetValue);
            break;
        case SemanticRole::Custom: {
            // §4/§8: Custom exposes exactly the application-advertised subset,
            // but only in a role-compatible form. The value domain decides
            // whether SetValue means RangeValue or text Value; an absent or
            // ambiguous domain fails closed instead of guessing.
            eligibility.invoke = info.supports(SemanticAction::Activate);
            eligibility.toggle = info.supports(SemanticAction::Toggle);
            eligibility.selection_item = info.supports(SemanticAction::Select);
            eligibility.expand_collapse =
                info.supports(SemanticAction::Expand) ||
                info.supports(SemanticAction::Collapse);
            if (info.supports(SemanticAction::SetValue)) {
                const bool numeric_domain =
                    info.numeric_value.has_value() || info.value_range.has_value();
                const bool text_domain = info.text_value.has_value();
                if (numeric_domain != text_domain) {
                    eligibility.range_value = numeric_domain;
                    eligibility.range_value_writable =
                        numeric_domain && !info.read_only;
                    eligibility.value = text_domain;
                }
            } else if (info.supports(SemanticAction::Increment) ||
                       info.supports(SemanticAction::Decrement)) {
                eligibility.range_value = true;
                eligibility.range_value_writable = !info.read_only;
            }
            break;
        }
        default:
            break;
    }

    eligibility.text = role_mapping->text_access;
    eligibility.selection =
        eligibility.selection || role_mapping->container_selection;
    eligibility.item_container = role_mapping->item_container;
    eligibility.virtualized_item =
        virtual_item && info.role == SemanticRole::ListItem;
    return eligibility;
}

} // namespace ui::detail
