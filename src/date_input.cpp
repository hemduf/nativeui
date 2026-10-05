#include "detail/widget_calendar.hpp"
#include "detail/widget_input_action.hpp"
#include <cmath>
#include <limits>
#include <nativeui/date_input.hpp>
#include <nativeui/detail/overlay_commands.hpp>
namespace ui {
namespace {
using Date = DateInput::Value;
float logical(double value) {
  return static_cast<float>(
      std::min(value, double(std::numeric_limits<float>::max())));
}
struct DateRuntime : std::enable_shared_from_this<DateRuntime> {
  explicit DateRuntime(Binding<Date> value)
      : source(std::move(value)), seen(source.get()), draft(seen) {}
  Binding<Date> source;
  Date seen;
  State<Date> draft;
  detail::CalendarConfig config;
  std::function<void(Date)> callback;
  std::function<void()> invalidate, layout;
  std::function<bool()> guard;
  std::optional<detail::OverlayComponentCommand> pending;
  OverlayHandle handle;
  NodeId node{kInvalidNodeId};
  std::uint64_t generation{};
  bool mounted{}, mutable_value{true}, live{};
  bool allowed() const {
    return mounted && mutable_value && source.valid() && (!guard || guard());
  }
  void sync() {
    if (!mounted)
      return;
    if (!source.valid()) {
      live = false;
      return;
    }
    const auto current = source.get();
    if (current == seen)
      return;
    ++generation;
    seen = current;
    draft.set(current);
    if (layout)
      layout();
    if (invalidate)
      invalidate();
  }
  void close() {
    if (!live && !handle.valid() && !pending)
      return;
    ++generation;
    live = false;
    const auto old = handle.valid() ? handle
                     : pending      ? pending->handle
                                    : OverlayHandle{};
    handle = {};
    pending =
        old.valid()
            ? std::optional{detail::OverlayComponentCommand::close_then_invoke(
                  old, node, false, {})}
            : std::nullopt;
    if (invalidate)
      invalidate();
  }
  void choose(Date next) {
    const auto keep = shared_from_this();
    if (!allowed())
      return;
    const auto expected = source.get();
    const auto serial = ++generation;
    auto notify = callback;
    auto model = source;
    const std::weak_ptr<DateRuntime> weak = keep;
    if (!allowed() || generation != serial || model.get() != expected)
      return;
    const auto old = handle;
    live = false;
    handle = {};
    auto publish = [weak, serial, expected, next, model,
                    notify = std::move(notify)]() mutable {
      const auto state = weak.lock();
      if (!state || !state->allowed() || state->generation != serial ||
          model.get() != expected)
        return;
      if (state->layout)
        state->layout();
      if (state->invalidate)
        state->invalidate();
      if (!state->allowed() || state->generation != serial ||
          model.get() != expected)
        return;
      if (next == expected)
        return;
      state->seen = next;
      model.set(next);
      if (state->allowed() && state->generation == serial &&
          model.get() == next && notify)
        notify(next);
    };
    pending = detail::OverlayComponentCommand::close_then_invoke(
        old, node, true, std::move(publish));
    if (invalidate)
      invalidate();
  }
};
class CalendarPopup final : public Component,
                            public detail::OverlayCommandSource {
public:
  explicit CalendarPopup(std::weak_ptr<DateRuntime> state)
      : state_(std::move(state)) {}
  bool clips_children() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &children) const override {
    return children.empty() ? Size{} : children.front().preferred;
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    if (!children.empty())
      children.front().bounds = bounds;
  }
  std::optional<detail::OverlayComponentCommand>
  take_overlay_command() override {
    const auto state = state_.lock();
    return state ? std::exchange(state->pending, {})
                 : std::optional<detail::OverlayComponentCommand>{};
  }
  void paint(PaintContext &) const override {}

private:
  std::weak_ptr<DateRuntime> state_;
};
class DateFrame final : public Component,
                        public detail::ThemeBinding,
                        public detail::OverlayCommandSource,
                        public detail::OverlayAnchorPolicy {
public:
  DateFrame(std::string label, Binding<Date> source, std::string placeholder,
            bool clearable, detail::CalendarConfig config,
            std::function<void(Date)> callback, DateInputStyle style)
      : label_(std::move(label)), placeholder_(std::move(placeholder)),
        style_(std::move(style)),
        state_(std::make_shared<DateRuntime>(std::move(source))),
        clearable_(clearable) {
    state_->config = std::move(config);
    state_->callback = std::move(callback);
  }
  bool focusable() const noexcept override { return true; }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool dismiss_overlay_on_tab() const noexcept override { return true; }
  bool dismiss_overlay_when_read_only() const noexcept override { return true; }
  bool overlay_session_valid() const noexcept override { return state_->live; }
  bool clips_children() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &) const override {
    return {
        std::max(
            logical(style_.minimum_width),
            TextService::measure(text(), logical(style_.text_size)).width +
                logical(2 * style_.padding + (clearable_ ? style_.height : 0))),
        logical(style_.height)};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    if (children.empty())
      return;
    const float width = std::min(bounds.w, logical(style_.height));
    children.front().bounds = {bounds.x + bounds.w - width, bounds.y, width,
                               bounds.h};
  }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->node = context.node_id();
    state_->guard = detail::InputMutationAccess::guard(context);
    state_->invalidate = context.invalidator();
    state_->layout = context.layout_invalidator();
    const std::weak_ptr<DateRuntime> weak = state_;
    subscription_ = state_->source.observe([weak](const auto &) {
      if (auto state = weak.lock())
        state->sync();
    });
  }
  void unmount(LifecycleContext &) override {
    state_->mounted = false;
    ++state_->generation;
    state_->live = false;
    state_->pending.reset();
    state_->handle = {};
    state_->guard = {};
    state_->invalidate = {};
    state_->layout = {};
    subscription_.reset();
  }
  void deactivate(LifecycleContext &context) override {
    press_.deactivate(context, false);
    state_->close();
    focused_ = false;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    focused_ = focused;
    press_.focus_changed(focused, context, false);
    context.invalidate();
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto state = state_;
    state->sync();
    if (!state->allowed()) {
      press_.cancel_pending_mutation(context, false);
      return EventResult::Handled;
    }
    const auto result = press_.input(event, context, true);
    if (!result.activate)
      return result.result;
    if (state->live) {
      state->close();
      return EventResult::Handled;
    }
    open(event.type == InputType::KeyDown ? event.key : Key::None, context);
    return EventResult::Handled;
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    if (action == SemanticAction::Activate) {
      if (state_->live)
        state_->close();
      else
        open(Key::None, context);
      return EventResult::Handled;
    }
    return EventResult::Ignored;
  }
  std::optional<detail::OverlayComponentCommand>
  take_overlay_command() override {
    return std::exchange(state_->pending, {});
  }
  std::vector<Spec> children() {
    if (!clearable_)
      return {};
    auto action = std::make_shared<detail::InputActionState>();
    const std::weak_ptr<DateRuntime> weak = state_;
    action->enabled = [weak] {
      const auto state = weak.lock();
      return state && state->allowed() && state->source.get().has_value();
    };
    action->generation = [weak] {
      const auto state = weak.lock();
      return state ? state->generation : 0;
    };
    action->context_action = [weak](InputContext &context) {
      if (const auto state = weak.lock()) {
        if (detail::InputMutationAccess::allowed(context))
          state->choose(std::nullopt);
      }
    };
    return {Spec{[action, style = style_.clear] {
                   return std::make_unique<detail::InputAction>(
                       "Effacer la date", "×", style, action, true, false);
                 },
                 {}}};
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Button;
    info.name = label_;
    info.text_value = text();
    info.focusable = true;
    info.focused = focused_;
    info.read_only = effective_read_only() || !state_->source.valid();
    info.enabled = effective_enabled();
    info.expanded = state_->live && state_->handle.valid()
                        ? SemanticExpandedState::Expanded
                        : SemanticExpandedState::Collapsed;
    if (state_->seen &&
        (!detail::calendar_day_valid(*state_->seen) ||
         (state_->config.minimum && *state_->seen < *state_->config.minimum) ||
         (state_->config.maximum && *state_->seen > *state_->config.maximum)))
      info.description = "Date indisponible";
    if (info.enabled)
      info.actions = {SemanticAction::Focus};
    if (info.enabled && !info.read_only)
      info.actions.push_back(SemanticAction::Activate);
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto bounds = context.bounds();
    auto &p = context.painter();
    auto clip = p.scoped_clip(bounds);
    p.fill_rounded_rect(
        bounds, logical(style_.corner_radius),
        style_.background.value_or(current_theme().palette.control_background));
    p.stroke_rounded_rect(
        bounds, logical(style_.corner_radius), logical(style_.border_width),
        focused_ ? style_.focus.value_or(current_theme().palette.focus)
                 : style_.border.value_or(current_theme().palette.border));
    const float reserved =
        clearable_ ? std::min(bounds.w, logical(style_.height)) : 0;
    auto textclip = p.scoped_clip(
        {bounds.x, bounds.y, std::max(0.f, bounds.w - reserved), bounds.h});
    p.text({bounds.x + logical(style_.padding), bounds.y + bounds.h * .5f},
           text(), logical(style_.text_size),
           state_->seen ? style_.text.value_or(current_theme().palette.text)
                        : style_.placeholder.value_or(
                              current_theme().palette.muted_text));
  }

private:
  std::string text() const {
    const auto formatted = detail::calendar_iso(state_->seen);
    return formatted.empty() ? placeholder_ : formatted;
  }
  void open(Key suppressed, InputContext &context) {
    const auto state = state_;
    if (!state->allowed() || state->pending)
      return;
    const auto serial = ++state->generation;
    state->draft.set(state->source.get());
    auto config = state->config;
    config.commit_navigation = false;
    config.suppressed = suppressed;
    const std::weak_ptr<DateRuntime> weak = state;
    config.picked = [weak](auto day) {
      if (const auto current = weak.lock())
        current->choose(day);
    };
    auto calendar = detail::calendar_spec(label_, state->draft.binding(),
                                          std::move(config));
    OverlaySpec overlay;
    overlay.anchor = state->node;
    overlay.mode = OverlayMode::Modal;
    overlay.placement = OverlayPlacement::Auto;
    overlay.dismiss_on_escape = true;
    overlay.dismiss_on_outside_pointer_down = true;
    overlay.content =
        Spec{[weak] { return std::make_unique<CalendarPopup>(weak); },
             {std::move(calendar)}};
    context.invalidate();
    if (!state->allowed() || state->generation != serial ||
        !detail::InputMutationAccess::allowed(context))
      return;
    state->live = true;
    state->pending = detail::OverlayComponentCommand::show(
        std::move(overlay), [weak, serial](OverlayHandle handle) {
          const auto current = weak.lock();
          if (!current)
            return;
          if (!current->mounted || current->generation != serial ||
              !current->live || !current->allowed()) {
            if (handle.valid())
              current->pending =
                  detail::OverlayComponentCommand::close_then_invoke(
                      handle, current->node, false, {});
            return;
          }
          current->handle = handle;
          if (!handle.valid())
            current->live = false;
        });
  }
  void retained_checkpoint() override {
    state_->sync();
    if (!state_->handle.valid() && !state_->pending)
      state_->live = false;
  }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    if (!state_->mutable_value)
      state_->live = false;
  }
  std::string label_, placeholder_;
  DateInputStyle style_;
  std::shared_ptr<DateRuntime> state_;
  bool clearable_{}, focused_{};
  detail::PressActivationState press_;
  Binding<Date>::Subscription subscription_;
};
} // namespace
DateInput::DateInput(std::string label, Binding<Value> value)
    : label_(std::move(label)), value_(std::move(value)) {}
DateInput::DateInput(std::string label, State<Value> &value)
    : DateInput(std::move(label), value.binding()) {}
DateInput &&DateInput::range(Value min, Value max) && {
  minimum_ = min;
  maximum_ = max;
  return std::move(*this);
}
DateInput &&DateInput::reference_day(std::chrono::sys_days value) && {
  reference_ = value;
  return std::move(*this);
}
DateInput &&DateInput::placeholder(std::string value) && {
  placeholder_ = std::move(value);
  return std::move(*this);
}
DateInput &&DateInput::clearable(bool value) && {
  clearable_ = value;
  return std::move(*this);
}
DateInput &&DateInput::on_change(std::function<void(Value)> value) && {
  callback_ = std::move(value);
  return std::move(*this);
}
DateInput &&DateInput::style(DateInputStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec DateInput::spec() && {
  detail::CalendarConfig config;
  config.minimum = minimum_;
  config.maximum = maximum_;
  config.reference = reference_;
  config.style = style_.calendar;
  detail::validate_calendar(config);
  for (double metric :
       {style_.minimum_width, style_.height, style_.padding, style_.text_size,
        style_.corner_radius, style_.border_width})
    if (!std::isfinite(metric) || metric < 0)
      throw std::invalid_argument(
          "DateInput metrics must be finite and nonnegative");
  Spec spec{[label = label_, value = value_, placeholder = placeholder_,
             clear = clearable_, config, callback = callback_, style = style_] {
              return std::make_unique<DateFrame>(
                  label, value, placeholder, clear, config, callback, style);
            },
            {}};
  spec.children_factory = [](Component &component) {
    return static_cast<DateFrame &>(component).children();
  };
  return spec;
}
} // namespace ui
