#include "detail/widget_unicode.hpp"
#include <nativeui/avatar.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ui {
namespace {
void validate_metric(double value) {
  if (!std::isfinite(value) || value < 0.0)
    throw std::invalid_argument(
        "Avatar metrics must be finite and nonnegative");
}
Color validated_color(Color value) {
  if (!std::isfinite(value.r) || !std::isfinite(value.g) ||
      !std::isfinite(value.b) || !std::isfinite(value.a))
    throw std::invalid_argument("Avatar color components must be finite");
  return {std::clamp(value.r, 0.0f, 1.0f), std::clamp(value.g, 0.0f, 1.0f),
          std::clamp(value.b, 0.0f, 1.0f), std::clamp(value.a, 0.0f, 1.0f)};
}
void validate_style(AvatarStyle &value) {
  validate_metric(value.border_width);
  if (value.initials_text)
    validate_metric(value.initials_text->size);
  if (value.background)
    value.background = validated_color(*value.background);
  if (value.foreground)
    value.foreground = validated_color(*value.foreground);
  if (value.border)
    value.border = validated_color(*value.border);
}
float logical(double value) noexcept {
  return static_cast<float>(
      std::min(value, static_cast<double>(std::numeric_limits<float>::max())));
}
Color name_background(std::string_view name) noexcept {
  std::uint32_t hash = 2166136261U;
  for (const unsigned char value : name) {
    hash ^= value;
    hash *= 16777619U;
  }
  const double hue = static_cast<double>(hash % 360U) / 360.0;
  constexpr double saturation = 0.45;
  constexpr double lightness = 0.55;
  constexpr double q = lightness + saturation - lightness * saturation;
  constexpr double p = 2.0 * lightness - q;
  const auto channel = [=](double t) {
    if (t < 0.0)
      t += 1.0;
    if (t > 1.0)
      t -= 1.0;
    if (t < 1.0 / 6.0)
      return p + (q - p) * 6.0 * t;
    if (t < 0.5)
      return q;
    if (t < 2.0 / 3.0)
      return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
    return p;
  };
  return {static_cast<float>(channel(hue + 1.0 / 3.0)),
          static_cast<float>(channel(hue)),
          static_cast<float>(channel(hue - 1.0 / 3.0)), 1.0f};
}
Color contrast_color(Color background) noexcept {
  const auto linear = [](float value) {
    return value <= 0.04045f
               ? static_cast<double>(value) / 12.92
               : std::pow((static_cast<double>(value) + 0.055) / 1.055, 2.4);
  };
  const double luminance = 0.2126 * linear(background.r) +
                           0.7152 * linear(background.g) +
                           0.0722 * linear(background.b);
  return (luminance + 0.05) / 0.05 >= 1.05 / (luminance + 0.05)
             ? Color{0, 0, 0, 1}
             : Color{1, 1, 1, 1};
}
} // namespace
namespace detail {
struct AvatarData {
  std::string raw_name;
  std::string name;
  std::string initials;
  Color background;
  Color foreground;
};
struct AvatarSnapshot {
  AvatarData data;
  bool mounted{};
};
namespace {
AvatarData prepare_name(const std::string &value,
                        const std::optional<std::string> &initials,
                        const std::optional<Color> &background) {
  AvatarData data;
  data.raw_name = value;
  data.name = widget_repair_utf8(value);
  data.initials =
      initials ? widget_repair_utf8(*initials) : widget_initials(data.name);
  data.background = background.value_or(name_background(data.name));
  data.foreground = contrast_color(data.background);
  return data;
}
} // namespace
AvatarComponent::AvatarComponent(
    std::string name, std::optional<Binding<std::string>> name_binding,
    Image image, std::optional<Binding<Image>> image_binding, double diameter,
    std::optional<std::string> initials, AvatarStyle style)
    : name_binding_(std::move(name_binding)), image_(std::move(image)),
      image_binding_(std::move(image_binding)), diameter_(diameter),
      initials_(std::move(initials)), style_(std::move(style)),
      snapshot_(std::make_shared<AvatarSnapshot>()) {
  validate_metric(diameter_);
  validate_style(style_);
  snapshot_->data = prepare_name(name_binding_ ? name_binding_->get() : name,
                                 initials_, style_.background);
}
void AvatarComponent::sync_name() const {
  if (name_binding_ && name_binding_->get() != snapshot_->data.raw_name) {
    auto prepared =
        prepare_name(name_binding_->get(), initials_, style_.background);
    snapshot_->data = std::move(prepared);
  }
}
Size AvatarComponent::measure(const std::vector<ChildMetrics> &) const {
  const float size = logical(diameter_);
  return {size, size};
}
SemanticInfo AvatarComponent::semantics() const {
  sync_name();
  SemanticInfo info;
  info.role = SemanticRole::Image;
  info.name = snapshot_->data.name;
  if (info.name.empty())
    info.description = "avatar sans nom";
  info.enabled = effective_enabled();
  info.read_only = effective_read_only();
  return info;
}
void AvatarComponent::mount(MountContext &context) {
  snapshot_->mounted = true;
  if (name_binding_) {
    const std::weak_ptr<AvatarSnapshot> weak = snapshot_;
    auto invalidate = context.invalidator();
    name_subscription_ = name_binding_->observe(
        [weak, initials = initials_, background = style_.background,
         invalidate = std::move(invalidate)](const std::string &value) {
          const auto current = weak.lock();
          if (!current || !current->mounted)
            return;
          auto prepared = prepare_name(value, initials, background);
          current->data = std::move(prepared);
          invalidate();
        });
  }
  if (image_binding_) {
    auto invalidate = context.invalidator();
    image_subscription_ = image_binding_->observe(
        [invalidate = std::move(invalidate)](const Image &) { invalidate(); });
  }
}
void AvatarComponent::unmount(LifecycleContext &) {
  snapshot_->mounted = false;
  name_subscription_.reset();
  image_subscription_.reset();
}
EventResult AvatarComponent::input(const InputEvent &, InputContext &) {
  return EventResult::Ignored;
}
void AvatarComponent::paint(PaintContext &context) const {
  sync_name();
  const auto data = snapshot_->data;
  const auto image = image_binding_ ? image_binding_->get() : image_;
  const auto bounds = context.bounds();
  const float size = std::min({bounds.w, bounds.h, logical(diameter_)});
  if (size <= 0.0f || bounds.empty())
    return;
  const Rect disc{bounds.x + (bounds.w - size) * 0.5f,
                  bounds.y + (bounds.h - size) * 0.5f, size, size};
  auto &painter = context.painter();
  auto clip = painter.scoped_clip(disc, size * 0.5f);
  painter.fill_rounded_rect(disc, size * 0.5f, data.background);
  if (image.valid())
    draw_image(painter, image, disc, ImageFit::Cover);
  else {
    TextStyle text = style_.initials_text.value_or(TextStyle{});
    if (!style_.initials_text) {
      text.size = size * 0.4f;
      text.family = current_theme().typography.family;
      text.fallback_families = current_theme().typography.fallback_families;
      text.weight = current_theme().typography.control_weight;
      text.color = data.foreground;
    }
    // Foreground is its own Avatar option; typography never silently disables
    // the automatic contrast choice for a font-only override.
    text.color = style_.foreground.value_or(data.foreground);
    if (!effective_enabled())
      text.color.a *= 0.6f;
    text.align = TextAlign::Center;
    painter.text({disc.x + size * 0.5f, disc.y + size * 0.5f}, data.initials,
                 text);
  }
  if (style_.border_width > 0.0)
    painter.stroke_rounded_rect(
        disc, size * 0.5f, logical(style_.border_width),
        style_.border.value_or(current_theme().palette.border));
}
} // namespace detail
Avatar::Avatar(std::string name) : name_(std::move(name)) {}
Avatar::Avatar(Binding<std::string> name) : name_binding_(std::move(name)) {}
Avatar::Avatar(State<std::string> &name) : Avatar(name.binding()) {}
Avatar &&Avatar::image(Image value) && {
  image_ = std::move(value);
  image_binding_.reset();
  return std::move(*this);
}
Avatar &&Avatar::image(Binding<Image> value) && {
  image_binding_ = std::move(value);
  return std::move(*this);
}
Avatar &&Avatar::image(State<Image> &value) && {
  return std::move(*this).image(value.binding());
}
Avatar &&Avatar::size(double diameter) && {
  validate_metric(diameter);
  diameter_ = diameter;
  return std::move(*this);
}
Avatar &&Avatar::initials(std::string value) && {
  initials_ = std::move(value);
  return std::move(*this);
}
Avatar &&Avatar::style(AvatarStyle value) && {
  validate_style(value);
  style_ = std::move(value);
  return std::move(*this);
}
Spec Avatar::spec() && {
  return {[name = std::move(name_), name_binding = name_binding_,
           image = image_, image_binding = image_binding_, diameter = diameter_,
           initials = std::move(initials_), style = std::move(style_)] {
            return std::make_unique<detail::AvatarComponent>(
                name, name_binding, image, image_binding, diameter, initials,
                style);
          },
          {}};
}
} // namespace ui
