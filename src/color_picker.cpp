#include "detail/widget_color.hpp"
#include "detail/widget_input_action.hpp"
#include "detail/widget_text_input_policy.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <nativeui/slider.hpp>
namespace ui {
namespace {
float logical(double value) {
  return static_cast<float>(
      std::min(value, double(std::numeric_limits<float>::max())));
}
std::size_t columns_for(double width, double cell, double gap,
                        std::size_t count) {
  count = std::max<std::size_t>(1, count);
  if (width < cell || cell + gap <= 0)
    return 1;
  const double ratio = 1 + (width - cell) / (cell + gap);
  if (!std::isfinite(ratio) || ratio >= static_cast<double>(count))
    return count;
  return std::max<std::size_t>(1, static_cast<std::size_t>(ratio));
}
bool finite(Color c) {
  return std::isfinite(c.r) && std::isfinite(c.g) && std::isfinite(c.b) &&
         std::isfinite(c.a);
}
bool same(Color a, Color b) {
  auto eq = [](float x, float y) {
    return x == y || (std::isnan(x) && std::isnan(y));
  };
  return eq(a.r, b.r) && eq(a.g, b.g) && eq(a.b, b.b) && eq(a.a, b.a);
}
Color normalized(Color c) {
  if (!finite(c))
    return {0, 0, 0, 1};
  return {std::clamp(c.r, 0.f, 1.f), std::clamp(c.g, 0.f, 1.f),
          std::clamp(c.b, 0.f, 1.f), std::clamp(c.a, 0.f, 1.f)};
}
struct Hsv {
  double h{}, s{}, v{};
};
Hsv hsv_of(Color color, double hue) {
  const auto c = normalized(color);
  const double r = c.r, g = c.g, b = c.b;
  const auto maximum = std::max({r, g, b}), minimum = std::min({r, g, b}),
             delta = maximum - minimum;
  Hsv result{hue, maximum == 0 ? 0 : delta / maximum, maximum};
  if (delta > 0) {
    result.h = (maximum == r   ? (g - b) / delta
                : maximum == g ? 2 + (b - r) / delta
                               : 4 + (r - g) / delta) /
               6;
    result.h -= std::floor(result.h);
  }
  return result;
}
Color color_of(Hsv hsv, double alpha) {
  hsv.h = std::clamp(hsv.h, 0.0, 1.0);
  hsv.s = std::clamp(hsv.s, 0.0, 1.0);
  hsv.v = std::clamp(hsv.v, 0.0, 1.0);
  const double sector = hsv.h * 6;
  const auto index = static_cast<int>(std::floor(sector)) % 6;
  const double part = sector - std::floor(sector), p = hsv.v * (1 - hsv.s),
               q = hsv.v * (1 - hsv.s * part),
               t = hsv.v * (1 - hsv.s * (1 - part));
  std::array<double, 3> rgb;
  switch (index) {
  case 0:
    rgb = {hsv.v, t, p};
    break;
  case 1:
    rgb = {q, hsv.v, p};
    break;
  case 2:
    rgb = {p, hsv.v, t};
    break;
  case 3:
    rgb = {p, q, hsv.v};
    break;
  case 4:
    rgb = {t, p, hsv.v};
    break;
  default:
    rgb = {hsv.v, p, q};
    break;
  }
  return {static_cast<float>(rgb[0]), static_cast<float>(rgb[1]),
          static_cast<float>(rgb[2]),
          static_cast<float>(std::clamp(alpha, 0.0, 1.0))};
}
std::string hex_of(Color color) {
  const auto c = normalized(color);
  auto byte = [](float channel) {
    return static_cast<unsigned>(std::lround(channel * 255));
  };
  std::array<char, 10> text{};
  if (c.a == 1)
    std::snprintf(text.data(), text.size(), "#%02x%02x%02x", byte(c.r),
                  byte(c.g), byte(c.b));
  else
    std::snprintf(text.data(), text.size(), "#%02x%02x%02x%02x", byte(c.r),
                  byte(c.g), byte(c.b), byte(c.a));
  return text.data();
}
std::optional<Color> parse_hex(std::string_view text, bool alpha,
                               Color current) {
  if ((text.size() != 7 && text.size() != 9) || text.front() != '#' ||
      (!alpha && text.size() == 9))
    return {};
  auto digit = [](char ch) -> int {
    if (ch >= '0' && ch <= '9')
      return ch - '0';
    if (ch >= 'a' && ch <= 'f')
      return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
      return ch - 'A' + 10;
    return -1;
  };
  std::array<float, 4> channels{0, 0, 0, normalized(current).a};
  for (std::size_t channel = 0; channel < (text.size() - 1) / 2; ++channel) {
    const int first = digit(text[1 + channel * 2]),
              second = digit(text[2 + channel * 2]);
    if (first < 0 || second < 0)
      return {};
    channels[channel] = static_cast<float>(first * 16 + second) / 255;
  }
  return Color{channels[0], channels[1], channels[2], channels[3]};
}
struct ColorContact {
  bool armed{};
  std::uint64_t generation{};
  std::function<void()> release;
  void cancel() noexcept {
    armed = false;
    auto old = std::move(release);
    if (old)
      try {
        old();
      } catch (...) {
      }
  }
};
struct ColorRuntime : std::enable_shared_from_this<ColorRuntime> {
  explicit ColorRuntime(Binding<Color> value)
      : source(std::move(value)), seen(source.get()), view(normalized(seen)),
        hsv(hsv_of(view, 0)), hex(hex_of(view)), draft(hex) {}
  Binding<Color> source;
  Color seen, view;
  Hsv hsv;
  std::string hex;
  State<std::string> draft;
  std::shared_ptr<detail::TextInputPolicy> policy;
  std::weak_ptr<detail::TextInputSession> editor;
  std::array<std::shared_ptr<ColorContact>, 3> contacts;
  std::function<bool()> guard, owner_guard;
  std::function<void()> invalidate, invalidate_availability;
  std::function<void(Color)> callback;
  std::optional<Color> expected;
  std::optional<Hsv> expected_hsv;
  std::uint64_t generation{}, edit_generation{};
  bool mounted{}, mutable_value{true}, alpha_enabled{true}, dirty{},
      source_valid{true};
  bool allowed() const {
    return mounted && mutable_value && source.valid() && (!guard || guard()) &&
           (!owner_guard || owner_guard());
  }
  void cancel() noexcept {
    for (const auto &contact : contacts)
      if (contact)
        contact->cancel();
  }
  void replace_hex(std::string text) {
    if (auto session = editor.lock()) {
      session->cancel_capture();
      session->replace(text);
      session->reset_baseline();
      edit_generation = session->snapshot().edit_generation;
    }
    draft.set(std::move(text));
    dirty = false;
  }
  void sync() {
    if (!mounted)
      return;
    const bool valid = source.valid();
    if (valid != source_valid) {
      source_valid = valid;
      if (invalidate_availability)
        invalidate_availability();
    }
    policy->read_only = !allowed();
    if (!valid)
      cancel();
    const auto current = source.get();
    if (!same(current, seen)) {
      const bool own = expected && same(*expected, current);
      if (!own) {
        cancel();
        ++generation;
      }
      seen = current;
      view = normalized(current);
      hsv = own && expected_hsv ? *expected_hsv : hsv_of(view, hsv.h);
      expected.reset();
      expected_hsv.reset();
      hex = hex_of(view);
      replace_hex(hex);
      if (invalidate)
        invalidate();
    }
    if (auto session = editor.lock())
      session->refresh_source();
  }
  void publish(Color next, std::optional<Hsv> next_hsv,
               const std::function<bool()> &permission = {}) {
    const auto keep = shared_from_this();
    if (!allowed() || (permission && !permission()))
      return;
    const auto previous = source.get();
    const auto serial = generation;
    auto notify = callback;
    auto model = source;
    if (invalidate)
      invalidate();
    if (!allowed() || generation != serial || !same(model.get(), previous) ||
        (permission && !permission()))
      return;
    if (same(next, previous)) {
      if (next_hsv)
        hsv = *next_hsv;
      return;
    }
    expected = next;
    expected_hsv = next_hsv;
    model.set(next);
    if (allowed() && generation == serial && same(model.get(), next) &&
        (!permission || permission()) && notify)
      notify(next);
  }
  void adjust(Hsv next, const std::function<bool()> &permission = {}) {
    publish(color_of(next, view.a), next, permission);
  }
  void alpha(double next, const std::function<bool()> &permission = {}) {
    if (!alpha_enabled)
      return;
    auto color = view;
    color.a = static_cast<float>(std::clamp(next, 0.0, 1.0));
    publish(color, hsv, permission);
  }
  void commit_hex(detail::TextInputSnapshot snapshot) {
    if (!dirty || !allowed() || snapshot.read_only)
      return;
    const auto previous = source.get();
    const auto serial = generation;
    const auto parsed = parse_hex(snapshot.text, alpha_enabled, previous);
    if (!parsed)
      return;
    replace_hex(hex_of(*parsed));
    if (auto session = editor.lock()) {
      session->cancel_capture();
      session->reset_baseline();
      session->select(snapshot.cursor, snapshot.cursor);
    }
    if (generation == serial && same(source.get(), previous))
      publish(*parsed, {}, snapshot.mutation_guard);
  }
};
class ColorSV final : public Component, public detail::ThemeBinding {
public:
  ColorSV(std::shared_ptr<ColorRuntime> state, ColorPickerStyle style)
      : state_(std::move(state)), style_(std::move(style)),
        contact_(std::make_shared<ColorContact>()) {
    state_->contacts[0] = contact_;
  }
  bool focusable() const noexcept override { return true; }
  bool pointer_targetable() const noexcept override { return true; }
  bool cancel_capture_on_read_only() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &) const override {
    return {
        logical(std::max(0.0, style_.preferred_width - 2 * style_.padding)),
        logical(std::max(0.0, style_.preferred_width - 2 * style_.padding))};
  }
  void unmount(LifecycleContext &) override { contact_->cancel(); }
  void deactivate(LifecycleContext &) override {
    contact_->cancel();
    focused_ = false;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    focused_ = focused;
    if (!focused)
      contact_->cancel();
    context.invalidate();
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto state = state_;
    const auto contact = contact_;
    state->sync();
    const auto request_generation = state->generation;
    const auto request_source = state->source.get();
    if (event.type == InputType::PointerCancel) {
      contact->cancel();
      context.invalidate();
      return EventResult::Handled;
    }
    if (event.type == InputType::PointerDown ||
        event.type == InputType::PointerMove ||
        event.type == InputType::PointerUp) {
      if (event.type == InputType::PointerMove && !contact->armed)
        return EventResult::Ignored;
      if (event.type == InputType::PointerUp && !contact->armed)
        return EventResult::Ignored;
      if (!state->allowed() || context.bounds().w <= 0 ||
          context.bounds().h <= 0) {
        contact->cancel();
        return EventResult::Handled;
      }
      if (!std::isfinite(event.position.x) ||
          !std::isfinite(event.position.y)) {
        if (event.type == InputType::PointerUp)
          contact->cancel();
        return EventResult::Handled;
      }
      auto permission = detail::InputMutationAccess::guard(context);
      const auto bounds = context.bounds();
      auto hsv = state->hsv;
      hsv.s =
          std::clamp(double(event.position.x - bounds.x) / bounds.w, 0.0, 1.0);
      hsv.v = 1 - std::clamp(double(event.position.y - bounds.y) / bounds.h,
                             0.0, 1.0);
      if (event.type == InputType::PointerDown) {
        contact->armed = true;
        contact->generation = state->generation;
        contact->release = context.pointer_releaser();
        context.capture_pointer();
      }
      if (event.type == InputType::PointerUp)
        contact->cancel();
      if (state->generation != request_generation ||
          !same(state->source.get(), request_source))
        return EventResult::Handled;
      try {
        state->adjust(hsv, permission);
      } catch (...) {
        contact->cancel();
        throw;
      }
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown) {
      auto hsv = state->hsv;
      const double step = event.shift ? .001 : .01;
      switch (event.key) {
      case Key::Left:
        hsv.s -= step;
        break;
      case Key::Right:
        hsv.s += step;
        break;
      case Key::Up:
        hsv.v += step;
        break;
      case Key::Down:
        hsv.v -= step;
        break;
      default:
        return EventResult::Ignored;
      }
      hsv.s = std::clamp(hsv.s, 0.0, 1.0);
      hsv.v = std::clamp(hsv.v, 0.0, 1.0);
      auto permission = detail::InputMutationAccess::guard(context);
      state->adjust(hsv, permission);
      return EventResult::Handled;
    }
    return EventResult::Ignored;
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    if (action != SemanticAction::Increment &&
        action != SemanticAction::Decrement)
      return EventResult::Ignored;
    InputEvent event;
    event.type = InputType::KeyDown;
    event.key = action == SemanticAction::Increment ? Key::Right : Key::Left;
    return input(event, context);
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Custom;
    info.name = "Saturation and value";
    info.description =
        "Saturation " +
        std::to_string(static_cast<int>(std::lround(state_->hsv.s * 100))) +
        " %, value " +
        std::to_string(static_cast<int>(std::lround(state_->hsv.v * 100))) +
        " %";
    info.focusable = true;
    info.focused = focused_;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only() || !state_->source.valid();
    info.numeric_value = state_->hsv.s;
    info.value_range = SemanticValueRange{0, 1, .01};
    if (info.enabled)
      info.actions = {SemanticAction::Focus};
    if (info.enabled && !info.read_only) {
      info.actions.push_back(SemanticAction::Increment);
      info.actions.push_back(SemanticAction::Decrement);
    }
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto bounds = context.bounds();
    auto &p = context.painter();
    auto clip = p.scoped_clip(bounds, logical(style_.corner_radius));
    const auto hue = color_of({state_->hsv.h, 1, 1}, 1);
    p.fill_rounded_rect(bounds, logical(style_.corner_radius),
                        LinearGradient{{bounds.x, bounds.y},
                                       {bounds.x + bounds.w, bounds.y},
                                       Color{1, 1, 1, 1},
                                       hue});
    p.fill_rounded_rect(bounds, logical(style_.corner_radius),
                        LinearGradient{{bounds.x, bounds.y},
                                       {bounds.x, bounds.y + bounds.h},
                                       Color{0, 0, 0, 0},
                                       Color{0, 0, 0, 1}});
    const Point thumb{bounds.x + bounds.w * static_cast<float>(state_->hsv.s),
                      bounds.y +
                          bounds.h * static_cast<float>(1 - state_->hsv.v)};
    p.circle(thumb, logical(style_.thumb_radius),
             style_.thumb.value_or(Color{1, 1, 1, 1}));
    p.circle(thumb, std::max(0.f, logical(style_.thumb_radius) - 2),
             state_->view);
    if (focused_)
      p.stroke_rounded_rect(
          bounds, logical(style_.corner_radius), 2,
          style_.focus.value_or(current_theme().palette.focus));
  }

private:
  std::shared_ptr<ColorRuntime> state_;
  ColorPickerStyle style_;
  std::shared_ptr<ColorContact> contact_;
  bool focused_{};
};
class ColorChannel final : public Component, public detail::ThemeBinding {
public:
  ColorChannel(std::shared_ptr<ColorRuntime> state, bool alpha,
               ColorPickerStyle style)
      : state_(std::move(state)), alpha_(alpha), style_(std::move(style)),
        contact_(std::make_shared<ColorContact>()) {
    state_->contacts[alpha ? 2 : 1] = contact_;
  }
  bool focusable() const noexcept override { return true; }
  bool pointer_targetable() const noexcept override { return true; }
  bool cancel_capture_on_read_only() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &) const override {
    return {logical(style_.preferred_width), logical(style_.channel_height)};
  }
  void unmount(LifecycleContext &) override { contact_->cancel(); }
  void deactivate(LifecycleContext &) override {
    contact_->cancel();
    focused_ = false;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    focused_ = focused;
    if (!focused)
      contact_->cancel();
    context.invalidate();
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto state = state_;
    const auto contact = contact_;
    state->sync();
    const auto request_generation = state->generation;
    const auto request_source = state->source.get();
    if (event.type == InputType::PointerCancel) {
      contact->cancel();
      context.invalidate();
      return EventResult::Handled;
    }
    auto permission = detail::InputMutationAccess::guard(context);
    double next = alpha_ ? state->view.a : state->hsv.h;
    if (event.type == InputType::KeyDown) {
      const detail::SliderDomain domain{0, 1, 0};
      const double step = domain.keyboard_increment(event.shift);
      switch (event.key) {
      case Key::Left:
      case Key::Down:
        next -= step;
        break;
      case Key::Right:
      case Key::Up:
        next += step;
        break;
      case Key::Home:
        next = 0;
        break;
      case Key::End:
        next = 1;
        break;
      default:
        return EventResult::Ignored;
      }
      next = std::clamp(next, 0.0, 1.0);
    } else if (event.type == InputType::PointerDown ||
               event.type == InputType::PointerMove ||
               event.type == InputType::PointerUp) {
      if (event.type != InputType::PointerDown && !contact->armed)
        return EventResult::Ignored;
      if (!state->allowed() || context.bounds().w <= 0 ||
          context.bounds().h <= 0) {
        contact->cancel();
        return EventResult::Handled;
      }
      const auto axis = detail::slider_track_axis(
          context.bounds(), SliderOrientation::Horizontal, false);
      next = axis.fraction(event.position.x);
      if (event.type == InputType::PointerDown) {
        contact->armed = true;
        contact->generation = state->generation;
        contact->release = context.pointer_releaser();
        context.capture_pointer();
      }
      if (event.type == InputType::PointerUp)
        contact->cancel();
    } else
      return EventResult::Ignored;
    if (state->generation != request_generation ||
        !same(state->source.get(), request_source))
      return EventResult::Handled;
    auto hsv = state->hsv;
    hsv.h = next;
    try {
      if (alpha_)
        state->alpha(next, permission);
      else
        state->adjust(hsv, permission);
    } catch (...) {
      contact->cancel();
      throw;
    }
    return EventResult::Handled;
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    if (action != SemanticAction::Increment &&
        action != SemanticAction::Decrement)
      return EventResult::Ignored;
    InputEvent event;
    event.type = InputType::KeyDown;
    event.key = action == SemanticAction::Increment ? Key::Right : Key::Left;
    return input(event, context);
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Slider;
    info.name = alpha_ ? "Alpha" : "Hue";
    info.numeric_value = alpha_ ? state_->view.a : state_->hsv.h;
    info.value_range = SemanticValueRange{0, 1, .01};
    info.text_value = std::to_string(static_cast<int>(
                          std::lround(*info.numeric_value * 100))) +
                      " %";
    info.focusable = true;
    info.focused = focused_;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only() || !state_->source.valid();
    if (info.enabled)
      info.actions = {SemanticAction::Focus};
    if (info.enabled && !info.read_only) {
      info.actions.push_back(SemanticAction::Increment);
      info.actions.push_back(SemanticAction::Decrement);
    }
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto bounds = context.bounds();
    auto &p = context.painter();
    auto clip = p.scoped_clip(bounds, logical(style_.corner_radius));
    if (alpha_) {
      detail::paint_color_preview(
          p, bounds, Color{0, 0, 0, 0}, style_.corner_radius,
          style_.checker_size,
          style_.checker_light.value_or(Color{.8f, .8f, .8f, 1}),
          style_.checker_dark.value_or(Color{.5f, .5f, .5f, 1}));
      auto c = state_->view;
      c.a = 0;
      auto opaque = c;
      opaque.a = 1;
      p.fill_rounded_rect(bounds, logical(style_.corner_radius),
                          LinearGradient{{bounds.x, bounds.y},
                                         {bounds.x + bounds.w, bounds.y},
                                         c,
                                         opaque});
    } else
      p.fill_rounded_rect(bounds, logical(style_.corner_radius),
                          LinearGradient{{bounds.x, bounds.y},
                                         {bounds.x + bounds.w, bounds.y},
                                         {{0, Color{1, 0, 0, 1}},
                                          {1.f / 6, Color{1, 1, 0, 1}},
                                          {2.f / 6, Color{0, 1, 0, 1}},
                                          {3.f / 6, Color{0, 1, 1, 1}},
                                          {4.f / 6, Color{0, 0, 1, 1}},
                                          {5.f / 6, Color{1, 0, 1, 1}},
                                          {1, Color{1, 0, 0, 1}}}});
    const auto axis =
        detail::slider_track_axis(bounds, SliderOrientation::Horizontal, false);
    p.circle({axis.position(alpha_ ? state_->view.a
                                   : static_cast<float>(state_->hsv.h)),
              bounds.y + bounds.h * .5f},
             logical(style_.thumb_radius),
             style_.thumb.value_or(Color{1, 1, 1, 1}));
    if (focused_)
      p.stroke_rounded_rect(
          bounds, logical(style_.corner_radius), 2,
          style_.focus.value_or(current_theme().palette.focus));
  }

private:
  std::shared_ptr<ColorRuntime> state_;
  bool alpha_{};
  ColorPickerStyle style_;
  std::shared_ptr<ColorContact> contact_;
  bool focused_{};
};
class ColorFrame final : public Component, public detail::ThemeBinding {
public:
  ColorFrame(std::string label, Binding<Color> source, bool alpha,
             std::vector<ColorSwatch> swatches,
             std::function<void(Color)> callback, ColorPickerStyle style,
             std::function<bool()> owner_guard)
      : label_(std::move(label)), swatches_(std::move(swatches)),
        style_(std::move(style)),
        state_(std::make_shared<ColorRuntime>(std::move(source))) {
    const auto state = state_;
    state->alpha_enabled = alpha;
    state->callback = std::move(callback);
    state->owner_guard = std::move(owner_guard);
    state->policy = std::make_shared<detail::TextInputPolicy>();
    state->policy->paint_label = false;
    const std::weak_ptr<ColorRuntime> weak = state;
    state->policy->committed_edit = [weak](detail::TextInputSnapshot snapshot) {
      if (const auto current = weak.lock()) {
        current->edit_generation = snapshot.edit_generation;
        current->dirty = true;
      }
    };
    state->policy->before_input =
        [weak](const InputEvent &event, InputContext &context,
               const detail::TextInputSnapshot &snapshot)
        -> std::optional<EventResult> {
      const auto current = weak.lock();
      if (!current)
        return EventResult::Handled;
      if (snapshot.composition_active && event.type == InputType::KeyDown &&
          (event.key == Key::Enter || event.key == Key::Escape)) {
        if (event.key == Key::Escape) {
          if (const auto session = current->editor.lock()) {
            session->cancel_capture();
            session->cancel_composition();
          }
          context.invalidate();
        }
        return EventResult::Handled;
      }
      if (event.type == InputType::KeyDown && event.key == Key::Enter) {
        current->commit_hex(snapshot);
        return EventResult::Handled;
      }
      if (event.type == InputType::KeyDown && event.key == Key::Escape) {
        current->replace_hex(current->hex);
        return EventResult::Handled;
      }
      return {};
    };
    state->policy->focus_changed = [weak](bool focused,
                                          detail::TextInputSnapshot snapshot) {
      if (!focused && snapshot.allow_edit_commit)
        if (const auto current = weak.lock())
          current->commit_hex(std::move(snapshot));
    };
  }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool clips_children() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &) const override {
    const double width = style_.preferred_width,
                 inner = std::max(0.0, width - 2 * style_.padding);
    const auto columns =
        columns_for(inner, style_.swatch_size, style_.gap, swatches_.size());
    const auto rows = (swatches_.size() + columns - 1) / columns;
    return {
        logical(width),
        logical(2 * style_.padding + inner +
                (state_->alpha_enabled ? 2 : 1) *
                    (style_.channel_height + style_.gap) +
                style_.hex_height +
                (rows ? style_.gap + rows * (style_.swatch_size + style_.gap) -
                            style_.gap
                      : 0))};
  }
  ChildMetrics measure_constrained(
      const Constraints &constraints,
      const std::vector<ChildMetrics> &children) const override {
    const auto preferred = measure(children);
    const float width = constraints.bounded_width()
                            ? std::min(preferred.w, constraints.max.w)
                            : preferred.w;
    const auto square = std::max(0.f, width - logical(2 * style_.padding));
    const float height =
        preferred.h - std::max(0.f, preferred.w - logical(2 * style_.padding)) +
        square;
    return {constraints.constrain({0, 0}),
            constraints.constrain({width, height})};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    if (children.empty())
      return;
    const float pad = std::min({logical(style_.padding),
                                std::max(0.f, bounds.w * .5f),
                                std::max(0.f, bounds.h * .5f)}),
                width = std::max(0.f, bounds.w - 2 * pad),
                gap = logical(style_.gap);
    float y = bounds.y + pad;
    std::size_t index{};
    const float square = std::min(width, std::max(0.f, bounds.h - 2 * pad));
    children[index++].bounds = {bounds.x + pad, y, square, square};
    y += square + gap;
    const auto channels = state_->alpha_enabled ? 2 : 1;
    for (int channel = 0; channel < channels; ++channel) {
      children[index++].bounds = {bounds.x + pad, y, width,
                                  logical(style_.channel_height)};
      y += logical(style_.channel_height) + gap;
    }
    const float preview = std::min(width, logical(style_.preview_width));
    preview_ = {bounds.x + pad, y, preview, logical(style_.hex_height)};
    children[index++].bounds = {
        bounds.x + pad + preview +
            std::min(gap, std::max(0.f, width - preview)),
        y, std::max(0.f, width - preview - gap), logical(style_.hex_height)};
    y += logical(style_.hex_height) + gap;
    const float size = std::min(width, logical(style_.swatch_size));
    const auto columns = columns_for(width, size, gap, swatches_.size());
    for (std::size_t swatch = 0; index < children.size(); ++index, ++swatch)
      children[index].bounds = {
          bounds.x + pad + static_cast<float>(swatch % columns) * (size + gap),
          y + static_cast<float>(swatch / columns) * (size + gap), size, size};
  }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->guard = detail::InputMutationAccess::guard(context);
    state_->invalidate = context.invalidator();
    state_->invalidate_availability = context.availability_invalidator();
    const std::weak_ptr<ColorRuntime> weak = state_;
    subscription_ = state_->source.observe([weak](const auto &) {
      if (const auto state = weak.lock())
        state->sync();
    });
  }
  void unmount(LifecycleContext &) override {
    state_->cancel();
    state_->mounted = false;
    ++state_->generation;
    state_->guard = {};
    state_->invalidate = {};
    state_->invalidate_availability = {};
    subscription_.reset();
  }
  void deactivate(LifecycleContext &) override {
    state_->cancel();
    ++state_->generation;
    state_->replace_hex(state_->hex);
  }
  std::vector<Spec> children() {
    const auto state = state_;
    std::vector<Spec> children;
    children.push_back(Spec{[state, style = style_] {
                              return std::make_unique<ColorSV>(state, style);
                            },
                            {}});
    children.push_back(Spec{[state, style = style_] {
                              return std::make_unique<ColorChannel>(
                                  state, false, style);
                            },
                            {}});
    if (state->alpha_enabled)
      children.push_back(Spec{[state, style = style_] {
                                return std::make_unique<ColorChannel>(
                                    state, true, style);
                              },
                              {}});
    auto hex_style = style_.hex;
    // The compact owner supplies the field geometry; hiding a standalone
    // TextInput label alone would retain its 24-point label offset.
    hex_style.base.control_height =
        hex_style.base.control_height.value_or(logical(style_.hex_height));
    hex_style.base.field_top = hex_style.base.field_top.value_or(0.f);
    hex_style.base.field_height =
        hex_style.base.field_height.value_or(logical(style_.hex_height));
    children.push_back(
        Spec{[state, style = std::move(hex_style)] {
               auto editor = std::make_unique<TextInputComponent>(
                   "Hexadecimal", state->draft.binding(), "#rrggbb", 0,
                   TextInputComponent::SubmitCallback{}, style);
               state->editor =
                   detail::TextInputAccess::configure(*editor, state->policy);
               return editor;
             },
             {}});
    const std::weak_ptr<ColorRuntime> weak = state;
    for (const auto &swatch : swatches_) {
      auto action = std::make_shared<detail::InputActionState>();
      action->enabled = [weak] {
        const auto current = weak.lock();
        // Availability is also queried before activation. Mutation guards
        // are checked by publish during input, rather than disabling a swatch
        // for the lifetime of its initial availability snapshot.
        return current && current->source.valid();
      };
      action->generation = [weak] {
        const auto current = weak.lock();
        return current ? current->generation : 0;
      };
      action->context_action = [weak,
                                color = swatch.value](InputContext &context) {
        if (const auto current = weak.lock()) {
          auto next = normalized(color);
          if (!current->alpha_enabled)
            next.a = current->view.a;
          auto permission = detail::InputMutationAccess::guard(context);
          current->publish(next, {}, permission);
        }
      };
      auto style = style_.swatch;
      style.base.fill = normalized(swatch.value);
      style.base.minimum_width = logical(style_.swatch_size);
      style.base.horizontal_padding = 0.0f;
      children.push_back(Spec{[action, name = swatch.name, style] {
                                return std::make_unique<detail::InputAction>(
                                    name, "", style, action, true, false);
                              },
                              {}});
    }
    return children;
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Group;
    info.name = label_;
    info.text_value = state_->hex;
    info.read_only = effective_read_only() || !state_->source.valid();
    if (!finite(state_->seen))
      info.description = "Invalid color";
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto bounds = context.bounds();
    auto &p = context.painter();
    auto clip = p.scoped_clip(bounds);
    p.fill_rounded_rect(
        bounds, logical(style_.corner_radius),
        style_.background.value_or(current_theme().palette.surface));
    detail::paint_color_preview(
        p, preview_, state_->view, style_.corner_radius, style_.checker_size,
        style_.checker_light.value_or(Color{.8f, .8f, .8f, 1}),
        style_.checker_dark.value_or(Color{.5f, .5f, .5f, 1}));
  }

private:
  void retained_checkpoint() override {
    state_->sync();
    if (auto editor = state_->editor.lock()) {
      const auto snapshot = editor->snapshot();
      if (snapshot.edit_generation > state_->edit_generation) {
        state_->edit_generation = snapshot.edit_generation;
        state_->dirty = true;
      }
    }
  }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    state_->policy->read_only =
        !state_->mutable_value || !state_->source.valid();
    if (!state_->mutable_value)
      state_->cancel();
  }
  std::string label_;
  std::vector<ColorSwatch> swatches_;
  ColorPickerStyle style_;
  std::shared_ptr<ColorRuntime> state_;
  Binding<Color>::Subscription subscription_;
  mutable Rect preview_;
};
} // namespace
namespace detail {
ColorPreview color_preview(Color value) {
  return {normalized(value), hex_of(value), !finite(value)};
}
void validate_swatches(const std::vector<ColorSwatch> &value) {
  std::vector<std::string> ids;
  for (const auto &swatch : value) {
    if (swatch.id.empty() ||
        std::find(ids.begin(), ids.end(), swatch.id) != ids.end())
      throw std::invalid_argument(
          "ColorSwatch ids must be nonempty and unique");
    ids.push_back(swatch.id);
  }
}
void paint_color_preview(Painter &p, Rect bounds, Color color, double radius,
                         double checker_size, Color light, Color dark) {
  auto clip = p.scoped_clip(bounds, logical(radius));
  if (bounds.empty())
    return;
  const float size =
      std::max({1.f, logical(checker_size), bounds.w / 256, bounds.h / 256});
  const auto columns = static_cast<std::size_t>(std::ceil(bounds.w / size)),
             rows = static_cast<std::size_t>(std::ceil(bounds.h / size));
  for (std::size_t y = 0; y < rows; ++y)
    for (std::size_t x = 0; x < columns; ++x)
      p.fill_rounded_rect({bounds.x + static_cast<float>(x) * size,
                           bounds.y + static_cast<float>(y) * size, size, size},
                          0, (x + y) % 2 ? dark : light);
  p.fill_rounded_rect(bounds, logical(radius), normalized(color));
}
} // namespace detail
ColorPicker::ColorPicker(std::string label, Binding<Color> value)
    : label_(std::move(label)), value_(std::move(value)) {}
ColorPicker::ColorPicker(std::string label, State<Color> &value)
    : ColorPicker(std::move(label), value.binding()) {}
ColorPicker &&ColorPicker::alpha_enabled(bool value) && {
  alpha_ = value;
  return std::move(*this);
}
ColorPicker &&ColorPicker::swatches(std::vector<ColorSwatch> value) && {
  detail::validate_swatches(value);
  swatches_ = std::move(value);
  return std::move(*this);
}
ColorPicker &&ColorPicker::on_change(std::function<void(Color)> value) && {
  callback_ = std::move(value);
  return std::move(*this);
}
ColorPicker &&ColorPicker::style(ColorPickerStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec ColorPicker::spec() && {
  return detail::color_picker_spec(std::move(label_), value_, alpha_,
                                   std::move(swatches_), std::move(callback_),
                                   std::move(style_));
}
namespace detail {
Spec color_picker_spec(std::string label, Binding<Color> value, bool alpha,
                       std::vector<ColorSwatch> swatches,
                       std::function<void(Color)> callback,
                       ColorPickerStyle style,
                       std::function<bool()> owner_guard) {
  validate_swatches(swatches);
  for (double metric :
       {style.preferred_width, style.padding, style.gap, style.channel_height,
        style.hex_height, style.preview_width, style.swatch_size,
        style.checker_size, style.thumb_radius, style.corner_radius,
        style.text_size})
    if (!std::isfinite(metric) || metric < 0)
      throw std::invalid_argument(
          "ColorPicker metrics must be finite and nonnegative");
  Spec spec{[label = std::move(label), value = std::move(value), alpha,
             swatches = std::move(swatches), callback = std::move(callback),
             style = std::move(style), owner_guard = std::move(owner_guard)] {
              return std::make_unique<ColorFrame>(label, value, alpha, swatches,
                                                  callback, style, owner_guard);
            },
            {}};
  spec.children_factory = [](Component &component) {
    return static_cast<ColorFrame &>(component).children();
  };
  return spec;
}
} // namespace detail
} // namespace ui
