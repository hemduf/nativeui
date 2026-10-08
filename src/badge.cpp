#include <nativeui/badge.hpp>
#include <nativeui/detail/widget_text_paint.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ui {
namespace {
void validate_style(const BadgeStyle &style) {
  for (const double value :
       {style.horizontal_padding, style.vertical_padding, style.minimum_width,
        static_cast<double>(style.text.size)}) {
    if (!std::isfinite(value) || value < 0.0)
      throw std::invalid_argument(
          "Badge dimensions must be finite and nonnegative");
  }
}
float logical_size(double value) noexcept {
  return static_cast<float>(std::clamp(
      value, 0.0, static_cast<double>(std::numeric_limits<float>::max())));
}
} // namespace
BadgeStyle::BadgeStyle() : background(colors::text) {
  text.size = 12.0f;
  background.a = 0.1f;
}

namespace detail {
BadgeComponent::BadgeComponent(std::string text,
                               std::optional<Binding<std::string>> source,
                               std::optional<BadgeStyle> style)
    : text_(std::move(text)), source_(std::move(source)),
      style_(std::move(style)) {
  if (style_)
    validate_style(*style_);
}
std::string BadgeComponent::current_text() const {
  return source_ ? source_->get() : text_;
}
BadgeStyle BadgeComponent::resolved_style() const {
  BadgeStyle style;
  if (style_)
    style = *style_;
  else {
    style.text.color = current_theme().palette.text;
    style.text.family = current_theme().typography.family;
    style.text.fallback_families = current_theme().typography.fallback_families;
    style.background = current_theme().palette.text;
    style.background.a *= 0.1f;
  }
  if (!effective_enabled())
    style.text.color = current_theme().palette.disabled;
  return style;
}
Size BadgeComponent::measure(const std::vector<ChildMetrics> &) const {
  const auto style = resolved_style();
  const auto metrics = TextService::measure(current_text(), style.text);
  return {logical_size(std::max(style.minimum_width,
                                static_cast<double>(metrics.width) +
                                    style.horizontal_padding * 2.0)),
          logical_size(static_cast<double>(metrics.height) +
                       style.vertical_padding * 2.0)};
}
SemanticInfo BadgeComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::Text;
  info.name = current_text();
  info.text_value = info.name;
  info.enabled = effective_enabled();
  info.read_only = effective_read_only();
  return info;
}
void BadgeComponent::mount(MountContext &context) {
  if (!source_)
    return;
  auto invalidate = context.layout_invalidator();
  subscription_ = source_->observe([invalidate = std::move(invalidate)](
                                       const std::string &) { invalidate(); });
}
void BadgeComponent::unmount(LifecycleContext &) { subscription_.reset(); }
EventResult BadgeComponent::input(const InputEvent &, InputContext &) {
  return EventResult::Ignored;
}
void BadgeComponent::paint(PaintContext &context) const {
  const auto bounds = context.bounds();
  if (bounds.empty())
    return;
  const auto style = resolved_style();
  const auto text = current_text();
  auto &painter = context.painter();
  auto clip = painter.scoped_clip(bounds);
  painter.fill_rounded_rect(bounds, bounds.h * 0.5f, style.background);
  const float padding =
      std::min(bounds.w * 0.5f, logical_size(style.horizontal_padding));
  auto text_style = style.text;
  text_style.align = TextAlign::Center;
  paint_text_in_rect(painter,
                     {bounds.x + padding, bounds.y,
                      std::max(0.0f, bounds.w - padding * 2.0f), bounds.h},
                     text, text_style);
}
} // namespace detail

Badge::Badge(std::string text) : text_(std::move(text)) {}
Badge::Badge(Binding<std::string> text) : source_(std::move(text)) {}
Badge::Badge(State<std::string> &text) : Badge(text.binding()) {}
Badge &&Badge::style(BadgeStyle value) && {
  validate_style(value);
  style_ = std::move(value);
  return std::move(*this);
}
Spec Badge::spec() && {
  return {[text = std::move(text_), source = std::move(source_),
           style = std::move(style_)] {
            return std::make_unique<detail::BadgeComponent>(text, source,
                                                            style);
          },
          {}};
}
} // namespace ui
