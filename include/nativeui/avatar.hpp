#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/image.hpp>
#include <nativeui/state.hpp>

#include <memory>
#include <optional>
#include <string>

namespace ui {
struct AvatarStyle {
  std::optional<TextStyle> initials_text;
  std::optional<Color> background;
  std::optional<Color> foreground;
  double border_width{};
  std::optional<Color> border;
};
namespace detail {
struct AvatarSnapshot;
class AvatarComponent final : public Component, public ThemeBinding {
public:
  AvatarComponent(std::string name,
                  std::optional<Binding<std::string>> name_binding, Image image,
                  std::optional<Binding<Image>> image_binding, double diameter,
                  std::optional<std::string> initials, AvatarStyle style);
  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] SemanticInfo semantics() const override;
  void mount(MountContext &) override;
  void unmount(LifecycleContext &) override;
  EventResult input(const InputEvent &, InputContext &) override;
  void paint(PaintContext &) const override;

private:
  void sync_name() const;
  std::optional<Binding<std::string>> name_binding_;
  Image image_;
  std::optional<Binding<Image>> image_binding_;
  double diameter_{32.0};
  std::optional<std::string> initials_;
  AvatarStyle style_;
  std::shared_ptr<AvatarSnapshot> snapshot_;
  Binding<std::string>::Subscription name_subscription_;
  Binding<Image>::Subscription image_subscription_;
};
} // namespace detail
class Avatar {
public:
  explicit Avatar(std::string name);
  explicit Avatar(Binding<std::string> name);
  explicit Avatar(State<std::string> &name);
  Avatar &&image(Image value) &&;
  Avatar &&image(Binding<Image> value) &&;
  Avatar &&image(State<Image> &value) &&;
  Avatar &&size(double diameter) &&;
  Avatar &&initials(std::string value) &&;
  Avatar &&style(AvatarStyle value) &&;
  Spec spec() &&;

private:
  std::string name_;
  std::optional<Binding<std::string>> name_binding_;
  Image image_;
  std::optional<Binding<Image>> image_binding_;
  double diameter_{32.0};
  std::optional<std::string> initials_;
  AvatarStyle style_;
};
} // namespace ui
