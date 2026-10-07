#pragma once

#include <nativeui/component.hpp>
#include <nativeui/text.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ui {
enum class FormLayout { Aligned, Stacked, Responsive };
struct FormStyle {
    float row_gap{8.0f};
    float column_gap{12.0f};
    TextAlign label_alignment{TextAlign::Right};
    std::string accessible_name;
};
class Form {
public:
    template <class... Children>
    explicit Form(Children&&... fields) { (children_.push_back(make_spec(std::forward<Children>(fields))),...); }
    Form&& layout(FormLayout value) &&;
    Form&& stacked_below(double logical_width) &&;
    Form&& on_submit(std::function<void()> callback) &&;
    Form&& on_cancel(std::function<void()> callback) &&;
    Form&& style(FormStyle value) &&;
    Spec spec() &&;
private:
    FormLayout layout_{FormLayout::Aligned};
    double threshold_{360.0};
    FormStyle style_;
    std::function<void()> submit_;
    std::function<void()> cancel_;
    std::vector<Spec> children_;
};
} // namespace ui
