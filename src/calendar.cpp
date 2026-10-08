#include "detail/widget_calendar.hpp"
#include "detail/widget_input_action.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <nativeui/detail/dynamic_source.hpp>
namespace ui {
namespace {
using Day = std::chrono::sys_days;
using Date = Calendar::Value;
constexpr std::array<const char *, 12> months{
    "January", "February", "March",      "April",   "May",      "June",
    "July", "August",    "September", "October", "November", "December"};
Day first_day() { return Day{std::chrono::year{1} / 1 / 1}; }
Day last_day() { return Day{std::chrono::year{9999} / 12 / 31}; }
float logical(double value) {
  return static_cast<float>(
      std::min(value, double(std::numeric_limits<float>::max())));
}
Day month_start(Day day) {
  auto ymd = std::chrono::year_month_day{day};
  return Day{ymd.year() / ymd.month() / 1};
}
Day month_move(Day day, int direction) {
  const auto ymd = std::chrono::year_month_day{day};
  auto month = ymd.year() / ymd.month() + std::chrono::months{direction};
  if (month < std::chrono::year{1} / 1)
    return first_day();
  if (month > std::chrono::year{9999} / 12)
    return last_day();
  const auto end = std::chrono::year_month_day{Day{month / std::chrono::last}};
  return Day{month / std::chrono::day{
                         std::min(unsigned(ymd.day()), unsigned(end.day()))}};
}
std::string day_name(Day day) {
  if (!detail::calendar_day_valid(day))
    return "Date out of range";
  const auto ymd = std::chrono::year_month_day{day};
  return std::to_string(unsigned(ymd.day())) + " " +
         months[unsigned(ymd.month()) - 1] + " " +
         std::to_string(int(ymd.year()));
}
struct GridGeometry {
  Rect previous, next, weekdays, grid;
  float cell_width{}, cell_height{}, gap{};
  Rect cell(std::size_t index) const {
    return {grid.x + static_cast<float>(index % 7) * (cell_width + gap),
            grid.y + static_cast<float>(index / 7) * (cell_height + gap),
            cell_width, cell_height};
  }
  std::optional<std::size_t> at(Point point) const {
    if (cell_width <= 0 || cell_height <= 0 || !grid.contains(point))
      return {};
    for (std::size_t index = 0; index < 42; ++index)
      if (cell(index).contains(point))
        return index;
    return {};
  }
};
GridGeometry geometry(Rect bounds, const CalendarStyle &style) {
  const float padding =
      std::min({logical(style.padding), std::max(0.f, bounds.w * .5f),
                std::max(0.f, bounds.h * .5f)});
  const float w = std::max(0.f, bounds.w - 2 * padding),
              h = std::max(0.f, bounds.h - 2 * padding);
  const float header = std::min(logical(style.header_height), h),
              weekdays = std::min(logical(style.weekday_height),
                                  std::max(0.f, h - header));
  const float gap = std::min(logical(style.gap), w / 6);
  const float remaining = std::max(0.f, h - header - weekdays);
  GridGeometry result;
  const float button = std::min(header, w * .5f);
  result.previous = {bounds.x + padding, bounds.y + padding, button, header};
  result.next = {bounds.x + padding + w - button, bounds.y + padding, button,
                 header};
  result.weekdays = {bounds.x + padding, bounds.y + padding + header, w,
                     weekdays};
  result.grid = {bounds.x + padding, bounds.y + padding + header + weekdays, w,
                 remaining};
  result.gap = std::min(gap, remaining / 5);
  result.cell_width = std::max(0.f, (w - 6 * result.gap) / 7);
  result.cell_height = std::max(0.f, (remaining - 5 * result.gap) / 6);
  return result;
}
struct CalendarRuntime : std::enable_shared_from_this<CalendarRuntime> {
  CalendarRuntime(Binding<Date> value, detail::CalendarConfig config)
      : source(std::move(value)), seen(source.get()),
        config(std::move(config)) {
    prepare(seen);
  }
  Binding<Date> source;
  Date seen;
  detail::CalendarConfig config;
  Day cursor{}, month{};
  std::array<Day, 42> days;
  std::function<void()> invalidate, structure, release;
  std::function<bool()> guard;
  std::uint64_t generation{};
  bool mounted{}, mutable_value{true}, armed{};
  Day armed_day{};
  std::uint64_t armed_generation{};
  bool selectable(Day day) const {
    return detail::calendar_day_valid(day) &&
           (!config.minimum || day >= *config.minimum) &&
           (!config.maximum || day <= *config.maximum);
  }
  Day clamp(Day day) const {
    return std::clamp(day, config.minimum.value_or(first_day()),
                      config.maximum.value_or(last_day()));
  }
  bool allowed() const {
    return mounted && mutable_value && source.valid() && (!guard || guard());
  }
  void end_contact() noexcept {
    armed = false;
    auto old = std::move(release);
    if (old)
      try {
        old();
      } catch (...) {
      }
  }
  void prepare(Date value) {
    cursor =
        clamp(value && detail::calendar_day_valid(*value) ? *value
                                                          : config.reference);
    month = month_start(cursor);
    const auto start =
        month -
        std::chrono::days{
            static_cast<int>(std::chrono::weekday{month}.iso_encoding()) - 1};
    for (std::size_t index = 0; index < days.size(); ++index)
      days[index] = start + std::chrono::days{static_cast<int>(index)};
  }
  void sync() {
    if (!mounted)
      return;
    const auto value = source.get();
    if (value == seen)
      return;
    end_contact();
    ++generation;
    seen = value;
    const auto old = month;
    prepare(value);
    if (month != old && structure)
      structure();
    if (invalidate)
      invalidate();
  }
  void publish(Day selected, const std::function<bool()> &permission) {
    const auto keep = shared_from_this();
    if (!allowed() || !selectable(selected) || (permission && !permission()))
      return;
    const auto expected = source.get();
    const auto serial = generation;
    auto notify = config.changed;
    auto picked = config.picked;
    auto model = source;
    if (invalidate)
      invalidate();
    if (!allowed() || generation != serial || model.get() != expected ||
        (permission && !permission()))
      return;
    if (picked) {
      picked(selected);
      return;
    }
    const Date next = selected;
    if (next == expected)
      return;
    seen = next;
    model.set(next);
    if (mounted && generation == serial && allowed() && model.get() == next &&
        (!permission || permission()) && notify)
      notify(next);
  }
  bool move(Day next, bool navigation,
            const std::function<bool()> &permission = {}) {
    const auto keep = shared_from_this();
    if (!mounted)
      return false;
    const auto expected = source.get();
    const auto revision = source.revision();
    const auto before_release = generation;
    end_contact();
    if (!mounted || generation != before_release ||
        source.revision() != revision || !source.valid()) {
      sync();
      return false;
    }
    next = clamp(next);
    if (next != cursor) {
      ++generation;
      const auto old = month;
      cursor = next;
      month = month_start(next);
      const auto start =
          month -
          std::chrono::days{
              static_cast<int>(std::chrono::weekday{month}.iso_encoding()) - 1};
      std::array<Day, 42> owned;
      for (std::size_t index = 0; index < owned.size(); ++index)
        owned[index] = start + std::chrono::days{static_cast<int>(index)};
      days = owned;
      const auto serial = generation;
      if (month != old && structure)
        structure();
      if (invalidate)
        invalidate();
      if (!mounted || generation != serial || cursor != next ||
          source.get() != expected)
        return false;
    }
    if (navigation && config.commit_navigation)
      publish(next, permission);
    return true;
  }
};
class CalendarCell final : public Component {
public:
  CalendarCell(std::shared_ptr<CalendarRuntime> state, Day day)
      : state_(std::move(state)), day_(day), name_(day_name(day)) {}
  Size measure(const std::vector<ChildMetrics> &) const override { return {}; }
  bool pointer_targetable() const noexcept override { return false; }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Button;
    info.name = name_;
    info.text_value = name_;
    info.enabled = state_->selectable(day_) && effective_enabled();
    info.read_only = effective_read_only() || !state_->source.valid();
    info.selected = state_->seen && *state_->seen == day_;
    if (state_->cursor == day_)
      info.description = "Cursor day";
    if (info.enabled && !info.read_only)
      info.actions = {SemanticAction::Activate, SemanticAction::Select};
    return info;
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    const auto state = state_;
    const auto day = day_;
    const auto serial = state->generation;
    if (action != SemanticAction::Activate && action != SemanticAction::Select)
      return EventResult::Ignored;
    auto permission = detail::InputMutationAccess::guard(context);
    if (state->mounted && state->generation == serial &&
        state->selectable(day) && state->move(day, false, permission))
      state->publish(day, permission);
    return EventResult::Handled;
  }
  void paint(PaintContext &) const override {}

private:
  std::shared_ptr<CalendarRuntime> state_;
  Day day_;
  std::string name_;
};
class CalendarGrid final : public Component,
                           public detail::ThemeBinding,
                           public detail::DynamicChildrenSource {
public:
  CalendarGrid(std::string label, Binding<Date> source,
               detail::CalendarConfig config)
      : label_(std::move(label)), state_(std::make_shared<CalendarRuntime>(
                                      std::move(source), std::move(config))) {}
  bool focusable() const noexcept override { return true; }
  bool pointer_targetable() const noexcept override { return true; }
  bool clips_children() const noexcept override { return true; }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool cancel_capture_on_read_only() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &) const override {
    const auto &s = state_->config.style;
    return {logical(2 * s.padding + 7 * s.cell_width + 6 * s.gap),
            logical(2 * s.padding + s.header_height + s.weekday_height +
                    6 * s.cell_height + 5 * s.gap)};
  }
  std::vector<std::string> desired_keys() const override {
    std::vector<std::string> keys{"previous", "next"};
    for (auto day : state_->days)
      keys.push_back(std::to_string(day.time_since_epoch().count()));
    return keys;
  }
  std::vector<detail::DynamicChildSpec> desired_children() const override {
    std::vector<detail::DynamicChildSpec> children;
    const auto keys = desired_keys();
    const std::weak_ptr<CalendarRuntime> weak = state_;
    for (int direction : {-1, 1}) {
      auto action = std::make_shared<detail::InputActionState>();
      action->enabled = [weak] {
        const auto state = weak.lock();
        return state && state->source.valid();
      };
      action->generation = [weak] {
        const auto state = weak.lock();
        return state ? state->generation : 0;
      };
      action->context_action = [weak, direction](InputContext &context) {
        if (const auto state = weak.lock()) {
          auto permission = detail::InputMutationAccess::guard(context);
          state->move(month_move(state->cursor, direction), true, permission);
        }
      };
      const auto style = state_->config.style.navigation;
      children.push_back(
          {keys[direction < 0 ? 0 : 1],
           Spec{[action, direction, style] {
                  return std::make_unique<detail::InputAction>(
                      direction < 0 ? "Previous month" : "Next month",
                      direction < 0 ? "‹" : "›", style, action, true, true);
                },
                {}}});
    }
    for (std::size_t index = 0; index < state_->days.size(); ++index)
      children.push_back(
          {keys[index + 2], Spec{[state = state_, day = state_->days[index]] {
                                   return std::make_unique<CalendarCell>(state,
                                                                         day);
                                 },
                                 {}}});
    return children;
  }
  void set_structure_invalidator(std::function<void()> callback) override {
    state_->structure = std::move(callback);
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    const auto layout = geometry(bounds, state_->config.style);
    if (children.size() < 2)
      return;
    children[0].bounds = layout.previous;
    children[1].bounds = layout.next;
    for (std::size_t index = 2; index < children.size(); ++index)
      children[index].bounds = layout.cell(index - 2);
  }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->guard = detail::InputMutationAccess::guard(context);
    state_->invalidate = context.invalidator();
    const std::weak_ptr<CalendarRuntime> weak = state_;
    subscription_ = state_->source.observe([weak](const auto &) {
      if (auto state = weak.lock())
        state->sync();
    });
  }
  void unmount(LifecycleContext &) override {
    state_->end_contact();
    state_->mounted = false;
    ++state_->generation;
    state_->guard = {};
    state_->invalidate = {};
    state_->structure = {};
    subscription_.reset();
  }
  void deactivate(LifecycleContext &) override {
    state_->end_contact();
    ++state_->generation;
    focused_ = false;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    focused_ = focused;
    if (!focused)
      state_->end_contact();
    context.invalidate();
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto state = state_;
    state->sync();
    if (state->config.suppressed != Key::None &&
        event.key == state->config.suppressed) {
      if (event.type == InputType::KeyUp)
        state->config.suppressed = Key::None;
      if (event.type == InputType::KeyUp || event.type == InputType::KeyDown)
        return EventResult::Handled;
    }
    if (event.type == InputType::PointerCancel ||
        (event.type == InputType::KeyDown && event.key == Key::Escape)) {
      state->end_contact();
      context.invalidate();
      return EventResult::Handled;
    }
    if (event.type == InputType::PointerDown) {
      const auto index =
          geometry(context.bounds(), state->config.style).at(event.position);
      if (index && state->selectable(state->days[*index]) && state->allowed()) {
        state->end_contact();
        state->armed = true;
        state->armed_day = state->days[*index];
        state->armed_generation = state->generation;
        state->release = context.pointer_releaser();
        context.capture_pointer();
      }
      return EventResult::Handled;
    }
    if (event.type == InputType::PointerUp) {
      const auto index =
          geometry(context.bounds(), state->config.style).at(event.position);
      const auto day = state->armed_day;
      const auto serial = state->armed_generation;
      const bool choose = state->armed && index && state->days[*index] == day;
      auto permission = detail::InputMutationAccess::guard(context);
      state->end_contact();
      if (choose && state->generation == serial &&
          state->move(day, false, permission))
        state->publish(day, permission);
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown) {
      auto permission = detail::InputMutationAccess::guard(context);
      if (event.key == Key::Enter || event.key == Key::Space) {
        state->end_contact();
        state->publish(state->cursor, permission);
        return EventResult::Handled;
      }
      auto next = state->cursor;
      bool navigation = true;
      switch (event.key) {
      case Key::Left:
        next -= std::chrono::days{1};
        break;
      case Key::Right:
        next += std::chrono::days{1};
        break;
      case Key::Up:
        next -= std::chrono::days{7};
        break;
      case Key::Down:
        next += std::chrono::days{7};
        break;
      case Key::Home:
        next = month_start(next);
        break;
      case Key::End: {
        const auto ymd = std::chrono::year_month_day{next};
        next = Day{ymd.year() / ymd.month() / std::chrono::last};
        break;
      }
      case Key::PageUp:
        next = month_move(next, -1);
        break;
      case Key::PageDown:
        next = month_move(next, 1);
        break;
      default:
        navigation = false;
        break;
      }
      if (navigation) {
        state->move(next, true, permission);
        return EventResult::Handled;
      }
    }
    return EventResult::Ignored;
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Group;
    info.name = label_;
    info.focusable = true;
    info.focused = focused_;
    info.read_only = effective_read_only() || !state_->source.valid();
    info.description = "Cursor: " + day_name(state_->cursor);
    if (state_->seen && (!detail::calendar_day_valid(*state_->seen) ||
                         !state_->selectable(*state_->seen)))
      info.description += "; selection unavailable";
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto &s = state_->config.style;
    const auto bounds = context.bounds();
    const auto layout = geometry(bounds, s);
    auto &p = context.painter();
    auto clip = p.scoped_clip(bounds);
    p.fill_rounded_rect(bounds, logical(s.corner_radius),
                        s.background.value_or(current_theme().palette.surface));
    const auto ymd = std::chrono::year_month_day{state_->month};
    const auto title = std::string(months[unsigned(ymd.month()) - 1]) + " " +
                       std::to_string(int(ymd.year()));
    p.text({bounds.x + bounds.w * .5f,
            layout.previous.y + layout.previous.h * .5f},
           title, logical(s.text_size),
           s.text.value_or(current_theme().palette.text), TextAlign::Center);
    constexpr std::array<const char *, 7> weekdays{"Mon", "Tue", "Wed", "Thu",
                                                   "Fri", "Sat", "Sun"};
    for (std::size_t index = 0; index < 7; ++index)
      p.text({layout.grid.x +
                  (static_cast<float>(index) + .5f) * layout.cell_width +
                  static_cast<float>(index) * layout.gap,
              layout.weekdays.y + layout.weekdays.h * .5f},
             weekdays[index], logical(s.text_size),
             s.muted_text.value_or(current_theme().palette.muted_text),
             TextAlign::Center);
    for (std::size_t index = 0; index < state_->days.size(); ++index) {
      const auto day = state_->days[index];
      const auto cell = layout.cell(index);
      if (!detail::calendar_day_valid(day))
        continue;
      const auto date = std::chrono::year_month_day{day};
      const bool chosen = state_->seen && *state_->seen == day;
      if (chosen)
        p.fill_rounded_rect(
            cell, logical(s.corner_radius),
            s.selected.value_or(current_theme().palette.selection));
      if (state_->cursor == day)
        p.stroke_rounded_rect(cell, logical(s.corner_radius), 1,
                              s.cursor.value_or(current_theme().palette.focus));
      if (state_->config.today && *state_->config.today == day)
        p.line({cell.x + cell.w * .3f, cell.y + cell.h - 3},
               {cell.x + cell.w * .7f, cell.y + cell.h - 3}, 2,
               s.today.value_or(current_theme().palette.accent));
      const auto text =
          !state_->selectable(day) ? current_theme().palette.disabled
          : date.month() != ymd.month()
              ? s.muted_text.value_or(current_theme().palette.muted_text)
              : s.text.value_or(current_theme().palette.text);
      p.text({cell.x + cell.w * .5f, cell.y + cell.h * .5f},
             std::to_string(unsigned(date.day())), logical(s.text_size), text,
             TextAlign::Center);
    }
  }

private:
  void retained_checkpoint() override { state_->sync(); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    if (!state_->mutable_value)
      state_->end_contact();
  }
  std::string label_;
  std::shared_ptr<CalendarRuntime> state_;
  Binding<Date>::Subscription subscription_;
  bool focused_{};
};
} // namespace
namespace detail {
bool calendar_day_valid(Day value) noexcept {
  return value >= first_day() && value <= last_day();
}
std::string calendar_iso(Date value) {
  if (!value || !calendar_day_valid(*value))
    return {};
  const auto ymd = std::chrono::year_month_day{*value};
  // Cover the full chrono field ranges even without the valid-date bound.
  std::array<char, 16> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%04d-%02u-%02u", int(ymd.year()),
                unsigned(ymd.month()), unsigned(ymd.day()));
  return buffer.data();
}
void validate_calendar(const CalendarConfig &value) {
  if (!calendar_day_valid(value.reference) ||
      (value.minimum && !calendar_day_valid(*value.minimum)) ||
      (value.maximum && !calendar_day_valid(*value.maximum)) ||
      (value.today && !calendar_day_valid(*value.today)) ||
      (value.minimum && value.maximum && *value.minimum > *value.maximum))
    throw std::invalid_argument(
        "Calendar dates must be in years 1..9999 with minimum <= maximum");
  const auto &s = value.style;
  for (double metric :
       {s.cell_width, s.cell_height, s.header_height, s.weekday_height,
        s.padding, s.gap, s.text_size, s.corner_radius})
    if (!std::isfinite(metric) || metric < 0)
      throw std::invalid_argument(
          "Calendar metrics must be finite and nonnegative");
}
Spec calendar_spec(std::string label, Binding<Date> value,
                   CalendarConfig config) {
  validate_calendar(config);
  Spec spec{[label = std::move(label), value = std::move(value),
             config = std::move(config)] {
              return std::make_unique<CalendarGrid>(label, value, config);
            },
            {}};
  spec.children_factory = [](Component &component) {
    auto desired = static_cast<CalendarGrid &>(component).desired_children();
    std::vector<Spec> children;
    children.reserve(desired.size());
    for (auto &child : desired) {
      child.spec.retained_key = std::move(child.key);
      children.push_back(std::move(child.spec));
    }
    return children;
  };
  return spec;
}
} // namespace detail
Calendar::Calendar(std::string label, Binding<Value> value)
    : label_(std::move(label)), value_(std::move(value)) {}
Calendar::Calendar(std::string label, State<Value> &value)
    : Calendar(std::move(label), value.binding()) {}
Calendar &&Calendar::range(Value min, Value max) && {
  minimum_ = min;
  maximum_ = max;
  return std::move(*this);
}
Calendar &&Calendar::reference_day(Day value) && {
  reference_ = value;
  return std::move(*this);
}
Calendar &&Calendar::today(Value value) && {
  today_ = value;
  return std::move(*this);
}
Calendar &&Calendar::commit_on_navigation(bool value) && {
  commit_ = value;
  return std::move(*this);
}
Calendar &&Calendar::on_change(std::function<void(Value)> value) && {
  callback_ = std::move(value);
  return std::move(*this);
}
Calendar &&Calendar::style(CalendarStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec Calendar::spec() && {
  return detail::calendar_spec(std::move(label_), value_,
                               {minimum_,
                                maximum_,
                                today_,
                                reference_,
                                commit_,
                                std::move(style_),
                                std::move(callback_),
                                {}});
}
} // namespace ui
