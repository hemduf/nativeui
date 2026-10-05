#pragma once

#include <nativeui/form.hpp>
#include <nativeui/field.hpp>
#include <nativeui/fieldset.hpp>
#include <nativeui/theme.hpp>

#include <cstdint>
#include <memory>

namespace ui::detail {
struct ParagraphLine { std::size_t begin{}; std::size_t end{}; float width{}; };
struct Paragraph {
    std::vector<ParagraphLine> lines;
    float width{};
    float height{};
    float line_height{};
    float first_baseline{};
};
[[nodiscard]] Paragraph wrap_form_text(std::string_view text,const TextStyle& style,float width);
void paint_form_text(Painter& painter,Rect bounds,std::string_view text,const TextStyle& style,
                     const Paragraph& paragraph,TextAlign alignment=TextAlign::Left);
[[nodiscard]] TextStyle form_text_style(const Theme& theme,const std::optional<TextStyle>& explicit_style,
                                        Color color,bool label=false);
[[nodiscard]] bool form_font_metrics_equal(const TextStyle& first,const TextStyle& second) noexcept;
void validate_form_text_style(const std::optional<TextStyle>& style);
void validate_form_extent(float value);

struct FormLabelRecord {
    std::uint64_t registration{};
    std::string label;
    std::shared_ptr<const TextStyle> text_style;
    float leading_inset{};
    float trailing_inset{};
    float control_minimum{};
    bool participates{true};
};
class FormContext {
public:
    FormContext(FormLayout layout,double threshold,FormStyle style);
    [[nodiscard]] std::uint64_t add(const std::shared_ptr<FormLabelRecord>& record);
    void remove(std::uint64_t registration) noexcept;
    void begin_pass(const Constraints& constraints,std::size_t pass) const;
    [[nodiscard]] bool stacked() const noexcept { return stacked_; }
    [[nodiscard]] bool collecting_minimum() const noexcept { return pass_ == 0; }
    [[nodiscard]] float label_width() const noexcept { return label_width_; }
    [[nodiscard]] bool has_labels() const noexcept { return has_labels_; }
    [[nodiscard]] const FormStyle& style() const noexcept { return style_; }
    void invalidate_layout() const;
    std::function<void()> layout_invalidator;
private:
    struct Participant { std::uint64_t registration; std::weak_ptr<FormLabelRecord> record; };
    FormLayout layout_;
    double threshold_;
    FormStyle style_;
    std::vector<Participant> participants_;
    std::uint64_t next_registration_{1};
    mutable bool stacked_{};
    mutable std::size_t pass_{};
    mutable float label_width_{};
    mutable bool has_labels_{};
};
class FormBindable {
public:
    virtual ~FormBindable() = default;
    virtual void bind_form(std::weak_ptr<FormContext> context,float leading_inset,float trailing_inset) = 0;
};
} // namespace ui::detail
