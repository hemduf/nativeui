#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/gesture.hpp>
#include <nativeui/state.hpp>
#include <nativeui/text_edit.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <nativeui/detail/widget_text_paint.hpp>

namespace ui {

class CanvasComponent final : public Component {
public:
  using DrawCallback = std::function<void(CanvasContext2D &)>;
  using InputCallback =
      std::function<EventResult(const InputEvent &, CanvasInputContext &)>;

  CanvasComponent(Size size, DrawCallback draw, InputCallback input,
                  bool focusable);

  [[nodiscard]] bool focusable() const noexcept override;

  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;

  EventResult input(const InputEvent &event, InputContext &context) override;

  void paint(PaintContext &context) const override;

private:
  Size size_{};
  DrawCallback draw_;
  InputCallback input_;
  bool focusable_{};
};

class Canvas {
public:
  using DrawCallback = CanvasComponent::DrawCallback;
  using InputCallback = CanvasComponent::InputCallback;

  Canvas(Size size, DrawCallback draw);

  Canvas(float width, float height, DrawCallback draw);

  template <class Callback> Canvas &&on_input(Callback &&callback) && {
    using CallbackType = std::decay_t<Callback>;
    using Result = std::invoke_result_t<CallbackType &, const InputEvent &,
                                        CanvasInputContext &>;

    if constexpr (std::is_same_v<Result, EventResult>) {
      input_ = std::forward<Callback>(callback);
    } else {
      static_assert(
          std::is_void_v<Result>,
          "Canvas input callbacks must return void or ui::EventResult");
      input_ = [callback = CallbackType(std::forward<Callback>(callback))](
                   const InputEvent &event,
                   CanvasInputContext &context) mutable {
        std::invoke(callback, event, context);
        return EventResult::Handled;
      };
    }

    focusable_ = true;
    return std::move(*this);
  }

  Canvas &&focusable(bool value = true) &&;

  Spec spec() &&;

private:
  Size size_{};
  DrawCallback draw_;
  InputCallback input_;
  bool focusable_{};
};

} // namespace ui
