#pragma once

#include <nativeui/combo_popup_style.hpp>
#include <nativeui/component.hpp>
#include <nativeui/detail/overlay_commands.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/state.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui {

/// One owned ComboBox option snapshot.
///
/// The popup session owns a vector of these values; no string or value borrow is
/// retained from the provider that produced the snapshot. `value` is compared
/// with the current selection using `operator==` and is copied into the deferred
/// commit path before the popup is detached. `label` is UTF-8 presentation text.
/// Disabled options remain visible but are skipped by keyboard/pointer commit.
///
/// This is an aggregate value. Copy/move/allocation behavior is therefore that of
/// `T` and `std::string`; creating option snapshots is UI-domain work and is
/// not intended for an audio/DSP real-time callback.
template <class T>
struct ComboBoxOption final {
    /// Value written to the selection Binding after a successful commit.
    T value;
    /// Owned UTF-8 row/anchor label.
    std::string label;
    /// Whether this row participates in navigation and can be committed.
    bool enabled{true};
};

/// One owned PopupMenu row snapshot.
///
/// Action rows own their UTF-8 label and callback object. The menu session owns
/// the snapshot, so provider-local strings/callback wrappers may be destroyed
/// immediately after the provider returns. Separators are structural and never
/// invoke application code.
///
/// Action callbacks execute in the UI domain only after the popup has closed and
/// detached at its retained safe commit point. They may allocate or perform
/// ordinary application work and are not audio-RT callbacks. An empty callback
/// deliberately makes an Action row non-actionable even when `enabled == true`.
struct PopupMenuItem final {
    /// Distinguishes callable action rows from visual separators.
    enum class Kind {
        /// A row that may invoke `callback` when enabled and non-empty.
        Action,
        /// A visual separator; never focusable/actionable.
        Separator,
    };

    /// Row category controlling interaction and rendering.
    Kind kind{Kind::Action};
    /// Owned UTF-8 label for Action rows; ignored by Separator rows.
    std::string label;
    /// Whether an Action row may be highlighted and invoked.
    bool enabled{true};
    /// Owned application callback. Empty callbacks are treated as non-actionable.
    std::function<void()> callback;

    /// Build an owned action row.
    ///
    /// `label` and `callback` are moved into the returned value. Setting
    /// `enabled` false keeps the row visible while excluding it from keyboard
    /// navigation and pointer/keyboard activation. Passing an empty callback is
    /// also valid and produces a non-actionable row.
    [[nodiscard]] static PopupMenuItem action(
        std::string label,
        std::function<void()> callback,
        bool enabled = true) {
        PopupMenuItem item;
        item.kind = Kind::Action;
        item.label = std::move(label);
        item.enabled = enabled;
        item.callback = std::move(callback);
        return item;
    }

    /// Build a disabled non-actionable separator row with no callback.
    [[nodiscard]] static PopupMenuItem separator() {
        PopupMenuItem item;
        item.kind = Kind::Separator;
        item.enabled = false;
        return item;
    }

    /// Test whether user activation can invoke this row.
    ///
    /// Returns true only for `Kind::Action`, `enabled == true`, and a non-empty
    /// callback. This is a pure, allocation-free query.
    [[nodiscard]] bool actionable() const noexcept {
        return kind == Kind::Action && enabled && static_cast<bool>(callback);
    }
};

namespace detail {

inline constexpr std::size_t kNoPopupIndex = std::numeric_limits<std::size_t>::max();

template <class Predicate>
[[nodiscard]] std::size_t first_popup_index(std::size_t count, Predicate&& eligible) {
    for (std::size_t i = 0; i < count; ++i) {
        if (eligible(i)) return i;
    }
    return kNoPopupIndex;
}

template <class Predicate>
[[nodiscard]] std::size_t last_popup_index(std::size_t count, Predicate&& eligible) {
    for (std::size_t i = count; i > 0; --i) {
        if (eligible(i - 1)) return i - 1;
    }
    return kNoPopupIndex;
}

template <class Predicate>
[[nodiscard]] std::size_t step_popup_index(
    std::size_t current, std::size_t count, int direction, Predicate&& eligible) {
    if (count == 0) return kNoPopupIndex;
    if (current == kNoPopupIndex || current >= count) {
        return direction < 0
            ? last_popup_index(count, std::forward<Predicate>(eligible))
            : first_popup_index(count, std::forward<Predicate>(eligible));
    }
    for (std::size_t step = 1; step <= count; ++step) {
        const auto index = direction < 0
            ? (current + count - (step % count)) % count
            : (current + step) % count;
        if (eligible(index)) return index;
    }
    return kNoPopupIndex;
}

[[nodiscard]] inline TextStyle combo_anchor_text_style(
    const ResolvedComboBoxStyle& resolved,
    TextAlign align = TextAlign::Center) {
    TextStyle style{};
    style.size = resolved.text_size;
    style.color = resolved.text;
    style.align = align;
    style.weight = resolved.text_weight;
    style.slant = resolved.text_slant;
    style.family = resolved.font_family;
    style.fallback_families = resolved.fallback_families;
    return style;
}

[[nodiscard]] inline TextStyle menu_item_text_style(
    const ResolvedMenuItemStyle& resolved,
    TextAlign align = TextAlign::Left) {
    TextStyle style{};
    style.size = resolved.text_size;
    style.color = resolved.text;
    style.align = align;
    style.weight = resolved.text_weight;
    style.slant = resolved.text_slant;
    style.family = resolved.font_family;
    style.fallback_families = resolved.fallback_families;
    return style;
}

[[nodiscard]] inline bool combo_box_layout_equal(const ResolvedComboBoxStyle& lhs,
                                                 const ResolvedComboBoxStyle& rhs) {
    return lhs.minimum_width == rhs.minimum_width &&
           lhs.control_height == rhs.control_height &&
           lhs.horizontal_padding == rhs.horizontal_padding &&
           lhs.text_size == rhs.text_size &&
           lhs.text_weight == rhs.text_weight &&
           lhs.text_slant == rhs.text_slant &&
           lhs.font_family == rhs.font_family &&
           lhs.fallback_families == rhs.fallback_families;
}

[[nodiscard]] inline bool menu_item_layout_equal(const ResolvedMenuItemStyle& lhs,
                                                 const ResolvedMenuItemStyle& rhs) {
    return lhs.row_height == rhs.row_height &&
           lhs.horizontal_padding == rhs.horizontal_padding &&
           lhs.text_size == rhs.text_size &&
           lhs.text_weight == rhs.text_weight &&
           lhs.text_slant == rhs.text_slant &&
           lhs.font_family == rhs.font_family &&
           lhs.fallback_families == rhs.fallback_families;
}

template <class Resolved, class Context, class SameLayout>
void invalidate_resolved_popup_style_transition(const Resolved& before,
                                                const Resolved& after,
                                                Context& context,
                                                SameLayout&& same_layout) {
    if (before == after) return;
    if (!same_layout(before, after)) {
        context.invalidate_layout();
        context.invalidate();
        return;
    }
    context.invalidate();
}

template <class T>
struct ComboAnchorRuntime final {
    std::optional<Binding<T>> selection;
    NodeId node_id{kInvalidNodeId};
    OverlayHandle handle;
    Key suppress_until_key_up{Key::None};
    bool mounted{};
};

template <class T>
struct ComboPopupSession final {
    std::shared_ptr<ComboAnchorRuntime<T>> anchor;
    std::vector<ComboBoxOption<T>> options;
    MenuItemStyle item_style;
    OverlayHandle handle;
    std::size_t highlighted{kNoPopupIndex};
    bool completion_queued{};
};

struct MenuAnchorRuntime final {
    NodeId node_id{kInvalidNodeId};
    OverlayHandle handle;
    Key suppress_until_key_up{Key::None};
    bool mounted{};
};

struct MenuPopupSession final {
    std::shared_ptr<MenuAnchorRuntime> anchor;
    std::vector<PopupMenuItem> items;
    MenuItemStyle item_style;
    OverlayHandle handle;
    std::size_t highlighted{kNoPopupIndex};
    bool completion_queued{};
};

template <class T>
class ComboPopupComponent final : public Component,
                                  public ThemeBinding,
                                  public OverlayCommandSource {
public:
    explicit ComboPopupComponent(std::shared_ptr<ComboPopupSession<T>> session)
        : session_(std::move(session)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto base = base_item_style();
        float width = current_theme().controls.minimum_width;
        float height = 0.0f;
        for (std::size_t i = 0; i < session_->options.size(); ++i) {
            const auto resolved = resolved_item_style(i);
            width = std::max(
                width,
                TextService::measure(session_->options[i].label, menu_item_text_style(resolved)).width +
                    resolved.horizontal_padding * 2.0f);
            height += std::max(0.0f, resolved.row_height);
        }
        if (session_->options.empty()) height = std::max(0.0f, base.row_height);
        return {width, height};
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        auto& anchor = *session_->anchor;
        if (anchor.suppress_until_key_up != Key::None) {
            if (event.type == InputType::KeyUp && event.key == anchor.suppress_until_key_up) {
                anchor.suppress_until_key_up = Key::None;
                return EventResult::Handled;
            }
            if (event.type == InputType::KeyDown && event.key == anchor.suppress_until_key_up) {
                return EventResult::Handled;
            }
        }

        if (event.type == InputType::KeyDown) {
            switch (event.key) {
                case Key::Up:
                    move_highlight(-1, context);
                    return EventResult::Handled;
                case Key::Down:
                    move_highlight(1, context);
                    return EventResult::Handled;
                case Key::Home:
                    set_highlight(first_enabled(), context);
                    return EventResult::Handled;
                case Key::End:
                    set_highlight(last_enabled(), context);
                    return EventResult::Handled;
                case Key::Enter:
                case Key::Space:
                    queue_commit(event.key);
                    return EventResult::Handled;
                default:
                    break;
            }
        }

        switch (event.type) {
            case InputType::PointerDown:
                set_pointer_armed(true, context);
                context.capture_pointer();
                update_pointer_highlight(event.position, context);
                return EventResult::Handled;
            case InputType::PointerMove:
                update_pointer_highlight(event.position, context);
                return EventResult::Handled;
            case InputType::PointerUp: {
                if (!pointer_armed_) return EventResult::Handled;
                set_pointer_armed(false, context);
                context.release_pointer();
                const auto index = index_at(event.position, context.bounds());
                if (index != kNoPopupIndex && session_->options[index].enabled) {
                    set_highlight(index, context);
                    queue_commit(Key::None);
                }
                return EventResult::Handled;
            }
            case InputType::PointerCancel:
                if (pointer_armed_) {
                    set_pointer_armed(false, context);
                    context.release_pointer();
                }
                return EventResult::Handled;
            default:
                break;
        }
        return EventResult::Ignored;
    }

    [[nodiscard]] std::optional<OverlayComponentCommand>
    take_overlay_command() override {
        auto command = std::move(pending_command_);
        pending_command_.reset();
        return command;
    }

    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        const auto& theme = current_theme();
        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds, theme.radii.medium, theme.palette.surface);
        painter.stroke_rounded_rect(
            bounds, theme.radii.medium, theme.controls.border_width, theme.palette.border);

        float y = bounds.y;
        for (std::size_t i = 0; i < session_->options.size(); ++i) {
            const auto resolved = resolved_item_style(i);
            const float row_height = std::max(0.0f, resolved.row_height);
            const Rect row{bounds.x, y, bounds.w, row_height};
            painter.fill_rounded_rect(row, resolved.corner_radius, resolved.fill);
            painter.text(
                {row.x + resolved.horizontal_padding, row.y + row.h * 0.5f},
                session_->options[i].label,
                menu_item_text_style(resolved));
            y += row_height;
        }
    }

private:
    [[nodiscard]] ResolvedMenuItemStyle base_item_style() const {
        return resolve_menu_item_style(
            default_menu_item_style(current_theme()), session_->item_style, VisualState{});
    }

    [[nodiscard]] ResolvedMenuItemStyle resolved_item_style(std::size_t index) const {
        const bool highlighted = index == session_->highlighted;
        return resolve_menu_item_style(
            default_menu_item_style(current_theme()),
            session_->item_style,
            VisualState{
                .enabled = session_->options[index].enabled,
                .hovered = highlighted,
                .pressed = highlighted && pointer_armed_,
                .selected = highlighted,
            });
    }

    [[nodiscard]] std::size_t first_enabled() const {
        return first_popup_index(session_->options.size(), [&](std::size_t index) {
            return session_->options[index].enabled;
        });
    }

    [[nodiscard]] std::size_t last_enabled() const {
        return last_popup_index(session_->options.size(), [&](std::size_t index) {
            return session_->options[index].enabled;
        });
    }

    void move_highlight(int direction, InputContext& context) {
        set_highlight(
            step_popup_index(
                session_->highlighted,
                session_->options.size(),
                direction,
                [&](std::size_t index) { return session_->options[index].enabled; }),
            context);
    }

    void set_highlight(std::size_t index, InputContext& context) {
        if (session_->highlighted == index) return;
        const auto previous = session_->highlighted;
        std::optional<ResolvedMenuItemStyle> before_previous;
        std::optional<ResolvedMenuItemStyle> before_next;
        if (previous < session_->options.size()) before_previous = resolved_item_style(previous);
        if (index < session_->options.size() && index != previous) before_next = resolved_item_style(index);

        session_->highlighted = index;

        bool presentation_changed = false;
        bool layout_changed = false;
        const auto classify = [&](const std::optional<ResolvedMenuItemStyle>& before, std::size_t item) {
            if (!before || item >= session_->options.size()) return;
            const auto after = resolved_item_style(item);
            presentation_changed = presentation_changed || !(*before == after);
            layout_changed = layout_changed || !menu_item_layout_equal(*before, after);
        };
        classify(before_previous, previous);
        classify(before_next, index);
        if (layout_changed) context.invalidate_layout();
        if (presentation_changed) context.invalidate();
    }

    void set_pointer_armed(bool armed, InputContext& context) {
        if (pointer_armed_ == armed) return;
        std::optional<ResolvedMenuItemStyle> before;
        if (session_->highlighted < session_->options.size()) {
            before = resolved_item_style(session_->highlighted);
        }
        pointer_armed_ = armed;
        if (!before || session_->highlighted >= session_->options.size()) return;
        const auto after = resolved_item_style(session_->highlighted);
        invalidate_resolved_popup_style_transition(
            *before, after, context, menu_item_layout_equal);
    }

    [[nodiscard]] std::size_t index_at(Point point, Rect bounds) const {
        if (!bounds.contains(point) || session_->options.empty()) return kNoPopupIndex;
        float y = bounds.y;
        for (std::size_t i = 0; i < session_->options.size(); ++i) {
            const float height = std::max(0.0f, resolved_item_style(i).row_height);
            if (point.y >= y && point.y < y + height) return i;
            y += height;
        }
        return kNoPopupIndex;
    }

    void update_pointer_highlight(Point point, InputContext& context) {
        const auto index = index_at(point, context.bounds());
        const auto next = index != kNoPopupIndex && session_->options[index].enabled
            ? index
            : kNoPopupIndex;
        set_highlight(next, context);
    }

    void queue_commit(Key trigger_key) {
        if (session_->completion_queued || pending_command_ ||
            session_->highlighted == kNoPopupIndex ||
            session_->highlighted >= session_->options.size() ||
            !session_->options[session_->highlighted].enabled) {
            return;
        }

        T value = session_->options[session_->highlighted].value;
        auto weak_anchor = std::weak_ptr<ComboAnchorRuntime<T>>{session_->anchor};
        const auto anchor_id = session_->anchor->node_id;
        if (trigger_key != Key::None) {
            session_->anchor->suppress_until_key_up = trigger_key;
        }
        session_->completion_queued = true;
        pending_command_ = OverlayComponentCommand::close_then_invoke(
            session_->handle,
            anchor_id,
            true,
            [weak_anchor, value = std::move(value)]() mutable {
                auto anchor = weak_anchor.lock();
                if (!anchor || !anchor->mounted || !anchor->selection) return;
                anchor->selection->set(std::move(value));
            });
    }

    std::shared_ptr<ComboPopupSession<T>> session_;
    std::optional<OverlayComponentCommand> pending_command_;
    bool pointer_armed_{};
};

class MenuPopupComponent final : public Component,
                                 public ThemeBinding,
                                 public OverlayCommandSource {
public:
    explicit MenuPopupComponent(std::shared_ptr<MenuPopupSession> session)
        : session_(std::move(session)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto base = base_item_style();
        float width = current_theme().controls.minimum_width;
        float height = 0.0f;
        for (std::size_t i = 0; i < session_->items.size(); ++i) {
            const auto& item = session_->items[i];
            if (item.kind == PopupMenuItem::Kind::Separator) {
                height += std::max(0.0f, base.separator_height);
                continue;
            }
            const auto resolved = resolved_item_style(i);
            width = std::max(
                width,
                TextService::measure(item.label, menu_item_text_style(resolved)).width +
                    resolved.horizontal_padding * 2.0f);
            height += std::max(0.0f, resolved.row_height);
        }
        return {width, std::max(height, std::max(0.0f, base.row_height))};
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        auto& anchor = *session_->anchor;
        if (anchor.suppress_until_key_up != Key::None) {
            if (event.type == InputType::KeyUp && event.key == anchor.suppress_until_key_up) {
                anchor.suppress_until_key_up = Key::None;
                return EventResult::Handled;
            }
            if (event.type == InputType::KeyDown && event.key == anchor.suppress_until_key_up) {
                return EventResult::Handled;
            }
        }

        if (event.type == InputType::KeyDown) {
            switch (event.key) {
                case Key::Up:
                    move_highlight(-1, context);
                    return EventResult::Handled;
                case Key::Down:
                    move_highlight(1, context);
                    return EventResult::Handled;
                case Key::Home:
                    set_highlight(first_enabled(), context);
                    return EventResult::Handled;
                case Key::End:
                    set_highlight(last_enabled(), context);
                    return EventResult::Handled;
                case Key::Enter:
                case Key::Space:
                    queue_action(event.key);
                    return EventResult::Handled;
                default:
                    break;
            }
        }

        switch (event.type) {
            case InputType::PointerDown: {
                const auto index = index_at(event.position, context.bounds());
                const bool armed = index != kNoPopupIndex && selectable(index);
                set_pointer_armed(armed, context);
                if (armed) context.capture_pointer();
                update_pointer_highlight(event.position, context);
                return EventResult::Handled;
            }
            case InputType::PointerMove:
                update_pointer_highlight(event.position, context);
                return EventResult::Handled;
            case InputType::PointerUp: {
                const bool armed = pointer_armed_;
                set_pointer_armed(false, context);
                if (armed) context.release_pointer();
                const auto index = index_at(event.position, context.bounds());
                if (armed && index != kNoPopupIndex && selectable(index)) {
                    set_highlight(index, context);
                    queue_action(Key::None);
                }
                return EventResult::Handled;
            }
            case InputType::PointerCancel:
                if (pointer_armed_) {
                    set_pointer_armed(false, context);
                    context.release_pointer();
                }
                return EventResult::Handled;
            default:
                break;
        }
        return EventResult::Ignored;
    }

    [[nodiscard]] std::optional<OverlayComponentCommand>
    take_overlay_command() override {
        auto command = std::move(pending_command_);
        pending_command_.reset();
        return command;
    }

    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        const auto& theme = current_theme();
        const auto base = base_item_style();
        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds, theme.radii.medium, theme.palette.surface);
        painter.stroke_rounded_rect(
            bounds, theme.radii.medium, theme.controls.border_width, theme.palette.border);

        float y = bounds.y;
        for (std::size_t i = 0; i < session_->items.size(); ++i) {
            const auto& item = session_->items[i];
            if (item.kind == PopupMenuItem::Kind::Separator) {
                const float separator_height = std::max(0.0f, base.separator_height);
                painter.line(
                    {bounds.x + base.separator_inset, y + separator_height * 0.5f},
                    {bounds.x + bounds.w - base.separator_inset,
                     y + separator_height * 0.5f},
                    base.separator_width,
                    base.separator);
                y += separator_height;
                continue;
            }

            const auto resolved = resolved_item_style(i);
            const float row_height = std::max(0.0f, resolved.row_height);
            const Rect row{bounds.x, y, bounds.w, row_height};
            painter.fill_rounded_rect(row, resolved.corner_radius, resolved.fill);
            painter.text(
                {row.x + resolved.horizontal_padding, row.y + row.h * 0.5f},
                item.label,
                menu_item_text_style(resolved));
            y += row_height;
        }
    }

private:
    [[nodiscard]] ResolvedMenuItemStyle base_item_style() const {
        return resolve_menu_item_style(
            default_menu_item_style(current_theme()), session_->item_style, VisualState{});
    }

    [[nodiscard]] ResolvedMenuItemStyle resolved_item_style(std::size_t index) const {
        const bool highlighted = index == session_->highlighted;
        return resolve_menu_item_style(
            default_menu_item_style(current_theme()),
            session_->item_style,
            VisualState{
                .enabled = selectable(index),
                .hovered = highlighted,
                .pressed = highlighted && pointer_armed_,
                .selected = highlighted,
            });
    }

    [[nodiscard]] bool selectable(std::size_t index) const noexcept {
        return index < session_->items.size() && session_->items[index].actionable();
    }

    [[nodiscard]] std::size_t first_enabled() const {
        return first_popup_index(session_->items.size(), [&](std::size_t index) {
            return selectable(index);
        });
    }

    [[nodiscard]] std::size_t last_enabled() const {
        return last_popup_index(session_->items.size(), [&](std::size_t index) {
            return selectable(index);
        });
    }

    void move_highlight(int direction, InputContext& context) {
        set_highlight(
            step_popup_index(
                session_->highlighted,
                session_->items.size(),
                direction,
                [&](std::size_t index) { return selectable(index); }),
            context);
    }

    void set_highlight(std::size_t index, InputContext& context) {
        if (session_->highlighted == index) return;
        const auto previous = session_->highlighted;
        std::optional<ResolvedMenuItemStyle> before_previous;
        std::optional<ResolvedMenuItemStyle> before_next;
        if (previous < session_->items.size() &&
            session_->items[previous].kind == PopupMenuItem::Kind::Action) {
            before_previous = resolved_item_style(previous);
        }
        if (index < session_->items.size() && index != previous &&
            session_->items[index].kind == PopupMenuItem::Kind::Action) {
            before_next = resolved_item_style(index);
        }

        session_->highlighted = index;

        bool presentation_changed = false;
        bool layout_changed = false;
        const auto classify = [&](const std::optional<ResolvedMenuItemStyle>& before, std::size_t item) {
            if (!before || item >= session_->items.size() ||
                session_->items[item].kind != PopupMenuItem::Kind::Action) {
                return;
            }
            const auto after = resolved_item_style(item);
            presentation_changed = presentation_changed || !(*before == after);
            layout_changed = layout_changed || !menu_item_layout_equal(*before, after);
        };
        classify(before_previous, previous);
        classify(before_next, index);
        if (layout_changed) context.invalidate_layout();
        if (presentation_changed) context.invalidate();
    }

    void set_pointer_armed(bool armed, InputContext& context) {
        if (pointer_armed_ == armed) return;
        std::optional<ResolvedMenuItemStyle> before;
        if (session_->highlighted < session_->items.size() &&
            session_->items[session_->highlighted].kind == PopupMenuItem::Kind::Action) {
            before = resolved_item_style(session_->highlighted);
        }
        pointer_armed_ = armed;
        if (!before || session_->highlighted >= session_->items.size() ||
            session_->items[session_->highlighted].kind != PopupMenuItem::Kind::Action) {
            return;
        }
        const auto after = resolved_item_style(session_->highlighted);
        invalidate_resolved_popup_style_transition(
            *before, after, context, menu_item_layout_equal);
    }

    [[nodiscard]] std::size_t index_at(Point point, Rect bounds) const {
        if (!bounds.contains(point)) return kNoPopupIndex;
        const auto base = base_item_style();
        float y = bounds.y;
        for (std::size_t i = 0; i < session_->items.size(); ++i) {
            float height = 0.0f;
            if (session_->items[i].kind == PopupMenuItem::Kind::Separator) {
                height = std::max(0.0f, base.separator_height);
            } else {
                height = std::max(0.0f, resolved_item_style(i).row_height);
            }
            if (point.y >= y && point.y < y + height) return i;
            y += height;
        }
        return kNoPopupIndex;
    }

    void update_pointer_highlight(Point point, InputContext& context) {
        const auto index = index_at(point, context.bounds());
        set_highlight(index != kNoPopupIndex && selectable(index) ? index : kNoPopupIndex, context);
    }

    void queue_action(Key trigger_key) {
        if (session_->completion_queued || pending_command_ ||
            session_->highlighted == kNoPopupIndex || !selectable(session_->highlighted)) {
            return;
        }

        auto callback = session_->items[session_->highlighted].callback;
        auto weak_anchor = std::weak_ptr<MenuAnchorRuntime>{session_->anchor};
        const auto anchor_id = session_->anchor->node_id;
        if (trigger_key != Key::None) {
            session_->anchor->suppress_until_key_up = trigger_key;
        }
        session_->completion_queued = true;
        pending_command_ = OverlayComponentCommand::close_then_invoke(
            session_->handle,
            anchor_id,
            false,
            [weak_anchor, callback = std::move(callback)]() mutable {
                auto anchor = weak_anchor.lock();
                if (!anchor || !anchor->mounted) return;
                if (callback) callback();
            });
    }

    std::shared_ptr<MenuPopupSession> session_;
    std::optional<OverlayComponentCommand> pending_command_;
    bool pointer_armed_{};
};

template <class T>
class ComboBoxComponent final : public Component,
                                public ThemeBinding,
                                public OverlayCommandSource,
                                public OverlayAnchorPolicy {
public:
    using OptionsProvider = std::function<std::vector<ComboBoxOption<T>>() >;

    ComboBoxComponent(
        Binding<T> selection,
        OptionsProvider options_provider,
        std::vector<ComboBoxOption<T>> display_options,
        std::string placeholder,
        ComboBoxStyle style,
        MenuItemStyle item_style,
        std::shared_ptr<ComboAnchorRuntime<T>> runtime)
        : selection_(std::move(selection)),
          options_provider_(std::move(options_provider)),
          display_options_(std::move(display_options)),
          placeholder_(std::move(placeholder)),
          style_(std::move(style)),
          item_style_(std::move(item_style)),
          runtime_(std::move(runtime)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool dismiss_overlay_on_tab() const noexcept override { return true; }
    [[nodiscard]] bool dismiss_overlay_when_read_only() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto resolved = resolved_style(focused_);
        const auto text = TextService::measure(
            display_text(), combo_anchor_text_style(resolved));
        return {
            std::max(resolved.minimum_width,
                     text.width + resolved.horizontal_padding * 2.0f),
            resolved.control_height};
    }

    void mount(MountContext& context) override {
        runtime_->node_id = context.node_id();
        runtime_->mounted = true;
        runtime_->selection = selection_;
        auto invalidate = context.invalidator();
        auto invalidate_layout = context.layout_invalidator();
        subscription_ = selection_.observe(
            [invalidate = std::move(invalidate),
             invalidate_layout = std::move(invalidate_layout)](const T&) {
                invalidate_layout();
                invalidate();
            });
    }

    void unmount(LifecycleContext&) override {
        subscription_.reset();
        runtime_->selection.reset();
        runtime_->mounted = false;
        runtime_->node_id = kInvalidNodeId;
        runtime_->handle = {};
        runtime_->suppress_until_key_up = Key::None;
        pending_command_.reset();
    }

    void focus_changed(bool focused, FocusContext& context) override {
        const auto before = resolved_style(focused_);
        focused_ = focused;
        if (!focused && !runtime_->handle.valid()) {
            runtime_->suppress_until_key_up = Key::None;
        }
        interaction_.focus_changed(focused, context, false);
        const auto after = resolved_style(focused_);
        invalidate_style_transition(before, after, context);
    }

    void deactivate(LifecycleContext& context) override {
        const auto before = resolved_style(focused_);
        focused_ = false;
        interaction_.deactivate(context, false);
        runtime_->suppress_until_key_up = Key::None;
        const auto after = resolved_style(focused_);
        invalidate_style_transition(before, after, context);
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        if (runtime_->suppress_until_key_up != Key::None) {
            if (event.type == InputType::KeyUp && event.key == runtime_->suppress_until_key_up) {
                runtime_->suppress_until_key_up = Key::None;
                return EventResult::Handled;
            }
            if (event.type == InputType::KeyDown && event.key == runtime_->suppress_until_key_up) {
                return EventResult::Handled;
            }
        }

        const auto before = resolved_style(focused_);
        if (effective_read_only()) {
            interaction_.cancel_pending_mutation(context, false);
            const auto after = resolved_style(focused_);
            invalidate_style_transition(before, after, context);
            if (event.type == InputType::PointerDown || event.type == InputType::PointerUp ||
                ((event.type == InputType::KeyDown || event.type == InputType::KeyUp) &&
                 (event.key == Key::Space || event.key == Key::Enter || event.key == Key::Down))) {
                return EventResult::Handled;
            }
            return EventResult::Ignored;
        }

        if (event.type == InputType::KeyDown && event.key == Key::Down) {
            return open_popup(context, Key::Down);
        }

        const auto outcome = interaction_.input(event, context, true, false);
        const auto after = resolved_style(focused_);
        invalidate_style_transition(before, after, context);
        if (!outcome.activate) return outcome.result;

        Key opening_key = Key::None;
        if (event.type == InputType::KeyDown && event.key == Key::Enter) {
            opening_key = Key::Enter;
        }
        return open_popup(context, opening_key);
    }

    [[nodiscard]] std::optional<OverlayComponentCommand>
    take_overlay_command() override {
        auto command = std::move(pending_command_);
        pending_command_.reset();
        return command;
    }

    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        const auto resolved = resolved_style(context.focused());
        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds, resolved.corner_radius, resolved.fill);
        painter.stroke_rounded_rect(
            bounds, resolved.corner_radius, resolved.border_width, resolved.border);
        painter.text(
            {bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
            display_text(),
            combo_anchor_text_style(resolved));
    }

private:
    [[nodiscard]] VisualState current_visual_state(bool focused) const noexcept {
        return VisualState{
            .enabled = effective_enabled(),
            .read_only = effective_read_only(),
            .hovered = interaction_.hovered(),
            .pressed = interaction_.pressed(),
            .focused = focused,
        };
    }

    [[nodiscard]] ResolvedComboBoxStyle resolved_style(bool focused) const {
        return resolve_combo_box_style(
            default_combo_box_style(current_theme()), style_, current_visual_state(focused));
    }

    template <class Context>
    static void invalidate_style_transition(const ResolvedComboBoxStyle& before,
                                            const ResolvedComboBoxStyle& after,
                                            Context& context) {
        invalidate_resolved_popup_style_transition(
            before, after, context, combo_box_layout_equal);
    }

    [[nodiscard]] std::string display_text() const {
        const auto& selected = selection_.get();
        const auto found = std::find_if(
            display_options_.begin(), display_options_.end(),
            [&](const ComboBoxOption<T>& option) { return option.value == selected; });
        return found == display_options_.end() ? placeholder_ : found->label;
    }

    [[nodiscard]] std::size_t initial_highlight(
        const std::vector<ComboBoxOption<T>>& options) const {
        const auto& selected = selection_.get();
        for (std::size_t i = 0; i < options.size(); ++i) {
            if (options[i].enabled && options[i].value == selected) return i;
        }
        return first_popup_index(options.size(), [&](std::size_t index) {
            return options[index].enabled;
        });
    }

    EventResult open_popup(InputContext& context, Key opening_key) {
        if (runtime_->handle.valid() || pending_command_) return EventResult::Handled;

        auto snapshot = options_provider_ ? options_provider_() : display_options_;
        display_options_ = snapshot;
        context.invalidate_layout();
        context.invalidate();

        auto session = std::make_shared<ComboPopupSession<T>>();
        session->anchor = runtime_;
        session->highlighted = initial_highlight(snapshot);
        session->options = std::move(snapshot);
        session->item_style = item_style_;

        OverlaySpec overlay;
        overlay.mode = OverlayMode::Modal;
        overlay.anchor = runtime_->node_id;
        overlay.placement = OverlayPlacement::AnchorBelow;
        overlay.dismiss_on_escape = true;
        overlay.dismiss_on_outside_pointer_down = true;
        overlay.content = Spec{
            [session] { return std::make_unique<ComboPopupComponent<T>>(session); },
            {}};

        if (opening_key != Key::None) runtime_->suppress_until_key_up = opening_key;
        pending_command_ = OverlayComponentCommand::show(
            std::move(overlay),
            [session, runtime = runtime_](OverlayHandle handle) {
                session->handle = handle;
                runtime->handle = handle;
                if (!handle.valid()) runtime->suppress_until_key_up = Key::None;
            });
        return EventResult::Handled;
    }

    Binding<T> selection_;
    OptionsProvider options_provider_;
    std::vector<ComboBoxOption<T>> display_options_;
    std::string placeholder_;
    ComboBoxStyle style_;
    MenuItemStyle item_style_;
    std::shared_ptr<ComboAnchorRuntime<T>> runtime_;
    typename Binding<T>::Subscription subscription_;
    PressActivationState interaction_;
    std::optional<OverlayComponentCommand> pending_command_;
    bool focused_{};
};

class PopupMenuComponent final : public Component,
                                 public ThemeBinding,
                                 public OverlayCommandSource,
                                 public OverlayAnchorPolicy {
public:
    using ItemsProvider = std::function<std::vector<PopupMenuItem>()>;

    PopupMenuComponent(
        std::string label,
        ItemsProvider items_provider,
        ComboBoxStyle style,
        MenuItemStyle item_style,
        std::shared_ptr<MenuAnchorRuntime> runtime)
        : label_(std::move(label)),
          items_provider_(std::move(items_provider)),
          style_(std::move(style)),
          item_style_(std::move(item_style)),
          runtime_(std::move(runtime)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool dismiss_overlay_on_tab() const noexcept override { return true; }
    [[nodiscard]] bool dismiss_overlay_when_read_only() const noexcept override { return false; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto resolved = resolved_style(focused_);
        const auto text = TextService::measure(label_, combo_anchor_text_style(resolved));
        return {
            std::max(resolved.minimum_width,
                     text.width + resolved.horizontal_padding * 2.0f),
            resolved.control_height};
    }

    void mount(MountContext& context) override {
        runtime_->node_id = context.node_id();
        runtime_->mounted = true;
    }

    void unmount(LifecycleContext&) override {
        runtime_->mounted = false;
        runtime_->node_id = kInvalidNodeId;
        runtime_->handle = {};
        runtime_->suppress_until_key_up = Key::None;
        pending_command_.reset();
    }

    void focus_changed(bool focused, FocusContext& context) override {
        const auto before = resolved_style(focused_);
        focused_ = focused;
        if (!focused && !runtime_->handle.valid()) {
            runtime_->suppress_until_key_up = Key::None;
        }
        interaction_.focus_changed(focused, context, false);
        const auto after = resolved_style(focused_);
        invalidate_style_transition(before, after, context);
    }

    void deactivate(LifecycleContext& context) override {
        const auto before = resolved_style(focused_);
        focused_ = false;
        interaction_.deactivate(context, false);
        runtime_->suppress_until_key_up = Key::None;
        const auto after = resolved_style(focused_);
        invalidate_style_transition(before, after, context);
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        if (runtime_->suppress_until_key_up != Key::None) {
            if (event.type == InputType::KeyUp && event.key == runtime_->suppress_until_key_up) {
                runtime_->suppress_until_key_up = Key::None;
                return EventResult::Handled;
            }
            if (event.type == InputType::KeyDown && event.key == runtime_->suppress_until_key_up) {
                return EventResult::Handled;
            }
        }

        const auto before = resolved_style(focused_);
        if (event.type == InputType::KeyDown && event.key == Key::Down) {
            return open_popup(context, Key::Down);
        }

        const auto outcome = interaction_.input(event, context, true, false);
        const auto after = resolved_style(focused_);
        invalidate_style_transition(before, after, context);
        if (!outcome.activate) return outcome.result;

        Key opening_key = Key::None;
        if (event.type == InputType::KeyDown && event.key == Key::Enter) {
            opening_key = Key::Enter;
        }
        return open_popup(context, opening_key);
    }

    [[nodiscard]] std::optional<OverlayComponentCommand>
    take_overlay_command() override {
        auto command = std::move(pending_command_);
        pending_command_.reset();
        return command;
    }

    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        const auto resolved = resolved_style(context.focused());
        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds, resolved.corner_radius, resolved.fill);
        painter.stroke_rounded_rect(
            bounds, resolved.corner_radius, resolved.border_width, resolved.border);
        painter.text(
            {bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
            label_,
            combo_anchor_text_style(resolved));
    }

private:
    [[nodiscard]] VisualState current_visual_state(bool focused) const noexcept {
        return VisualState{
            .enabled = effective_enabled(),
            .read_only = effective_read_only(),
            .hovered = interaction_.hovered(),
            .pressed = interaction_.pressed(),
            .focused = focused,
        };
    }

    [[nodiscard]] ResolvedComboBoxStyle resolved_style(bool focused) const {
        return resolve_combo_box_style(
            default_combo_box_style(current_theme()), style_, current_visual_state(focused));
    }

    template <class Context>
    static void invalidate_style_transition(const ResolvedComboBoxStyle& before,
                                            const ResolvedComboBoxStyle& after,
                                            Context& context) {
        invalidate_resolved_popup_style_transition(
            before, after, context, combo_box_layout_equal);
    }

    [[nodiscard]] static std::size_t initial_highlight(
        const std::vector<PopupMenuItem>& items) {
        return first_popup_index(items.size(), [&](std::size_t index) {
            return items[index].actionable();
        });
    }

    EventResult open_popup(InputContext&, Key opening_key) {
        if (runtime_->handle.valid() || pending_command_) return EventResult::Handled;

        auto snapshot = items_provider_ ? items_provider_() : std::vector<PopupMenuItem>{};
        auto session = std::make_shared<MenuPopupSession>();
        session->anchor = runtime_;
        session->highlighted = initial_highlight(snapshot);
        session->items = std::move(snapshot);
        session->item_style = item_style_;

        OverlaySpec overlay;
        overlay.mode = OverlayMode::Modal;
        overlay.anchor = runtime_->node_id;
        overlay.placement = OverlayPlacement::AnchorBelow;
        overlay.dismiss_on_escape = true;
        overlay.dismiss_on_outside_pointer_down = true;
        overlay.content = Spec{
            [session] { return std::make_unique<MenuPopupComponent>(session); },
            {}};

        if (opening_key != Key::None) runtime_->suppress_until_key_up = opening_key;
        pending_command_ = OverlayComponentCommand::show(
            std::move(overlay),
            [session, runtime = runtime_](OverlayHandle handle) {
                session->handle = handle;
                runtime->handle = handle;
                if (!handle.valid()) runtime->suppress_until_key_up = Key::None;
            });
        return EventResult::Handled;
    }

    std::string label_;
    ItemsProvider items_provider_;
    ComboBoxStyle style_;
    MenuItemStyle item_style_;
    std::shared_ptr<MenuAnchorRuntime> runtime_;
    PressActivationState interaction_;
    std::optional<OverlayComponentCommand> pending_command_;
    bool focused_{};
};

} // namespace detail

/// Focusable retained ComboBox backed by one selection Binding.
///
/// `T` must be copy-constructible and equality-comparable. The builder owns its
/// option/provider configuration; each open popup owns a fresh option snapshot.
/// Provider-backed instances call the provider synchronously once during builder
/// construction to seed anchor display state and again whenever the popup opens.
/// A provider may allocate/call application code; exceptions are not translated
/// into fallback options.
///
/// Selection is read and written through the normal State/Binding UI-thread and
/// lifetime contract. A successful row activation first closes/detaches the
/// overlay, then writes the selected value at the retained safe commit point.
/// Selection observers can therefore re-enter ordinary UI/state work without
/// observing a half-detached popup subtree.
///
/// Construction, provider evaluation, text measurement, opening and `spec()`
/// may allocate and are UI-domain operations, not audio/DSP real-time work.
template <class T>
    requires std::copy_constructible<T> && std::equality_comparable<T>
class ComboBox {
public:
    /// Produces a fresh owned option snapshot synchronously in the UI domain.
    ///
    /// The returned vector is moved into NativeUI-owned display/session storage;
    /// no references into provider-local storage are retained. An empty provider
    /// result is valid and yields a popup with no selectable rows. Provider
    /// exceptions propagate to the invoking construction/input path.
    using OptionsProvider = std::function<std::vector<ComboBoxOption<T>>() >;

    /// Construct from a stable owned option set.
    ///
    /// `selection` and `options` are moved into the builder. The stable vector
    /// is retained as the anchor-display snapshot and copied for each popup open,
    /// so later caller mutations of the original vector have no effect.
    ComboBox(Binding<T> selection, std::vector<ComboBoxOption<T>> options)
        : selection_(std::move(selection)),
          initial_options_(options),
          options_provider_([options = std::move(options)] { return options; }) {}

    /// Convenience overload using `selection.binding()`; State/Binding lifetime
    /// and UI-thread rules are unchanged.
    ComboBox(State<T>& selection, std::vector<ComboBoxOption<T>> options)
        : ComboBox(selection.binding(), std::move(options)) {}

    /// Construct from a dynamic provider refreshed on each popup open.
    ///
    /// A non-empty provider is invoked immediately once to seed the anchor
    /// display snapshot, then synchronously again on every open. The latest
    /// returned snapshot replaces the anchor's display snapshot. An empty
    /// `std::function` is valid and behaves as an empty option set.
    ComboBox(Binding<T> selection, OptionsProvider options_provider)
        : selection_(std::move(selection)), options_provider_(std::move(options_provider)) {
        if (options_provider_) initial_options_ = options_provider_();
    }

    /// Convenience provider overload using `selection.binding()`.
    ComboBox(State<T>& selection, OptionsProvider options_provider)
        : ComboBox(selection.binding(), std::move(options_provider)) {}

    /// Set owned UTF-8 text shown when the selected value is absent from the
    /// current display snapshot. The default is "No selection".
    ComboBox&& placeholder(std::string value) && {
        placeholder_ = std::move(value);
        return std::move(*this);
    }

    /// Override the anchor's typed style recipe.
    ///
    /// The style value is owned by the builder/component; length-like fields use
    /// the logical UI units defined by the style contract.
    ComboBox&& style(ComboBoxStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    /// Override popup-row styling for this ComboBox.
    ///
    /// The owned recipe is applied to every row in each popup snapshot.
    ComboBox&& item_style(MenuItemStyle value) && {
        item_style_ = std::move(value);
        return std::move(*this);
    }

    /// Consume this builder into a retained Spec.
    ///
    /// The resulting factory owns the Binding/provider/options/style/runtime
    /// state needed by the mounted component. This operation may allocate
    /// shared runtime state and is not real-time safe.
    Spec spec() && {
        auto selection = std::move(selection_);
        auto provider = std::move(options_provider_);
        auto initial_options = std::move(initial_options_);
        auto placeholder = std::move(placeholder_);
        auto style = std::move(style_);
        auto item_style = std::move(item_style_);
        auto runtime = std::make_shared<detail::ComboAnchorRuntime<T>>();
        return Spec{
            [selection = std::move(selection),
             provider = std::move(provider),
             initial_options = std::move(initial_options),
             placeholder = std::move(placeholder),
             style = std::move(style),
             item_style = std::move(item_style),
             runtime = std::move(runtime)]() mutable {
                return std::make_unique<detail::ComboBoxComponent<T>>(
                    std::move(selection),
                    std::move(provider),
                    std::move(initial_options),
                    std::move(placeholder),
                    std::move(style),
                    std::move(item_style),
                    std::move(runtime));
            },
            {}};
    }

private:
    Binding<T> selection_;
    std::vector<ComboBoxOption<T>> initial_options_;
    OptionsProvider options_provider_;
    std::string placeholder_{"No selection"};
    ComboBoxStyle style_;
    MenuItemStyle item_style_;
};

/// Focusable retained action menu presented through NativeUI's in-view overlay
/// stack, not a separate native popup window.
///
/// Item providers are evaluated synchronously on each open and their returned
/// vectors become NativeUI-owned session snapshots. Provider work may allocate,
/// call application code, or throw; there is no asynchronous/provider retry
/// mechanism and exceptions are not converted into empty/fallback rows.
///
/// Action callbacks run in the UI domain only after popup close/detach reaches
/// its retained safe commit point. This makes ordinary state/UI reentrancy from
/// an action callback safe with respect to popup teardown. Providers, callbacks,
/// construction, opening and `spec()` are not audio/DSP real-time operations.
class PopupMenu {
public:
    /// Produces a fresh owned item snapshot synchronously in the UI domain.
    ///
    /// Returned strings/callbacks are owned by the vector/session. An empty
    /// provider result is valid. Exceptions propagate to the invoking input path.
    using ItemsProvider = std::function<std::vector<PopupMenuItem>()>;

    /// Construct with an owned UTF-8 anchor label and stable item set.
    ///
    /// Both arguments are moved into the builder. The captured stable vector is
    /// copied into a fresh popup-session snapshot on each open.
    PopupMenu(std::string label, std::vector<PopupMenuItem> items)
        : label_(std::move(label)),
          items_provider_([items = std::move(items)] { return items; }) {}

    /// Construct with an owned UTF-8 anchor label and dynamic item provider.
    ///
    /// Unlike ComboBox, the provider is not called during builder construction;
    /// it is invoked only when the menu opens.
    PopupMenu(std::string label, ItemsProvider items_provider)
        : label_(std::move(label)), items_provider_(std::move(items_provider)) {}

    /// Override the menu anchor's owned ComboBox-style recipe.
    PopupMenu&& style(ComboBoxStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    /// Override the owned style recipe applied to action/separator rows.
    PopupMenu&& item_style(MenuItemStyle value) && {
        item_style_ = std::move(value);
        return std::move(*this);
    }

    /// Consume this builder into a retained Spec.
    ///
    /// The Spec owns the anchor label, provider, styles and shared runtime state.
    /// Building it may allocate and is not real-time safe.
    Spec spec() && {
        auto label = std::move(label_);
        auto provider = std::move(items_provider_);
        auto style = std::move(style_);
        auto item_style = std::move(item_style_);
        auto runtime = std::make_shared<detail::MenuAnchorRuntime>();
        return Spec{
            [label = std::move(label),
             provider = std::move(provider),
             style = std::move(style),
             item_style = std::move(item_style),
             runtime = std::move(runtime)]() mutable {
                return std::make_unique<detail::PopupMenuComponent>(
                    std::move(label),
                    std::move(provider),
                    std::move(style),
                    std::move(item_style),
                    std::move(runtime));
            },
            {}};
    }

private:
    std::string label_;
    ItemsProvider items_provider_;
    ComboBoxStyle style_;
    MenuItemStyle item_style_;
};

} // namespace ui
