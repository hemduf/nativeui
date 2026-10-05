#include "detail/widget_input_action.hpp"
#include "detail/widget_search_field_policy.hpp"
#include <nativeui/search_field.hpp>
namespace ui {
namespace {
struct SearchState {
  explicit SearchState(Binding<std::string> value)
      : source(std::move(value)), seen(source.get()) {}
  Binding<std::string> source;
  std::string seen;
  std::function<void(const std::string &)> submit;
  std::shared_ptr<detail::SearchFieldBridge> bridge;
  std::shared_ptr<detail::TextInputPolicy> policy;
  std::weak_ptr<detail::TextInputSession> editor;
  std::function<void()> invalidate, availability;
  std::uint64_t generation{};
  bool mounted{}, mutable_value{true}, enter_down{}, focused{};
  void sync() {
    if (!mounted)
      return;
    const bool valid = source.valid();
    policy->read_only = !valid;
    if (auto session = editor.lock())
      session->refresh_source();
    if (seen != source.get()) {
      ++generation;
      seen = source.get();
      if (availability)
        availability();
      if (invalidate)
        invalidate();
    }
  }
  void clear(InputContext &context) {
    if (!mounted || !mutable_value || !source.valid() || source.get().empty())
      return;
    const auto session = editor.lock();
    if (!session || !session->mounted())
      return;
    const std::string expected = source.get();
    const auto serial = generation;
    auto source_copy = source;
    session->cancel_capture();
    if (!mounted || !mutable_value || !source_copy.valid() ||
        generation != serial || source_copy.get() != expected)
      return;
    session->replace("");
    session->reset_baseline();
    if (availability)
      availability();
    if (invalidate)
      invalidate();
    session->request_focus();
    if (mounted && mutable_value && source_copy.valid() &&
        generation == serial && source_copy.get() == expected &&
        detail::InputMutationAccess::allowed(context))
      source_copy.set("");
  }
};
class Magnifier final : public Component, public detail::ThemeBinding {
public:
  explicit Magnifier(SearchFieldStyle style) : style_(std::move(style)) {}
  Size measure(const std::vector<ChildMetrics> &) const override {
    return {style_.magnifier_size, style_.magnifier_size};
  }
  void paint(PaintContext &context) const override {
    const auto bounds = context.bounds();
    auto &painter = context.painter();
    auto clip = painter.scoped_clip(bounds);
    const float size = std::min({bounds.w, bounds.h, style_.magnifier_size});
    const Point center{bounds.x + bounds.w * 0.5f - size * 0.12f,
                       bounds.y + bounds.h * 0.5f - size * 0.12f};
    const auto color =
        style_.magnifier_color.value_or(current_theme().palette.muted_text);
    painter.arc(center, size * 0.28f, 0.0f, 6.28318530718f, 1.5f, color);
    painter.line({center.x + size * 0.2f, center.y + size * 0.2f},
                 {center.x + size * 0.45f, center.y + size * 0.45f}, 1.5f,
                 color);
  }

private:
  SearchFieldStyle style_;
};
class SearchComponent final : public Component, public detail::ThemeBinding {
public:
  SearchComponent(std::string label, Binding<std::string> value,
                  std::string placeholder, std::size_t maximum,
                  std::function<void(const std::string &)> submit,
                  SearchFieldStyle style,
                  std::shared_ptr<detail::SearchFieldBridge> bridge)
      : label_(std::move(label)), placeholder_(std::move(placeholder)),
        maximum_(maximum), style_(std::move(style)),
        state_(std::make_shared<SearchState>(std::move(value))) {
    state_->submit = std::move(submit);
    state_->bridge = std::move(bridge);
    state_->policy = std::make_shared<detail::TextInputPolicy>();
    state_->policy->paint_chrome = false;
    state_->policy->paint_label = false;
    const std::weak_ptr<SearchState> weak = state_;
    state_->policy->before_input =
        [weak](const InputEvent &event, InputContext &context,
               const detail::TextInputSnapshot &snapshot)
        -> std::optional<EventResult> {
      const auto state = weak.lock();
      if (!state || !state->mounted)
        return EventResult::Ignored;
      if (state->bridge && state->bridge->before_input) {
        auto policy = state->bridge->before_input;
        if (auto result = policy(event, context, snapshot))
          return result;
      }
      if (!state->mounted || !state->source.valid()) {
        if (event.type == InputType::TextInput ||
            event.type == InputType::Composition ||
            event.type == InputType::Command ||
            event.type == InputType::KeyDown)
          return EventResult::Handled;
        return {};
      }
      if (event.type == InputType::KeyUp && event.key == Key::Enter) {
        state->enter_down = false;
        return EventResult::Handled;
      }
      if (event.type == InputType::TextInput)
        state->enter_down = false;
      const bool cancel =
          (event.type == InputType::KeyDown && event.key == Key::Escape) ||
          (event.type == InputType::Command &&
           event.command == Command::Cancel);
      if (cancel) {
        if (snapshot.composition_active) {
          if (const auto editor = state->editor.lock()) {
            editor->cancel_capture();
            editor->cancel_composition();
          }
          context.invalidate();
          return EventResult::Handled;
        }
        if (state->source.get().empty())
          return EventResult::Ignored;
        if (!snapshot.read_only && state->mutable_value)
          state->clear(context);
        return EventResult::Handled;
      }
      const bool submit =
          (event.type == InputType::KeyDown && event.key == Key::Enter) ||
          (event.type == InputType::Command &&
           event.command == Command::Submit);
      if (submit) {
        if (snapshot.composition_active)
          return EventResult::Handled;
        if (event.type == InputType::KeyDown &&
            std::exchange(state->enter_down, true))
          return EventResult::Handled;
        const std::string value = state->source.get();
        const auto serial = state->generation;
        auto callback = state->submit;
        if (const auto editor = state->editor.lock()) {
          editor->cancel_capture();
          editor->reset_baseline();
        }
        context.invalidate();
        if (state->mounted && state->source.valid() &&
            state->generation == serial && state->source.get() == value &&
            callback)
          callback(value);
        return EventResult::Handled;
      }
      return {};
    };
    state_->policy->focus_changed = [weak](bool focused,
                                           detail::TextInputSnapshot snapshot) {
      const auto state = weak.lock();
      if (!state)
        return;
      state->focused = focused;
      if (!focused)
        state->enter_down = false;
      if (state->bridge && state->bridge->focus_changed) {
        auto callback = state->bridge->focus_changed;
        callback(focused, std::move(snapshot));
      }
    };
  }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool clips_children() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &children) const override {
    if (children.size() != 3)
      return {};
    return {children[1].preferred.w + style_.magnifier_size +
                style_.clear_width + 2.0f * style_.gap,
            std::max({children[0].preferred.h, children[1].preferred.h,
                      children[2].preferred.h})};
  }
  Size minimum_size(const std::vector<ChildMetrics> &children) const override {
    const auto preferred = measure(children);
    return {style_.magnifier_size + style_.clear_width + 2.0f * style_.gap,
            preferred.h};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &out) const override {
    if (out.size() != 3)
      return;
    const float icon = std::min(style_.magnifier_size, bounds.w),
                clear = std::min(style_.clear_width,
                                 std::max(0.0f, bounds.w - icon));
    const float gap =
        std::min(style_.gap, std::max(0.0f, (bounds.w - icon - clear) * 0.5f));
    out[0].bounds = {bounds.x,
                     bounds.y + (bounds.h - style_.magnifier_size) * 0.5f, icon,
                     std::min(style_.magnifier_size, bounds.h)};
    out[1].bounds = {bounds.x + icon + gap, bounds.y,
                     std::max(0.0f, bounds.w - icon - clear - 2.0f * gap),
                     bounds.h};
    out[2].bounds = {bounds.x + bounds.w - clear, bounds.y, clear, bounds.h};
  }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->invalidate = context.invalidator();
    state_->availability = context.availability_invalidator();
    state_->mutable_value = effective_enabled() && !effective_read_only();
    const std::weak_ptr<SearchState> weak = state_;
    subscription_ = state_->source.observe([weak](const auto &) {
      if (const auto state = weak.lock())
        state->sync();
    });
  }
  void unmount(LifecycleContext &) override {
    state_->mounted = false;
    state_->invalidate = {};
    state_->availability = {};
    subscription_.reset();
  }
  void deactivate(LifecycleContext &) override { state_->enter_down = false; }
  std::vector<Spec> children() const {
    const auto state = state_;
    const auto style = style_;
    Spec icon{[style] { return std::make_unique<Magnifier>(style); }, {}};
    Spec editor{[state, label = label_, placeholder = placeholder_,
                 maximum = maximum_, style] {
                  auto input = std::make_unique<TextInputComponent>(
                      label, state->source, placeholder, maximum,
                      TextInputComponent::SubmitCallback{}, style.text_input);
                  state->editor =
                      detail::TextInputAccess::configure(*input, state->policy);
                  if (state->bridge)
                    state->bridge->session = state->editor;
                  return input;
                },
                {}};
    auto action = std::make_shared<detail::InputActionState>();
    const std::weak_ptr<SearchState> weak = state;
    action->context_action = [weak](InputContext &context) {
      if (auto value = weak.lock())
        value->clear(context);
    };
    action->enabled = [weak] {
      const auto value = weak.lock();
      return value && value->mounted && value->mutable_value &&
             value->source.valid() && !value->source.get().empty();
    };
    auto visible = [weak] {
      const auto value = weak.lock();
      return value && !value->source.get().empty();
    };
    auto clear_style = style.clear;
    if (!clear_style.base.minimum_width)
      clear_style.base.minimum_width = style.clear_width;
    if (!clear_style.base.horizontal_padding)
      clear_style.base.horizontal_padding = 0.0f;
    Spec clear{[action, clear_style, visible] {
                 return std::make_unique<detail::InputAction>(
                     "Effacer la recherche", "×", clear_style, action, false,
                     false, visible);
               },
               {}};
    return {std::move(icon), std::move(editor), std::move(clear)};
  }
  void paint(PaintContext &context) const override {
    VisualState visual;
    visual.enabled = effective_enabled();
    visual.read_only = effective_read_only();
    visual.focused = state_->focused;
    const auto style = resolve_text_input_style(
        default_text_input_style(current_theme()), style_.text_input, visual);
    auto &painter = context.painter();
    const auto bounds = context.bounds();
    painter.fill_rounded_rect(bounds, style.corner_radius, style.field_fill);
    painter.stroke_rounded_rect(bounds, style.corner_radius, style.border_width,
                                style.border);
  }

private:
  void retained_checkpoint() override { state_->sync(); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    if (!state_->mutable_value)
      state_->enter_down = false;
  }
  std::string label_, placeholder_;
  std::size_t maximum_{};
  SearchFieldStyle style_;
  std::shared_ptr<SearchState> state_;
  Binding<std::string>::Subscription subscription_;
};
void validate_style(const SearchFieldStyle &style) {
  for (float value : {style.magnifier_size, style.gap, style.clear_width})
    detail::validate_control_extent(value);
}
} // namespace
namespace detail {
Spec search_field_spec(std::string label, Binding<std::string> query,
                       std::string placeholder, std::size_t maximum,
                       std::function<void(const std::string &)> submit,
                       SearchFieldStyle style,
                       std::shared_ptr<SearchFieldBridge> bridge) {
  validate_style(style);
  Spec spec{[label = std::move(label), query,
             placeholder = std::move(placeholder), maximum,
             submit = std::move(submit), style = std::move(style),
             bridge = std::move(bridge)] {
              return std::make_unique<SearchComponent>(
                  label, query, placeholder, maximum, submit, style, bridge);
            },
            {}};
  spec.children_factory = [](Component &component) {
    return static_cast<SearchComponent &>(component).children();
  };
  return spec;
}
} // namespace detail
SearchField::SearchField(std::string label, Binding<std::string> query)
    : label_(std::move(label)), query_(std::move(query)) {}
SearchField::SearchField(std::string label, State<std::string> &query)
    : SearchField(std::move(label), query.binding()) {}
SearchField &&SearchField::placeholder(std::string value) && {
  placeholder_ = std::move(value);
  return std::move(*this);
}
SearchField &&SearchField::max_length(std::size_t value) && {
  max_length_ = value;
  return std::move(*this);
}
SearchField &&
SearchField::on_submit(std::function<void(const std::string &)> callback) && {
  on_submit_ = std::move(callback);
  return std::move(*this);
}
SearchField &&SearchField::style(SearchFieldStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec SearchField::spec() && {
  return detail::search_field_spec(std::move(label_), query_,
                                   std::move(placeholder_), max_length_,
                                   std::move(on_submit_), std::move(style_));
}
} // namespace ui
