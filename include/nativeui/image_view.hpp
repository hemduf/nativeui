#pragma once

#include <nativeui/component.hpp>
#include <nativeui/image.hpp>
#include <nativeui/state.hpp>
#include <nativeui/svg.hpp>

#include <optional>
#include <string>
#include <variant>

namespace ui {
using ImageViewSource = std::variant<Image, SvgIcon>;
namespace detail {
class ImageViewComponent final : public Component {
public:
  ImageViewComponent(ImageViewSource source,
                     std::optional<Binding<ImageViewSource>> binding,
                     ImageFit fit, std::optional<Size> size, double pixel_scale,
                     std::string alt, bool decorative);
  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] SemanticInfo semantics() const override;
  void mount(MountContext &context) override;
  void unmount(LifecycleContext &) override;
  EventResult input(const InputEvent &, InputContext &) override;
  void paint(PaintContext &context) const override;

private:
  [[nodiscard]] ImageViewSource current_source() const;
  ImageViewSource source_;
  std::optional<Binding<ImageViewSource>> binding_;
  ImageFit fit_;
  std::optional<Size> size_;
  double pixel_scale_;
  std::string alt_;
  bool decorative_;
  Binding<ImageViewSource>::Subscription subscription_;
};
} // namespace detail
class ImageView {
public:
  using Source = ImageViewSource;
  explicit ImageView(Image value);
  explicit ImageView(SvgIcon value);
  explicit ImageView(Binding<Source> value);
  explicit ImageView(State<Source> &value);
  ImageView &&fit(ImageFit value) &&;
  ImageView &&size(Size value) &&;
  ImageView &&pixel_scale(double value) &&;
  ImageView &&alt(std::string value) &&;
  ImageView &&decorative(bool value = true) &&;
  Spec spec() &&;

private:
  Source source_;
  std::optional<Binding<Source>> binding_;
  ImageFit fit_{ImageFit::Contain};
  std::optional<Size> size_;
  double pixel_scale_{1.0};
  std::string alt_;
  bool decorative_{true};
};
} // namespace ui
