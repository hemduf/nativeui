#include "detail/widget_editable_text_context.hpp"
#include "detail/widget_input_action.hpp"
#include "detail/widget_text_input_policy.hpp"
#include <nativeui/editable_text.hpp>
namespace ui {
namespace detail {
struct EditableModel;
enum class EditRequest { None, Begin, Accept, Cancel };
struct EditableTextControl {
  std::weak_ptr<EditableModel> target;
  EditRequest pending{EditRequest::None};
  std::uint64_t request_generation{};
};
struct EditableTextAccess {
  static std::shared_ptr<EditableTextControl>
  control(const std::shared_ptr<EditableTextController> &controller) {
    return controller->control_;
  }
};
struct EditableModel : std::enable_shared_from_this<EditableModel> {
  explicit EditableModel(Binding<std::string> value)
      : source(std::move(value)), seen(source.get()), draft(seen) {}
  Binding<std::string> source;
  std::string seen, error;
  State<std::string> draft;
  EditableText::Validator validator;
  std::shared_ptr<EditableTextControl> control;
  std::shared_ptr<TextInputPolicy> policy;
  std::weak_ptr<TextInputSession> editor;
  std::shared_ptr<const EditableTextRowContext> row;
  Dispatcher dispatcher;
  TimerHandle rename_timer;
  std::function<void()> invalidate, availability, parent_focus;
  std::function<bool()> owner_guard;
  std::uint64_t generation{}, timer_generation{}, activation_generation{};
  bool mounted{}, active{}, editing{}, mutable_value{true}, select_stem{},
      validating{}, posted{}, enter_down{};
  bool can_write() const {
    return mounted && active && mutable_value && source.valid() &&
           (!row || !row->attached || row->attached());
  }
  std::size_t selection_end(std::string_view text) const noexcept {
    const auto dot = text.rfind('.');
    return select_stem && dot != std::string_view::npos && dot != 0
               ? dot
               : text.size();
  }
  void select_draft() {
    if (auto session = editor.lock())
      session->select(0, selection_end(draft.get()));
  }
  void cancel_timer() noexcept {
    ++timer_generation;
    (void)dispatcher.cancel(rename_timer);
    rename_timer = {};
  }
  void phase_invalidation() {
    if (availability)
      availability();
    if (invalidate)
      invalidate();
  }
  void begin() {
    if (!can_write() || editing)
      return;
    auto text = source.get();
    seen = text;
    error.clear();
    ++generation;
    draft.set(std::move(text));
    if (!can_write())
      return;
    editing = true;
    cancel_timer();
    if (auto session = editor.lock()) {
      session->replace(draft.get());
      select_draft();
    }
    phase_invalidation();
    if (can_write())
      if (auto session = editor.lock())
        session->request_focus();
  }
  std::uint64_t finish(bool focus_parent) {
    const auto serial = ++generation;
    editing = false;
    error.clear();
    cancel_timer();
    if (auto session = editor.lock()) {
      session->cancel_capture();
      session->cancel_composition();
    }
    phase_invalidation();
    if (focus_parent && mounted && parent_focus)
      parent_focus();
    return serial;
  }
  void cancel(bool focus_parent = true) {
    if (!mounted || !editing)
      return;
    finish(focus_parent);
  }
  void accept(bool blur = false) {
    if (!editing || !can_write() || validating)
      return;
    const std::string text = draft.get();
    const auto serial = generation;
    const auto expected = source.get();
    auto validate = validator;
    if (!editing || !can_write() || generation != serial)
      return;
    std::optional<std::string> message;
    const bool before = std::exchange(validating, true);
    try {
      if (validate)
        message = validate(text);
    } catch (...) {
      validating = before;
      enter_down = false;
      throw;
    }
    validating = before;
    if (!editing || !can_write() || generation != serial ||
        source.get() != expected)
      return;
    if (message) {
      if (blur) {
        cancel(false);
        return;
      }
      error = std::move(*message);
      policy->border_override = Color{0.85f, 0.2f, 0.2f, 1};
      if (invalidate)
        invalidate();
      return;
    }
    auto source_copy = source;
    policy->border_override = {};
    const auto committed = finish(!blur);
    if (!editing && can_write() && generation == committed &&
        source_copy.valid() && source_copy.get() == expected &&
        source_copy.get() != text && (!owner_guard || owner_guard())) {
      seen = text;
      source_copy.set(text);
    }
  }
  void sync() {
    if (!mounted)
      return;
    policy->read_only = !can_write();
    if (!source.valid()) {
      if (editing)
        cancel(false);
      control->pending = EditRequest::None;
      cancel_timer();
      return;
    }
    if (source.get() != seen) {
      std::string text = source.get();
      ++generation;
      seen = text;
      error.clear();
      policy->border_override = {};
      draft.set(std::move(text));
      if (auto session = editor.lock()) {
        session->replace(draft.get());
        if (editing)
          select_draft();
      }
      if (invalidate)
        invalidate();
    }
    if (auto session = editor.lock())
      session->refresh_source();
    if (row && rename_timer.valid() &&
        ((row->attached && !row->attached()) ||
         (row->selected && !row->selected())))
      cancel_timer();
    queue();
  }
  void queue() noexcept {
    if (!mounted || !active || posted ||
        control->pending == EditRequest::None || !dispatcher.valid())
      return;
    const std::weak_ptr<EditableModel> weak = shared_from_this();
    const auto identity = control->request_generation;
    const auto epoch = activation_generation;
    try {
      const bool accepted = dispatcher.post([weak, identity, epoch] {
        const auto model = weak.lock();
        if (!model || !model->mounted || !model->active ||
            model->activation_generation != epoch)
          return;
        model->posted = false;
        if (!model->mounted)
          return;
        if (model->control->request_generation != identity) {
          model->queue();
          return;
        }
        const auto request =
            std::exchange(model->control->pending, EditRequest::None);
        if (request == EditRequest::Begin)
          model->begin();
        else if (request == EditRequest::Accept)
          model->accept();
        else if (request == EditRequest::Cancel)
          model->cancel();
      });
      posted = accepted;
    } catch (...) {
      posted = false;
    }
  }
  void slow_click() {
    cancel_timer();
    if (!row || !can_write())
      return;
    const bool selected = !row->selected || row->selected();
    auto request_select = row->request_select;
    if (!selected) {
      if (request_select)
        request_select();
      return;
    }
    const auto row_generation =
        row->interaction_generation ? row->interaction_generation() : 0;
    const auto serial = timer_generation;
    const std::weak_ptr<EditableModel> weak = shared_from_this();
    if (!dispatcher.valid())
      return;
    try {
      rename_timer = dispatcher.schedule_after(
          DispatcherDuration{0.5}, [weak, serial, row_generation] {
            const auto model = weak.lock();
            if (!model || model->timer_generation != serial)
              return;
            model->rename_timer = {};
            if (!model->can_write() || !model->row ||
                (model->row->selected && !model->row->selected()) ||
                (model->row->interaction_generation &&
                 model->row->interaction_generation() != row_generation))
              return;
            model->begin();
          });
    } catch (...) {
      rename_timer = {};
    }
  }
};
} // namespace detail
namespace {
class EditorSlot final : public Component {
public:
  explicit EditorSlot(std::shared_ptr<detail::EditableModel> state)
      : state_(std::move(state)) {}
  ComponentAvailability local_availability() const noexcept override {
    return {state_->editing ? VisibilityMode::Visible : VisibilityMode::Hidden,
            true, false};
  }
  Size measure(const std::vector<ChildMetrics> &children) const override {
    return children.empty() ? Size{} : children.front().preferred;
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    if (!children.empty())
      children.front().bounds = bounds;
  }
  void paint(PaintContext &) const override {}

private:
  std::shared_ptr<detail::EditableModel> state_;
};
class EditableComponent final : public Component,
                                public detail::ThemeBinding,
                                public detail::EditableTextContextConsumer {
public:
  EditableComponent(std::string label, Binding<std::string> value,
                    std::shared_ptr<EditableTextController> controller,
                    bool stem, EditableText::Validator validator,
                    EditableTextStyle style)
      : label_(std::move(label)), style_(std::move(style)),
        controller_(std::move(controller)),
        state_(std::make_shared<detail::EditableModel>(std::move(value))) {
    state_->select_stem = stem;
    state_->validator = std::move(validator);
    state_->control = detail::EditableTextAccess::control(controller_);
    if (auto other = state_->control->target.lock(); other && other->mounted)
      throw std::logic_error("EditableText controller is already mounted");
    state_->control->target = state_;
    state_->control->pending = detail::EditRequest::None;
    ++state_->control->request_generation;
    state_->policy = std::make_shared<detail::TextInputPolicy>();
    const std::weak_ptr<detail::EditableModel> weak = state_;
    state_->policy->before_input =
        [weak](const InputEvent &event, InputContext &context,
               const detail::TextInputSnapshot &snapshot)
        -> std::optional<EventResult> {
      const auto state = weak.lock();
      if (!state || !state->mounted)
        return EventResult::Ignored;
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
      const bool accept =
          (event.type == InputType::KeyDown && event.key == Key::Enter) ||
          (event.type == InputType::Command &&
           event.command == Command::Submit);
      if (snapshot.composition_active) {
        if (cancel) {
          if (auto editor = state->editor.lock()) {
            editor->cancel_capture();
            editor->cancel_composition();
          }
          context.invalidate();
          return EventResult::Handled;
        }
        if (accept)
          return EventResult::Handled;
        return {};
      }
      if (cancel) {
        state->cancel();
        return EventResult::Handled;
      }
      if (accept) {
        if (event.type == InputType::KeyDown &&
            std::exchange(state->enter_down, true))
          return EventResult::Handled;
        state->accept();
        return EventResult::Handled;
      }
      if (!state->source.valid() && (event.type == InputType::TextInput ||
                                     event.type == InputType::Command ||
                                     event.type == InputType::Composition))
        return EventResult::Handled;
      return {};
    };
    state_->policy->focus_changed = [weak](bool focused,
                                           detail::TextInputSnapshot snapshot) {
      const auto state = weak.lock();
      if (!state)
        return;
      if (!focused) {
        state->enter_down = false;
        if (snapshot.allow_edit_commit)
          state->accept(true);
        else
          state->cancel(false);
      }
    };
  }
  bool focusable() const noexcept override { return !state_->editing; }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool clips_children() const noexcept override { return true; }
  void bind_editable_row_context(
      std::shared_ptr<const detail::EditableTextRowContext> row) override {
    state_->row = std::move(row);
  }
  Size measure(const std::vector<ChildMetrics> &children) const override {
    const auto text = TextService::measure(state_->source.get(), style_.text);
    const float height =
        children.empty() ? text.height
                         : std::max(text.height, children.front().preferred.h);
    return {std::max(text.width,
                     children.empty() ? 0.0f : children.front().preferred.w),
            height + style_.error_height};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    if (!children.empty())
      children.front().bounds = {
          bounds.x, bounds.y, bounds.w,
          std::max(0.0f, bounds.h - style_.error_height)};
  }
  void mount(MountContext &context) override {
    if (auto other = state_->control->target.lock();
        other && other != state_ && other->mounted)
      throw std::logic_error("EditableText controller is already mounted");
    state_->control->target = state_;
    state_->mounted = true;
    state_->mutable_value = effective_enabled() && !effective_read_only();
    state_->invalidate = context.invalidator();
    state_->availability = context.availability_invalidator();
    state_->parent_focus = context.focus_requester();
    state_->owner_guard = detail::InputMutationAccess::guard(context);
    const std::weak_ptr<detail::EditableModel> weak = state_;
    source_subscription_ = state_->source.observe([weak](const auto &) {
      if (auto state = weak.lock())
        state->sync();
    });
    draft_subscription_ = state_->draft.observe([weak](const auto &) {
      if (auto state = weak.lock()) {
        ++state->generation;
        state->enter_down = false;
        state->error.clear();
        state->policy->border_override = {};
        if (state->invalidate)
          state->invalidate();
      }
    });
  }
  void activate(LifecycleContext &context) override {
    state_->active = true;
    ++state_->activation_generation;
    state_->dispatcher = context.dispatcher();
    state_->queue();
  }
  void deactivate(LifecycleContext &) override {
    state_->active = false;
    ++state_->activation_generation;
    if (state_->posted) {
      state_->control->pending = detail::EditRequest::None;
      ++state_->control->request_generation;
    }
    state_->posted = false;
    state_->cancel_timer();
    state_->dispatcher = {};
    state_->enter_down = false;
    state_->editing = false;
  }
  void unmount(LifecycleContext &) override {
    state_->mounted = false;
    state_->editing = false;
    ++state_->generation;
    state_->cancel_timer();
    source_subscription_.reset();
    draft_subscription_.reset();
    state_->invalidate = {};
    state_->availability = {};
    state_->parent_focus = {};
    state_->owner_guard = {};
    if (state_->control->target.lock() == state_) {
      state_->control->target.reset();
      state_->control->pending = detail::EditRequest::None;
      ++state_->control->request_generation;
    }
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto state = state_;
    if (event.type == InputType::KeyUp && event.key == Key::Enter) {
      state->enter_down = false;
      return EventResult::Handled;
    }
    if (event.type == InputType::PointerCancel ||
        event.type == InputType::PointerWheel) {
      state->cancel_timer();
      return EventResult::Ignored;
    }
    if (event.type == InputType::PointerDown) {
      state->cancel_timer();
      if (!state->can_write())
        return EventResult::Handled;
      if (event.clicks >= 2) {
        if (state->row)
          return EventResult::Ignored;
        state->begin();
        return EventResult::Handled;
      }
      if (state->row)
        state->slow_click();
      context.invalidate();
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown &&
        (event.key == Key::Enter || event.key == Key::F2)) {
      if (event.key == Key::Enter && std::exchange(state->enter_down, true))
        return EventResult::Handled;
      state->begin();
      return EventResult::Handled;
    }
    if (event.type == InputType::Command && event.command == Command::Submit) {
      state->begin();
      return EventResult::Handled;
    }
    return EventResult::Ignored;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    focused_ = focused;
    if (!focused)
      state_->cancel_timer();
    context.invalidate();
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = state_->editing ? SemanticRole::Group : SemanticRole::Text;
    info.name = label_;
    info.text_value = state_->source.get();
    info.description = state_->error;
    info.focusable = !state_->editing;
    info.focused = focused_;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only() || !state_->source.valid();
    if (info.enabled && info.focusable) {
      info.actions = {SemanticAction::Focus};
      if (!info.read_only)
        info.actions.push_back(SemanticAction::Activate);
    }
    return info;
  }
  std::vector<Spec> children() const {
    const auto state = state_;
    const auto style = style_;
    const auto label = label_;
    Spec input{[state, style, label] {
                 auto editor = std::make_unique<TextInputComponent>(
                     label, state->draft.binding(), "", 0,
                     TextInputComponent::SubmitCallback{}, style.text_input);
                 state->editor =
                     detail::TextInputAccess::configure(*editor, state->policy);
                 return editor;
               },
               {}};
    return {Spec{[state] { return std::make_unique<EditorSlot>(state); },
                 {std::move(input)}}};
  }
  void paint(PaintContext &context) const override {
    const auto bounds = context.bounds();
    auto &painter = context.painter();
    auto clip = painter.scoped_clip(bounds);
    if (!state_->editing) {
      auto text = style_.text;
      if (text.color.a == 0)
        text.color = current_theme().palette.text;
      painter.text({bounds.x, bounds.y + bounds.h * 0.5f}, state_->source.get(),
                   text);
      if (focused_)
        painter.stroke_rounded_rect(bounds, current_theme().radii.sm, 1.0f,
                                    current_theme().palette.focus);
    }
    if (!state_->error.empty() && style_.error_height > 0) {
      auto text = style_.text;
      text.color = style_.invalid_color.value_or(Color{0.85f, 0.2f, 0.2f, 1});
      painter.text({bounds.x, bounds.y + bounds.h - style_.error_height * 0.5f},
                   state_->error, text);
    }
  }

private:
  void retained_checkpoint() override { state_->sync(); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    state_->policy->read_only =
        !state_->mounted || !state_->mutable_value || !state_->source.valid();
    if (!state_->mutable_value)
      state_->cancel_timer();
  }
  std::string label_;
  EditableTextStyle style_;
  std::shared_ptr<EditableTextController> controller_;
  std::shared_ptr<detail::EditableModel> state_;
  Binding<std::string>::Subscription source_subscription_, draft_subscription_;
  bool focused_{};
};
} // namespace
EditableTextController::EditableTextController()
    : control_(std::make_shared<detail::EditableTextControl>()) {}
EditableTextController::~EditableTextController() = default;
namespace {
void request_edit(const std::shared_ptr<detail::EditableTextControl> &control,
                  detail::EditRequest request) {
  const auto target = control->target.lock();
  if (!target || !target->mounted || !target->source.valid())
    return;
  control->pending = request;
  ++control->request_generation;
  target->queue();
}
} // namespace
void EditableTextController::begin() {
  request_edit(control_, detail::EditRequest::Begin);
}
void EditableTextController::accept() {
  request_edit(control_, detail::EditRequest::Accept);
}
void EditableTextController::cancel() {
  request_edit(control_, detail::EditRequest::Cancel);
}
bool EditableTextController::editing() const noexcept {
  const auto model = control_->target.lock();
  return model && model->mounted && model->editing;
}
EditableText::EditableText(std::string label, Binding<std::string> value)
    : label_(std::move(label)), value_(std::move(value)) {}
EditableText::EditableText(std::string label, State<std::string> &value)
    : EditableText(std::move(label), value.binding()) {}
EditableText &&
EditableText::controller(std::shared_ptr<EditableTextController> value) && {
  if (!value)
    throw std::invalid_argument("EditableText controller is missing");
  controller_ = std::move(value);
  return std::move(*this);
}
EditableText &&EditableText::select_stem(bool value) && {
  select_stem_ = value;
  return std::move(*this);
}
EditableText &&EditableText::validator(Validator value) && {
  validator_ = std::move(value);
  return std::move(*this);
}
EditableText &&EditableText::style(EditableTextStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec EditableText::spec() && {
  detail::validate_control_extent(style_.error_height);
  Spec spec{[label = std::move(label_), source = value_,
             controller = std::move(controller_), stem = select_stem_,
             validator = std::move(validator_), style = std::move(style_)] {
              const auto effective =
                  controller ? controller
                             : std::make_shared<EditableTextController>();
              return std::make_unique<EditableComponent>(
                  label, source, effective, stem, validator, style);
            },
            {}};
  spec.children_factory = [](Component &component) {
    return static_cast<EditableComponent &>(component).children();
  };
  return spec;
}
} // namespace ui
