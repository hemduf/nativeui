#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/state.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace ui {
struct RatingStyle {
  double star_size{24.0};
  double gap{4.0};
  double outline_width{1.0};
  std::optional<Color> empty;
  std::optional<Color> filled;
  std::optional<Color> preview;
  std::optional<Color> disabled;
  std::optional<Color> outline;
  std::optional<Color> focus_ring;
};
namespace detail {
class RatingComponent final : public Component, public ThemeBinding {
public:
  RatingComponent(std::string label, Binding<double> value, std::size_t stars,
                  double step, bool clearable, RatingStyle style);
  [[nodiscard]] bool focusable() const noexcept override;
  [[nodiscard]] bool cancel_capture_on_read_only() const noexcept override;
  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] SemanticInfo semantics() const override;
  void mount(MountContext &) override;
  void unmount(LifecycleContext &) override;
  void deactivate(LifecycleContext &) override;
  void focus_changed(bool, FocusContext &) override;
  EventResult input(const InputEvent &, InputContext &) override;
  void paint(PaintContext &) const override;

private:
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &) noexcept override;
  void layout_committed(Rect, Rect) noexcept override;
  void cancel_press() noexcept;
  [[nodiscard]] double effective() const noexcept;
  [[nodiscard]] std::optional<double> value_at(Rect, Point) const noexcept;
  std::string label_;
  Binding<double> value_;
  Binding<double>::Subscription subscription_;
  std::size_t stars_{};
  double step_{1.0};
  bool clearable_{true};
  RatingStyle style_;
  Path star_path_;
  std::optional<double> preview_;
  std::function<void()> release_pointer_;
  bool armed_{};
  bool focused_{};
};
} // namespace detail
class Rating {
public:
  Rating(std::string label, Binding<double> value, std::size_t stars = 5);
  Rating(std::string label, State<double> &value, std::size_t stars = 5);
  Rating &&step(double value) &&;
  Rating &&clearable(bool value = true) &&;
  Rating &&style(RatingStyle value) &&;
  Spec spec() &&;

private:
  std::string label_;
  Binding<double> value_;
  std::size_t stars_{5};
  double step_{1.0};
  bool clearable_{true};
  RatingStyle style_;
};
} // namespace ui
