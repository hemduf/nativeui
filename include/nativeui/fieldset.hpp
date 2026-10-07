#pragma once

#include <nativeui/component.hpp>
#include <nativeui/state.hpp>
#include <nativeui/text.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui {
struct FieldsetStyle {
    float padding{};
    float row_gap{8.0f};
    float legend_gap{8.0f};
    float description_gap{4.0f};
    float border_width{1.0f};
    float corner_radius{4.0f};
    std::optional<TextStyle> legend_text;
    std::optional<TextStyle> description_text;
    std::optional<Color> background;
    std::optional<Color> border_color;
};
class Fieldset {
public:
    template <class... Children>
    Fieldset(std::string legend,Children&&... fields) : legend_(std::move(legend)) {
        (children_.push_back(make_spec(std::forward<Children>(fields))),...);
    }
    Fieldset&& enabled(Binding<bool> value) &&;
    Fieldset&& enabled(State<bool>& value) &&;
    Fieldset&& read_only(Binding<bool> value) &&;
    Fieldset&& read_only(State<bool>& value) &&;
    Fieldset&& description(std::string value) &&;
    Fieldset&& style(FieldsetStyle value) &&;
    Spec spec() &&;
private:
    std::string legend_;
    std::string description_;
    FieldsetStyle style_;
    std::optional<Binding<bool>> enabled_;
    std::optional<Binding<bool>> read_only_;
    std::vector<Spec> children_;
};
} // namespace ui
