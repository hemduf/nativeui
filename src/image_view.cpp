#include <nativeui/image_view.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ui {
namespace {
void validate_size(Size size) {
  if (!std::isfinite(size.w) || !std::isfinite(size.h) || size.w < 0.0f ||
      size.h < 0.0f)
    throw std::invalid_argument(
        "ImageView size must be finite and nonnegative");
}
void validate_scale(double value) {
  if (!std::isfinite(value) || value <= 0.0)
    throw std::invalid_argument(
        "ImageView pixel scale must be finite and positive");
}
float logical_size(double value) noexcept {
  return static_cast<float>(std::clamp(
      value, 0.0, static_cast<double>(std::numeric_limits<float>::max())));
}
Size intrinsic_size(const ImageViewSource &source,
                    double pixel_scale) noexcept {
  return std::visit(
      [pixel_scale](const auto &value) -> Size {
        if (!value.valid())
          return {};
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Image>) {
          const auto pixels = value.size();
          return {logical_size(static_cast<double>(pixels.w) / pixel_scale),
                  logical_size(static_cast<double>(pixels.h) / pixel_scale)};
        } else {
          return value.intrinsic_size();
        }
      },
      source);
}
bool valid_bounds(Rect bounds) noexcept {
  return bounds.w > 0.0f && bounds.h > 0.0f && std::isfinite(bounds.x) &&
         std::isfinite(bounds.y) && std::isfinite(bounds.w) &&
         std::isfinite(bounds.h);
}
// Adapt the prepared SVG resource to all fits using only the backend-neutral
// Painter transform/clip seam. Resource decoding and DOM ownership remain in
// SvgIcon.
void paint_svg_fit(Painter &painter, const SvgIcon &icon, Rect bounds,
                   ImageFit fit) {
  const auto natural = icon.intrinsic_size();
  if (!icon.valid() || natural.w <= 0.0f || natural.h <= 0.0f ||
      !valid_bounds(bounds))
    return;
  if (fit == ImageFit::Contain) {
    detail::draw_svg(painter, icon, bounds);
    return;
  }
  const double x_scale = static_cast<double>(bounds.w) / natural.w;
  const double y_scale = static_cast<double>(bounds.h) / natural.h;
  const double scale = std::max(x_scale, y_scale);
  const double x = fit == ImageFit::Fill ? x_scale : scale;
  const double y = fit == ImageFit::Fill ? y_scale : scale;
  if (!std::isfinite(x) || !std::isfinite(y) ||
      x > std::numeric_limits<float>::max() ||
      y > std::numeric_limits<float>::max())
    return;
  const double offset_x = static_cast<double>(bounds.x) +
                          (static_cast<double>(bounds.w) - natural.w * x) * 0.5;
  const double offset_y = static_cast<double>(bounds.y) +
                          (static_cast<double>(bounds.h) - natural.h * y) * 0.5;
  if (std::abs(offset_x) > std::numeric_limits<float>::max() ||
      std::abs(offset_y) > std::numeric_limits<float>::max())
    return;
  auto state = painter.scoped_state();
  painter.translate(static_cast<float>(offset_x), static_cast<float>(offset_y));
  painter.scale(static_cast<float>(x), static_cast<float>(y));
  detail::draw_svg(painter, icon, {0.0f, 0.0f, natural.w, natural.h});
}
} // namespace
namespace detail {
ImageViewComponent::ImageViewComponent(
    ImageViewSource source, std::optional<Binding<ImageViewSource>> binding,
    ImageFit fit, std::optional<Size> size, double pixel_scale, std::string alt,
    bool decorative)
    : source_(std::move(source)), binding_(std::move(binding)), fit_(fit),
      size_(size), pixel_scale_(pixel_scale), alt_(std::move(alt)),
      decorative_(decorative) {
  validate_scale(pixel_scale);
  if (size_)
    validate_size(*size_);
}
ImageViewSource ImageViewComponent::current_source() const {
  return binding_ ? binding_->get() : source_;
}
Size ImageViewComponent::measure(const std::vector<ChildMetrics> &) const {
  return size_.value_or(intrinsic_size(current_source(), pixel_scale_));
}
SemanticInfo ImageViewComponent::semantics() const {
  SemanticInfo info;
  if (!decorative_) {
    info.role = SemanticRole::Image;
    info.name = alt_;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only();
  }
  return info;
}
void ImageViewComponent::mount(MountContext &context) {
  if (!binding_)
    return;
  auto previous =
      std::make_shared<Size>(intrinsic_size(current_source(), pixel_scale_));
  auto invalidate = context.invalidator();
  auto invalidate_layout = context.layout_invalidator();
  subscription_ = binding_->observe(
      [previous = std::move(previous), pixel_scale = pixel_scale_,
       fixed_size = size_.has_value(), invalidate = std::move(invalidate),
       invalidate_layout =
           std::move(invalidate_layout)](const ImageViewSource &source) {
        const auto next = intrinsic_size(source, pixel_scale);
        const bool size_changed =
            previous->w != next.w || previous->h != next.h;
        *previous = next;
        if (!fixed_size && size_changed)
          invalidate_layout();
        else
          invalidate();
      });
}
void ImageViewComponent::unmount(LifecycleContext &) { subscription_.reset(); }
EventResult ImageViewComponent::input(const InputEvent &, InputContext &) {
  return EventResult::Ignored;
}
void ImageViewComponent::paint(PaintContext &context) const {
  const auto source = current_source();
  const auto bounds = context.bounds();
  if (!valid_bounds(bounds))
    return;
  auto &painter = context.painter();
  auto clip = painter.scoped_clip(bounds);
  std::visit(
      [&](const auto &value) {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Image>)
          draw_image(painter, value, bounds, fit_);
        else
          paint_svg_fit(painter, value, bounds, fit_);
      },
      source);
}
} // namespace detail
ImageView::ImageView(Image value) : source_(std::move(value)) {}
ImageView::ImageView(SvgIcon value) : source_(std::move(value)) {}
ImageView::ImageView(Binding<Source> value) : binding_(std::move(value)) {}
ImageView::ImageView(State<Source> &value) : ImageView(value.binding()) {}
ImageView &&ImageView::fit(ImageFit value) && {
  fit_ = value;
  return std::move(*this);
}
ImageView &&ImageView::size(Size value) && {
  validate_size(value);
  size_ = value;
  return std::move(*this);
}
ImageView &&ImageView::pixel_scale(double value) && {
  validate_scale(value);
  pixel_scale_ = value;
  return std::move(*this);
}
ImageView &&ImageView::alt(std::string value) && {
  alt_ = std::move(value);
  return std::move(*this);
}
ImageView &&ImageView::decorative(bool value) && {
  decorative_ = value;
  return std::move(*this);
}
Spec ImageView::spec() && {
  return {[source = std::move(source_), binding = std::move(binding_),
           fit = fit_, size = size_, pixel_scale = pixel_scale_,
           alt = std::move(alt_), decorative = decorative_] {
            return std::make_unique<detail::ImageViewComponent>(
                source, binding, fit, size, pixel_scale, alt, decorative);
          },
          {}};
}
} // namespace ui
