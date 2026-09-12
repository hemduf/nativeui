#pragma once

#include <nativeui/geometry.hpp>

#include <algorithm>
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

using VirtualSemanticItemToken = std::uint64_t;
inline constexpr VirtualSemanticItemToken kInvalidVirtualSemanticItemToken = 0;

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

enum class SemanticExpandedState {
    NotApplicable,
    Collapsed,
    Expanded,
};

enum class SemanticChange {
    StructureChanged,
    FocusChanged,
    SelectionChanged,
    ValueChanged,
    BoundsChanged,
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
    SemanticExpandedState expanded{SemanticExpandedState::NotApplicable};
    bool focusable{};
    bool focused{};
    std::vector<SemanticAction> actions;

    [[nodiscard]] bool supports(SemanticAction action) const noexcept {
        return std::find(actions.begin(), actions.end(), action) != actions.end();
    }

    bool operator==(const SemanticInfo&) const = default;
};

// A virtual semantic item token is not a hash. The owning virtual collection
// mints a non-zero token when a logical key enters the accepted dataset, keeps
// it stable while that key remains present (including reorder), and never
// reuses it for another live/stale item identity in that collection lifetime.
struct VirtualSemanticItemMetadata {
    VirtualSemanticItemToken token{kInvalidVirtualSemanticItemToken};
    std::string name;
    std::string description;
    bool enabled{true};
    bool read_only{};
    SemanticCheckedState checked{SemanticCheckedState::NotApplicable};
    std::vector<SemanticAction> actions;

    bool operator==(const VirtualSemanticItemMetadata&) const = default;
};

struct VirtualSemanticItem {
    VirtualSemanticItemToken token{kInvalidVirtualSemanticItemToken};
    SemanticInfo info;
    Rect logical_bounds{};
};

// Immutable, data-only virtual ListView snapshot. The O(N) metadata allocation
// is shared between semantic generations while selection/scroll geometry can
// vary in this cheap value. item_at() never calls application code or a visual
// row factory and therefore remains safe for immutable native read snapshots.
class VirtualSemanticChildren {
public:
    using Metadata = std::vector<VirtualSemanticItemMetadata>;
    using MetadataSnapshot = std::shared_ptr<const Metadata>;

    VirtualSemanticChildren()
        : metadata_(std::make_shared<const Metadata>()) {}

    [[nodiscard]] static VirtualSemanticChildren from_metadata(
        std::uint64_t dataset_generation,
        MetadataSnapshot metadata,
        std::optional<VirtualSemanticItemToken> selected,
        Rect list_bounds,
        float row_height,
        float scroll_y) {
        if (!metadata) {
            metadata = std::make_shared<const Metadata>();
        }
        return VirtualSemanticChildren{
            dataset_generation, std::move(metadata), selected, list_bounds, row_height, scroll_y};
    }

    [[nodiscard]] std::uint64_t dataset_generation() const noexcept {
        return dataset_generation_;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return metadata_->size();
    }

    [[nodiscard]] std::optional<VirtualSemanticItem> item_at(std::size_t index) const {
        if (index >= metadata_->size()) {
            return std::nullopt;
        }

        const auto& metadata = (*metadata_)[index];
        VirtualSemanticItem item;
        item.token = metadata.token;
        item.info.role = SemanticRole::ListItem;
        item.info.name = metadata.name;
        item.info.description = metadata.description;
        item.info.enabled = metadata.enabled;
        item.info.read_only = metadata.read_only;
        item.info.checked = metadata.checked;
        item.info.selected = selected_.has_value() && *selected_ == metadata.token;
        item.info.focusable = true;
        item.info.actions = metadata.actions;
        item.logical_bounds = {
            list_bounds_.x,
            list_bounds_.y + static_cast<float>(index) * row_height_ - scroll_y_,
            list_bounds_.w,
            row_height_,
        };
        return item;
    }

    [[nodiscard]] std::optional<std::size_t> index_of_selected_item() const noexcept {
        if (!selected_.has_value()) {
            return std::nullopt;
        }
        for (std::size_t index = 0; index < metadata_->size(); ++index) {
            if ((*metadata_)[index].token == *selected_) {
                return index;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] const MetadataSnapshot& metadata_snapshot() const noexcept {
        return metadata_;
    }

    [[nodiscard]] std::optional<VirtualSemanticItemToken> selected_token() const noexcept {
        return selected_;
    }

    [[nodiscard]] Rect list_bounds() const noexcept {
        return list_bounds_;
    }

    [[nodiscard]] float row_height() const noexcept {
        return row_height_;
    }

    [[nodiscard]] float scroll_y() const noexcept {
        return scroll_y_;
    }

private:
    VirtualSemanticChildren(std::uint64_t dataset_generation,
                            MetadataSnapshot metadata,
                            std::optional<VirtualSemanticItemToken> selected,
                            Rect list_bounds,
                            float row_height,
                            float scroll_y)
        : dataset_generation_(dataset_generation),
          metadata_(std::move(metadata)),
          selected_(selected),
          list_bounds_(list_bounds),
          row_height_(row_height),
          scroll_y_(scroll_y) {}

    std::uint64_t dataset_generation_{};
    MetadataSnapshot metadata_;
    std::optional<VirtualSemanticItemToken> selected_;
    Rect list_bounds_{};
    float row_height_{};
    float scroll_y_{};
};

struct SemanticNodeSnapshot {
    SemanticId id{kInvalidSemanticId};
    SemanticId parent{kInvalidSemanticId};
    SemanticInfo info;
    Rect bounds{};
    std::vector<SemanticId> children;
    std::optional<VirtualSemanticChildren> virtual_children;
};

struct SemanticTreeSnapshot {
    std::uint64_t generation{};
    SemanticId root{kInvalidSemanticId};
    std::vector<SemanticNodeSnapshot> nodes;
};

} // namespace ui
