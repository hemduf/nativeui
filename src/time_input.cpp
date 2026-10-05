#include "detail/widget_input_action.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/time_input.hpp>
#include <stdexcept>
namespace ui {
namespace {
using Time = TimeInput::Value;
float logical(double value) {
  return static_cast<float>(
      std::min(value, double(std::numeric_limits<float>::max())));
}
bool valid_time(Time value) {
  return value && value->count() >= 0 && value->count() < 86400;
}
std::array<int, 3> parts(Time value) {
  const auto seconds = valid_time(value) ? value->count() : 0;
  return {static_cast<int>(seconds / 3600), static_cast<int>(seconds / 60 % 60),
          static_cast<int>(seconds % 60)};
}
Time joined(const std::array<int, 3> &value) {
  return std::chrono::seconds{value[0] * 3600 + value[1] * 60 + value[2]};
}
std::string digits(int value) {
  return std::string(value < 10 ? "0" : "") + std::to_string(value);
}
struct TimeRuntime : std::enable_shared_from_this<TimeRuntime> {
  explicit TimeRuntime(Binding<Time> value)
      : source(std::move(value)), seen(source.get()) {}
  Binding<Time> source;
  Time seen;
  std::function<void(Time)> callback;
  std::function<void()> invalidate;
  std::function<bool()> guard;
  std::array<std::function<void()>, 3> focus_requests;
  std::uint64_t generation{};
  int active{}, buffered{-1}, count{2};
  bool mounted{}, mutable_value{true};
  bool allowed() const {
    return mounted && mutable_value && source.valid() && (!guard || guard());
  }
  void sync() {
    if (!mounted)
      return;
    const auto current = source.get();
    if (current != seen) {
      seen = current;
      buffered = -1;
      ++generation;
      if (invalidate)
        invalidate();
    }
  }
  void move(int index) {
    index = std::clamp(index, 0, count - 1);
    buffered = -1;
    active = index;
    auto focus = focus_requests[static_cast<std::size_t>(index)];
    if (focus)
      focus();
  }
  void publish(Time next, const std::function<bool()> &permission) {
    const auto keep = shared_from_this();
    if (!allowed() || (permission && !permission()))
      return;
    const auto expected = source.get();
    const auto serial = generation;
    auto notify = callback;
    auto model = source;
    auto invalidate_copy = invalidate;
    if (invalidate_copy)
      invalidate_copy();
    if (!allowed() || generation != serial || model.get() != expected ||
        (permission && !permission()))
      return;
    if (next == expected)
      return;
    seen = next;
    model.set(next);
    if (mounted && generation == serial && allowed() && model.valid() &&
        model.get() == next && (!permission || permission()) && notify)
      notify(next);
  }
  void adjust(int index, int direction,
              const std::function<bool()> &permission) {
    if (!allowed() || (permission && !permission()))
      return;
    buffered = -1;
    auto value = parts(source.get());
    const auto limit = index == 0 ? 24 : 60;
    value[static_cast<std::size_t>(index)] =
        (value[static_cast<std::size_t>(index)] + direction + limit) % limit;
    publish(joined(value), permission);
  }
  void digit(int value, const std::function<bool()> &permission) {
    if (!allowed() || (permission && !permission()))
      return;
    const auto expected = source.get();
    const auto revision = source.revision();
    const auto serial = generation;
    auto current = parts(expected);
    const auto index = active;
    const auto limit = index == 0 ? 24 : 60;
    const bool second = buffered >= 0 && buffered * 10 + value < limit;
    const auto accepted = second ? buffered * 10 + value : value;
    buffered = second ? -1 : value;
    const bool advance = second || value > (index == 0 ? 2 : 5);
    current[static_cast<std::size_t>(index)] = accepted;
    if (advance)
      move(index + 1);
    if (!allowed() || generation != serial || source.revision() != revision ||
        (permission && !permission())) {
      sync();
      return;
    }
    publish(joined(current), permission);
  }
};
class TimeSegment final : public Component, public detail::ThemeBinding {
public:
  TimeSegment(std::shared_ptr<TimeRuntime> state, int index,
              TimeInputStyle style)
      : state_(std::move(state)), index_(index), style_(std::move(style)) {}
  bool focusable() const noexcept override { return true; }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &) const override {
    return {logical(style_.segment_width), logical(style_.height)};
  }
  void mount(MountContext &context) override {
    state_->focus_requests[static_cast<std::size_t>(index_)] =
        context.focus_requester();
  }
  void unmount(LifecycleContext &) override {
    state_->focus_requests[static_cast<std::size_t>(index_)] = {};
    state_->buffered = -1;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    const auto state = state_;
    focused_ = focused;
    state->buffered = -1;
    if (focused)
      state->active = index_;
    context.set_text_input(focused, context.bounds(), 0);
    context.invalidate();
  }
  void deactivate(LifecycleContext &) override {
    focused_ = false;
    state_->buffered = -1;
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto state = state_;
    state->sync();
    if (event.type == InputType::PointerDown) {
      state->buffered = -1;
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown &&
        (event.key == Key::Left || event.key == Key::Right)) {
      state->move(index_ + (event.key == Key::Right ? 1 : -1));
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown &&
        (event.key == Key::Escape || event.key == Key::Enter)) {
      state->buffered = -1;
      context.invalidate();
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown &&
        (event.key == Key::Up || event.key == Key::Down)) {
      auto permission = detail::InputMutationAccess::guard(context);
      state->adjust(index_, event.key == Key::Up ? 1 : -1, permission);
      return EventResult::Handled;
    }
    if (event.type == InputType::TextInput) {
      const std::string committed = event.text;
      auto permission = detail::InputMutationAccess::guard(context);
      for (char ch : committed) {
        if (ch < '0' || ch > '9')
          continue;
        state->digit(ch - '0', permission);
      }
      return EventResult::Handled;
    }
    return EventResult::Ignored;
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    const auto state = state_;
    if (action == SemanticAction::Focus) {
      state->move(index_);
      return EventResult::Handled;
    }
    if (action == SemanticAction::Increment ||
        action == SemanticAction::Decrement) {
      auto permission = detail::InputMutationAccess::guard(context);
      state->adjust(index_, action == SemanticAction::Increment ? 1 : -1,
                    permission);
      return EventResult::Handled;
    }
    return EventResult::Ignored;
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Custom;
    info.name = index_ == 0 ? "hours" : index_ == 1 ? "minutes" : "seconds";
    info.focusable = true;
    info.focused = focused_;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only() || !state_->source.valid();
    const auto current = state_->seen;
    info.text_value =
        valid_time(current)
            ? digits(parts(current)[static_cast<std::size_t>(index_)])
            : "--";
    if (valid_time(current))
      info.numeric_value = parts(current)[static_cast<std::size_t>(index_)];
    info.value_range = SemanticValueRange{0, index_ == 0 ? 23.0 : 59.0, 1};
    if (info.enabled)
      info.actions = {SemanticAction::Focus};
    if (info.enabled && !info.read_only) {
      info.actions.push_back(SemanticAction::Increment);
      info.actions.push_back(SemanticAction::Decrement);
    }
    if (current && !valid_time(current))
      info.description = "Time out of range";
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto bounds = context.bounds();
    auto &painter = context.painter();
    auto clip = painter.scoped_clip(bounds);
    if (focused_)
      painter.fill_rounded_rect(
          bounds, logical(style_.corner_radius),
          style_.selected.value_or(current_theme().palette.selection));
    painter.text(
        {bounds.x + bounds.w * .5f, bounds.y + bounds.h * .5f},
        valid_time(state_->seen)
            ? digits(parts(state_->seen)[static_cast<std::size_t>(index_)])
            : "--",
        logical(style_.text_size),
        style_.text.value_or(current_theme().palette.text), TextAlign::Center);
  }

private:
  void retained_checkpoint() override { state_->sync(); }
  std::shared_ptr<TimeRuntime> state_;
  int index_{};
  TimeInputStyle style_;
  bool focused_{};
};
class TimeFrame final : public Component, public detail::ThemeBinding {
public:
  TimeFrame(std::string label, Binding<Time> value, bool seconds,
            bool clearable, std::function<void(Time)> callback,
            TimeInputStyle style)
      : label_(std::move(label)), style_(std::move(style)),
        state_(std::make_shared<TimeRuntime>(std::move(value))),
        clearable_(clearable) {
    state_->count = seconds ? 3 : 2;
    state_->callback = std::move(callback);
  }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool clips_children() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &) const override {
    return {logical(2 * style_.padding + state_->count * style_.segment_width +
                    (state_->count - 1) * style_.gap +
                    (clearable_ ? style_.height + style_.gap : 0)),
            logical(style_.height + 2 * style_.padding)};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    const auto boxes = geometry(bounds);
    for (int index = 0; index < state_->count; ++index)
      children[static_cast<std::size_t>(index)].bounds =
          boxes[static_cast<std::size_t>(index)];
    if (clearable_)
      children.back().bounds = boxes[3];
  }

  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->guard = detail::InputMutationAccess::guard(context);
    state_->invalidate = context.invalidator();
    const std::weak_ptr<TimeRuntime> weak = state_;
    subscription_ = state_->source.observe([weak](const auto &) {
      if (auto state = weak.lock())
        state->sync();
    });
  }
  void unmount(LifecycleContext &) override {
    state_->mounted = false;
    ++state_->generation;
    state_->buffered = -1;
    state_->invalidate = {};
    state_->guard = {};
    subscription_.reset();
  }
  std::vector<Spec> children() {
    std::vector<Spec> result;
    for (int index = 0; index < state_->count; ++index)
      result.push_back(Spec{[state = state_, index, style = style_] {
                              return std::make_unique<TimeSegment>(state, index,
                                                                   style);
                            },
                            {}});
    if (clearable_) {
      auto action = std::make_shared<detail::InputActionState>();
      const std::weak_ptr<TimeRuntime> weak = state_;
      action->enabled = [weak] {
        const auto state = weak.lock();
        return state && state->allowed() && state->source.get().has_value();
      };
      action->generation = [weak] {
        const auto state = weak.lock();
        return state ? state->generation : 0;
      };
      action->context_action = [weak](InputContext &context) {
        if (auto state = weak.lock()) {
          state->buffered = -1;
          auto permission = detail::InputMutationAccess::guard(context);
          state->publish(std::nullopt, permission);
        }
      };
      result.push_back(Spec{[action, style = style_.clear] {
                              return std::make_unique<detail::InputAction>(
                                  "Clear time", "×", style, action, true,
                                  false);
                            },
                            {}});
    }
    return result;
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Group;
    info.name = label_;
    info.read_only = effective_read_only() || !state_->source.valid();
    if (state_->seen && !valid_time(state_->seen))
      info.description = "Time out of range";
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto bounds = context.bounds();
    auto &painter = context.painter();
    auto clip = painter.scoped_clip(bounds);
    painter.fill_rounded_rect(
        bounds, logical(style_.corner_radius),
        style_.background.value_or(current_theme().palette.control_background));
    painter.stroke_rounded_rect(
        bounds, logical(style_.corner_radius), logical(style_.border_width),
        style_.border.value_or(current_theme().palette.border));
    const auto boxes = geometry(bounds);
    for (int index = 0; index + 1 < state_->count; ++index) {
      const auto left = boxes[static_cast<std::size_t>(index)],
                 right = boxes[static_cast<std::size_t>(index + 1)];
      const float gap = std::max(0.f, right.x - left.x - left.w);
      auto colon_clip =
          painter.scoped_clip({left.x + left.w, left.y, gap, left.h});
      painter.text(
          {left.x + left.w + gap * .5f, left.y + left.h * .5f}, ":",
          logical(style_.text_size),
          style_.separator.value_or(current_theme().palette.muted_text),
          TextAlign::Center);
    }
  }

private:
  std::array<Rect, 4> geometry(Rect bounds) const {
    std::array<Rect, 4> result{};
    const float padding =
        std::min(logical(style_.padding), std::max(0.f, bounds.w * .5f));
    const float h = std::max(0.f, bounds.h - 2 * padding),
                available = std::max(0.f, bounds.w - 2 * padding);
    const float clear = clearable_ ? std::min(h, available) : 0;
    const float gap = logical(style_.gap);
    const float width = std::max(
        0.f,
        (available - clear -
         gap * static_cast<float>(state_->count - 1 + (clearable_ ? 1 : 0))) /
            static_cast<float>(state_->count));
    float x = bounds.x + padding;
    for (int index = 0; index < state_->count; ++index) {
      result[static_cast<std::size_t>(index)] = {x, bounds.y + padding, width,
                                                 h};
      x += width + gap;
    }
    if (clearable_)
      result[3] = {bounds.x + bounds.w - padding - clear, bounds.y + padding,
                   clear, h};
    return result;
  }
  void retained_checkpoint() override { state_->sync(); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    if (!state_->mutable_value) {
      state_->buffered = -1;
      ++state_->generation;
    }
  }
  std::string label_;
  TimeInputStyle style_;
  std::shared_ptr<TimeRuntime> state_;
  bool clearable_{};
  Binding<Time>::Subscription subscription_;
};
} // namespace
TimeInput::TimeInput(std::string label, Binding<Value> value)
    : label_(std::move(label)), value_(std::move(value)) {}
TimeInput::TimeInput(std::string label, State<Value> &value)
    : TimeInput(std::move(label), value.binding()) {}
TimeInput &&TimeInput::show_seconds(bool value) && {
  seconds_ = value;
  return std::move(*this);
}
TimeInput &&TimeInput::clearable(bool value) && {
  clearable_ = value;
  return std::move(*this);
}
TimeInput &&TimeInput::on_change(std::function<void(Value)> value) && {
  callback_ = std::move(value);
  return std::move(*this);
}
TimeInput &&TimeInput::style(TimeInputStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec TimeInput::spec() && {
  for (double metric :
       {style_.text_size, style_.segment_width, style_.height, style_.padding,
        style_.gap, style_.corner_radius, style_.border_width})
    if (!std::isfinite(metric) || metric < 0)
      throw std::invalid_argument(
          "TimeInput style metrics must be finite and nonnegative");
  Spec spec{[label = label_, value = value_, seconds = seconds_,
             clear = clearable_, callback = callback_, style = style_] {
              return std::make_unique<TimeFrame>(label, value, seconds, clear,
                                                 callback, style);
            },
            {}};
  spec.children_factory = [](Component &component) {
    return static_cast<TimeFrame &>(component).children();
  };
  return spec;
}
} // namespace ui
