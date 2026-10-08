#include <nativeui/label.hpp>

namespace ui {

LabelComponent::LabelComponent(std::string text, TextStyle style)
    : text_(std::move(text)), style_(style) {}

Size LabelComponent::measure(const std::vector<ChildMetrics> &) const {
  const auto metrics = TextService::measure(text_, style_);
  return Size{metrics.width, metrics.height};
}

SemanticInfo LabelComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::Text;
  info.name = text_;
  info.text_value = text_;
  return info;
}

void LabelComponent::paint(PaintContext &context) const {
  detail::paint_text_in_rect(context.painter(), context.bounds(), text_,
                             style_);
}

Label::Label(std::string text) : text_(std::move(text)) {}

Label &&Label::size(float value) && {
  style_.size = std::max(0.0f, value);
  return std::move(*this);
}

Label &&Label::color(Color value) && {
  style_.color = value;
  return std::move(*this);
}

Label &&Label::align(TextAlign value) && {
  style_.align = value;
  return std::move(*this);
}

Label &&Label::weight(FontWeight value) && {
  style_.weight = value;
  return std::move(*this);
}

Label &&Label::bold(bool value) && {
  style_.weight = value ? FontWeight::Bold : FontWeight::Regular;
  return std::move(*this);
}

Label &&Label::slant(FontSlant value) && {
  style_.slant = value;
  return std::move(*this);
}

Label &&Label::italic(bool value) && {
  style_.slant = value ? FontSlant::Italic : FontSlant::Upright;
  return std::move(*this);
}

Label &&Label::family(std::string value) && {
  style_.family = std::move(value);
  return std::move(*this);
}

Label &&Label::fallback_families(std::vector<std::string> value) && {
  style_.fallback_families = std::move(value);
  return std::move(*this);
}

Label &&Label::style(TextStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}

Spec Label::spec() && {
  auto text = std::move(text_);
  auto style = std::move(style_);
  return Spec{[text = std::move(text), style = std::move(style)]() mutable {
                return std::make_unique<LabelComponent>(std::move(text),
                                                        std::move(style));
              },
              {}};
}

} // namespace ui

namespace ui::detail {
void paint_text_in_rect(Painter &painter, Rect bounds, std::string_view text,
                        const TextStyle &style) {
  Point anchor{};
  anchor.y = bounds.y + bounds.h * 0.5f;
  switch (style.align) {
  case TextAlign::Left:
    anchor.x = bounds.x;
    break;
  case TextAlign::Center:
    anchor.x = bounds.x + bounds.w * 0.5f;
    break;
  case TextAlign::Right:
    anchor.x = bounds.x + bounds.w;
    break;
  }
  painter.text(anchor, text, style);
}
} // namespace ui::detail
