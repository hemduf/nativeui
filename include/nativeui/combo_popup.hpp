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

template <class T>
struct ComboBoxOption final {
    T value;
    std::string label;
    bool enabled{true};
};

struct PopupMenuItem final {
    enum class Kind {
        Action,
        Separator,
    };

    Kind kind{Kind::Action};
    std::string label;
    bool enabled{true};
    std::function<void()> callback;

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

    [[nodiscard]] static PopupMenuItem separator() {
        PopupMenuItem item;
        item.kind = Kind::Separator;
        item.enabled = false;
        return item;
    }

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

template <class T>
struct ComboAnchorRuntime final {
    State<T>* selection{};
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
        const auto text_style = menu_item_text_style(base);
        for (const auto& option : session_->options) {
            width = std::max(
                width,
                TextService::measure(option.label, text_style).width +
                    base.horizontal_padding * 2.0f);
        }
        const auto rows = std::max<std::size_t>(session_->options.size(), 1);
        return {width, base.row_height * static_cast<float>(rows)};
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
                pointer_armed_ = true;
                context.capture_pointer();
                update_pointer_highlight(event.position, context);
                return EventResult::Handled;
            case InputType::PointerMove:
                update_pointer_highlight(event.position, context);
                return EventResult::Handled;
            case InputType::PointerUp: {
                if (!pointer_armed_) return EventResult::Handled;
                pointer_armed_ = false;
                context.release_pointer();
                const auto base = base_item_style();
                const auto index = index_at(event.position, context.bounds(), base.row_height);
                if (index != kNoPopupIndex && session_->options[index].enabled) {
                    set_highlight(index, context);
                    queue_commit(Key::None);
                }
                return EventResult::Handled;
            }
            case InputType::PointerCancel:
                if (pointer_armed_) {
                    pointer_armed_ = false;
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

        const float row_height = base.row_height;
        for (std::size_t i = 0; i < session_->options.size(); ++i) {
            const auto resolved = resolved_item_style(i);
            const Rect row{
                bounds.x,
                bounds.y + static_cast<float>(i) * row_height,
                bounds.w,
                row_height};
            painter.fill_rounded_rect(row, resolved.corner_radius, resolved.fill);
            painter.text(
                {row.x + base.horizontal_padding, row.y + row.h * 0.5f},
                session_->options[i].label,
                menu_item_text_style(resolved));
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
        session_->highlighted = index;
        context.invalidate();
    }

    [[nodiscard]] std::size_t index_at(
        Point point, Rect bounds, float row_height) const noexcept {
        if (!bounds.contains(point) || session_->options.empty() || row_height <= 0.0f) {
            return kNoPopupIndex;
        }
        const auto index = static_cast<std::size_t>((point.y - bounds.y) / row_height);
        return index < session_->options.size() ? index : kNoPopupIndex;
    }

    void update_pointer_highlight(Point point, InputContext& context) {
        const auto base = base_item_style();
        const auto index = index_at(point, context.bounds(), base.row_height);
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
        const auto text_style = menu_item_text_style(base);
        for (const auto& item : session_->items) {
            if (item.kind == PopupMenuItem::Kind::Separator) {
                height += base.separator_height;
                continue;
            }
            width = std::max(
                width,
                TextService::measure(item.label, text_style).width +
                    base.horizontal_padding * 2.0f);
            height += base.row_height;
        }
        return {width, std::max(height, base.row_height)};
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
                const auto base = base_item_style();
                const auto index = index_at(
                    event.position, context.bounds(), base.row_height, base.separator_height);
                pointer_armed_ = index != kNoPopupIndex && selectable(index);
                if (pointer_armed_) context.capture_pointer();
                update_pointer_highlight(event.position, context);
                return EventResult::Handled;
            }
            case InputType::PointerMove:
                update_pointer_highlight(event.position, context);
                return EventResult::Handled;
            case InputType::PointerUp: {
                const bool armed = pointer_armed_;
                pointer_armed_ = false;
                if (armed) context.release_pointer();
                const auto base = base_item_style();
                const auto index = index_at(
                    event.position, context.bounds(), base.row_height, base.separator_height);
                if (armed && index != kNoPopupIndex && selectable(index)) {
                    set_highlight(index, context);
                    queue_action(Key::None);
                }
                return EventResult::Handled;
            }
            case InputType::PointerCancel:
                if (pointer_armed_) {
                    pointer_armed_ = false;
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
                painter.line(
                    {bounds.x + base.separator_inset, y + base.separator_height * 0.5f},
                    {bounds.x + bounds.w - base.separator_inset,
                     y + base.separator_height * 0.5f},
                    base.separator_width,
                    base.separator);
                y += base.separator_height;
                continue;
            }

            const auto resolved = resolved_item_style(i);
            const Rect row{bounds.x, y, bounds.w, base.row_height};
            painter.fill_rounded_rect(row, resolved.corner_radius, resolved.fill);
            painter.text(
                {row.x + base.horizontal_padding, row.y + row.h * 0.5f},
                item.label,
                menu_item_text_style(resolved));
            y += row.h;
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
        session_->highlighted = index;
        context.invalidate();
    }

    [[nodiscard]] std::size_t index_at(
        Point point,
        Rect bounds,
        float row_height,
        float separator_height) const noexcept {
        if (!bounds.contains(point)) return kNoPopupIndex;
        float y = bounds.y;
        for (std::size_t i = 0; i < session_->items.size(); ++i) {
            const float height = session_->items[i].kind == PopupMenuItem::Kind::Separator
                ? separator_height
                : row_height;
            if (point.y >= y && point.y < y + height) return i;
            y += height;
        }
        return kNoPopupIndex;
    }

    void update_pointer_highlight(Point point, InputContext& context) {
        const auto base = base_item_style();
        const auto index = index_at(
            point, context.bounds(), base.row_height, base.separator_height);
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
        State<T>& selection,
        OptionsProvider options_provider,
        std::vector<ComboBoxOption<T>> display_options,
        std::string placeholder,
        ComboBoxStyle style,
        MenuItemStyle item_style,
        std::shared_ptr<ComboAnchorRuntime<T>> runtime)
        : selection_(&selection),
          options_provider_(std::move(options_provider)),
          display_options_(std::move(display_options)),
          placeholder_(std::move(placeholder)),
          style_(std::move(style)),
          item_style_(std::move(item_style)),
          runtime_(std::move(runtime)) {
        runtime_->selection = selection_;
    }

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
        subscription_ = selection_->observe(
            [invalidate = context.layout_invalidator()](const T&) { invalidate(); });
    }

    void unmount(LifecycleContext&) override {
        subscription_.reset();
        runtime_->mounted = false;
        runtime_->node_id = kInvalidNodeId;
        runtime_->handle = {};
        runtime_->suppress_until_key_up = Key::None;
        pending_command_.reset();
    }

    void focus_changed(bool focused, FocusContext& context) override {
        focused_ = focused;
        if (!focused && !runtime_->handle.valid()) {
            runtime_->suppress_until_key_up = Key::None;
        }
        interaction_.focus_changed(focused, context);
    }

    void deactivate(LifecycleContext& context) override {
        focused_ = false;
        interaction_.deactivate(context);
        runtime_->suppress_until_key_up = Key::None;
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

        if (effective_read_only()) {
            interaction_.cancel_pending_mutation(context);
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

        const auto outcome = interaction_.input(event, context, true);
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

    [[nodiscard]] std::string display_text() const {
        const auto& selected = selection_->get();
        const auto found = std::find_if(
            display_options_.begin(), display_options_.end(),
            [&](const ComboBoxOption<T>& option) { return option.value == selected; });
        return found == display_options_.end() ? placeholder_ : found->label;
    }

    [[nodiscard]] std::size_t initial_highlight(
        const std::vector<ComboBoxOption<T>>& options) const {
        const auto& selected = selection_->get();
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

    State<T>* selection_{};
    OptionsProvider options_provider_;
    std::vector<ComboBoxOption<T>> display_options_;
    std::string placeholder_;
    ComboBoxStyle style_;
    MenuItemStyle item_style_;
    std::shared_ptr<ComboAnchorRuntime<T>> runtime_;
    typename State<T>::Subscription subscription_;
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
        focused_ = focused;
        if (!focused && !runtime_->handle.valid()) {
            runtime_->suppress_until_key_up = Key::None;
        }
        interaction_.focus_changed(focused, context);
    }

    void deactivate(LifecycleContext& context) override {
        focused_ = false;
        interaction_.deactivate(context);
        runtime_->suppress_until_key_up = Key::None;
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

        if (event.type == InputType::KeyDown && event.key == Key::Down) {
            return open_popup(context, Key::Down);
        }

        const auto outcome = interaction_.input(event, context, true);
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

template <class T>
    requires std::copy_constructible<T> && std::equality_comparable<T>
class ComboBox {
public:
    using OptionsProvider = std::function<std::vector<ComboBoxOption<T>>() >;

    ComboBox(State<T>& selection, std::vector<ComboBoxOption<T>> options)
        : selection_(&selection),
          initial_options_(options),
          options_provider_([options = std::move(options)] { return options; }) {}

    ComboBox(State<T>& selection, OptionsProvider options_provider)
        : selection_(&selection), options_provider_(std::move(options_provider)) {
        if (options_provider_) initial_options_ = options_provider_();
    }

    ComboBox&& placeholder(std::string value) && {
        placeholder_ = std::move(value);
        return std::move(*this);
    }

    ComboBox&& style(ComboBoxStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    ComboBox&& item_style(MenuItemStyle value) && {
        item_style_ = std::move(value);
        return std::move(*this);
    }

    Spec spec() && {
        auto* selection = selection_;
        auto provider = std::move(options_provider_);
        auto initial_options = std::move(initial_options_);
        auto placeholder = std::move(placeholder_);
        auto style = std::move(style_);
        auto item_style = std::move(item_style_);
        auto runtime = std::make_shared<detail::ComboAnchorRuntime<T>>();
        return Spec{
            [selection,
             provider = std::move(provider),
             initial_options = std::move(initial_options),
             placeholder = std::move(placeholder),
             style = std::move(style),
             item_style = std::move(item_style),
             runtime = std::move(runtime)]() mutable {
                return std::make_unique<detail::ComboBoxComponent<T>>(
                    *selection,
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
    State<T>* selection_{};
    std::vector<ComboBoxOption<T>> initial_options_;
    OptionsProvider options_provider_;
    std::string placeholder_{"No selection"};
    ComboBoxStyle style_;
    MenuItemStyle item_style_;
};

class PopupMenu {
public:
    using ItemsProvider = std::function<std::vector<PopupMenuItem>()>;

    PopupMenu(std::string label, std::vector<PopupMenuItem> items)
        : label_(std::move(label)),
          items_provider_([items = std::move(items)] { return items; }) {}

    PopupMenu(std::string label, ItemsProvider items_provider)
        : label_(std::move(label)), items_provider_(std::move(items_provider)) {}

    PopupMenu&& style(ComboBoxStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    PopupMenu&& item_style(MenuItemStyle value) && {
        item_style_ = std::move(value);
        return std::move(*this);
    }

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