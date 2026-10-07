#include "detail/widget_input_action.hpp"
#include "detail/widget_suggestions.hpp"
#include <algorithm>
#include <nativeui/editable_combo_box.hpp>
namespace ui::detail {
namespace {
bool ascii_blank(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' ||
         ch == '\v';
}
std::string_view trimmed(std::string_view text) {
  while (!text.empty() && ascii_blank(text.front()))
    text.remove_prefix(1);
  while (!text.empty() && ascii_blank(text.back()))
    text.remove_suffix(1);
  return text;
}
std::string folded(std::string_view text) {
  std::string result(text);
  for (char &ch : result)
    if (ch >= 'A' && ch <= 'Z')
      ch = static_cast<char>(ch + ('a' - 'A'));
  return result;
}
} // namespace
bool suggestions_blank(std::string_view text) { return trimmed(text).empty(); }
bool SuggestionsEngine::expanded() const noexcept {
  return mounted && session && session->live && handle.valid();
}
void SuggestionsEngine::detach() noexcept {
  mounted = false;
  ++generation;
  if (session)
    session->live = false;
  pending.reset();
  handle = {};
  invalidate = {};
  allowed = {};
  prepare_choice = {};
}
std::optional<OverlayComponentCommand> SuggestionsEngine::take_command() {
  return std::exchange(pending, {});
}
void SuggestionsEngine::close() {
  const bool live = session && session->live;
  const bool completing =
      pending &&
      pending->kind == OverlayComponentCommandKind::CloseThenInvoke &&
      static_cast<bool>(pending->after_close);
  if (!live && !handle.valid() &&
      (!pending ||
       (pending->kind == OverlayComponentCommandKind::CloseThenInvoke &&
        !completing)))
    return;
  const auto previous = handle.valid() ? handle
                        : pending      ? pending->handle
                                       : OverlayHandle{};
  ++generation;
  if (session)
    session->live = false;
  if (previous.valid())
    pending =
        OverlayComponentCommand::close_then_invoke(previous, owner, false, {});
  else
    pending.reset();
  handle = {};
  if (invalidate)
    invalidate();
}
void SuggestionsEngine::rebuild(std::string query, bool full) {
  const auto keep = shared_from_this();
  if (!mounted || (allowed && !allowed())) {
    close();
    return;
  }
  if (freeform && suggestions_blank(query)) {
    close();
    return;
  }
  const auto serial = ++generation;
  const auto old_session = session;
  if (old_session)
    old_session->live = false;
  try {
    auto get = provider;
    auto include = filter;
    if (!mounted || generation != serial || (allowed && !allowed()))
      return;
    auto source = get ? get() : std::vector<std::string>{};
    if (!mounted || generation != serial || (allowed && !allowed()))
      return;
    const auto key = folded(trimmed(query));
    std::vector<std::string> unique, prefix, other;
    for (auto &candidate : source) {
      if (std::find(unique.begin(), unique.end(), candidate) != unique.end())
        continue;
      unique.push_back(candidate);
      const auto normalized = folded(candidate);
      if (freeform && normalized == key)
        continue;
      if (full || (include ? include(candidate, query)
                           : normalized.find(key) != std::string::npos)) {
        if (!include && !full && normalized.starts_with(key))
          prefix.push_back(std::move(candidate));
        else
          other.push_back(std::move(candidate));
      }
      if (!mounted || generation != serial || (allowed && !allowed()))
        return;
    }
    prefix.insert(prefix.end(), std::make_move_iterator(other.begin()),
                  std::make_move_iterator(other.end()));
    if (freeform && prefix.empty()) {
      close();
      return;
    }
    if (!mounted || generation != serial || (allowed && !allowed()))
      return;
    auto next = old_session && handle.valid()
                    ? old_session
                    : std::make_shared<ChoicePopupSession>();
    std::vector<ComboBoxRow> rows;
    rows.reserve(prefix.size());
    for (auto &label : prefix)
      rows.push_back({std::move(label), true});
    next->rows = std::move(rows);
    next->style = style;
    next->focusable = false;
    next->wrap = false;
    next->maximum_rows = maximum_rows;
    next->minimum_width = minimum_width;
    next->generation = serial;
    next->highlighted = freeform || next->rows.empty() ? no_choice : 0;
    next->selected = no_choice;
    for (std::size_t index = 0; index < next->rows.size(); ++index)
      if (next->rows[index].label == selection) {
        next->selected = index;
        break;
      }
    next->scroll = 0.f;
    next->live = true;
    const std::weak_ptr<SuggestionsEngine> weak = keep;
    next->allowed = [weak, serial] {
      const auto engine = weak.lock();
      return engine && engine->mounted && engine->generation == serial &&
             (!engine->allowed || engine->allowed());
    };
    next->choose = [weak](std::size_t index, Key) {
      if (const auto engine = weak.lock())
        engine->choose(index);
    };
    next->take_command = [weak] {
      const auto engine = weak.lock();
      return engine ? engine->take_command()
                    : std::optional<OverlayComponentCommand>{};
    };
    session = next;
    if (handle.valid()) {
      if (next->structure_invalidator)
        next->structure_invalidator();
      if (next->invalidator)
        next->invalidator();
    } else {
      OverlaySpec overlay;
      overlay.anchor = owner;
      overlay.mode = OverlayMode::NonModal;
      overlay.placement = OverlayPlacement::AnchorBelow;
      overlay.dismiss_on_escape = false;
      overlay.dismiss_on_outside_pointer_down = true;
      overlay.content = choice_popup_spec(next);
      pending = OverlayComponentCommand::show(
          std::move(overlay), [weak, next, serial](OverlayHandle shown) {
            const auto engine = weak.lock();
            if (!engine) {
              next->live = false;
              return;
            }
            if (!engine->mounted || engine->generation != serial ||
                engine->session != next ||
                (engine->allowed && !engine->allowed())) {
              next->live = false;
              if (shown.valid())
                engine->pending = OverlayComponentCommand::close_then_invoke(
                    shown, engine->owner, false, {});
              return;
            }
            engine->handle = shown;
            if (!shown.valid())
              next->live = false;
          });
    }
    if (invalidate)
      invalidate();
  } catch (...) {
    if (mounted && generation == serial) {
      try {
        close();
      } catch (...) {
      }
    }
    throw;
  }
}
void SuggestionsEngine::move(int direction, std::string query) {
  if (!expanded()) {
    rebuild(std::move(query));
    if (!session || !session->live)
      return;
    session->highlighted = no_choice;
  }
  if (session && session->live)
    choice_highlight(session, choice_step(*session, direction));
}
bool SuggestionsEngine::choose_highlight() {
  if (!session || !session->live ||
      session->highlighted >= session->rows.size())
    return false;
  choose(session->highlighted);
  return true;
}
void SuggestionsEngine::choose(std::size_t index) {
  if (!mounted || !session || !session->live || index >= session->rows.size() ||
      (allowed && !allowed()) || !prepare_choice)
    return;
  const auto serial = generation;
  const auto chosen_session = session;
  const std::string value = chosen_session->rows[index].label;
  auto prepare = prepare_choice;
  if (!mounted || generation != serial || session != chosen_session ||
      (allowed && !allowed()) || generation != serial)
    return;
  auto publish = prepare(value);
  if (!mounted || generation != serial || session != chosen_session ||
      (allowed && !allowed()) || generation != serial)
    return;
  chosen_session->live = false;
  const std::weak_ptr<SuggestionsEngine> weak = shared_from_this();
  pending = OverlayComponentCommand::close_then_invoke(
      handle, owner, true,
      [weak, serial, chosen_session, publish = std::move(publish)]() mutable {
        const auto engine = weak.lock();
        if (!engine || !engine->mounted || engine->generation != serial ||
            engine->session != chosen_session ||
            (engine->allowed && !engine->allowed()) ||
            engine->generation != serial)
          return;
        auto callback = std::move(publish);
        if (callback)
          callback();
      });
  handle = {};
  if (invalidate)
    invalidate();
}
} // namespace ui::detail
namespace ui {
namespace {
struct EditableChoice : std::enable_shared_from_this<EditableChoice> {
  explicit EditableChoice(Binding<std::string> value)
      : source(std::move(value)), seen(source.get()), draft(seen) {}
  Binding<std::string> source;
  std::string seen;
  State<std::string> draft;
  std::shared_ptr<detail::TextInputPolicy> policy;
  std::weak_ptr<detail::TextInputSession> editor;
  std::shared_ptr<detail::SuggestionsEngine> engine;
  std::function<bool()> guard;
  std::function<void()> invalidate;
  std::uint64_t generation{}, edit_generation{};
  bool mounted{}, mutable_value{true}, typed{}, focused{};
  bool allowed() const {
    return mounted && mutable_value && source.valid() && (!guard || guard());
  }
  void restore() {
    ++generation;
    typed = false;
    engine->close();
    auto text = source.get();
    seen = text;
    if (auto session = editor.lock()) {
      session->cancel_capture();
      session->replace(text);
      session->reset_baseline();
    }
    draft.set(std::move(text));
    if (invalidate)
      invalidate();
  }
  void sync() {
    if (!mounted)
      return;
    policy->read_only = !allowed();
    if (source.get() != seen)
      restore();
    else if (!source.valid()) {
      typed = false;
      engine->close();
    }
    if (auto session = editor.lock())
      session->refresh_source();
  }
  std::function<void()> prepare(std::string value) {
    const std::string expected = source.get();
    const auto serial = generation;
    const std::weak_ptr<EditableChoice> weak = shared_from_this();
    return [weak, serial, expected, value = std::move(value)] {
      const auto state = weak.lock();
      if (!state || !state->allowed() || state->generation != serial ||
          state->source.get() != expected)
        return;
      auto source = state->source;
      if (auto session = state->editor.lock()) {
        session->cancel_capture();
        session->replace(value);
        session->select(0, value.size());
        session->reset_baseline();
      }
      state->draft.set(value);
      state->typed = false;
      if (state->invalidate)
        state->invalidate();
      if (!state->allowed() || state->generation != serial ||
          source.get() != expected)
        return;
      state->seen = value;
      source.set(value);
    };
  }
};
class EditableChoiceFrame final : public Component,
                                  public detail::ThemeBinding,
                                  public detail::OverlayCommandSource,
                                  public detail::OverlayAnchorPolicy {
public:
  EditableChoiceFrame(std::string label, Binding<std::string> source,
                      EditableComboBox::OptionsProvider provider,
                      EditableComboBox::Filter filter, std::string placeholder,
                      EditableComboBoxStyle style)
      : label_(std::move(label)), placeholder_(std::move(placeholder)),
        style_(std::move(style)),
        state_(std::make_shared<EditableChoice>(std::move(source))) {
    state_->policy = std::make_shared<detail::TextInputPolicy>();
    state_->policy->paint_chrome = false;
    state_->engine = std::make_shared<detail::SuggestionsEngine>();
    state_->engine->provider = std::move(provider);
    state_->engine->filter = std::move(filter);
    state_->engine->style = style_.item;
    state_->engine->maximum_rows = style_.maximum_visible_rows;
    const std::weak_ptr<EditableChoice> weak = state_;
    state_->engine->allowed = [weak] {
      const auto state = weak.lock();
      return state && state->allowed();
    };
    state_->engine->prepare_choice = [weak](std::string value) {
      const auto state = weak.lock();
      return state ? state->prepare(std::move(value)) : std::function<void()>{};
    };
    state_->policy->committed_edit =
        [weak](detail::TextInputSnapshot snapshot) {
          const auto state = weak.lock();
          if (!state || !state->allowed())
            return;
          state->edit_generation = snapshot.edit_generation;
          state->typed = true;
          ++state->generation;
          state->engine->selection = state->source.get();
          state->engine->rebuild(std::move(snapshot.text));
        };
    state_->policy->focus_changed = [weak](bool focused,
                                           detail::TextInputSnapshot) {
      const auto state = weak.lock();
      if (!state)
        return;
      state->focused = focused;
      if (!focused && state->mounted)
        state->restore();
    };
    state_->policy->before_input =
        [weak](const InputEvent &event, InputContext &context,
               const detail::TextInputSnapshot &snapshot)
        -> std::optional<EventResult> {
      const auto state = weak.lock();
      if (!state || !state->mounted)
        return EventResult::Ignored;
      const bool escape =
          event.type == InputType::KeyDown && event.key == Key::Escape;
      const bool enter =
          event.type == InputType::KeyDown && event.key == Key::Enter;
      const bool arrow = event.type == InputType::KeyDown &&
                         (event.key == Key::Down || event.key == Key::Up);
      if (snapshot.composition_active && (escape || enter || arrow)) {
        if (escape)
          if (const auto session = state->editor.lock()) {
            session->cancel_capture();
            session->cancel_composition();
            context.invalidate();
          }
        return EventResult::Handled;
      }
      if (escape ||
          (event.type == InputType::KeyDown && event.key == Key::Tab)) {
        state->restore();
        return escape ? EventResult::Handled : EventResult::Ignored;
      }
      if (arrow) {
        if (state->allowed() && !snapshot.read_only)
          state->engine->move(event.key == Key::Down ? 1 : -1, snapshot.text);
        return EventResult::Handled;
      }
      if (enter || (event.type == InputType::Command &&
                    event.command == Command::Submit)) {
        if (state->allowed() && !snapshot.read_only)
          state->engine->choose_highlight();
        return EventResult::Handled;
      }
      return {};
    };
  }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool dismiss_overlay_on_tab() const noexcept override { return true; }
  bool dismiss_overlay_when_read_only() const noexcept override { return true; }
  bool overlay_session_valid() const noexcept override {
    return !state_->engine->session || state_->engine->session->live;
  }
  bool clips_children() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &children) const override {
    if (children.empty())
      return {};
    return {children.front().preferred.w + style_.chevron_width,
            children.front().preferred.h};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    if (children.size() != 2)
      return;
    const float width = std::min(bounds.w, style_.chevron_width);
    children[0].bounds = {bounds.x, bounds.y, bounds.w - width, bounds.h};
    children[1].bounds = {bounds.x + bounds.w - width, bounds.y, width,
                          bounds.h};
  }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->guard = detail::InputMutationAccess::guard(context);
    state_->mutable_value = effective_enabled() && !effective_read_only();
    state_->invalidate = context.invalidator();
    state_->engine->invalidate = state_->invalidate;
    state_->engine->mounted = true;
    state_->engine->owner = context.node_id();
    const std::weak_ptr<EditableChoice> weak = state_;
    subscription_ = state_->source.observe([weak](const auto &) {
      if (const auto state = weak.lock())
        state->sync();
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
    info.text_value = state_->draft.get();
    info.description = state_->source.get();
    info.read_only = effective_read_only() || !state_->source.valid();
    info.enabled = effective_enabled();
    info.expanded = state_->engine->expanded()
                        ? SemanticExpandedState::Expanded
                        : SemanticExpandedState::Collapsed;
    if (info.enabled && !info.read_only)
      info.actions = {SemanticAction::Expand};
    return info;
  }
  std::vector<Spec> children() const {
    const auto state = state_;
    auto style = style_;
    if (label_.empty()) {
      if (!style.text_input.base.field_top)
        style.text_input.base.field_top = 0.0f;
      if (!style.text_input.base.control_height)
        style.text_input.base.control_height =
            *style.text_input.base.field_top +
            style.text_input.base.field_height.value_or(46.0f);
    }
    const auto label = label_, placeholder = placeholder_;
    auto action = std::make_shared<detail::InputActionState>();
    const std::weak_ptr<EditableChoice> weak = state;
    action->enabled = [weak] {
      const auto owner = weak.lock();
      return owner && owner->mutable_value && owner->source.valid();
    };
    action->context_action = [weak](InputContext &) {
      const auto owner = weak.lock();
      if (!owner || !owner->allowed())
        return;
      if (const auto session = owner->editor.lock())
        session->request_focus();
      owner->engine->selection = owner->source.get();
      owner->engine->rebuild(owner->draft.get(), !owner->typed);
    };
    ButtonStyle button;
    button.base.minimum_width = style.chevron_width;
    button.base.horizontal_padding = 0.f;
    return {Spec{[state, style, label, placeholder] {
                   auto editor = std::make_unique<TextInputComponent>(
                       label, state->draft.binding(), placeholder, 0,
                       TextInputComponent::SubmitCallback{}, style.text_input);
                   state->editor = detail::TextInputAccess::configure(
                       *editor, state->policy);
                   return editor;
                 },
                 {}},
            Spec{[action, button] {
                   return std::make_unique<detail::InputAction>(
                       "Options", "⌄", button, action, false, false);
                 },
                 {}}};
  }
  void paint(PaintContext &context) const override {
    VisualState visual;
    visual.enabled = effective_enabled();
    visual.read_only = effective_read_only();
    visual.focused = state_->focused;
    const auto style = resolve_combo_box_style(
        default_combo_box_style(current_theme()), style_.frame, visual);
    const auto bounds = context.bounds();
    auto &painter = context.painter();
    painter.fill_rounded_rect(bounds, style.corner_radius, style.fill);
    painter.stroke_rounded_rect(bounds, style.corner_radius, style.border_width,
                                style.border);
  }

private:
  void retained_checkpoint() override { state_->sync(); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    state_->policy->read_only =
        !state_->mutable_value || !state_->source.valid();
    if (!state_->mutable_value && state_->engine->session)
      state_->engine->session->live = false;
  }
  void layout_committed(Rect, Rect next) noexcept override {
    state_->engine->minimum_width = next.w;
  }
  std::string label_, placeholder_;
  EditableComboBoxStyle style_;
  std::shared_ptr<EditableChoice> state_;
  Binding<std::string>::Subscription subscription_;
};
} // namespace
EditableComboBox::EditableComboBox(std::string label,
                                   Binding<std::string> source,
                                   std::vector<std::string> values)
    : EditableComboBox(std::move(label), std::move(source),
                       [values = std::move(values)] { return values; }) {}
EditableComboBox::EditableComboBox(std::string label,
                                   State<std::string> &source,
                                   std::vector<std::string> values)
    : EditableComboBox(std::move(label), source.binding(), std::move(values)) {}
EditableComboBox::EditableComboBox(std::string label,
                                   Binding<std::string> source,
                                   OptionsProvider provider)
    : label_(std::move(label)), selected_(std::move(source)),
      options_(std::move(provider)) {}
EditableComboBox::EditableComboBox(std::string label,
                                   State<std::string> &source,
                                   OptionsProvider provider)
    : EditableComboBox(std::move(label), source.binding(),
                       std::move(provider)) {}
EditableComboBox &&EditableComboBox::filter(Filter value) && {
  filter_ = std::move(value);
  return std::move(*this);
}
EditableComboBox &&EditableComboBox::placeholder(std::string value) && {
  placeholder_ = std::move(value);
  return std::move(*this);
}
EditableComboBox &&EditableComboBox::style(EditableComboBoxStyle value) && {
  detail::validate_control_extent(value.chevron_width);
  if (!value.maximum_visible_rows)
    throw std::invalid_argument("Popup must display at least one row");
  style_ = std::move(value);
  return std::move(*this);
}
Spec EditableComboBox::spec() && {
  Spec result{[label = std::move(label_), source = selected_,
               provider = std::move(options_), filter = std::move(filter_),
               placeholder = std::move(placeholder_),
               style = std::move(style_)] {
                return std::make_unique<EditableChoiceFrame>(
                    label, source, provider, filter, placeholder, style);
              },
              {}};
  result.children_factory = [](Component &component) {
    return static_cast<EditableChoiceFrame &>(component).children();
  };
  return result;
}
} // namespace ui
