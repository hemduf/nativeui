#pragma once

#include <nativeui/component.hpp>
#include <nativeui/layout.hpp>
#include <nativeui/state.hpp>
#include <nativeui/virtual_list.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ui {

namespace detail {

inline constexpr Color kListTabsHoverSurface{0.145f, 0.155f, 0.175f, 1.0f};

class OwnedSpec final {
public:
    explicit OwnedSpec(Spec spec) : spec_(std::move(spec)) {}
    Spec spec() && { return std::move(spec_); }

private:
    Spec spec_;
};

template <class T>
struct ListViewRuntime final {
    using ActivationCallback = std::function<void(const T&)>;

    explicit ListViewRuntime(State<std::optional<T>>& selection)
        : selection(&selection), scroll(ScrollAxis::Vertical) {}

    [[nodiscard]] bool enabled_at(std::size_t index) const noexcept {
        return index < enabled.size() && enabled[index];
    }

    [[nodiscard]] std::optional<std::size_t> selected_index() const {
        const auto& selected = selection->get();
        if (!selected) return std::nullopt;
        for (std::size_t index = 0; index < keys.size(); ++index) {
            if (enabled_at(index) && keys[index] == *selected) return index;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> first_enabled() const noexcept {
        for (std::size_t index = 0; index < enabled.size(); ++index) {
            if (enabled_at(index)) return index;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> last_enabled() const noexcept {
        for (std::size_t index = enabled.size(); index > 0; --index) {
            if (enabled_at(index - 1)) return index - 1;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> next_enabled(std::size_t from) const noexcept {
        for (std::size_t index = from + 1; index < enabled.size(); ++index) {
            if (enabled_at(index)) return index;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> previous_enabled(std::size_t from) const noexcept {
        for (std::size_t index = from; index > 0; --index) {
            if (enabled_at(index - 1)) return index - 1;
        }
        return std::nullopt;
    }

    bool select(std::size_t index) {
        if (index >= keys.size() || !enabled_at(index)) return false;

        // Finish every framework-owned mutation before State observers can
        // re-enter application code. The runtime is retained by the caller for
        // the duration of dispatch, but no framework member is touched after
        // State::set() begins notifying arbitrary observers.
        const T key = keys[index];
        if (index < row_bounds.size()) {
            (void)ensure_visible(scroll, row_bounds[index], ScrollAlignment::Nearest);
        }
        auto* const selected = selection;
        selected->set(std::optional<T>{key});
        return true;
    }

    bool activate(std::size_t index) {
        if (index >= keys.size() || !enabled_at(index)) return false;
        const T key = keys[index];
        auto callback = activation_callback;
        if (!select(index)) return false;
        if (callback) callback(key);
        return true;
    }

    State<std::optional<T>>* selection{};
    ScrollState scroll;
    std::vector<T> keys;
    std::vector<bool> enabled;
    std::vector<Rect> row_bounds;
    ActivationCallback activation_callback;
};

template <class T>
class ListRowComponent final : public Component {
public:
    ListRowComponent(std::shared_ptr<ListViewRuntime<T>> runtime, std::size_t index)
        : runtime_(std::move(runtime)), index_(index) {}

    [[nodiscard]] ComponentAvailability local_availability() const noexcept override {
        ComponentAvailability result{};
        result.enabled = runtime_->enabled_at(index_);
        return result;
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void mount(MountContext& context) override {
        auto invalidate = context.invalidator();
        auto runtime = runtime_;
        const auto index = index_;
        subscription_ = runtime_->selection->observe(
            [runtime = std::move(runtime), index, invalidate = std::move(invalidate)](
                const std::optional<T>& selected) {
                if (selected && index < runtime->keys.size() &&
                    runtime->keys[index] == *selected && index < runtime->row_bounds.size()) {
                    (void)ensure_visible(
                        runtime->scroll,
                        runtime->row_bounds[index],
                        ScrollAlignment::Nearest);
                }
                invalidate();
            });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

    void paint(PaintContext& context) const override {
        if (index_ >= runtime_->keys.size()) return;

        const auto bounds = context.bounds();
        auto& painter = context.painter();
        const auto& selected = runtime_->selection->get();
        if (selected && *selected == runtime_->keys[index_]) {
            const Rect highlight{
                bounds.x + 4.0f,
                bounds.y + 2.0f,
                std::max(0.0f, bounds.w - 8.0f),
                std::max(0.0f, bounds.h - 4.0f)};
            painter.fill_rounded_rect(highlight, 6.0f, colors::selection);
            painter.fill_rounded_rect(
                {highlight.x + 3.0f,
                 highlight.y + 7.0f,
                 3.0f,
                 std::max(0.0f, highlight.h - 14.0f)},
                1.5f,
                colors::accent);
        }

        if (index_ + 1 < runtime_->keys.size() && bounds.w > 24.0f) {
            const float y = bounds.y + bounds.h - 0.5f;
            painter.line(
                {bounds.x + 12.0f, y},
                {bounds.x + bounds.w - 12.0f, y},
                1.0f,
                Color{0.20f, 0.22f, 0.25f, 0.7f});
        }
    }

private:
    std::shared_ptr<ListViewRuntime<T>> runtime_;
    std::size_t index_{};
    typename State<std::optional<T>>::Subscription subscription_;
};

template <class T>
class ListContentComponent final : public Component {
public:
    explicit ListContentComponent(std::shared_ptr<ListViewRuntime<T>> runtime)
        : runtime_(std::move(runtime)) {}

    [[nodiscard]] bool is_focus_scope() const noexcept override { return true; }
    [[nodiscard]] bool focus_scope_active() const noexcept override { return false; }
    [[nodiscard]] bool focus_scope_traps() const noexcept override { return false; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        Size result{};
        for (const auto& child : children) {
            result.w = std::max(result.w, child.preferred.w);
            result.h += child.preferred.h;
        }
        return result;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        Size result{};
        for (const auto& child : children) {
            result.w = std::max(result.w, child.minimum.w);
            result.h += child.minimum.h;
        }
        return result;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override {
        runtime_->row_bounds.assign(placements.size(), Rect{});
        float y = bounds.y;
        for (std::size_t index = 0; index < placements.size(); ++index) {
            const float height = index < children.size()
                ? std::max(0.0f, children[index].preferred.h)
                : 0.0f;
            const Rect row{bounds.x, y, bounds.w, height};
            placements[index].bounds = row;
            runtime_->row_bounds[index] = Rect{0.0f, y - bounds.y, bounds.w, height};
            y += height;
        }
    }

    void paint(PaintContext&) const override {}

private:
    std::shared_ptr<ListViewRuntime<T>> runtime_;
};

template <class T>
class ListViewComponent final : public Component {
public:
    explicit ListViewComponent(std::shared_ptr<ListViewRuntime<T>> runtime)
        : runtime_(std::move(runtime)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void focus_changed(bool focused, FocusContext&) override {
        if (!focused) {
            active_index_.reset();
            return;
        }
        active_index_ = runtime_->selected_index();
        if (!active_index_) active_index_ = runtime_->first_enabled();
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        auto runtime = runtime_;
        if (event.type == InputType::KeyDown) {
            if (const auto selected = runtime->selected_index()) {
                active_index_ = selected;
            } else if (!active_index_) {
                active_index_ = runtime->first_enabled();
            }

            if (event.key == Key::Enter || event.key == Key::Space) {
                if (active_index_) {
                    (void)runtime->activate(*active_index_);
                    return EventResult::Handled;
                }
                return EventResult::Ignored;
            }

            std::optional<std::size_t> target;
            switch (event.key) {
            case Key::Down:
                target = active_index_
                    ? runtime->next_enabled(*active_index_)
                    : runtime->first_enabled();
                break;
            case Key::Up:
                target = active_index_
                    ? runtime->previous_enabled(*active_index_)
                    : runtime->last_enabled();
                break;
            case Key::Home:
                target = runtime->first_enabled();
                break;
            case Key::End:
                target = runtime->last_enabled();
                break;
            default:
                return EventResult::Ignored;
            }

            if (target) {
                active_index_ = target;
                (void)runtime->select(*target);
            }
            return EventResult::Handled;
        }

        switch (event.type) {
        case InputType::PointerMove: {
            const auto index = row_at(event.position, context.bounds());
            set_hovered(
                index && runtime->enabled_at(*index) ? index : std::nullopt,
                context);
            return EventResult::Ignored;
        }

        case InputType::PointerLeave:
            set_hovered(std::nullopt, context);
            return EventResult::Ignored;

        case InputType::PointerDown: {
            const auto index = row_at(event.position, context.bounds());
            if (!index) {
                set_hovered(std::nullopt, context);
                return EventResult::Ignored;
            }
            if (!runtime->enabled_at(*index)) {
                set_hovered(std::nullopt, context);
                return EventResult::Handled;
            }
            set_hovered(index, context);
            pointer_armed_ = index;
            context.capture_pointer();
            return EventResult::Handled;
        }

        case InputType::PointerUp: {
            const auto index = row_at(event.position, context.bounds());
            set_hovered(
                index && runtime->enabled_at(*index) ? index : std::nullopt,
                context);
            if (!pointer_armed_) return EventResult::Ignored;
            const auto armed = *pointer_armed_;
            pointer_armed_.reset();
            context.release_pointer();
            if (index && *index == armed) {
                active_index_ = armed;
                (void)runtime->activate(armed);
            }
            return EventResult::Handled;
        }

        case InputType::PointerCancel:
            if (!pointer_armed_) return EventResult::Ignored;
            pointer_armed_.reset();
            context.release_pointer();
            return EventResult::Handled;

        default:
            return EventResult::Ignored;
        }
    }

    void deactivate(LifecycleContext&) override {
        pointer_armed_.reset();
        hovered_index_.reset();
    }

    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds, 9.0f, colors::panel);

        if (effective_enabled() && hovered_index_ &&
            runtime_->enabled_at(*hovered_index_) &&
            *hovered_index_ < runtime_->row_bounds.size()) {
            const auto& selected = runtime_->selection->get();
            const bool hovered_selected = selected &&
                *hovered_index_ < runtime_->keys.size() &&
                *selected == runtime_->keys[*hovered_index_];
            if (!hovered_selected) {
                const auto row = runtime_->row_bounds[*hovered_index_];
                const auto offset = runtime_->scroll.offset();
                const Rect highlight{
                    bounds.x + 4.0f,
                    bounds.y + row.y - offset.y + 2.0f,
                    std::max(0.0f, bounds.w - 8.0f),
                    std::max(0.0f, row.h - 4.0f)};
                const auto visible = intersect(highlight, bounds);
                if (!visible.empty()) {
                    painter.fill_rounded_rect(visible, 6.0f, kListTabsHoverSurface);
                }
            }
        }

        painter.stroke_rounded_rect(
            bounds,
            9.0f,
            context.focused() ? 1.5f : 1.0f,
            context.focused() ? colors::borderFocus : colors::border);
    }

private:
    void set_hovered(std::optional<std::size_t> index, InputContext& context) {
        if (hovered_index_ == index) return;
        hovered_index_ = index;
        context.invalidate();
    }

    [[nodiscard]] std::optional<std::size_t> row_at(Point position, Rect bounds) const noexcept {
        if (!bounds.contains(position)) return std::nullopt;
        const auto offset = runtime_->scroll.offset();
        const Point content_point{
            position.x - bounds.x + offset.x,
            position.y - bounds.y + offset.y};
        for (std::size_t index = 0; index < runtime_->row_bounds.size(); ++index) {
            if (runtime_->row_bounds[index].contains(content_point)) return index;
        }
        return std::nullopt;
    }

    std::shared_ptr<ListViewRuntime<T>> runtime_;
    std::optional<std::size_t> active_index_;
    std::optional<std::size_t> pointer_armed_;
    std::optional<std::size_t> hovered_index_;
};

template <class T>
struct TabsRuntime final {
    explicit TabsRuntime(State<T>& selection) : selection(&selection) {}

    [[nodiscard]] bool enabled_at(std::size_t index) const noexcept {
        return index < enabled.size() && enabled[index];
    }

    [[nodiscard]] std::optional<std::size_t> selected_index() const {
        for (std::size_t index = 0; index < keys.size(); ++index) {
            if (enabled_at(index) && keys[index] == selection->get()) return index;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> first_enabled() const noexcept {
        for (std::size_t index = 0; index < enabled.size(); ++index) {
            if (enabled_at(index)) return index;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> last_enabled() const noexcept {
        for (std::size_t index = enabled.size(); index > 0; --index) {
            if (enabled_at(index - 1)) return index - 1;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> next_enabled_wrapped(std::size_t from) const noexcept {
        if (enabled.empty()) return std::nullopt;
        for (std::size_t distance = 1; distance <= enabled.size(); ++distance) {
            const auto index = (from + distance) % enabled.size();
            if (enabled_at(index)) return index;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> previous_enabled_wrapped(std::size_t from) const noexcept {
        if (enabled.empty()) return std::nullopt;
        for (std::size_t distance = 1; distance <= enabled.size(); ++distance) {
            const auto index = (from + enabled.size() - (distance % enabled.size())) % enabled.size();
            if (enabled_at(index)) return index;
        }
        return std::nullopt;
    }

    bool select(std::size_t index) {
        if (index >= keys.size() || !enabled_at(index)) return false;
        const T key = keys[index];
        auto* const selected = selection;
        selected->set(key);
        return true;
    }

    State<T>* selection{};
    std::vector<T> keys;
    std::vector<std::string> labels;
    std::vector<bool> enabled;
};

template <class T>
class TabPanelComponent final : public Component {
public:
    TabPanelComponent(std::shared_ptr<TabsRuntime<T>> runtime, std::size_t index)
        : runtime_(std::move(runtime)), index_(index) {}

    [[nodiscard]] ComponentAvailability local_availability() const noexcept override {
        ComponentAvailability result{};
        result.visibility = selected() ? VisibilityMode::Visible : VisibilityMode::Collapsed;
        return result;
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void mount(MountContext& context) override {
        auto invalidate_availability = context.availability_invalidator();
        subscription_ = runtime_->selection->observe(
            [invalidate_availability = std::move(invalidate_availability)](const T&) {
                invalidate_availability();
            });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }
    void paint(PaintContext&) const override {}

private:
    [[nodiscard]] bool selected() const noexcept {
        return index_ < runtime_->keys.size() && runtime_->selection->get() == runtime_->keys[index_];
    }

    std::shared_ptr<TabsRuntime<T>> runtime_;
    std::size_t index_{};
    typename State<T>::Subscription subscription_;
};

template <class T>
class TabsComponent final : public Component {
public:
    explicit TabsComponent(std::shared_ptr<TabsRuntime<T>> runtime)
        : runtime_(std::move(runtime)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        Size result{120.0f, kHeaderHeight};
        for (const auto& child : children) {
            result.w = std::max(result.w, child.preferred.w);
            result.h = std::max(
                result.h,
                kHeaderHeight + kPanelGap + child.preferred.h);
        }
        return result;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        Size result{0.0f, kHeaderHeight};
        for (const auto& child : children) {
            result.w = std::max(result.w, child.minimum.w);
            result.h = std::max(
                result.h,
                kHeaderHeight + kPanelGap + child.minimum.h);
        }
        return result;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        const float header_height = std::min(bounds.h, kHeaderHeight);
        const float remaining = std::max(0.0f, bounds.h - header_height);
        const float gap = std::min(remaining, kPanelGap);
        const Rect panel{
            bounds.x,
            bounds.y + header_height + gap,
            bounds.w,
            std::max(0.0f, bounds.h - header_height - gap)};
        for (auto& placement : placements) placement.bounds = panel;
    }

    void mount(MountContext& context) override {
        auto invalidate = context.invalidator();
        subscription_ = runtime_->selection->observe(
            [invalidate = std::move(invalidate)](const T&) { invalidate(); });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

    void focus_changed(bool focused, FocusContext&) override {
        if (!focused) {
            active_index_.reset();
            return;
        }
        active_index_ = runtime_->selected_index();
        if (!active_index_) active_index_ = runtime_->first_enabled();
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        auto runtime = runtime_;
        if (event.type == InputType::KeyDown) {
            if (const auto selected = runtime->selected_index()) {
                active_index_ = selected;
            } else if (!active_index_) {
                active_index_ = runtime->first_enabled();
            }

            std::optional<std::size_t> target;
            switch (event.key) {
            case Key::Right:
                target = active_index_
                    ? runtime->next_enabled_wrapped(*active_index_)
                    : runtime->first_enabled();
                break;
            case Key::Left:
                target = active_index_
                    ? runtime->previous_enabled_wrapped(*active_index_)
                    : runtime->last_enabled();
                break;
            case Key::Home:
                target = runtime->first_enabled();
                break;
            case Key::End:
                target = runtime->last_enabled();
                break;
            default:
                return EventResult::Ignored;
            }
            if (target) {
                active_index_ = target;
                (void)runtime->select(*target);
            }
            return EventResult::Handled;
        }

        switch (event.type) {
        case InputType::PointerMove: {
            const auto index = tab_at(event.position, context.bounds());
            set_hovered(
                index && runtime->enabled_at(*index) ? index : std::nullopt,
                context);
            return EventResult::Ignored;
        }

        case InputType::PointerLeave:
            set_hovered(std::nullopt, context);
            return EventResult::Ignored;

        case InputType::PointerDown: {
            const auto index = tab_at(event.position, context.bounds());
            if (!index) {
                set_hovered(std::nullopt, context);
                return EventResult::Ignored;
            }
            if (!runtime->enabled_at(*index)) {
                set_hovered(std::nullopt, context);
                return EventResult::Handled;
            }
            set_hovered(index, context);
            pointer_armed_ = index;
            context.capture_pointer();
            return EventResult::Handled;
        }

        case InputType::PointerUp: {
            const auto index = tab_at(event.position, context.bounds());
            set_hovered(
                index && runtime->enabled_at(*index) ? index : std::nullopt,
                context);
            if (!pointer_armed_) return EventResult::Ignored;
            const auto armed = *pointer_armed_;
            pointer_armed_.reset();
            context.release_pointer();
            if (index && *index == armed) {
                active_index_ = armed;
                (void)runtime->select(armed);
            }
            return EventResult::Handled;
        }

        case InputType::PointerCancel:
            if (!pointer_armed_) return EventResult::Ignored;
            pointer_armed_.reset();
            context.release_pointer();
            return EventResult::Handled;

        default:
            return EventResult::Ignored;
        }
    }

    void deactivate(LifecycleContext&) override {
        pointer_armed_.reset();
        hovered_index_.reset();
    }

    void paint(PaintContext& context) const override {
        if (runtime_->keys.empty()) return;

        const auto bounds = context.bounds();
        const float header_height = std::min(bounds.h, kHeaderHeight);
        if (!(header_height > 0.0f) || !(bounds.w > 0.0f)) return;

        auto& painter = context.painter();
        const Rect header{bounds.x, bounds.y, bounds.w, header_height};
        painter.fill_rounded_rect(header, kCornerRadius, colors::panel);
        painter.stroke_rounded_rect(
            header,
            kCornerRadius,
            context.focused() ? 1.5f : 1.0f,
            context.focused() ? colors::borderFocus : colors::border);

        const float width = bounds.w / static_cast<float>(runtime_->keys.size());
        for (std::size_t index = 0; index < runtime_->keys.size(); ++index) {
            const Rect tab{
                bounds.x + width * static_cast<float>(index),
                bounds.y,
                index + 1 == runtime_->keys.size()
                    ? std::max(
                          0.0f,
                          bounds.x + bounds.w -
                              (bounds.x + width * static_cast<float>(index)))
                    : width,
                header_height};
            const bool selected = runtime_->selection->get() == runtime_->keys[index];
            const bool enabled = runtime_->enabled_at(index);
            const bool hovered = effective_enabled() && enabled && !selected &&
                hovered_index_ && *hovered_index_ == index;

            if (hovered) {
                painter.fill_rounded_rect(
                    {tab.x + 3.0f,
                     tab.y + 3.0f,
                     std::max(0.0f, tab.w - 6.0f),
                     std::max(0.0f, tab.h - 6.0f)},
                    6.0f,
                    kListTabsHoverSurface);
            }

            if (selected) {
                painter.fill_rounded_rect(
                    {tab.x + 3.0f,
                     tab.y + 3.0f,
                     std::max(0.0f, tab.w - 6.0f),
                     std::max(0.0f, tab.h - 6.0f)},
                    6.0f,
                    colors::input);
                const float underline_width = std::max(0.0f, tab.w - 28.0f);
                if (underline_width > 0.0f) {
                    painter.fill_rounded_rect(
                        {tab.x + 14.0f,
                         tab.y + tab.h - 4.0f,
                         underline_width,
                         2.0f},
                        1.0f,
                        colors::accent);
                }
            }

            if (index > 0) {
                const float x = tab.x;
                painter.line(
                    {x, tab.y + 10.0f},
                    {x, tab.y + tab.h - 10.0f},
                    1.0f,
                    Color{0.20f, 0.22f, 0.25f, 0.75f});
            }

            if (index < runtime_->labels.size()) {
                const Color text_color = !enabled
                    ? Color{0.39f, 0.42f, 0.47f, 1.0f}
                    : ((selected || hovered) ? colors::text : colors::textMuted);
                painter.text(
                    Point{tab.x + tab.w * 0.5f, tab.y + tab.h * 0.5f},
                    runtime_->labels[index],
                    13.0f,
                    text_color,
                    TextAlign::Center);
            }
        }

        const float remaining = std::max(0.0f, bounds.h - header_height);
        const float gap = std::min(remaining, kPanelGap);
        const Rect panel{
            bounds.x,
            bounds.y + header_height + gap,
            bounds.w,
            std::max(0.0f, bounds.h - header_height - gap)};
        if (!panel.empty()) {
            painter.fill_rounded_rect(panel, 10.0f, colors::panel);
            painter.stroke_rounded_rect(panel, 10.0f, 1.0f, colors::border);
        }
    }

private:
    void set_hovered(std::optional<std::size_t> index, InputContext& context) {
        if (hovered_index_ == index) return;
        hovered_index_ = index;
        context.invalidate();
    }

    [[nodiscard]] std::optional<std::size_t> tab_at(Point position, Rect bounds) const noexcept {
        if (runtime_->keys.empty() || !(bounds.w > 0.0f) ||
            position.y < bounds.y || position.y >= bounds.y + std::min(bounds.h, kHeaderHeight) ||
            position.x < bounds.x || position.x >= bounds.x + bounds.w) {
            return std::nullopt;
        }
        const double relative = static_cast<double>(position.x - bounds.x) /
                                static_cast<double>(bounds.w);
        auto index = static_cast<std::size_t>(
            std::floor(relative * static_cast<double>(runtime_->keys.size())));
        if (index >= runtime_->keys.size()) index = runtime_->keys.size() - 1;
        return index;
    }

    static constexpr float kHeaderHeight = 40.0f;
    static constexpr float kPanelGap = 10.0f;
    static constexpr float kCornerRadius = 8.0f;

    std::shared_ptr<TabsRuntime<T>> runtime_;
    std::optional<std::size_t> active_index_;
    std::optional<std::size_t> pointer_armed_;
    std::optional<std::size_t> hovered_index_;
    typename State<T>::Subscription subscription_;
};

} // namespace detail

template <class T>
class ListView {
public:
    using ActivationCallback = std::function<void(const T&)>;

    explicit ListView(State<std::optional<T>>& selection) : selection_(&selection) {}
    explicit ListView(VirtualListState<T>& state)
        : selection_(&state.selection()), virtual_state_(&state) {}

    template <class Child>
    ListView&& item(T key, Child&& child, bool enabled = true) && {
        if (virtual_state_) {
            throw std::logic_error("ListView::item is unavailable for a virtualized ListView");
        }
        reject_duplicate(key);
        items_.push_back(Item{
            std::move(key), enabled, make_spec(std::forward<Child>(child))});
        return std::move(*this);
    }

    ListView&& on_activate(ActivationCallback callback) && {
        if (virtual_state_) {
            throw std::logic_error(
                "ListView::on_activate is not available on the virtualized path yet");
        }
        activation_callback_ = std::move(callback);
        return std::move(*this);
    }

    Spec spec() && {
        if (virtual_state_) {
            return detail::make_virtual_list_retained_spec(virtual_state_->runtime());
        }

        auto runtime = std::make_shared<detail::ListViewRuntime<T>>(*selection_);
        runtime->keys.reserve(items_.size());
        runtime->enabled.reserve(items_.size());
        runtime->activation_callback = std::move(activation_callback_);

        std::vector<Spec> rows;
        rows.reserve(items_.size());
        for (std::size_t index = 0; index < items_.size(); ++index) {
            runtime->keys.push_back(items_[index].key);
            runtime->enabled.push_back(items_[index].enabled);
            std::vector<Spec> row_children;
            row_children.push_back(std::move(items_[index].content));
            rows.push_back(Spec{
                [runtime, index] {
                    return std::make_unique<detail::ListRowComponent<T>>(runtime, index);
                },
                std::move(row_children)});
        }

        Spec content{
            [runtime] {
                return std::make_unique<detail::ListContentComponent<T>>(runtime);
            },
            std::move(rows)};
        auto scroll = std::move(ScrollView{runtime->scroll, detail::OwnedSpec{std::move(content)}}).spec();

        std::vector<Spec> children;
        children.push_back(std::move(scroll));
        return Spec{
            [runtime] {
                return std::make_unique<detail::ListViewComponent<T>>(runtime);
            },
            std::move(children)};
    }

private:
    struct Item final {
        T key;
        bool enabled{};
        Spec content;
    };

    void reject_duplicate(const T& key) const {
        for (const auto& item : items_) {
            if (item.key == key) {
                throw std::invalid_argument("ListView item keys must be unique");
            }
        }
    }

    State<std::optional<T>>* selection_{};
    VirtualListState<T>* virtual_state_{};
    std::vector<Item> items_;
    ActivationCallback activation_callback_;
};

template <class T>
class Tabs {
public:
    explicit Tabs(State<T>& selection) : selection_(&selection) {}

    template <class Child>
    Tabs&& tab(T key, std::string label, Child&& panel, bool enabled = true) && {
        reject_duplicate(key);
        tabs_.push_back(Tab{
            std::move(key), std::move(label), enabled,
            make_spec(std::forward<Child>(panel))});
        return std::move(*this);
    }

    Spec spec() && {
        auto runtime = std::make_shared<detail::TabsRuntime<T>>(*selection_);
        runtime->keys.reserve(tabs_.size());
        runtime->labels.reserve(tabs_.size());
        runtime->enabled.reserve(tabs_.size());

        std::vector<Spec> panels;
        panels.reserve(tabs_.size());
        for (std::size_t index = 0; index < tabs_.size(); ++index) {
            runtime->keys.push_back(tabs_[index].key);
            runtime->labels.push_back(tabs_[index].label);
            runtime->enabled.push_back(tabs_[index].enabled);
            std::vector<Spec> panel_children;
            panel_children.push_back(std::move(tabs_[index].panel));
            panels.push_back(Spec{
                [runtime, index] {
                    return std::make_unique<detail::TabPanelComponent<T>>(runtime, index);
                },
                std::move(panel_children)});
        }

        return Spec{
            [runtime] {
                return std::make_unique<detail::TabsComponent<T>>(runtime);
            },
            std::move(panels)};
    }

private:
    struct Tab final {
        T key;
        std::string label;
        bool enabled{};
        Spec panel;
    };

    void reject_duplicate(const T& key) const {
        for (const auto& tab : tabs_) {
            if (tab.key == key) {
                throw std::invalid_argument("Tabs keys must be unique");
            }
        }
    }

    State<T>* selection_{};
    std::vector<Tab> tabs_;
};

} // namespace ui