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

/// Stable backend-neutral identity of one exposed semantic node. Zero is
/// reserved as invalid.
using SemanticId = std::uint64_t;
inline constexpr SemanticId kInvalidSemanticId = 0;

/// Stable identity of one logical item inside a virtual semantic collection.
/// Tokens are collection-owned identities, not hashes. Zero is invalid.
using VirtualSemanticItemToken = std::uint64_t;
inline constexpr VirtualSemanticItemToken kInvalidVirtualSemanticItemToken = 0;

/// Closed platform-neutral role set projected by NativeUI components.
///
/// `None` means the node itself is not an exposed semantic object and may be
/// flattened while semantic descendants remain visible.
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

/// Semantic actions a node explicitly advertises as currently supported.
/// Platform integrations must not assume an action that is absent here.
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

/// Tri-state checked projection. NotApplicable distinguishes controls that do
/// not expose checked state from an unchecked control.
enum class SemanticCheckedState {
    NotApplicable,
    Unchecked,
    Checked,
    Mixed,
};

/// Expansion state for controls such as combo boxes and expandable containers.
enum class SemanticExpandedState {
    NotApplicable,
    Collapsed,
    Expanded,
};

/// Coarse notification categories used when comparing semantic generations.
enum class SemanticChange {
    StructureChanged,
    FocusChanged,
    SelectionChanged,
    ValueChanged,
    BoundsChanged,
};

/// Numeric accessibility range for value controls.
///
/// Values are semantic/application-domain values, not screen coordinates.
struct SemanticValueRange {
    double minimum{};
    double maximum{};
    double step{};

    bool operator==(const SemanticValueRange&) const = default;
};

/// Owned platform-neutral semantic description of one component/node.
///
/// Strings, optional values and the action list are owned by the value; no
/// native accessibility object or retained-tree pointer is exposed. Availability
/// fields describe the current effective semantic state.
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

    /// Exact membership test for an advertised semantic action.
    [[nodiscard]] bool supports(SemanticAction action) const noexcept {
        return std::find(actions.begin(), actions.end(), action) != actions.end();
    }

    bool operator==(const SemanticInfo&) const = default;
};

/// Immutable metadata for one logical item in a virtual semantic collection.
///
/// A token is not a hash. The owning collection mints a non-zero token when a
/// logical key enters the accepted dataset, keeps it stable while that key
/// remains present (including reorder), and never reuses it for another
/// live/stale item identity during that collection lifetime.
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

/// Materialized semantic projection for one requested virtual item.
///
/// `logical_bounds` is NativeUI logical view geometry, not physical/screen
/// coordinates.
struct VirtualSemanticItem {
    VirtualSemanticItemToken token{kInvalidVirtualSemanticItemToken};
    SemanticInfo info;
    Rect logical_bounds{};
};

/// Immutable, data-only semantic view of a virtualized fixed-row collection.
///
/// The O(N) metadata allocation is shared through `MetadataSnapshot`; selection
/// and scroll geometry remain cheap value state. `item_at()` performs only
/// deterministic metadata lookup/geometry arithmetic and never calls application
/// code or a visual row factory.
class VirtualSemanticChildren {
public:
    using Metadata = std::vector<VirtualSemanticItemMetadata>;
    using MetadataSnapshot = std::shared_ptr<const Metadata>;

    VirtualSemanticChildren()
        : metadata_(std::make_shared<const Metadata>()) {}

    /// Build a snapshot from immutable metadata and current selection/geometry.
    /// A null metadata pointer is normalized to an empty immutable metadata set.
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

    /// Application/collection generation associated with the immutable metadata.
    [[nodiscard]] std::uint64_t dataset_generation() const noexcept {
        return dataset_generation_;
    }

    /// Full logical item count represented by this semantic collection.
    [[nodiscard]] std::size_t size() const noexcept {
        return metadata_->size();
    }

    /// Materialize one logical item by index, or nullopt when out of range.
    ///
    /// Bounds are computed from `list_bounds`, fixed `row_height` and current
    /// vertical scroll offset without mounting a visual row.
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

    /// Find the current selected token in the immutable metadata, if present.
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

    /// Borrow the shared immutable metadata owner. The returned reference is
    /// tied to this VirtualSemanticChildren value's lifetime.
    [[nodiscard]] const MetadataSnapshot& metadata_snapshot() const noexcept {
        return metadata_;
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

/// Immutable value projection of one exposed semantic node.
///
/// `bounds` uses NativeUI logical view-relative coordinates. `children` stores
/// semantic identities rather than retained pointers.
struct SemanticNodeSnapshot {
    SemanticId id{kInvalidSemanticId};
    SemanticId parent{kInvalidSemanticId};
    SemanticInfo info;
    Rect bounds{};
    std::vector<SemanticId> children;
    std::optional<VirtualSemanticChildren> virtual_children;
};

/// One immutable semantic tree generation.
///
/// Snapshots are data-only and contain no live component/native object pointers,
/// allowing readers to retain an older generation while a newer one is published.
struct SemanticTreeSnapshot {
    std::uint64_t generation{};
    SemanticId root{kInvalidSemanticId};
    std::vector<SemanticNodeSnapshot> nodes;
};

} // namespace ui
