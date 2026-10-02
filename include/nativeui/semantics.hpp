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
/// Canonical invalid semantic-node identity; live runtime nodes use non-zero IDs.
/// This scalar owns no component, tree node, view, or native accessibility object.
inline constexpr SemanticId kInvalidSemanticId = 0;

/// Stable identity of one logical item inside a virtual semantic collection.
/// Tokens are collection-owned identities, not hashes. Zero is invalid.
using VirtualSemanticItemToken = std::uint64_t;
/// Canonical invalid virtual-item token. Tokens are collection-local values and
/// do not retain the collection that minted them.
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
    /// Inclusive lower application-domain value; not a screen coordinate.
    double minimum{};
    /// Inclusive upper application-domain value in the same units as minimum.
    double maximum{};
    /// Producer-declared increment in the same application-domain units.
    double step{};

    /// Exact field-wise equality; floating-point values are not epsilon-normalized.
    /// This value performs no validation or clamping; producers own range coherence.
    bool operator==(const SemanticValueRange&) const = default;
};

/// Owned platform-neutral semantic description of one component/node.
///
/// Strings, optional values and the action list are owned by the value; no
/// native accessibility object or retained-tree pointer is exposed. Availability
/// fields describe the current effective semantic state.
struct SemanticInfo {
    /// Backend-neutral role; None is the flatten/no-direct-object default.
    SemanticRole role{SemanticRole::None};
    /// Owned UTF-8 accessible name or label.
    std::string name;
    /// Owned UTF-8 help or description text.
    std::string description;
    /// Optional owned UTF-8 textual value for text-like controls.
    std::optional<std::string> text_value;
    /// Optional application-domain numeric value; not a pixel coordinate.
    std::optional<double> numeric_value;
    /// Optional application-domain range and step associated with numeric_value.
    std::optional<SemanticValueRange> value_range;
    /// Effective enabled state exposed to semantic consumers.
    bool enabled{true};
    /// Effective read-only capability state.
    bool read_only{};
    /// Checked/indeterminate state, or NotApplicable when not meaningful.
    SemanticCheckedState checked{SemanticCheckedState::NotApplicable};
    /// Current logical selection state for selectable objects.
    bool selected{};
    /// Expansion state, or NotApplicable when the role has no expansion concept.
    SemanticExpandedState expanded{SemanticExpandedState::NotApplicable};
    /// Whether semantic or keyboard focus may currently be requested.
    bool focusable{};
    /// Whether this object is the current NativeUI focus owner.
    bool focused{};
    /// Owned explicit action set; missing actions are not inferred from role.
    std::vector<SemanticAction> actions;

    /// Exact membership test for an advertised semantic action.
    /// Linear and allocation-free; duplicate entries are not canonicalized.
    [[nodiscard]] bool supports(SemanticAction action) const noexcept {
        return std::find(actions.begin(), actions.end(), action) != actions.end();
    }

    /// Exact field-wise equality, including floating-point values and action ordering.
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
    /// Owned O(N) metadata container for one virtual dataset generation.
    using Metadata = std::vector<VirtualSemanticItemMetadata>;
    /// Shared immutable metadata owner; copying extends metadata lifetime in O(1).
    using MetadataSnapshot = std::shared_ptr<const Metadata>;

    /// Construct an empty collection. Creating shared empty metadata may allocate;
    /// this convenience construction is not audio/DSP real-time safe.
    VirtualSemanticChildren()
        : metadata_(std::make_shared<const Metadata>()) {}

    /// Build a snapshot from immutable metadata and current selection/geometry.
    /// A null metadata pointer is normalized to an empty immutable metadata set.
    /// Geometry inputs use NativeUI logical units and are carried verbatim: this
    /// low-level constructor performs no finite/positive/clamping validation.
    /// Non-null metadata is shared without O(N) copying and must remain immutable
    /// after publication. Null metadata allocates a shared empty set. No callbacks
    /// or retained-tree mutation are performed.
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
    /// Producer-owned generation token; this value type does not enforce monotonicity.
    [[nodiscard]] std::uint64_t dataset_generation() const noexcept {
        return dataset_generation_;
    }

    /// Full logical item count represented by this semantic collection.
    /// O(1), allocation-free logical item count; no row materialization occurs.
    [[nodiscard]] std::size_t size() const noexcept {
        return metadata_->size();
    }

    /// Materialize one logical item by index, or nullopt when out of range.
    ///
    /// Bounds are computed from `list_bounds`, fixed `row_height` and current
    /// vertical scroll offset without mounting a visual row.
    /// Returned bounds are not clipped to the visible list viewport. Materializing
    /// owned strings/actions may allocate or throw. No application callback or
    /// retained-tree mutation occurs; this is not audio/DSP real-time work.
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
    /// Allocation-free O(N) token scan; nullopt also covers a selected token
    /// absent from this metadata generation.
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
    /// The returned shared_ptr reference is borrowed from this value; copy the
    /// handle to extend metadata lifetime independently.
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
    /// Stable semantic identity; runtime-produced exposed nodes use non-zero IDs.
    SemanticId id{kInvalidSemanticId};
    /// Parent identity, or kInvalidSemanticId for the exposed root.
    SemanticId parent{kInvalidSemanticId};
    /// Owned semantic value captured for this generation.
    SemanticInfo info;
    /// View-relative NativeUI logical bounds; no device-scale conversion applied.
    Rect bounds{};
    /// Ordered owned child identities; no raw child pointers are retained.
    std::vector<SemanticId> children;
    /// Optional data-only virtual collection projection owned by this snapshot.
    std::optional<VirtualSemanticChildren> virtual_children;
};

/// One immutable semantic tree generation.
///
/// Snapshots are data-only and contain no live component/native object pointers,
/// allowing readers to retain an older generation while a newer one is published.
struct SemanticTreeSnapshot {
    /// Producer-owned generation token; the aggregate does not enforce monotonicity.
    std::uint64_t generation{};
    /// Root identity, or kInvalidSemanticId for an empty snapshot.
    SemanticId root{kInvalidSemanticId};
    /// Owned records; use IDs rather than vector element addresses as stable identity.
    std::vector<SemanticNodeSnapshot> nodes;
};

} // namespace ui
