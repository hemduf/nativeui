#include <nativeui/header.hpp>

#include <stdexcept>

namespace ui {
namespace {
void validate_style(const HeaderStyle &style) {
  for (const double value : {style.padding, style.gap, style.border_width,
                             static_cast<double>(style.title.size),
                             static_cast<double>(style.subtitle.size)}) {
    if (!std::isfinite(value) || value < 0.0)
      throw std::invalid_argument(
          "Header dimensions must be finite and nonnegative");
  }
}

float logical_size(double value) noexcept {
  return static_cast<float>(std::clamp(
      value, 0.0, static_cast<double>(std::numeric_limits<float>::max())));
}
} // namespace

HeaderStyle::HeaderStyle() {
  title.size = 21.0f;
  subtitle.size = 11.0f;
  subtitle.color = colors::textMuted;
}

HeaderComponent::HeaderComponent(std::string title)
    : HeaderComponent(std::move(title), std::nullopt, std::nullopt) {}

HeaderComponent::HeaderComponent(std::string title,
                                 std::optional<std::string> subtitle,
                                 std::optional<HeaderStyle> style)
    : title_(std::move(title)), style_(std::move(style)),
      legacy_(!subtitle.has_value() && !style_.has_value()) {
  if (subtitle)
    subtitle_ = std::move(*subtitle);
  if (style_)
    validate_style(*style_);
}

HeaderStyle HeaderComponent::resolved_style() const {
  if (style_)
    return *style_;
  HeaderStyle style;
  style.title.color = current_theme().palette.text;
  style.subtitle.color = current_theme().palette.muted_text;
  style.border_color = current_theme().palette.border;
  style.title.family = current_theme().typography.family;
  style.title.fallback_families = current_theme().typography.fallback_families;
  style.subtitle.family = style.title.family;
  style.subtitle.fallback_families = style.title.fallback_families;
  return style;
}

Size HeaderComponent::measure(const std::vector<ChildMetrics> &) const {
  if (legacy_)
    return {640.0f, 70.0f};
  const auto style = resolved_style();
  const auto title = TextService::measure(title_, style.title);
  const auto subtitle = subtitle_.empty()
                            ? TextMetrics{}
                            : TextService::measure(subtitle_, style.subtitle);
  const double gap = subtitle_.empty() ? 0.0 : style.gap;
  return {
      logical_size(std::max(title.width, subtitle.width) + style.padding * 2.0),
      logical_size(title.height + subtitle.height + gap + style.padding * 2.0 +
                   style.border_width)};
}

SemanticInfo HeaderComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::Group;
  info.name = title_;
  info.description = subtitle_;
  return info;
}

void HeaderComponent::paint(PaintContext &p) const {
  const auto b = p.bounds();
  auto &painter = p.painter();
  auto clip = painter.scoped_clip(b);
  if (legacy_) {
    TextStyle title_style{};
    title_style.size = 21.0f;
    title_style.color = colors::text;
    TextStyle subtitle_style{};
    subtitle_style.size = 11.0f;
    subtitle_style.color = colors::textMuted;
    detail::paint_text_in_rect(painter, {b.x, b.y + 10.0f, b.w, 26.0f}, title_,
                               title_style);
    detail::paint_text_in_rect(painter, {b.x, b.y + 43.0f, b.w, 12.0f},
                               subtitle_, subtitle_style);
    painter.line({b.x, b.y + b.h - 1.0f}, {b.x + b.w, b.y + b.h - 1.0f}, 1.0f,
                 colors::border);
    return;
  }
  const auto style = resolved_style();
  const float padding = std::min(logical_size(style.padding), b.w * 0.5f);
  const float width = std::max(0.0f, b.w - padding * 2.0f);
  const auto title_metrics = TextService::measure(title_, style.title);
  const float top = b.y + std::min(logical_size(style.padding), b.h * 0.5f);
  detail::paint_text_in_rect(painter,
                             {b.x + padding, top, width, title_metrics.height},
                             title_, style.title);
  if (!subtitle_.empty()) {
    const auto subtitle_metrics =
        TextService::measure(subtitle_, style.subtitle);
    const double subtitle_top =
        static_cast<double>(top) + title_metrics.height + style.gap;
    if (subtitle_top <= static_cast<double>(b.y) + b.h)
      detail::paint_text_in_rect(painter,
                                 {b.x + padding,
                                  static_cast<float>(subtitle_top), width,
                                  subtitle_metrics.height},
                                 subtitle_, style.subtitle);
  }
  const float border = std::min(logical_size(style.border_width), b.h);
  if (border > 0.0f)
    painter.fill_rounded_rect({b.x, b.y + b.h - border, b.w, border}, 0.0f,
                              style.border_color);
}

Header::Header(std::string title) : title_(std::move(title)) {}

Header &&Header::subtitle(std::string value) && {
  subtitle_ = std::move(value);
  return std::move(*this);
}

Header &&Header::style(HeaderStyle value) && {
  validate_style(value);
  style_ = std::move(value);
  return std::move(*this);
}

Spec Header::spec() && {
  auto title = std::move(title_);
  auto subtitle = std::move(subtitle_);
  auto style = std::move(style_);
  return {[title = std::move(title), subtitle = std::move(subtitle),
           style = std::move(style)]() mutable {
            return std::make_unique<HeaderComponent>(
                std::move(title), std::move(subtitle), std::move(style));
          },
          {}};
}
} // namespace ui
