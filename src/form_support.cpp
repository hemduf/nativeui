#include "detail/form_kernel.hpp"
#include "detail/layout_support.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ui::detail {
namespace {
std::size_t next_scalar(std::string_view bytes,std::size_t offset) noexcept {
    if (offset >= bytes.size()) return bytes.size();
    const auto byte = static_cast<unsigned char>(bytes[offset]);
    const std::size_t expected = byte < 0x80 ? 1 : (byte & 0xE0) == 0xC0 ? 2 :
        (byte & 0xF0) == 0xE0 ? 3 : (byte & 0xF8) == 0xF0 ? 4 : 1;
    if (expected == 1) return offset+1;
    const auto suffix = bytes.substr(offset);
    const auto prefix = text::utf8_prefix(suffix,expected);
    return offset+(prefix && prefix->size() == expected ? expected : 1);
}
}
Paragraph wrap_form_text(std::string_view value,const TextStyle& style,float available) {
    Paragraph result;
    if (value.empty()) return result;
    const auto font = TextService::measure("Mg",style);
    result.line_height = font.height;
    result.first_baseline = font.height*0.5f-(font.ascent+font.descent)*0.5f;
    const auto width = std::isfinite(available) ? std::max(0.0f,available) : kUnboundedExtent;
    std::size_t begin = 0;
    while (begin < value.size()) {
        std::size_t end = begin,last_space = begin;
        float measured = 0.0f;
        bool hard_break = false;
        while (end < value.size()) {
            if (value[end] == '\n' || value[end] == '\r') { hard_break = true; break; }
            const auto next = next_scalar(value,end);
            const auto candidate = TextService::measure(value.substr(begin,next-begin),style).width;
            if (candidate > width && end > begin) {
                if (last_space > begin) end = last_space;
                break;
            }
            end = next; measured = candidate;
            if (value[end-1] == ' ' || value[end-1] == '\t') last_space = end;
            if (candidate > width) break; // One scalar remains drawable under clipping.
        }
        if (end > begin) measured = TextService::measure(value.substr(begin,end-begin),style).width;
        result.lines.push_back({begin,end,measured});
        result.width = std::max(result.width,measured);
        begin = end;
        if (hard_break) {
            const auto newline = value[begin++];
            if (newline == '\r' && begin < value.size() && value[begin] == '\n') ++begin;
            if (begin == value.size()) result.lines.push_back({begin,begin,0.0f});
        } else {
            while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t')) ++begin;
        }
    }
    result.height = saturating_extent(static_cast<double>(result.lines.size())*result.line_height);
    return result;
}
void paint_form_text(Painter& painter,Rect bounds,std::string_view text_value,const TextStyle& style,
                     const Paragraph& paragraph,TextAlign alignment) {
    auto text_style = style; text_style.align = TextAlign::Left;
    auto clip = painter.scoped_clip(bounds);
    for (std::size_t i = 0; i < paragraph.lines.size(); ++i) {
        const auto& line = paragraph.lines[i];
        float offset = 0.0f;
        if (alignment == TextAlign::Right) offset = std::max(0.0f,bounds.w-line.width);
        else if (alignment == TextAlign::Center) offset = std::max(0.0f,(bounds.w-line.width)*0.5f);
        const Point point{saturating_coordinate(static_cast<double>(bounds.x)+offset),
            saturating_coordinate(static_cast<double>(bounds.y)+(static_cast<double>(i)+0.5)*paragraph.line_height)};
        painter.text(point,text_value.substr(line.begin,line.end-line.begin),text_style);
    }
}
TextStyle form_text_style(const Theme& theme,const std::optional<TextStyle>& explicit_style,Color color,bool label) {
    if (explicit_style) return *explicit_style;
    TextStyle result;
    result.size = label ? theme.typography.label_size : theme.typography.control_size;
    result.weight = label ? theme.typography.label_weight : theme.typography.control_weight;
    result.slant = theme.typography.slant;
    result.family = theme.typography.family; result.fallback_families = theme.typography.fallback_families;
    result.color = color;
    return result;
}
bool form_font_metrics_equal(const TextStyle& first,const TextStyle& second) noexcept {
    return first.size == second.size && first.weight == second.weight && first.slant == second.slant &&
        first.family == second.family && first.fallback_families == second.fallback_families;
}
void validate_form_text_style(const std::optional<TextStyle>& style) {
    if (style && (!std::isfinite(style->size) || style->size <= 0.0f))
        throw std::invalid_argument("Form text size must be finite and positive");
}
void validate_form_extent(float value) {
    if (!std::isfinite(value) || value < 0.0f)
        throw std::invalid_argument("Form extents must be finite and nonnegative");
}
FormContext::FormContext(FormLayout layout,double threshold,FormStyle style)
    : layout_(layout),threshold_(threshold),style_(std::move(style)) {}
std::uint64_t FormContext::add(const std::shared_ptr<FormLabelRecord>& record) {
    if (next_registration_ == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Form registration exhausted");
    const auto id = next_registration_;
    participants_.push_back({id,record}); ++next_registration_;
    return id;
}
void FormContext::remove(std::uint64_t registration) noexcept {
    std::erase_if(participants_,[registration](const auto& participant) { return participant.registration == registration; });
}
void FormContext::begin_pass(const Constraints& constraints,std::size_t pass) const {
    std::vector<std::shared_ptr<FormLabelRecord>> snapshot;
    snapshot.reserve(participants_.size());
    for (const auto& participant : participants_)
        if (const auto record = participant.record.lock(); record && record->participates) snapshot.push_back(record);
    double label = 0.0,control = 0.0; bool has_labels=false;
    for (const auto& record : snapshot) {
        label=std::max(label,static_cast<double>(record->leading_inset));
        if (record->text_style && !record->label.empty()) {
            label = std::max(label,static_cast<double>(TextService::measure(record->label,*record->text_style).width)+record->leading_inset);
            has_labels=true;
        }
        control = std::max(control,static_cast<double>(record->control_minimum)+record->trailing_inset);
    }
    const bool stacked = layout_ == FormLayout::Stacked ||
        (layout_ == FormLayout::Responsive && constraints.bounded_width() && constraints.max.w < threshold_);
    if (pass != 0 && constraints.bounded_width()) label = std::min(label,
        std::max(0.0,static_cast<double>(constraints.max.w)-control-(has_labels?style_.column_gap:0.0f)));
    label_width_ = saturating_extent(label); stacked_ = stacked; pass_ = pass; has_labels_=has_labels;
}
void FormContext::invalidate_layout() const { if (layout_invalidator) layout_invalidator(); }
} // namespace ui::detail
