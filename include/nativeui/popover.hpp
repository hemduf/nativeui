#pragma once

#include <nativeui/component.hpp>
#include <nativeui/overlay.hpp>
#include <nativeui/state.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace ui {

struct PopoverStyle {
    std::optional<Color> surface{};
    std::optional<Color> border{};
    std::optional<float> border_width{};
    std::optional<float> radius{};
    std::optional<float> padding{};
    std::optional<Size> max_size{};
};

/// Retained anchor decorator. The opening intent is borrowed through Binding;
/// each compilation and each open period owns a separate popup session.
class Popover {
  public:
    template <class Anchor, class Content>
    Popover(Binding<bool> open, Anchor &&anchor, Content &&content)
        : open_(std::move(open)), anchor_(make_spec(std::forward<Anchor>(anchor))),
          content_(make_spec(std::forward<Content>(content))) {}

    template <class Anchor, class Content>
    Popover(State<bool> &open, Anchor &&anchor, Content &&content)
        : Popover(open.binding(), std::forward<Anchor>(anchor), std::forward<Content>(content)) {}

    Popover &&placement(OverlayPlacement value) &&;
    Popover &&match_anchor_width(bool value = true) &&;
    Popover &&focus_on_open(bool value = true) &&;
    Popover &&on_close(std::function<void()> callback) &&;
    Popover &&style(PopoverStyle value) &&;
    Spec spec() &&;

  private:
    Binding<bool> open_;
    Spec anchor_;
    Spec content_;
    OverlayPlacement placement_{OverlayPlacement::Auto};
    bool match_anchor_width_{true};
    bool focus_on_open_{};
    std::function<void()> on_close_;
    PopoverStyle style_;
};

} // namespace ui
