#include "detail/widget_input_action.hpp"
#include "detail/widget_suggestions.hpp"
#include <cmath>
#include <limits>
#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/token_field.hpp>
namespace ui {
namespace {
using Tokens = std::vector<std::string>;
float logical(double value) {
  return static_cast<float>(
      std::min(value, double(std::numeric_limits<float>::max())));
}
std::string trim(std::string_view text) {
  auto blank = [](char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' ||
           ch == '\v';
  };
  while (!text.empty() && blank(text.front()))
    text.remove_prefix(1);
  while (!text.empty() && blank(text.back()))
    text.remove_suffix(1);
  return std::string{text};
}
struct TokenRuntime : std::enable_shared_from_this<TokenRuntime> {
  explicit TokenRuntime(Binding<Tokens> value)
      : source(std::move(value)), seen(source.get()), draft(std::string{}) {}
  Binding<Tokens> source;
  Tokens seen, options;
  State<std::string> draft;
  std::shared_ptr<detail::TextInputPolicy> policy;
  std::weak_ptr<detail::TextInputSession> editor;
  std::shared_ptr<detail::SuggestionsEngine> engine;
  std::function<bool()> guard;
  std::function<void()> invalidate, layout, structure;
  std::uint64_t generation{}, dataset_generation{}, edit_generation{};
  std::size_t maximum{}, active{detail::no_choice};
  bool mounted{}, mutable_value{true}, custom{true};
  bool allowed() const {
    return mounted && mutable_value && source.valid() && (!guard || guard());
  }
  bool can_add(std::string_view value) const {
    return custom ||
           std::find(options.begin(), options.end(), value) != options.end();
  }
  void replace_draft(std::string value) {
    if (auto session = editor.lock()) {
      session->cancel_capture();
      session->replace(value);
      session->reset_baseline();
    }
    draft.set(std::move(value));
    active = detail::no_choice;
  }
  void sync(bool checkpoint = false) {
    if (!mounted)
      return;
    policy->read_only = !allowed();
    const auto current = source.get();
    if (current != seen) {
      ++generation;
      ++dataset_generation;
      seen = current;
      active = detail::no_choice;
      const bool refresh = engine->expanded();
      engine->close();
      if (structure)
        structure();
      if (layout)
        layout();
      if (invalidate)
        invalidate();
      if (refresh && allowed())
        engine->rebuild(draft.get());
    }
    if (!source.valid())
      engine->close();
    if (auto session = editor.lock()) {
      if (checkpoint) {
        const auto snapshot = session->snapshot();
        if (snapshot.edit_generation > edit_generation) {
          edit_generation = snapshot.edit_generation;
          engine->close();
        }
      }
      session->refresh_source();
    }
  }
  void publish(Tokens next, std::string remainder, const Tokens &expected,
               const std::function<bool()> &permission) {
    const auto keep = shared_from_this();
    if (!allowed() || (permission && !permission()))
      return;
    const auto serial = ++generation;
    auto model = source;
    engine->close();
    replace_draft(std::move(remainder));
    if (invalidate)
      invalidate();
    if (!allowed() || generation != serial || model.get() != expected ||
        (permission && !permission()))
      return;
    if (next != expected)
      model.set(std::move(next));
  }
  void add(std::string text, const std::function<bool()> &permission = {}) {
    if (!allowed() || (permission && !permission()))
      return;
    const auto expected = source.get();
    auto next = expected;
    const auto candidate = trim(text);
    if (candidate.empty())
      return;
    if (std::find(next.begin(), next.end(), candidate) != next.end()) {
      publish(std::move(next), {}, expected, permission);
      return;
    }
    if (!can_add(candidate) || (maximum && next.size() >= maximum))
      return;
    next.push_back(candidate);
    publish(std::move(next), {}, expected, permission);
  }
  void batch(detail::TextInputSnapshot snapshot) {
    if (!allowed() || snapshot.read_only)
      return;
    edit_generation = snapshot.edit_generation;
    ++generation;
    active = detail::no_choice;
    if (snapshot.text.find(',') == std::string::npos) {
      engine->rebuild(snapshot.text);
      return;
    }
    const auto expected = source.get();
    auto next = expected;
    std::string remainder;
    std::size_t begin{};
    for (;;) {
      const auto end = snapshot.text.find(',', begin);
      if (end == std::string::npos) {
        const auto tail = snapshot.text.substr(begin);
        if (!tail.empty()) {
          if (!remainder.empty())
            remainder += ',';
          remainder += tail;
        }
        break;
      }
      auto candidate =
          trim(std::string_view{snapshot.text}.substr(begin, end - begin));
      if (!candidate.empty() &&
          std::find(next.begin(), next.end(), candidate) == next.end()) {
        if (can_add(candidate) && (!maximum || next.size() < maximum))
          next.push_back(std::move(candidate));
        else {
          if (!remainder.empty())
            remainder += ',';
          remainder += candidate;
        }
      }
      begin = end + 1;
    }
    publish(std::move(next), std::move(remainder), expected,
            snapshot.mutation_guard);
  }
  void remove(std::size_t index, std::uint64_t serial,
              const std::function<bool()> &permission = {}) {
    if (!allowed() || dataset_generation != serial ||
        (permission && !permission()))
      return;
    const auto expected = source.get();
    if (index >= expected.size() || expected != seen)
      return;
    auto next = expected;
    next.erase(next.begin() + static_cast<std::ptrdiff_t>(index));
    const auto keep = shared_from_this();
    auto model = source;
    ++generation;
    const auto accepted = generation;
    active = detail::no_choice;
    engine->close();
    if (auto session = editor.lock()) {
      session->cancel_capture();
      session->request_focus();
    }
    if (invalidate)
      invalidate();
    if (allowed() && generation == accepted && model.get() == expected &&
        (!permission || permission()))
      model.set(std::move(next));
  }
  std::function<void()> prepare(std::string value) {
    const auto serial = generation;
    const auto expected = source.get();
    const std::weak_ptr<TokenRuntime> weak = shared_from_this();
    return [weak, serial, expected, value = std::move(value)] {
      const auto state = weak.lock();
      if (state && state->allowed() && state->generation == serial &&
          state->source.get() == expected)
        state->add(value);
    };
  }
  std::optional<EventResult> input(const InputEvent &event,
                                   InputContext &context,
                                   const detail::TextInputSnapshot &snapshot) {
    sync();
    if (event.type != InputType::KeyDown)
      return {};
    if (snapshot.composition_active) {
      if (event.key == Key::Escape) {
        if (const auto session = editor.lock()) {
          session->cancel_capture();
          session->cancel_composition();
        }
        context.invalidate();
      }
      return EventResult::Handled;
    }
    if (event.key == Key::Tab) {
      engine->close();
      active = detail::no_choice;
      return EventResult::Ignored;
    }
    if (event.key == Key::Escape) {
      engine->close();
      active = detail::no_choice;
      context.invalidate();
      return EventResult::Handled;
    }
    if (active != detail::no_choice) {
      if (event.key == Key::Delete || event.key == Key::Backspace) {
        auto permission = detail::InputMutationAccess::guard(context);
        remove(active, dataset_generation, permission);
        return EventResult::Handled;
      }
      if (event.key == Key::Left) {
        if (active > 0)
          --active;
        context.invalidate();
        return EventResult::Handled;
      }
      if (event.key == Key::Right) {
        if (active + 1 < seen.size())
          ++active;
        else
          active = detail::no_choice;
        context.invalidate();
        return EventResult::Handled;
      }
      active = detail::no_choice;
    }
    if (event.key == Key::Left && snapshot.cursor == 0 &&
        snapshot.anchor == 0 && !seen.empty()) {
      active = seen.size() - 1;
      context.invalidate();
      return EventResult::Handled;
    }
    if (event.key == Key::Backspace && snapshot.text.empty() && !seen.empty()) {
      auto permission = detail::InputMutationAccess::guard(context);
      remove(seen.size() - 1, dataset_generation, permission);
      return EventResult::Handled;
    }
    if (event.key == Key::Up || event.key == Key::Down) {
      if (allowed())
        engine->move(event.key == Key::Down ? 1 : -1, snapshot.text);
      return EventResult::Handled;
    }
    if (event.key == Key::Enter) {
      if (allowed()) {
        if (!engine->choose_highlight()) {
          auto permission = detail::InputMutationAccess::guard(context);
          add(snapshot.text, permission);
        }
      }
      return EventResult::Handled;
    }
    return {};
  }
};
class TokenChip final : public Component, public detail::ThemeBinding {
public:
  TokenChip(std::shared_ptr<TokenRuntime> state, std::size_t index,
            std::uint64_t generation, std::string label, TokenFieldStyle style)
      : state_(std::move(state)), index_(index), generation_(generation),
        label_(std::move(label)), style_(std::move(style)) {}
  bool clips_children() const noexcept override { return true; }
  bool pointer_targetable() const noexcept override { return false; }
  Size measure(const std::vector<ChildMetrics> &) const override {
    return {TextService::measure(label_, logical(style_.text_size)).width +
                logical(2 * style_.chip_padding + style_.remove_width),
            logical(style_.chip_height)};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    if (!children.empty()) {
      const float width =
          std::min(std::max(0.f, bounds.w), logical(style_.remove_width));
      children.front().bounds = {bounds.x + bounds.w - width, bounds.y, width,
                                 bounds.h};
    }
  }
  std::vector<Spec> children() {
    auto action = std::make_shared<detail::InputActionState>();
    const std::weak_ptr<TokenRuntime> weak = state_;
    const auto index = index_;
    const auto serial = generation_;
    action->enabled = [weak, serial] {
      const auto state = weak.lock();
      return state && state->mutable_value && state->source.valid() &&
             state->dataset_generation == serial;
    };
    action->generation = [weak] {
      const auto state = weak.lock();
      return state ? state->generation : 0;
    };
    action->context_action = [weak, index, serial](InputContext &context) {
      if (const auto state = weak.lock()) {
        auto permission = detail::InputMutationAccess::guard(context);
        state->remove(index, serial, permission);
      }
    };
    return {Spec{[action, name = "Remove " + label_, style = style_.remove] {
                   return std::make_unique<detail::InputAction>(
                       name, "×", style, action, false, false);
                 },
                 {}}};
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::ListItem;
    info.name = label_;
    info.text_value = label_;
    info.selected =
        state_->dataset_generation == generation_ && state_->active == index_;
    info.read_only = effective_read_only() || !state_->source.valid();
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto bounds = context.bounds();
    auto &p = context.painter();
    auto clip = p.scoped_clip(bounds);
    p.fill_rounded_rect(
        bounds, logical(style_.corner_radius),
        style_.chip_background.value_or(current_theme().palette.surface));
    p.stroke_rounded_rect(
        bounds, logical(style_.corner_radius), 1,
        state_->active == index_
            ? style_.active.value_or(current_theme().palette.focus)
            : style_.chip_border.value_or(current_theme().palette.border));
    const float width = std::max(
        0.f, bounds.w - logical(style_.remove_width + style_.chip_padding));
    auto textclip = p.scoped_clip({bounds.x, bounds.y, width, bounds.h});
    std::string display = label_;
    const float text_width =
        std::max(0.f, width - logical(style_.chip_padding));
    if (TextService::measure(display, logical(style_.text_size)).width >
        text_width) {
      while (
          !display.empty() &&
          TextService::measure(display + "…", logical(style_.text_size)).width >
              text_width) {
        std::size_t end = display.size() - 1;
        while (end > 0 &&
               (static_cast<unsigned char>(display[end]) & 0xc0u) == 0x80u)
          --end;
        display.resize(end);
      }
      if (TextService::measure("…", logical(style_.text_size)).width <=
          text_width)
        display += "…";
    }
    p.text({bounds.x + logical(style_.chip_padding), bounds.y + bounds.h * .5f},
           display, logical(style_.text_size),
           style_.chip_text.value_or(current_theme().palette.text));
  }

private:
  std::shared_ptr<TokenRuntime> state_;
  std::size_t index_;
  std::uint64_t generation_;
  std::string label_;
  TokenFieldStyle style_;
};
struct WrapLayout {
  std::vector<Rect> children;
  float height{};
};
WrapLayout wrap(Rect bounds, const std::vector<ChildMetrics> &children,
                const TokenFieldStyle &style) {
  WrapLayout result;
  const float pad =
      std::min(logical(style.padding), std::max(0.f, bounds.w * .5f));
  const float width = std::max(0.f, bounds.w - 2 * pad),
              gap = logical(style.gap);
  float x = 0, y = 0, row = 0;
  for (std::size_t index = 0; index < children.size(); ++index) {
    const float w = std::min(width, index + 1 == children.size()
                                        ? logical(style.draft_width)
                                        : children[index].preferred.w);
    const float h = children[index].preferred.h;
    if (x > 0 && x + w > width) {
      x = 0;
      y += row + gap;
      row = 0;
    }
    result.children.push_back({bounds.x + pad + x, bounds.y + pad + y, w, h});
    x += w + gap;
    row = std::max(row, h);
  }
  result.height = y + row + 2 * pad;
  return result;
}
class TokenFrame final : public Component,
                         public detail::ThemeBinding,
                         public detail::DynamicChildrenSource,
                         public detail::OverlayCommandSource,
                         public detail::OverlayAnchorPolicy {
public:
  TokenFrame(std::string label, Binding<Tokens> source, Tokens options,
             bool custom, std::size_t maximum, std::string placeholder,
             TokenFieldStyle style)
      : label_(std::move(label)), placeholder_(std::move(placeholder)),
        style_(std::move(style)),
        state_(std::make_shared<TokenRuntime>(std::move(source))) {
    const auto state = state_;
    state->options = std::move(options);
    state->custom = custom;
    state->maximum = maximum;
    state->policy = std::make_shared<detail::TextInputPolicy>();
    state->policy->paint_label = false;
    state->policy->paint_chrome = false;
    state->engine = std::make_shared<detail::SuggestionsEngine>();
    state->engine->freeform = true;
    state->engine->style = style_.suggestions;
    const std::weak_ptr<TokenRuntime> weak = state;
    state->engine->provider = [weak] {
      Tokens result;
      if (const auto model = weak.lock())
        for (const auto &option : model->options)
          if (std::find(model->seen.begin(), model->seen.end(), option) ==
              model->seen.end())
            result.push_back(option);
      return result;
    };
    state->engine->allowed = [weak] {
      const auto model = weak.lock();
      return model && model->allowed();
    };
    state->engine->prepare_choice = [weak](std::string value) {
      const auto model = weak.lock();
      return model ? model->prepare(std::move(value)) : std::function<void()>{};
    };
    state->policy->before_input =
        [weak](const InputEvent &event, InputContext &context,
               const detail::TextInputSnapshot &snapshot) {
          const auto model = weak.lock();
          return model ? model->input(event, context, snapshot)
                       : std::optional<EventResult>{EventResult::Handled};
        };
    state->policy->committed_edit = [weak](detail::TextInputSnapshot snapshot) {
      if (const auto model = weak.lock())
        model->batch(std::move(snapshot));
    };
    state->policy->focus_changed = [weak](bool focused,
                                          detail::TextInputSnapshot) {
      if (!focused)
        if (const auto model = weak.lock()) {
          model->active = detail::no_choice;
          model->engine->close();
        }
    };
  }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool clips_children() const noexcept override { return true; }
  bool dismiss_overlay_on_tab() const noexcept override { return true; }
  bool dismiss_overlay_when_read_only() const noexcept override { return true; }
  bool overlay_session_valid() const noexcept override {
    return !state_->engine->session || state_->engine->session->live;
  }
  std::vector<std::string> desired_keys() const override {
    std::vector<std::string> keys;
    for (std::size_t index = 0; index < state_->seen.size(); ++index)
      keys.push_back(std::to_string(state_->dataset_generation) + ":" +
                     std::to_string(index));
    keys.push_back("editor");
    return keys;
  }
  std::vector<detail::DynamicChildSpec> desired_children() const override {
    std::vector<detail::DynamicChildSpec> result;
    auto keys = desired_keys();
    for (std::size_t index = 0; index < state_->seen.size(); ++index) {
      Spec chip{[state = state_, index, serial = state_->dataset_generation,
                 label = state_->seen[index], style = style_] {
                  return std::make_unique<TokenChip>(state, index, serial,
                                                     label, style);
                },
                {}};
      chip.children_factory = [](Component &component) {
        return static_cast<TokenChip &>(component).children();
      };
      result.push_back({keys[index], std::move(chip)});
    }
    result.push_back(
        {"editor",
         Spec{[state = state_, label = label_, placeholder = placeholder_,
               style = style_.text_input] {
                auto editor = std::make_unique<TextInputComponent>(
                    label, state->draft.binding(), placeholder, 0,
                    TextInputComponent::SubmitCallback{}, style);
                state->editor =
                    detail::TextInputAccess::configure(*editor, state->policy);
                return editor;
              },
              {}}});
    return result;
  }
  void set_structure_invalidator(std::function<void()> callback) override {
    state_->structure = std::move(callback);
  }
  std::vector<Spec> initial_children() const {
    auto desired = desired_children();
    std::vector<Spec> children;
    children.reserve(desired.size());
    for (auto &child : desired) {
      child.spec.retained_key = child.key;
      children.push_back(std::move(child.spec));
    }
    return children;
  }
  Size measure(const std::vector<ChildMetrics> &children) const override {
    const auto width = logical(style_.minimum_width);
    return {width, wrap({0, 0, width, 0}, children, style_).height};
  }
  ChildMetrics measure_constrained(
      const Constraints &constraints,
      const std::vector<ChildMetrics> &children) const override {
    const auto width = constraints.bounded_width()
                           ? constraints.max.w
                           : logical(style_.minimum_width);
    const auto value = constraints.constrain(
        {width, wrap({0, 0, width, 0}, children, style_).height});
    return {constraints.constrain({0, 0}), value};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &metrics,
                       std::vector<ChildPlacement> &children) const override {
    const auto layout = wrap(bounds, metrics, style_);
    for (std::size_t index = 0; index < children.size(); ++index)
      children[index].bounds = layout.children[index];
  }
  void layout_committed(Rect, Rect bounds) noexcept override {
    state_->engine->minimum_width = bounds.w;
  }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->guard = detail::InputMutationAccess::guard(context);
    state_->invalidate = context.invalidator();
    state_->layout = context.layout_invalidator();
    state_->engine->mounted = true;
    state_->engine->owner = context.node_id();
    state_->engine->invalidate = state_->invalidate;
    const std::weak_ptr<TokenRuntime> weak = state_;
    subscription_ = state_->source.observe([weak](const auto &) {
      if (const auto state = weak.lock())
        state->sync();
    });
  }
  void unmount(LifecycleContext &) override {
    state_->mounted = false;
    ++state_->generation;
    state_->engine->detach();
    state_->invalidate = {};
    state_->layout = {};
    state_->structure = {};
    state_->guard = {};
    subscription_.reset();
  }
  void deactivate(LifecycleContext &) override {
    state_->active = detail::no_choice;
    state_->engine->close();
  }
  std::optional<detail::OverlayComponentCommand>
  take_overlay_command() override {
    return state_->engine->take_command();
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Group;
    info.name = label_;
    info.read_only = effective_read_only() || !state_->source.valid();
    info.description = std::to_string(state_->seen.size()) + " items";
    return info;
  }
  void paint(PaintContext &context) const override {
    auto &p = context.painter();
    const auto bounds = context.bounds();
    p.fill_rounded_rect(
        bounds, logical(style_.corner_radius),
        style_.background.value_or(current_theme().palette.control_background));
    p.stroke_rounded_rect(
        bounds, logical(style_.corner_radius), 1,
        style_.border.value_or(current_theme().palette.border));
  }

private:
  void retained_checkpoint() override { state_->sync(true); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    state_->policy->read_only =
        !state_->mutable_value || !state_->source.valid();
    if (!state_->mutable_value && state_->engine->session)
      state_->engine->session->live = false;
  }
  std::string label_, placeholder_;
  TokenFieldStyle style_;
  std::shared_ptr<TokenRuntime> state_;
  Binding<Tokens>::Subscription subscription_;
};
} // namespace
TokenField::TokenField(std::string label, Binding<Tokens> tokens,
                       Tokens suggestions)
    : label_(std::move(label)), tokens_(std::move(tokens)),
      suggestions_(std::move(suggestions)) {}
TokenField::TokenField(std::string label, State<Tokens> &tokens,
                       Tokens suggestions)
    : TokenField(std::move(label), tokens.binding(), std::move(suggestions)) {}
TokenField &&TokenField::allow_custom(bool value) && {
  custom_ = value;
  return std::move(*this);
}
TokenField &&TokenField::maximum_tokens(std::size_t value) && {
  maximum_ = value;
  return std::move(*this);
}
TokenField &&TokenField::placeholder(std::string value) && {
  placeholder_ = std::move(value);
  return std::move(*this);
}
TokenField &&TokenField::style(TokenFieldStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec TokenField::spec() && {
  for (double metric :
       {style_.minimum_width, style_.draft_width, style_.chip_height,
        style_.chip_padding, style_.remove_width, style_.padding, style_.gap,
        style_.text_size, style_.corner_radius})
    if (!std::isfinite(metric) || metric < 0)
      throw std::invalid_argument(
          "TokenField metrics must be finite and nonnegative");
  Spec result{[label = label_, tokens = tokens_, options = suggestions_,
               custom = custom_, maximum = maximum_, placeholder = placeholder_,
               style = style_] {
                return std::make_unique<TokenFrame>(label, tokens, options,
                                                    custom, maximum,
                                                    placeholder, style);
              },
              {}};
  result.children_factory = [](Component &component) {
    return static_cast<TokenFrame &>(component).initial_children();
  };
  return result;
}
} // namespace ui
