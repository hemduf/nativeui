#include "detail/widget_suggestions.hpp"
#include <nativeui/autocomplete.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <stdexcept>
namespace ui {
namespace {
struct AutocompleteModel : std::enable_shared_from_this<AutocompleteModel> {
  explicit AutocompleteModel(Binding<std::string> value)
      : source(std::move(value)), seen(source.get()) {}
  Binding<std::string> source;
  std::string seen;
  std::function<void(const std::string &)> submit;
  std::shared_ptr<detail::TextInputPolicy> policy;
  std::weak_ptr<detail::TextInputSession> editor;
  std::shared_ptr<detail::SuggestionsEngine> engine;
  std::function<bool()> guard;
  std::function<void()> invalidate;
  std::uint64_t generation{}, edit_generation{};
  bool mounted{}, mutable_value{true}, enter_down{};
  bool allowed() const {
    return mounted && mutable_value && source.valid() && (!guard || guard());
  }
  void sync(bool checkpoint = false) {
    if (!mounted)
      return;
    policy->read_only = !allowed();
    const auto session = editor.lock();
    const auto snapshot =
        session ? session->snapshot() : detail::TextInputSnapshot{};
    const bool own_edit = snapshot.edit_generation > edit_generation &&
                          snapshot.text == source.get();
    if (source.get() != seen) {
      seen = source.get();
      ++generation;
      if (!own_edit)
        engine->close();
    }
    if (!source.valid())
      engine->close();
    if (checkpoint && snapshot.edit_generation > edit_generation) {
      // A failed publication/hook is read-only recovery, never provider replay.
      edit_generation = snapshot.edit_generation;
      engine->close();
    }
    if (session)
      session->refresh_source();
  }
  std::function<void()> prepare(std::string value) {
    const std::string expected = source.get();
    const auto serial = generation;
    const std::weak_ptr<AutocompleteModel> weak = shared_from_this();
    return [weak, expected, serial, value = std::move(value)] {
      const auto state = weak.lock();
      if (!state || !state->allowed() || state->generation != serial ||
          state->source.get() != expected)
        return;
      auto source = state->source;
      if (const auto session = state->editor.lock()) {
        session->cancel_capture();
        session->replace(value);
        session->select(value.size(), value.size());
      }
      if (state->invalidate)
        state->invalidate();
      if (!state->allowed() || state->generation != serial ||
          source.get() != expected)
        return;
      state->seen = value;
      source.set(value);
    };
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
    const bool arrow = event.type == InputType::KeyDown &&
                       (event.key == Key::Up || event.key == Key::Down);
    if (snapshot.composition_active && (cancel || accept || arrow)) {
      if (cancel)
        if (const auto session = editor.lock()) {
          session->cancel_capture();
          session->cancel_composition();
          context.invalidate();
        }
      return EventResult::Handled;
    }
    if (cancel && engine->session && engine->session->live) {
      engine->close();
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown && event.key == Key::Tab) {
      engine->close();
      return EventResult::Ignored;
    }
    if (arrow) {
      if (allowed() && !snapshot.read_only)
        engine->move(event.key == Key::Down ? 1 : -1, snapshot.text);
      return EventResult::Handled;
    }
    if (accept) {
      if (!allowed() || snapshot.read_only)
        return EventResult::Handled;
      if (event.type == InputType::KeyDown && std::exchange(enter_down, true))
        return EventResult::Handled;
      if (engine->choose_highlight())
        return EventResult::Handled;
      const std::string value = source.get();
      const auto serial = generation;
      auto callback = submit;
      engine->close();
      if (const auto session = editor.lock()) {
        session->cancel_capture();
        session->reset_baseline();
        session->select(snapshot.cursor, snapshot.cursor);
      }
      context.invalidate();
      if (allowed() && generation == serial && source.get() == value &&
          callback)
        callback(value);
      return EventResult::Handled;
    }
    return {};
  }
};
class AutocompleteFrame final : public Component,
                                public detail::OverlayCommandSource,
                                public detail::OverlayAnchorPolicy {
public:
  AutocompleteFrame(std::string label, Binding<std::string> value,
                    Autocomplete::SuggestionsProvider provider,
                    Autocomplete::Filter filter, std::string placeholder,
                    std::function<void(const std::string &)> submit,
                    AutocompleteStyle style)
      : label_(std::move(label)), placeholder_(std::move(placeholder)),
        style_(std::move(style)),
        state_(std::make_shared<AutocompleteModel>(std::move(value))) {
    state_->submit = std::move(submit);
    state_->policy = std::make_shared<detail::TextInputPolicy>();
    state_->engine = std::make_shared<detail::SuggestionsEngine>();
    state_->engine->freeform = true;
    state_->engine->provider = std::move(provider);
    state_->engine->filter = std::move(filter);
    state_->engine->style = style_.item;
    state_->engine->maximum_rows = style_.maximum_visible_rows;
    const std::weak_ptr<AutocompleteModel> weak = state_;
    state_->engine->allowed = [weak] {
      const auto model = weak.lock();
      return model && model->allowed();
    };
    state_->engine->prepare_choice = [weak](std::string value) {
      const auto model = weak.lock();
      return model ? model->prepare(std::move(value)) : std::function<void()>{};
    };
    state_->policy->committed_edit =
        [weak](detail::TextInputSnapshot snapshot) {
          const auto model = weak.lock();
          if (!model || !model->mounted)
            return;
          model->edit_generation = snapshot.edit_generation;
          if (model->allowed()) {
            ++model->generation;
            model->engine->rebuild(std::move(snapshot.text));
          }
        };
    state_->policy->before_input =
        [weak](const InputEvent &event, InputContext &context,
               const detail::TextInputSnapshot &snapshot)
        -> std::optional<EventResult> {
      const auto model = weak.lock();
      return model && model->mounted
                 ? model->input(event, context, snapshot)
                 : std::optional<EventResult>{EventResult::Ignored};
    };
    state_->policy->focus_changed = [weak](bool focused,
                                           detail::TextInputSnapshot) {
      const auto model = weak.lock();
      if (!model || !model->mounted)
        return;
      if (!focused) {
        model->enter_down = false;
        model->engine->close();
      }
    };
  }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool dismiss_overlay_on_tab() const noexcept override { return true; }
  bool dismiss_overlay_when_read_only() const noexcept override { return true; }
  bool overlay_session_valid() const noexcept override {
    return !state_->engine->session || state_->engine->session->live;
  }
  Size measure(const std::vector<ChildMetrics> &children) const override {
    return children.empty() ? Size{} : children.front().preferred;
  }
  Size minimum_size(const std::vector<ChildMetrics> &children) const override {
    return children.empty() ? Size{} : children.front().minimum;
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    if (!children.empty())
      children.front().bounds = bounds;
  }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->guard = detail::InputMutationAccess::guard(context);
    state_->mutable_value = effective_enabled() && !effective_read_only();
    state_->invalidate = context.invalidator();
    state_->engine->owner = context.node_id();
    state_->engine->mounted = true;
    state_->engine->invalidate = state_->invalidate;
    const std::weak_ptr<AutocompleteModel> weak = state_;
    subscription_ = state_->source.observe([weak](const auto &) {
      if (const auto model = weak.lock())
        model->sync();
    });
  }
  void unmount(LifecycleContext &) override {
    state_->mounted = false;
    ++state_->generation;
    state_->engine->detach();
    subscription_.reset();
    state_->invalidate = {};
    state_->guard = {};
  }
  void deactivate(LifecycleContext &) override { state_->engine->close(); }
  std::optional<detail::OverlayComponentCommand>
  take_overlay_command() override {
    return state_->engine->take_command();
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::ComboBox;
    info.name = label_;
    info.text_value = state_->source.get();
    info.read_only = effective_read_only() || !state_->source.valid();
    info.enabled = effective_enabled();
    info.expanded = state_->engine->expanded()
                        ? SemanticExpandedState::Expanded
                        : SemanticExpandedState::Collapsed;
    if (state_->engine->session && state_->engine->session->live)
      info.description =
          std::to_string(state_->engine->session->rows.size()) + " suggestions";
    return info;
  }
  std::vector<Spec> children() const {
    const auto state = state_;
    const auto style = style_;
    const auto label = label_, placeholder = placeholder_;
    return {Spec{[state, style, label, placeholder] {
                   auto editor = std::make_unique<TextInputComponent>(
                       label, state->source, placeholder, 0,
                       TextInputComponent::SubmitCallback{}, style.text_input);
                   state->editor = detail::TextInputAccess::configure(
                       *editor, state->policy);
                   return editor;
                 },
                 {}}};
  }
  void paint(PaintContext &) const override {}

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
  void layout_committed(Rect, Rect bounds) noexcept override {
    state_->engine->minimum_width = bounds.w;
  }
  std::string label_, placeholder_;
  AutocompleteStyle style_;
  std::shared_ptr<AutocompleteModel> state_;
  Binding<std::string>::Subscription subscription_;
};
} // namespace
Autocomplete::Autocomplete(std::string label, Binding<std::string> value,
                           std::vector<std::string> options)
    : Autocomplete(std::move(label), std::move(value),
                   [options = std::move(options)] { return options; }) {}
Autocomplete::Autocomplete(std::string label, State<std::string> &value,
                           std::vector<std::string> options)
    : Autocomplete(std::move(label), value.binding(), std::move(options)) {}
Autocomplete::Autocomplete(std::string label, Binding<std::string> value,
                           SuggestionsProvider provider)
    : label_(std::move(label)), value_(std::move(value)),
      suggestions_(std::move(provider)) {}
Autocomplete::Autocomplete(std::string label, State<std::string> &value,
                           SuggestionsProvider provider)
    : Autocomplete(std::move(label), value.binding(), std::move(provider)) {}
Autocomplete &&Autocomplete::filter(Filter value) && {
  filter_ = std::move(value);
  return std::move(*this);
}
Autocomplete &&Autocomplete::placeholder(std::string value) && {
  placeholder_ = std::move(value);
  return std::move(*this);
}
Autocomplete &&
Autocomplete::on_submit(std::function<void(const std::string &)> value) && {
  submit_ = std::move(value);
  return std::move(*this);
}
Autocomplete &&Autocomplete::style(AutocompleteStyle value) && {
  if (!value.maximum_visible_rows)
    throw std::invalid_argument("Popup must display at least one row");
  style_ = std::move(value);
  return std::move(*this);
}
Spec Autocomplete::spec() && {
  Spec result{[label = std::move(label_), value = value_,
               provider = std::move(suggestions_), filter = std::move(filter_),
               placeholder = std::move(placeholder_),
               submit = std::move(submit_), style = std::move(style_)] {
                return std::make_unique<AutocompleteFrame>(
                    label, value, provider, filter, placeholder, submit, style);
              },
              {}};
  result.children_factory = [](Component &component) {
    return static_cast<AutocompleteFrame &>(component).children();
  };
  return result;
}
} // namespace ui
