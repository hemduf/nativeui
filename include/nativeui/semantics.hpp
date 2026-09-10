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
struct VirtualSemanticItem {
    VirtualSemanticItemToken token{kInvalidVirtualSemanticItemToken};
    SemanticInfo info;
    Rect logical_bounds{};
};

// Immutable snapshot interface for a logical virtual collection generation.
// Implementations may share an O(N) immutable metadata object and combine it
// with small per-generation selection/geometry state; item_at() must not mount
// or otherwise materialize visual retained rows.
class VirtualSemanticChildren {
public:
    virtual ~VirtualSemanticChildren() = default;

    [[nodiscard]] virtual std::uint64_t dataset_generation() const noexcept = 0;
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
    [[nodiscard]] virtual std::optional<VirtualSemanticItem> item_at(
        std::size_t index) const = 0;
    [[nodiscard]] virtual std::optional<std::size_t> index_of_selected_item() const noexcept = 0;
};

using VirtualSemanticChildrenSnapshot = std::shared_ptr<const VirtualSemanticChildren>;

struct SemanticNodeSnapshot {
    SemanticId id{kInvalidSemanticId};
    SemanticId parent{kInvalidSemanticId};
    SemanticInfo info;
    Rect bounds{};
    std::vector<SemanticId> children;
    VirtualSemanticChildrenSnapshot virtual_children;
};

struct SemanticTreeSnapshot {
    std::uint64_t generation{};
    SemanticId root{kInvalidSemanticId};
    std::vector<SemanticNodeSnapshot> nodes;
};

} // namespace ui
