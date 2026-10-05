#pragma once

#include <nativeui/component.hpp>
#include <nativeui/state.hpp>

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace ui {

enum class DisclosureContentPolicy { Retain, UnmountWhenClosed };

struct CollapsibleStyle {
    float gap{8.0f};
    float padding{8.0f};
    float chevron_size{12.0f};
    float chevron_gap{6.0f};
    float corner_radius{4.0f};
    std::optional<Color> background;
    std::optional<Color> hover_background;
    std::optional<Color> pressed_background;
    std::optional<Color> text_color;
    std::optional<Color> chevron_color;
    std::optional<Color> focus_color;
    std::string accessible_name;
    bool reduced_motion{};
};

class Collapsible {
public:
    template <class Child>
    Collapsible(std::string title, Binding<bool> open, Child&& child)
        : title_(std::move(title)), open_(std::move(open)),
          child_(make_spec(std::forward<Child>(child))) {}
    template <class Child>
    Collapsible(std::string title, State<bool>& open, Child&& child)
        : Collapsible(std::move(title), open.binding(), std::forward<Child>(child)) {}
    Collapsible&& content_policy(DisclosureContentPolicy value) &&;
    Collapsible&& on_change(std::function<void(bool)> callback) &&;
    Collapsible&& style(CollapsibleStyle value) &&;
    Spec spec() &&;

private:
    std::string title_;
    Binding<bool> open_;
    Spec child_;
    DisclosureContentPolicy policy_{DisclosureContentPolicy::Retain};
    std::function<void(bool)> on_change_;
    CollapsibleStyle style_;
};

} // namespace ui
