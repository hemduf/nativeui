#pragma once

#include <nativeui/component.hpp>
#include <nativeui/layout.hpp>
#include <nativeui/state.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ui {

namespace detail {

class OwnedSpec final {
public:
    explicit OwnedSpec(Spec spec) : spec_(std::move(spec)) {}
    Spec spec() && { return std::move(spec_); }

private:
    Spec spec_;
};

template <class T>
struct ListViewRuntime final {
    explicit ListViewRuntime(State<std::optional<T>>& selection)
        : selection(&selection), scroll(ScrollAxis::Vertical) {}

    State<std::optional<T>>* selection{};
    ScrollState scroll;
    std::vector<T> keys;
    std::vector<bool> enabled;
    std::vector<Rect> row_bounds;
};

template <class T>
class ListRowComponent final : public Component {
public:
    ListRowComponent(std::shared_ptr<ListViewRuntime<T>> runtime, std::size_t index)
        : runtime_(std::move(runtime)), index_(index) {}

    [[nodiscard]] ComponentAvailability local_availability() const noexcept override {
        ComponentAvailability result{};
        result.enabled = index_ < runtime_->enabled.size() && runtime_->enabled[index_];
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
        subscription_ = runtime_->selection->observe(
            [invalidate = std::move(invalidate)](const std::optional<T>&) { invalidate(); });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

    void paint(PaintContext& context) const override {
        if (index_ >= runtime_->keys.size()) return;
        const auto& selected = runtime_->selection->get();
        if (selected && *selected == runtime_->keys[index_]) {
            context.painter().fill_rounded_rect(context.bounds(), 4.0f, colors::selection);
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

    void paint(PaintContext& context) const override {
        context.painter().stroke_rounded_rect(
            context.bounds(), 6.0f, context.focused() ? 2.0f : 1.0f,
            context.focused() ? colors::borderFocus : colors::border);
    }

private:
    std::shared_ptr<ListViewRuntime<T>> runtime_;
};

template <class T>
struct TabsRuntime final {
    explicit TabsRuntime(State<T>& selection) : selection(&selection) {}

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
            result.h = std::max(result.h, kHeaderHeight + child.preferred.h);
        }
        return result;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        Size result{0.0f, kHeaderHeight};
        for (const auto& child : children) {
            result.w = std::max(result.w, child.minimum.w);
            result.h = std::max(result.h, kHeaderHeight + child.minimum.h);
        }
        return result;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        const Rect panel{
            bounds.x,
            bounds.y + std::min(bounds.h, kHeaderHeight),
            bounds.w,
            std::max(0.0f, bounds.h - kHeaderHeight)};
        for (auto& placement : placements) placement.bounds = panel;
    }

    void mount(MountContext& context) override {
        auto invalidate = context.invalidator();
        subscription_ = runtime_->selection->observe(
            [invalidate = std::move(invalidate)](const T&) { invalidate(); });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

    void paint(PaintContext& context) const override {
        if (runtime_->keys.empty()) return;
        const auto bounds = context.bounds();
        const float width = bounds.w / static_cast<float>(runtime_->keys.size());
        auto& painter = context.painter();
        for (std::size_t index = 0; index < runtime_->keys.size(); ++index) {
            const Rect tab{
                bounds.x + width * static_cast<float>(index), bounds.y,
                index + 1 == runtime_->keys.size()
                    ? std::max(0.0f, bounds.x + bounds.w - (bounds.x + width * static_cast<float>(index)))
                    : width,
                std::min(bounds.h, kHeaderHeight)};
            const bool selected = runtime_->selection->get() == runtime_->keys[index];
            const bool enabled = index < runtime_->enabled.size() && runtime_->enabled[index];
            painter.fill_rounded_rect(tab, 4.0f, selected ? colors::selection : colors::panel);
            painter.stroke_rounded_rect(
                tab, 4.0f, selected || context.focused() ? 2.0f : 1.0f,
                selected || context.focused() ? colors::borderFocus : colors::border);
            if (index < runtime_->labels.size()) {
                painter.text(
                    Point{tab.x + tab.w * 0.5f, tab.y + tab.h * 0.5f},
                    runtime_->labels[index], 13.0f,
                    enabled ? colors::text : colors::textMuted,
                    TextAlign::Center);
            }
        }
    }

private:
    static constexpr float kHeaderHeight = 36.0f;

    std::shared_ptr<TabsRuntime<T>> runtime_;
    typename State<T>::Subscription subscription_;
};

} // namespace detail

template <class T>
class ListView {
public:
    explicit ListView(State<std::optional<T>>& selection) : selection_(&selection) {}

    template <class Child>
    ListView&& item(T key, Child&& child, bool enabled = true) && {
        reject_duplicate(key);
        items_.push_back(Item{
            std::move(key), enabled, make_spec(std::forward<Child>(child))});
        return std::move(*this);
    }

    Spec spec() && {
        auto runtime = std::make_shared<detail::ListViewRuntime<T>>(*selection_);
        runtime->keys.reserve(items_.size());
        runtime->enabled.reserve(items_.size());

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
    std::vector<Item> items_;
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
