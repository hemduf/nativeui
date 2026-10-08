#pragma once

#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/layout.hpp>
#include <nativeui/ui.hpp>
#include <nativeui/widgets.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ui {

using DialogActionId = std::string;

enum class DialogResultKind {
    Action,
    Dismissed,
};

struct DialogResult {
    DialogResultKind kind{DialogResultKind::Dismissed};
    DialogActionId action_id;
};

enum class DialogActionRole {
    Normal,
    Default,
    Cancel,
};

struct DialogAction {
    DialogActionId id;
    std::string label;
    bool enabled{true};
    DialogActionRole role{DialogActionRole::Normal};
};

struct DialogStyle {
    std::optional<float> width, viewport_margin, padding, section_gap, action_gap;
    std::optional<float> radius, border_width;
    std::optional<Color> background, border;
};
struct AlertDialogSpec {
    std::string title;
    std::string message;
    std::vector<DialogAction> actions;
};

struct DialogSpec {
    std::string title;
    Spec body;
    std::vector<DialogAction> actions;
    Color backdrop_color{0.0f, 0.0f, 0.0f, 0.48f};
    std::optional<DialogStyle> style;
    std::string description;
};

enum class DialogShowResult {
    Shown,
    Busy,
    InvalidSpec,
    Unavailable,
};

namespace detail {

inline constexpr float kDialogViewportMargin = 24.0f;
inline constexpr float kDialogMaximumWidth = 560.0f;
inline constexpr float kDialogPadding = 20.0f;
inline constexpr float kDialogSectionGap = 12.0f;
inline constexpr float kDialogActionGap = 8.0f;

struct DialogSpecValue {
    Spec value;
    Spec spec() && { return std::move(value); }
};

class DialogFixedEnabledComponent final : public Component {
public:
    explicit DialogFixedEnabledComponent(bool enabled);

    [[nodiscard]] ComponentAvailability local_availability() const noexcept override;

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override;

    void paint(PaintContext&) const override;

private:
    bool enabled_{true};
};

/// Full-viewport T063 content shell. T061 remains the sole overlay/modal stack;
/// this component only supplies the Dialog-specific visual backdrop and centers
/// the bounded panel inside the viewport. Because the T061 OverlayEntry itself
/// remains pointer-targetable over these full bounds, backdrop clicks are
/// consumed without introducing another hit-test or dismissal layer.
class DialogBackdropComponent final : public Component {
public:
    explicit DialogBackdropComponent(Color color);

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override;

    [[nodiscard]] ChildMetrics measure_constrained(
        const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override;

    void paint(PaintContext& context) const override;

private:
    Color color_{};
};

struct DialogPanelLayout {
    std::size_t body_index{};
    std::optional<std::size_t> title_index;
    std::vector<std::size_t> action_indices;
};

class DialogPanelComponent final : public Component, public ThemeBinding {
public:
    DialogPanelComponent(
        DialogPanelLayout layout,
        std::shared_ptr<ScrollState> body_scroll,
        std::function<void()> on_default);
    DialogPanelComponent(DialogPanelLayout layout, std::shared_ptr<ScrollState> body_scroll,
                         std::function<void()> on_default, DialogStyle style,
                         std::string title, std::string description);
    [[nodiscard]] SemanticInfo semantics() const override;

    // The nested focus scope lets T061 first enter this Dialog as one modal
    // unit, then select its first logical descendant. build_content() orders an
    // enabled Default action first when one exists; otherwise body descendants
    // are first. If there is no focusable descendant the panel itself remains
    // focused, while UI-level T063 Escape/programmatic close stay operational.
    [[nodiscard]] bool focusable() const noexcept override;
    [[nodiscard]] bool is_focus_scope() const noexcept override;
    [[nodiscard]] bool focus_scope_active() const noexcept override;
    [[nodiscard]] bool focus_scope_traps() const noexcept override;
    [[nodiscard]] std::size_t focus_scope_default_index() const noexcept override;
    [[nodiscard]] bool clips_children() const noexcept override;

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override;

    [[nodiscard]] ChildMetrics measure_constrained(
        const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override;

    EventResult input(const InputEvent& event, InputContext&) override;

    void paint(PaintContext& context) const override;

private:
    [[nodiscard]] Constraints outer_constraints(const Constraints& viewport) const noexcept;

    [[nodiscard]] float action_row_width(
        const std::vector<ChildMetrics>& children, bool minimum) const;

    [[nodiscard]] float action_row_height(
        const std::vector<ChildMetrics>& children, bool minimum) const noexcept;

    [[nodiscard]] Size panel_extent(
        const std::vector<ChildMetrics>& children, bool minimum) const;

    struct ActionRow { std::vector<std::size_t> indices; float width{}, height{}; };
    [[nodiscard]] std::vector<ActionRow> action_rows(const std::vector<ChildMetrics>& children,
                                                   float width, bool minimum) const;
    [[nodiscard]] float action_rows_height(const std::vector<ActionRow>& rows) const noexcept;
    [[nodiscard]] float padding() const noexcept;
    [[nodiscard]] float section_gap() const noexcept;
    [[nodiscard]] float action_gap() const noexcept;
    DialogStyle style_;
    std::string title_, description_;
    DialogPanelLayout layout_;
    // Own the canonical T034 state at the panel lifetime boundary. ScrollView
    // descendants only borrow it and are destroyed before this parent.
    std::shared_ptr<ScrollState> body_scroll_;
    std::function<void()> on_default_;
};

} // namespace detail

class Dialog {
public:
    using Completion = std::function<void(DialogResult)>;

    explicit Dialog(UI& ui);

    Dialog(const Dialog&) = delete;
    Dialog& operator=(const Dialog&) = delete;
    Dialog(Dialog&&) = delete;
    Dialog& operator=(Dialog&&) = delete;

    ~Dialog() noexcept;

    [[nodiscard]] DialogShowResult show(DialogSpec spec, Completion completion);
    [[nodiscard]] DialogShowResult show_alert(AlertDialogSpec spec, Completion completion);

    [[nodiscard]] bool active() const noexcept;

    /// Explicit controller/programmatic close follows the same exactly-once
    /// teardown path as an action but reports Dismissed.
    bool close();

private:
    [[nodiscard]] static bool valid_spec(const DialogSpec& spec);

    [[nodiscard]] static DialogResult escape_result_for(const DialogSpec& spec);

    [[nodiscard]] std::function<void()> guarded_completion(DialogResult result);

    [[nodiscard]] std::function<void()> guarded_abandon();

    [[nodiscard]] Spec action_spec(const DialogAction& action);

    [[nodiscard]] Spec build_content(DialogSpec spec);

    [[nodiscard]] static bool close_transferred(
        const detail::DialogState& state, std::uint64_t generation) noexcept;

    bool complete(DialogResult result);

    void abandon_without_completion();

    void finish_destructor_close_noexcept() noexcept;

    void clear_local_state() noexcept;

    UI* ui_{};
    std::weak_ptr<detail::DialogState> state_;
    std::shared_ptr<int> lifetime_;
    std::uint64_t generation_{};
    OverlayHandle overlay_{};
    Completion completion_;
    std::optional<DialogResult> pending_result_;
};

} // namespace ui
