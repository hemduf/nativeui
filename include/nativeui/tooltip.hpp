#pragma once

#include <nativeui/component.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ui {
namespace detail {

class TooltipComponent final : public Component {
public:
    TooltipComponent(std::string text, std::chrono::milliseconds delay)
        : text_(std::move(text)), delay_(delay) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override {
        return constraints;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void paint(PaintContext&) const override {}

private:
    std::string text_;
    std::chrono::milliseconds delay_;
};

} // namespace detail

class Tooltip {
public:
    static constexpr std::chrono::milliseconds kDefaultDelay{500};
    static constexpr float kDefaultMaxWidth = 320.0f;

    template <class Child>
    Tooltip(std::string text, Child&& child)
        : text_(std::move(text)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Tooltip&& delay(std::chrono::milliseconds value) && noexcept {
        delay_ = value.count() < 0 ? std::chrono::milliseconds{0} : value;
        return std::move(*this);
    }

    [[nodiscard]] std::chrono::milliseconds delay() const noexcept { return delay_; }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }

    Spec spec() && {
        auto text = std::move(text_);
        const auto delay_value = delay_;
        return Spec{
            [text = std::move(text), delay_value]() mutable {
                return std::make_unique<detail::TooltipComponent>(
                    std::move(text), delay_value);
            },
            std::move(children_)};
    }

private:
    std::string text_;
    std::chrono::milliseconds delay_{kDefaultDelay};
    std::vector<Spec> children_;
};

} // namespace ui
