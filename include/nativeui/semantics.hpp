#pragma once

#include <nativeui/geometry.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui {

using SemanticId = std::uint64_t;
inline constexpr SemanticId kInvalidSemanticId = 0;

enum class SemanticRole {
    None,
    Button,
    Checkbox,
    RadioButton,
    Toggle,
    Slider,
    RangeSliderHandle,
    ProgressBar,
    Meter,
    Text,
    TextInput,
    TextArea,
    ComboBox,
    PopupMenu,
    MenuItem,
    ListView,
    ListItem,
    Tabs,
    Tab,
    TabPanel,
    Dialog,
    Group,
    Image,
    Custom,
};

enum class SemanticAction {
    Activate,
    Toggle,
    Focus,
    Increment,
    Decrement,
    SetValue,
    Select,
    Expand,
    Collapse,
};

enum class SemanticCheckedState {
    NotApplicable,
    Unchecked,
    Checked,
    Mixed,
};

struct SemanticValueRange {
    double minimum{};
    double maximum{};
    double step{};

    bool operator==(const SemanticValueRange&) const = default;
};

struct SemanticInfo {
    SemanticRole role{SemanticRole::None};
    std::string name;
    std::string description;
    std::optional<std::string> text_value;
    std::optional<double> numeric_value;
    std::optional<SemanticValueRange> value_range;
    bool enabled{true};
    bool read_only{};
    SemanticCheckedState checked{SemanticCheckedState::NotApplicable};
    bool selected{};
    bool expanded{};
    bool focusable{};
    bool focused{};
    std::vector<SemanticAction> actions;

    bool operator==(const SemanticInfo&) const = default;
};

struct VirtualSemanticItem {
    SemanticId id{kInvalidSemanticId};
    std::string name;
    std::string description;
    bool enabled{true};
    bool selected{};
    Rect logical_bounds{};
    std::vector<SemanticAction> actions;
};

class VirtualSemanticChildren {
public:
    VirtualSemanticChildren()
        : items_(std::make_shared<const std::vector<VirtualSemanticItem>>()) {}

    [[nodiscard]] static VirtualSemanticChildren from_items(
        std::vector<VirtualSemanticItem> items) {
        return VirtualSemanticChildren{
            std::make_shared<const std::vector<VirtualSemanticItem>>(std::move(items))};
    }

    [[nodiscard]] std::size_t size() const noexcept { return items_->size(); }

    [[nodiscard]] std::optional<VirtualSemanticItem> item_at(std::size_t index) const {
        if (index >= items_->size()) {
            return std::nullopt;
        }
        return (*items_)[index];
    }

    [[nodiscard]] std::optional<std::size_t> index_of_selected_item() const noexcept {
        for (std::size_t index = 0; index < items_->size(); ++index) {
            if ((*items_)[index].selected) {
                return index;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::shared_ptr<const std::vector<VirtualSemanticItem>> snapshot() const noexcept {
        return items_;
    }

private:
    explicit VirtualSemanticChildren(
        std::shared_ptr<const std::vector<VirtualSemanticItem>> items)
        : items_(std::move(items)) {}

    std::shared_ptr<const std::vector<VirtualSemanticItem>> items_;
};

struct SemanticNodeSnapshot {
    SemanticId id{kInvalidSemanticId};
    SemanticId parent{kInvalidSemanticId};
    SemanticInfo info;
    Rect bounds{};
    std::vector<SemanticId> children;
    std::optional<VirtualSemanticChildren> virtual_children;
};

} // namespace ui
