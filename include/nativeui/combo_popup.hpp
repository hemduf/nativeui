#pragma once

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

[[nodiscard]] inline TextStyle popup_text_style(
    const Theme& theme, Color color, TextAlign align = TextAlign::Left) {
    TextStyle style{};
    style.size = theme.typography.control_size;
    style.color = color;
    style.align = align;
    style.weight = theme.typography.control_weight;
    style.slant = theme.typography.slant;
    style.family = theme.typography.family;
    style.fallback_families = theme.typography.fallback_families;
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
        const auto& theme = current_theme();
        float width = theme.controls.minimum_width;
        const auto style = popup_text_style(theme, theme.palette.text);
        for (const auto& option : session_->options) {
            width = std::max(
                width,
                TextService::measure(option.label, style).width + theme.spacing.large * 2.0f);
        }
        const auto rows = std::max<std::size_t>(session_->options.size(), 1);
        return {width, theme.controls.control_height * static_cast<float>(rows)};
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
                const auto index = index_at(event.position, context.bounds());
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
        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds, theme.radii.medium, theme.palette.surface);
        painter.stroke_rounded_rect(
            bounds, theme.radii.medium, theme.controls.border_width, theme.palette.border);

        const float row_height = theme.controls.control_height;
        for (std::size_t i = 0; i < session_->options.size(); ++i) {
            const Rect row{
                bounds.x,
                bounds.y + static_cast<float>(i) * row_height,
                bounds.w,
                row_height};
            if (i == session_->highlighted && session_->options[i].enabled) {
                painter.fill_rounded_rect(row, 0.0f, theme.palette.selection);
            }
            const auto color = session_->options[i].enabled
                ? theme.palette.text
                : theme.palette.disabled;
            painter.text(
                {row.x + theme.spacing.medium, row.y + row.h * 0.5f},
                session_->options[i].label,
                popup_text_style(theme, color));
        }
    }

private:
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

    [[nodiscard]] std::size_t index_at(Point point, Rect bounds) const noexcept {
        if (!bounds.contains(point) || session_->options.empty()) return kNoPopupIndex;
        const float row_height = current_theme().controls.control_height;
        if (row_height <= 0.0f) return kNoPopupIndex;
        const auto index = static_cast<std::size_t>((point.y - bounds.y) / row_height);
        return index < session_->options.size() ? index : kNoPopupIndex;
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
        const auto& theme = current_theme();
        float width = theme.controls.minimum_width;
        float height = 0.0f;
        const auto style = popup_text_style(theme, theme.palette.text);
        for (const auto& item : session_->items) {
            if (item.kind == PopupMenuItem::Kind::Separator) {
                height += theme.spacing.sm;
                continue;
            }
            width = std::max(
                width,
                TextService::measure(item.label, style).width + theme.spacing.large * 2.0f);
            height += theme.controls.control_height;
        }
        return {width, std::max(height, theme.controls.control_height)};
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
                const auto index = index_at(event.position, context.bounds());
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
        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds, theme.radii.medium, theme.palette.surface);
        painter.stroke_rounded_rect(
            bounds, theme.radii.medium, theme.controls.border_width, theme.palette.border);

        float y = bounds.y;
        for (std::size_t i = 0; i < session_->items.size(); ++i) {
            const auto& item = session_->items[i];
            if (item.kind == PopupMenuItem::Kind::Separator) {
                const float height = theme.spacing.sm;
                painter.line(
                    {bounds.x + theme.spacing.sm, y + height * 0.5f},
                    {bounds.x + bounds.w - theme.spacing.sm, y + height * 0.5f},
                    theme.controls.border_width,
                    theme.palette.border);
                y += height;
                continue;
            }

            const Rect row{bounds.x, y, bounds.w, theme.controls.control_height};
            if (i == session_->highlighted && item.actionable()) {
                painter.fill_rounded_rect(row, 0.0f, theme.palette.selection);
            }
            painter.text(
                {row.x + theme.spacing.medium, row.y + row.h * 0.5f},
                item.label,
                popup_text_style(
                    theme, item.actionable() ? theme.palette.text : theme.palette.disabled));
            y += row.h;
        }
    }

private:
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

    [[nodiscard]] std::size_t index_at(Point point, Rect bounds) const noexcept {
        if (!bounds.contains(point)) return kNoPopupIndex;
        const auto& theme = current_theme();
        float y = bounds.y;
        for (std::size_t i = 0; i < session_->items.size(); ++i) {
            const float height = session_->items[i].kind == PopupMenuItem::Kind::Separator
                ? theme.spacing.sm
                : theme.controls.control_height;
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
        State<T>& selection,
        OptionsProvider options_provider,
        std::vector<ComboBoxOption<T>> display_options,
        std::string placeholder,
        std::shared_ptr<ComboAnchorRuntime<T>> runtime)
        : selection_(&selection),
          options_provider_(std::move(options_provider)),
          display_options_(std::move(display_options)),
          placeholder_(std::move(placeholder)),
          runtime_(std::move(runtime)) {
        runtime_->selection = selection_;
    }

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool dismiss_overlay_on_tab() const noexcept override { return true; }
    [[nodiscard]] bool dismiss_overlay_when_read_only() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto& theme = current_theme();
        const auto style = popup_text_style(theme, theme.palette.text, TextAlign::Center);
        const auto text = TextService::measure(display_text(), style);
        return {
            std::max(theme.controls.minimum_width, text.width + theme.spacing.large * 2.0f),
            theme.controls.control_height};
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
        if (!focused && !runtime_->handle.valid()) {
            runtime_->suppress_until_key_up = Key::None;
        }
        interaction_.focus_changed(focused, context);
    }

    void deactivate(LifecycleContext& context) override {
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
        const auto& theme = current_theme();
        auto& painter = context.painter();

        const Color fill = effective_enabled()
            ? theme.palette.surface
            : theme.palette.control_background;
        const Color text = effective_enabled()
            ? theme.palette.text
            : theme.palette.disabled;
        painter.fill_rounded_rect(bounds, theme.radii.medium, fill);
        painter.stroke_rounded_rect(
            bounds,
            theme.radii.medium,
            context.focused() ? theme.controls.focus_ring_width : theme.controls.border_width,
            context.focused() ? theme.palette.focus : theme.palette.border);

        painter.text(
            {bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
            display_text(),
            popup_text_style(theme, text, TextAlign::Center));
    }

private:
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
    std::shared_ptr<ComboAnchorRuntime<T>> runtime_;
    typename State<T>::Subscription subscription_;
    PressActivationState interaction_;
    std::optional<OverlayComponentCommand> pending_command_;
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
        std::shared_ptr<MenuAnchorRuntime> runtime)
        : label_(std::move(label)),
          items_provider_(std::move(items_provider)),
          runtime_(std::move(runtime)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool dismiss_overlay_on_tab() const noexcept override { return true; }
    [[nodiscard]] bool dismiss_overlay_when_read_only() const noexcept override { return false; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto& theme = current_theme();
        const auto style = popup_text_style(theme, theme.palette.text, TextAlign::Center);
        const auto text = TextService::measure(label_, style);
        return {
            std::max(theme.controls.minimum_width, text.width + theme.spacing.large * 2.0f),
            theme.controls.control_height};
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
        if (!focused && !runtime_->handle.valid()) {
            runtime_->suppress_until_key_up = Key::None;
        }
        interaction_.focus_changed(focused, context);
    }

    void deactivate(LifecycleContext& context) override {
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
        const auto& theme = current_theme();
        auto& painter = context.painter();

        const Color fill = effective_enabled()
            ? theme.palette.surface
            : theme.palette.control_background;
        const Color text = effective_enabled()
            ? theme.palette.text
            : theme.palette.disabled;
        painter.fill_rounded_rect(bounds, theme.radii.medium, fill);
        painter.stroke_rounded_rect(
            bounds,
            theme.radii.medium,
            context.focused() ? theme.controls.focus_ring_width : theme.controls.border_width,
            context.focused() ? theme.palette.focus : theme.palette.border);
        painter.text(
            {bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
            label_,
            popup_text_style(theme, text, TextAlign::Center));
    }

private:
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
    std::shared_ptr<MenuAnchorRuntime> runtime_;
    PressActivationState interaction_;
    std::optional<OverlayComponentCommand> pending_command_;
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

    Spec spec() && {
        auto* selection = selection_;
        auto provider = std::move(options_provider_);
        auto initial_options = std::move(initial_options_);
        auto placeholder = std::move(placeholder_);
        auto runtime = std::make_shared<detail::ComboAnchorRuntime<T>>();
        return Spec{
            [selection,
             provider = std::move(provider),
             initial_options = std::move(initial_options),
             placeholder = std::move(placeholder),
             runtime = std::move(runtime)]() mutable {
                return std::make_unique<detail::ComboBoxComponent<T>>(
                    *selection,
                    std::move(provider),
                    std::move(initial_options),
                    std::move(placeholder),
                    std::move(runtime));
            },
            {}};
    }

private:
    State<T>* selection_{};
    std::vector<ComboBoxOption<T>> initial_options_;
    OptionsProvider options_provider_;
    std::string placeholder_{"No selection"};
};

class PopupMenu {
public:
    using ItemsProvider = std::function<std::vector<PopupMenuItem>()>;

    PopupMenu(std::string label, std::vector<PopupMenuItem> items)
        : label_(std::move(label)),
          items_provider_([items = std::move(items)] { return items; }) {}

    PopupMenu(std::string label, ItemsProvider items_provider)
        : label_(std::move(label)), items_provider_(std::move(items_provider)) {}

    Spec spec() && {
        auto label = std::move(label_);
        auto provider = std::move(items_provider_);
        auto runtime = std::make_shared<detail::MenuAnchorRuntime>();
        return Spec{
            [label = std::move(label),
             provider = std::move(provider),
             runtime = std::move(runtime)]() mutable {
                return std::make_unique<detail::PopupMenuComponent>(
                    std::move(label), std::move(provider), std::move(runtime));
            },
            {}};
    }

private:
    std::string label_;
    ItemsProvider items_provider_;
};

} // namespace ui
