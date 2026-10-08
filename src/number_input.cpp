#include "detail/widget_number_parse.hpp"
#include "detail/widget_stepper_policy.hpp"
#include "detail/widget_text_input_policy.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <nativeui/number_input.hpp>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace ui {
namespace detail {
std::optional<double> parse_decimal_number(std::string_view text) noexcept {
  const auto digit = [](char c) { return c >= '0' && c <= '9'; };
  std::size_t position{};
  if (!text.empty() && (text.front() == '+' || text.front() == '-'))
    ++position;
  bool mantissa_digit{}, nonzero_mantissa{};
  const auto consume_digits = [&] {
    while (position < text.size() && digit(text[position])) {
      mantissa_digit = true;
      nonzero_mantissa |= text[position] != '0';
      ++position;
    }
  };
  consume_digits();
  if (position < text.size() && text[position] == '.') {
    ++position;
    consume_digits();
  }
  if (!mantissa_digit)
    return {};
  if (position < text.size() &&
      (text[position] == 'e' || text[position] == 'E')) {
    ++position;
    if (position < text.size() &&
        (text[position] == '+' || text[position] == '-'))
      ++position;
    const auto exponent_start = position;
    while (position < text.size() && digit(text[position]))
      ++position;
    if (position == exponent_start)
      return {};
  }
  if (position != text.size())
    return {};
  // Exact zero remains representable even with an extreme exponent. Exponent
  // digits must never be mistaken for a nonzero mantissa.
  if (!nonzero_mantissa)
    return text.front() == '-' ? -0.0 : 0.0;
  try {
    // Floating from_chars is absent from older Apple standard libraries.
    // Never change the host's locale or accept locale-specific punctuation.
    std::istringstream input{std::string{text.data(), text.size()}};
    input.imbue(std::locale::classic());
    double value{};
    input >> std::noskipws >> value;
    if (!input.eof() || input.bad() || !std::isfinite(value) || value == 0.0)
      return {};
    // libc++/MSVC can report ERANGE as failbit for a representable subnormal,
    // including a boundary rounded to minimum normal. Overflow remains invalid.
    if (input.fail() && std::abs(value) > std::numeric_limits<double>::min())
      return {};
    return value;
  } catch (...) {
    // No source edit has been accepted; a later draft can parse normally.
    return {};
  }
}
} // namespace detail
namespace {
bool ascii_space(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' ||
         c == '\v';
}
std::optional<double> parse_number(std::string_view text, double minimum,
                                   double maximum) noexcept {
  while (!text.empty() && ascii_space(text.front()))
    text.remove_prefix(1);
  while (!text.empty() && ascii_space(text.back()))
    text.remove_suffix(1);
  const auto value = detail::parse_decimal_number(text);
  if (!value || *value < minimum || *value > maximum)
    return {};
  return value;
}
std::string format_number(double value, unsigned digits) {
  std::array<char, 768> buffer{};
  const auto result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                    std::chars_format::fixed, static_cast<int>(digits));
  if (result.ec != std::errc{})
    throw std::runtime_error("NumberInput formatting failed");
  return {buffer.data(), result.ptr};
}
unsigned precision_from_step(double step) {
  std::array<char, 64> buffer{};
  const auto result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), step);
  if (result.ec != std::errc{})
    return 17;
  const std::string_view text{
      buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())};
  const auto exponent = text.find_first_of("eE");
  const auto dot = text.find('.');
  const auto end = exponent == std::string_view::npos ? text.size() : exponent;
  int decimals =
      dot == std::string_view::npos ? 0 : static_cast<int>(end - dot - 1);
  if (exponent != std::string_view::npos) {
    int power{};
    auto exp = text.substr(exponent + 1);
    if (!exp.empty() && exp.front() == '+')
      exp.remove_prefix(1);
    (void)std::from_chars(exp.data(), exp.data() + exp.size(), power);
    decimals -= power;
  }
  return static_cast<unsigned>(std::clamp(decimals, 0, 17));
}
bool same_double(double a, double b) noexcept {
  return a == b || (std::isnan(a) && std::isnan(b));
}
struct NumberState : std::enable_shared_from_this<NumberState> {
  NumberState(Binding<double> value, double lo, double hi, double increment,
              unsigned digits)
      : source(std::move(value)), minimum(lo), maximum(hi), step(increment),
        seen(source.get()), baseline(effective()), precision(digits),
        draft(format_number(effective(), precision)) {
    accepted_number = parse_number(draft.get(), minimum, maximum);
    if (std::isfinite(source.get()) && source.get() >= minimum &&
        source.get() <= maximum)
      accepted_number = source.get();
    else
      accepted_number.reset();
  }
  Binding<double> source;
  double minimum{}, maximum{}, step{}, seen{}, baseline{};
  unsigned precision{};
  State<std::string> draft;
  std::shared_ptr<detail::TextInputPolicy> policy;
  std::weak_ptr<detail::TextInputSession> editor;
  std::function<void(double)> submit;
  std::function<void()> invalidate;
  std::uint64_t generation{}, edit_generation{};
  std::optional<double> accepted_number;
  bool mounted{}, mutable_value{true}, formatting{}, invalid{}, enter_down{};
  double effective() const noexcept {
    return std::isfinite(source.get())
               ? std::clamp(source.get(), minimum, maximum)
               : minimum;
  }
  bool can_write() const noexcept {
    return mounted && mutable_value && source.valid() && minimum < maximum;
  }
  bool set_draft(std::string text) {
    const auto serial = generation;
    if (auto session = editor.lock()) {
      session->cancel_capture();
      if (!mounted || generation != serial)
        return false;
      session->replace(text);
    }
    const bool before = std::exchange(formatting, true);
    try {
      draft.set(std::move(text));
    } catch (...) {
      formatting = before;
      throw;
    }
    formatting = before;
    return mounted && generation == serial;
  }
  void presentation_validity(bool value) {
    invalid = value;
    policy->border_override = value ? invalid_color : std::optional<Color>{};
  }
  std::optional<Color> invalid_color;
  void sync() {
    if (!mounted)
      return;
    policy->read_only = !can_write();
    const double current = source.get();
    if (!same_double(current, seen)) {
      auto text = format_number(effective(), precision);
      const auto previous_seen = seen, previous_baseline = baseline;
      const auto previous_accepted = accepted_number;
      const bool previous_invalid = invalid;
      const auto serial = ++generation;
      seen = current;
      baseline = effective();
      accepted_number =
          std::isfinite(current) && current >= minimum && current <= maximum
              ? std::optional<double>{current}
              : std::optional<double>{};
      presentation_validity(!accepted_number);

      // Preserve reentrant source-notification coalescing by marking 'seen'
      // before replacing editor text. If replacement fails, restore the old
      // marker so the next retained checkpoint retries the committed source.
      // A newer nested generation must never be rolled back by this older one.
      const auto restore_unpublished = [&] {
        if (generation != serial) return;
        seen = previous_seen;
        baseline = previous_baseline;
        accepted_number = previous_accepted;
        presentation_validity(previous_invalid);
      };
      try {
        if (!set_draft(std::move(text))) {
          restore_unpublished();
          return;
        }
      } catch (...) {
        restore_unpublished();
        throw;
      }
      if (invalidate)
        invalidate();
    }
    if (auto session = editor.lock()) {
      const auto snapshot = session->snapshot();
      if (snapshot.edit_generation != edit_generation) {
        // A prior editor publication/hook failed. Read the committed numeric
        // source only; an aborted publication never becomes an implicit retry.
        edit_generation = snapshot.edit_generation;
        accepted_number = parse_number(snapshot.text, minimum, maximum)
                              ? std::optional<double>{effective()}
                              : std::optional<double>{};
        presentation_validity(!accepted_number);
      }
      session->refresh_source();
    }
  }
  void edited(detail::TextInputSnapshot snapshot) {
    edit_generation = snapshot.edit_generation;
    if (formatting || !can_write())
      return;
    const auto parsed = parse_number(snapshot.text, minimum, maximum);
    const double expected = source.get();
    const auto serial = ++generation;
    accepted_number = parsed;
    presentation_validity(!parsed.has_value());
    if (invalidate)
      invalidate();
    if (!parsed || !can_write() || generation != serial ||
        !same_double(source.get(), expected) ||
        (snapshot.mutation_guard && !snapshot.mutation_guard()))
      return;
    seen = *parsed;
    auto copy = source;
    copy.set(*parsed);
  }
  void step_to(double value, std::function<bool()> guard = {}) {
    if (!can_write())
      return;
    // Validate the future display before any publication. An invalidator can
    // throw, retire the editor, or synchronously change the backing value.
    // Do not stage the draft or accepted_number ahead of that boundary:
    // source observers/retained checkpoints perform authoritative reconciliation.
    auto text = format_number(value, precision);
    const double expected = source.get();
    const auto serial = ++generation;
    if (invalidate)
      invalidate();
    if (!can_write() || generation != serial ||
        !same_double(source.get(), expected) || (guard && !guard()))
      return;
    if (same_double(value, expected)) {
      // An edge step still normalizes an invalid draft, without a value write.
      if (set_draft(std::move(text))) {
        accepted_number = value;
        presentation_validity(false);
      }
      return;
    }
    auto copy = source;
    copy.set(value);
  }
  std::optional<EventResult> input(const InputEvent &event,
                                   InputContext &context,
                                   const detail::TextInputSnapshot &snapshot) {
    if (event.type == InputType::KeyUp && event.key == Key::Enter) {
      enter_down = false;
      return EventResult::Handled;
    }
    const bool cancel =
        (event.type == InputType::KeyDown && event.key == Key::Escape) ||
        (event.type == InputType::Command && event.command == Command::Cancel);
    const bool accept =
        (event.type == InputType::KeyDown && event.key == Key::Enter) ||
        (event.type == InputType::Command && event.command == Command::Submit);
    if (snapshot.composition_active) {
      if (cancel) {
        if (auto session = editor.lock()) {
          session->cancel_capture();
          session->cancel_composition();
        }
        context.invalidate();
        return EventResult::Handled;
      }
      if (accept || (event.type == InputType::KeyDown &&
                     (event.key == Key::Up || event.key == Key::Down)))
        return EventResult::Handled;
      return {};
    }
    if (cancel) {
      if (!can_write() || snapshot.read_only)
        return EventResult::Handled;
      const double target = baseline, expected = source.get();
      auto text = format_number(target, precision);
      const auto serial = ++generation;
      accepted_number = target;
      presentation_validity(false);
      if (!set_draft(std::move(text)))
        return EventResult::Handled;
      if (auto session = editor.lock())
        session->reset_baseline();
      context.invalidate();
      if (can_write() && generation == serial &&
          same_double(source.get(), expected) &&
          detail::InputMutationAccess::allowed(context)) {
        seen = target;
        auto copy = source;
        copy.set(target);
      }
      return EventResult::Handled;
    }
    if (accept) {
      if (!can_write() || snapshot.read_only)
        return EventResult::Handled;
      const std::optional<double> value =
          accepted_number && std::isfinite(source.get()) &&
                  source.get() >= minimum && source.get() <= maximum
              ? std::optional<double>{source.get()}
              : std::optional<double>{};
      if (!value) {
        presentation_validity(true);
        context.invalidate();
        return EventResult::Handled;
      }
      if (event.type == InputType::KeyDown && std::exchange(enter_down, true))
        return EventResult::Handled;
      auto text = format_number(*value, precision);
      const auto serial = generation;
      auto callback = submit;
      if (!can_write() || generation != serial)
        return EventResult::Handled;
      baseline = *value;
      presentation_validity(false);
      set_draft(std::move(text));
      if (auto session = editor.lock())
        session->reset_baseline();
      context.invalidate();
      if (can_write() && generation == serial && callback)
        callback(*value);
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown &&
        (event.key == Key::Up || event.key == Key::Down)) {
      if (can_write() && !snapshot.read_only)
        step_to(detail::StepperAccess::stepped_value(
                    source.get(), minimum, maximum, step,
                    event.key == Key::Up ? 1 : -1),
                detail::InputMutationAccess::guard(context));
      return EventResult::Handled;
    }
    return {};
  }
  void focus(bool focused, detail::TextInputSnapshot snapshot) {
    if (!mounted)
      return;
    if (focused) {
      baseline = effective();
      enter_down = false;
      return;
    }
    enter_down = false;
    if (!snapshot.allow_edit_commit)
      return;
    auto text = format_number(effective(), precision);
    accepted_number = std::isfinite(source.get()) && source.get() >= minimum &&
                              source.get() <= maximum
                          ? std::optional<double>{source.get()}
                          : std::optional<double>{};
    presentation_validity(!accepted_number);
    set_draft(std::move(text));
    if (invalidate)
      invalidate();
  }
};
class NumberComponent final : public Component, public detail::ThemeBinding {
public:
  NumberComponent(std::string label, Binding<double> source, double lo,
                  double hi, double step, unsigned precision,
                  std::function<void(double)> submit, NumberInputStyle style)
      : label_(std::move(label)), style_(std::move(style)),
        state_(std::make_shared<NumberState>(std::move(source), lo, hi, step,
                                             precision)) {
    state_->submit = std::move(submit);
    state_->policy = std::make_shared<detail::TextInputPolicy>();
    state_->invalid_color =
        style_.invalid_color.value_or(Color{0.85f, 0.2f, 0.2f, 1});
    state_->presentation_validity(!std::isfinite(state_->source.get()));
    const std::weak_ptr<NumberState> weak = state_;
    state_->policy->before_input =
        [weak](const InputEvent &event, InputContext &context,
               const detail::TextInputSnapshot &snapshot)
        -> std::optional<EventResult> {
      if (auto state = weak.lock())
        return state->input(event, context, snapshot);
      return EventResult::Ignored;
    };
    state_->policy->committed_edit =
        [weak](detail::TextInputSnapshot snapshot) {
          if (auto state = weak.lock())
            state->edited(std::move(snapshot));
        };
    state_->policy->focus_changed = [weak](bool focus,
                                           detail::TextInputSnapshot snapshot) {
      if (auto state = weak.lock())
        state->focus(focus, std::move(snapshot));
    };
  }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool clips_children() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &children) const override {
    if (children.size() != 2)
      return {};
    return {children[0].preferred.w + children[1].preferred.w +
                static_cast<float>(style_.gap),
            std::max(children[0].preferred.h, children[1].preferred.h)};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &children,
                       std::vector<ChildPlacement> &placements) const override {
    if (placements.size() != 2)
      return;
    std::optional<std::pair<float, float>> geometry;
    if (const auto editor = state_->editor.lock())
      geometry = editor->field_geometry();
    if (!geometry) {
      const auto text_style = resolve_text_input_style(
          default_text_input_style(current_theme()), style_.text_input,
          VisualState{});
      geometry = std::pair{text_style.field_top, text_style.field_height};
    }
    float field_top = geometry->first;
    float field_height = geometry->second;
    const float stepper = std::min(bounds.w, children[1].preferred.w),
                gap = std::min(static_cast<float>(style_.gap),
                               std::max(0.0f, bounds.w - stepper));
    field_top = std::clamp(field_top, 0.0f, bounds.h);
    field_height = std::clamp(field_height, 0.0f, bounds.h - field_top);
    const float stepper_height = std::min(field_height, children[1].preferred.h);
    placements[0].bounds = {bounds.x, bounds.y,
                            std::max(0.0f, bounds.w - stepper - gap), bounds.h};
    placements[1].bounds = {bounds.x + bounds.w - stepper,
                            bounds.y + field_top +
                                (field_height - stepper_height) * 0.5f,
                            stepper,
                            stepper_height};
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Custom;
    info.name = label_;
    info.numeric_value = state_->effective();
    info.text_value = state_->draft.get();
    info.value_range =
        SemanticValueRange{state_->minimum, state_->maximum, state_->step};
    info.enabled = effective_enabled();
    info.read_only = effective_read_only() || !state_->source.valid() ||
                     state_->minimum == state_->maximum;
    if (state_->invalid)
      info.description = style_.invalid_message;
    if (info.enabled && !info.read_only)
      info.actions = {SemanticAction::Increment, SemanticAction::Decrement,
                      SemanticAction::SetValue};
    return info;
  }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->invalidate = context.invalidator();
    state_->mutable_value = effective_enabled() && !effective_read_only();
    const std::weak_ptr<NumberState> weak = state_;
    source_subscription_ = state_->source.observe([weak](double) {
      if (auto state = weak.lock())
        state->sync();
    });
  }
  void unmount(LifecycleContext &) override {
    state_->mounted = false;
    ++state_->generation;
    state_->invalidate = {};
    source_subscription_.reset();
  }
  void deactivate(LifecycleContext &) override { state_->enter_down = false; }
  std::vector<Spec> children() const {
    auto state = state_;
    auto style = style_;
    const std::weak_ptr<NumberState> weak = state;
    Spec text{[state, label = label_, style] {
                auto input = std::make_unique<TextInputComponent>(
                    label, state->draft.binding(), "", 0,
                    TextInputComponent::SubmitCallback{}, style.text_input);
                state->editor =
                    detail::TextInputAccess::configure(*input, state->policy);
                return input;
              },
              {}};
    Spec stepper{[state, weak, label = label_, style] {
                   auto input = std::make_unique<detail::StepperComponent>(
                       state->source, label, state->minimum, state->maximum,
                       state->step, style.stepper);
                   detail::StepperAccess::configure(
                       *input, false,
                       [weak] {
                         if (auto current = weak.lock())
                           if (auto session = current->editor.lock())
                             session->request_focus();
                       },
                       [weak](double value, std::function<bool()> guard) {
                         if (auto current = weak.lock())
                           current->step_to(value, std::move(guard));
                       });
                   return input;
                 },
                 {}};
    return {std::move(text), std::move(stepper)};
  }
  void paint(PaintContext &) const override {}

private:
  void retained_checkpoint() override { state_->sync(); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    state_->policy->read_only = !state_->can_write();
  }
  std::string label_;
  NumberInputStyle style_;
  std::shared_ptr<NumberState> state_;
  Binding<double>::Subscription source_subscription_;
};
} // namespace
NumberInput::NumberInput(std::string label, Binding<double> value)
    : label_(std::move(label)), value_(std::move(value)) {}
NumberInput::NumberInput(std::string label, State<double> &value)
    : NumberInput(std::move(label), value.binding()) {}
NumberInput &&NumberInput::range(double lo, double hi) && {
  if (!std::isfinite(lo) || !std::isfinite(hi) || lo > hi)
    throw std::invalid_argument("NumberInput range must be finite and ordered");
  minimum_ = lo;
  maximum_ = hi;
  return std::move(*this);
}
NumberInput &&NumberInput::step(double value) && {
  if (!std::isfinite(value) || value <= 0)
    throw std::invalid_argument("NumberInput step must be finite and positive");
  step_ = value;
  return std::move(*this);
}
NumberInput &&NumberInput::precision(unsigned value) && {
  if (value > 17)
    throw std::invalid_argument("NumberInput precision must be at most 17");
  precision_ = value;
  return std::move(*this);
}
NumberInput &&NumberInput::on_submit(std::function<void(double)> value) && {
  on_submit_ = std::move(value);
  return std::move(*this);
}
NumberInput &&NumberInput::style(NumberInputStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec NumberInput::spec() && {
  if (!std::isfinite(style_.gap) || style_.gap < 0 ||
      style_.gap > std::numeric_limits<float>::max())
    throw std::invalid_argument(
        "NumberInput gap must be finite and nonnegative");
  const auto digits = precision_.value_or(precision_from_step(step_));
  Spec spec{[label = std::move(label_), source = value_, lo = minimum_,
             hi = maximum_, step = step_, digits,
             submit = std::move(on_submit_), style = std::move(style_)] {
              return std::make_unique<NumberComponent>(
                  label, source, lo, hi, step, digits, submit, style);
            },
            {}};
  spec.children_factory = [](Component &component) {
    return static_cast<NumberComponent &>(component).children();
  };
  return spec;
}
} // namespace ui
