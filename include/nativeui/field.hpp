#pragma once

#include <nativeui/component.hpp>
#include <nativeui/state.hpp>
#include <nativeui/text.hpp>

#include <optional>
#include <string>
#include <utility>

namespace ui {
struct FieldStyle {
    float label_gap{6.0f};
    float description_gap{4.0f};
    float error_gap{4.0f};
    float error_indicator_width{2.0f};
    std::optional<TextStyle> label_text;
    std::optional<TextStyle> description_text;
    std::optional<TextStyle> error_text;
    std::optional<Color> error_indicator_color;
};
class Field {
public:
    template <class Child>
    Field(std::string label,Child&& control) : label_(std::move(label)),child_(make_spec(std::forward<Child>(control))) {}
    Field&& description(std::string text) &&;
    Field&& description(Binding<std::string> text) &&;
    Field&& description(State<std::string>& text) &&;
    Field&& error(std::string text) &&;
    Field&& error(Binding<std::string> text) &&;
    Field&& error(State<std::string>& text) &&;
    Field&& target(std::string descendant_key) &&;
    Field&& required(bool value=true) &&;
    Field&& style(FieldStyle value) &&;
    Spec spec() &&;
private:
    std::string label_;
    Spec child_;
    std::string description_;
    std::string error_;
    std::optional<Binding<std::string>> description_binding_;
    std::optional<Binding<std::string>> error_binding_;
    std::string target_;
    bool required_{};
    FieldStyle style_;
};
} // namespace ui
