#include "detail/widget_choice_popup.hpp"
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
namespace ui {
namespace {
TextStyle anchor_text(const ResolvedComboBoxStyle &style) {
  TextStyle text;
  text.size = style.text_size;
  text.color = style.text;
  text.weight = style.text_weight;
  text.slant = style.text_slant;
  text.family = style.font_family;
  text.fallback_families = style.fallback_families;
  text.align = TextAlign::Center;
  return text;
}
TextStyle row_text(const ResolvedMenuItemStyle &style) {
  TextStyle text;
  text.size = style.text_size;
  text.color = style.text;
  text.weight = style.text_weight;
  text.slant = style.text_slant;
  text.family = style.font_family;
  text.fallback_families = style.fallback_families;
  return text;
}
class ChoiceSemanticRow final : public Component {
public:
  ChoiceSemanticRow(std::shared_ptr<detail::ChoicePopupSession> session,
                    std::size_t index)
      : session_(std::move(session)), index_(index),
        generation_(session_->generation),
        label_(index < session_->rows.size() ? session_->rows[index].label
                                             : "Aucun résultat") {}
  Size measure(const std::vector<ChildMetrics> &) const override { return {}; }
  bool pointer_targetable() const noexcept override { return false; }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.name = label_;
    info.text_value = label_;
    info.role = index_ == detail::no_choice ? SemanticRole::Text
                                            : SemanticRole::ListItem;
    const bool current = session_->live && generation_ == session_->generation;
    info.enabled = current && index_ < session_->rows.size() &&
                   session_->rows[index_].enabled;
    info.selected = current && index_ == session_->selected;
    if (current && index_ == session_->highlighted)
      info.description = "Option surlignée";
    if (info.enabled)
      info.actions = {SemanticAction::Select};
    return info;
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    const auto session = session_;
    const auto index = index_;
    const auto generation = generation_;
    auto callback = session->choose;
    context.invalidate();
    if (action == SemanticAction::Select && session->live &&
        session->generation == generation && index < session->rows.size() &&
        session->rows[index].enabled &&
        (!session->allowed || session->allowed()) &&
        detail::InputMutationAccess::allowed(context) && callback)
      callback(index, Key::None);
    return EventResult::Handled;
  }
  void paint(PaintContext &) const override {}

private:
  std::shared_ptr<detail::ChoicePopupSession> session_;
  std::size_t index_{};
  std::uint64_t generation_{};
  std::string label_;
};
class ChoicePanel final : public Component,
                          public detail::ThemeBinding,
                          public detail::OverlayCommandSource,
                          public detail::DynamicChildrenSource {
public:
  explicit ChoicePanel(std::shared_ptr<detail::ChoicePopupSession> session)
      : session_(std::move(session)) {}
  bool focusable() const noexcept override { return session_->focusable; }
  bool pointer_targetable() const noexcept override { return true; }
  bool clips_children() const noexcept override { return true; }
  std::vector<std::string> desired_keys() const override {
    std::vector<std::string> keys;
    if (session_->focusable)
      return keys;
    for (std::size_t index = 0; index < session_->rows.size(); ++index)
      keys.push_back(std::to_string(session_->generation) + ":" +
                     std::to_string(index));
    if (keys.empty())
      keys.push_back(std::to_string(session_->generation) + ":empty");
    return keys;
  }
  std::vector<detail::DynamicChildSpec> desired_children() const override {
    auto keys = desired_keys();
    std::vector<detail::DynamicChildSpec> children;
    for (std::size_t index = 0; index < keys.size(); ++index) {
      const auto item = session_->rows.empty() ? detail::no_choice : index;
      children.push_back(
          {keys[index], Spec{[session = session_, item] {
                               return std::make_unique<ChoiceSemanticRow>(
                                   session, item);
                             },
                             {}}});
    }
    return children;
  }
  void set_structure_invalidator(std::function<void()> callback) override {
    session_->structure_invalidator = std::move(callback);
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    float y = bounds.y - session_->scroll;
    for (std::size_t index = 0; index < children.size(); ++index) {
      const float height = index < session_->rows.size()
                               ? resolved(index).row_height
                               : base().row_height;
      children[index].bounds = {bounds.x, y, bounds.w, std::max(0.f, height)};
      y += height;
    }
  }
  Size measure(const std::vector<ChildMetrics> &) const override {
    float width = std::max(session_->minimum_width,
                           current_theme().controls.minimum_width);
    float height{};
    for (std::size_t index = 0; index < session_->rows.size(); ++index) {
      const auto style = resolved(index);
      width = std::max(width, TextService::measure(session_->rows[index].label,
                                                   row_text(style))
                                      .width +
                                  2 * style.horizontal_padding);
      if (!session_->maximum_rows || index < session_->maximum_rows)
        height += std::max(0.f, style.row_height);
    }
    if (session_->rows.empty())
      height = base().row_height;
    return {width, height};
  }
  void mount(MountContext &context) override {
    session_->invalidator = context.invalidator();
  }
  void unmount(LifecycleContext &) override {
    session_->invalidator = {};
    session_->structure_invalidator = {};
    armed_ = false;
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto session = session_;
    if (!session->live || (session->allowed && !session->allowed()))
      return EventResult::Handled;
    if (session->suppressed != Key::None && event.key == session->suppressed) {
      if (event.type == InputType::KeyUp)
        session->suppressed = Key::None;
      if (event.type == InputType::KeyUp || event.type == InputType::KeyDown)
        return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown) {
      if (event.key == Key::Up || event.key == Key::Down) {
        highlight(
            detail::choice_step(*session, event.key == Key::Down ? 1 : -1),
            context);
        return EventResult::Handled;
      }
      if (event.key == Key::Home || event.key == Key::End) {
        const auto before = session->highlighted;
        session->highlighted = detail::no_choice;
        const auto next =
            detail::choice_step(*session, event.key == Key::Home ? 1 : -1);
        session->highlighted = before;
        highlight(next, context);
        return EventResult::Handled;
      }
      if (event.key == Key::Enter || event.key == Key::Space) {
        choose(session->highlighted, event.key, context);
        return EventResult::Handled;
      }
    }
    if (event.type == InputType::PointerWheel) {
      const float amount = -event.delta.y;
      session->scroll =
          std::clamp(session->scroll + amount, 0.f,
                     std::max(0.f, total_height() - context.bounds().h));
      context.invalidate();
      return EventResult::Handled;
    }
    if (event.type == InputType::PointerCancel) {
      armed_ = false;
      context.release_pointer();
      context.invalidate();
      return EventResult::Handled;
    }
    if (event.type == InputType::PointerDown) {
      armed_ = true;
      armed_generation_ = session->generation;
      context.capture_pointer();
      highlight(index_at(event.position, context.bounds()), context);
      return EventResult::Handled;
    }
    if (event.type == InputType::PointerMove) {
      highlight(index_at(event.position, context.bounds()), context);
      return EventResult::Handled;
    }
    if (event.type == InputType::PointerUp) {
      const bool activate = std::exchange(armed_, false) &&
                            session->generation == armed_generation_;
      context.release_pointer();
      if (activate && session->generation == armed_generation_)
        choose(index_at(event.position, context.bounds()), Key::None, context);
      return EventResult::Handled;
    }
    return EventResult::Ignored;
  }
  std::optional<detail::OverlayComponentCommand>
  take_overlay_command() override {
    auto callback = session_->take_command;
    return callback ? callback()
                    : std::optional<detail::OverlayComponentCommand>{};
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::ListView;
    info.description = session_->rows.empty() ? "Aucun résultat" : "Options";
    return info;
  }
  void paint(PaintContext &context) const override {
    if (!session_->live)
      return;
    const auto bounds = context.bounds();
    auto &painter = context.painter();
    auto clip = painter.scoped_clip(bounds);
    painter.fill_rounded_rect(bounds, current_theme().radii.medium,
                              current_theme().palette.surface);
    float y = bounds.y - session_->scroll;
    if (session_->rows.empty()) {
      auto style = base();
      painter.text(
          {bounds.x + style.horizontal_padding, bounds.y + bounds.h * .5f},
          "Aucun résultat", row_text(style));
    }
    for (std::size_t index = 0; index < session_->rows.size(); ++index) {
      const auto style = resolved(index);
      const float height = std::max(0.f, style.row_height);
      Rect row{bounds.x, y, bounds.w, height};
      if (!intersect(row, bounds).empty()) {
        painter.fill_rounded_rect(row, style.corner_radius, style.fill);
        painter.text({row.x + style.horizontal_padding, row.y + row.h * .5f},
                     session_->rows[index].label, row_text(style));
      }
      y += height;
    }
    painter.stroke_rounded_rect(bounds, current_theme().radii.medium,
                                current_theme().controls.border_width,
                                current_theme().palette.border);
  }

private:
  ResolvedMenuItemStyle base() const {
    return resolve_menu_item_style(default_menu_item_style(current_theme()),
                                   session_->style, {});
  }
  ResolvedMenuItemStyle resolved(std::size_t index) const {
    VisualState visual;
    visual.enabled = session_->rows[index].enabled;
    visual.hovered = index == session_->highlighted;
    visual.selected = visual.hovered;
    visual.pressed = visual.hovered && armed_;
    return resolve_menu_item_style(default_menu_item_style(current_theme()),
                                   session_->style, visual);
  }
  float total_height() const {
    float height{};
    for (std::size_t i = 0; i < session_->rows.size(); ++i)
      height += std::max(0.f, resolved(i).row_height);
    return height;
  }
  std::size_t index_at(Point point, Rect bounds) const {
    if (!bounds.contains(point))
      return detail::no_choice;
    float y = bounds.y - session_->scroll;
    for (std::size_t i = 0; i < session_->rows.size(); ++i) {
      const float height = std::max(0.f, resolved(i).row_height);
      if (point.y >= y && point.y < y + height)
        return session_->rows[i].enabled ? i : detail::no_choice;
      y += height;
    }
    return detail::no_choice;
  }
  void highlight(std::size_t index, InputContext &context) {
    if (session_->highlighted == index)
      return;
    session_->highlighted = index;
    float top{};
    for (std::size_t i = 0; i < index && i < session_->rows.size(); ++i)
      top += std::max(0.f, resolved(i).row_height);
    if (index < session_->rows.size()) {
      const float bottom = top + std::max(0.f, resolved(index).row_height);
      if (top < session_->scroll)
        session_->scroll = top;
      else if (bottom > session_->scroll + context.bounds().h)
        session_->scroll = bottom - context.bounds().h;
    }
    context.invalidate();
  }
  void choose(std::size_t index, Key key, InputContext &context) {
    const auto session = session_;
    const auto generation = session->generation;
    if (index >= session->rows.size() || !session->rows[index].enabled)
      return;
    auto callback = session->choose;
    context.invalidate();
    if (session->live && session->generation == generation &&
        (!session->allowed || session->allowed()) && callback)
      callback(index, key);
  }
  std::shared_ptr<detail::ChoicePopupSession> session_;
  bool armed_{};
  std::uint64_t armed_generation_{};
};
struct ComboState : std::enable_shared_from_this<ComboState> {
  explicit ComboState(detail::ComboBoxAdapter value)
      : adapter(std::move(value)), display(adapter.initial) {}
  detail::ComboBoxAdapter adapter;
  detail::ComboBoxSnapshot display;
  std::shared_ptr<detail::ChoicePopupSession> session;
  std::shared_ptr<void> subscription;
  std::optional<detail::OverlayComponentCommand> pending;
  std::function<void()> invalidate, layout;
  std::function<bool()> owner_guard;
  std::string text, placeholder;
  MenuItemStyle item_style;
  OverlayHandle handle;
  NodeId node{kInvalidNodeId};
  Key suppressed{Key::None};
  std::uint64_t generation{};
  std::uint64_t source_revision{};
  bool mounted{}, mutable_value{true};
  bool allowed() const {
    return mounted && mutable_value && adapter.valid() &&
           (!owner_guard || owner_guard());
  }
  void refresh() {
    if (!mounted)
      return;
    if (!adapter.valid()) {
      if (session)
        session->live = false;
      return;
    }
    const auto revision = adapter.revision();
    const bool changed = revision != source_revision;
    if (changed) {
      source_revision = revision;
      ++generation;
      if (session && session->live)
        session->generation = generation;
    }
    const auto serial = generation;
    const auto index = display.selected_index();
    if (!mounted || generation != serial || adapter.revision() != revision)
      return;
    if (session && session->live) {
      session->selected = index;
      if (changed && session->invalidator)
        session->invalidator();
      if (!mounted || generation != serial || adapter.revision() != revision)
        return;
    }
    std::string next =
        index < display.rows.size() ? display.rows[index].label : placeholder;
    if (next == text)
      return;
    text = std::move(next);
    if (layout)
      layout();
    if (invalidate)
      invalidate();
  }
  void commit(std::size_t index, Key key) {
    if (!allowed() || !session || !session->live || pending ||
        index >= display.rows.size())
      return;
    const auto serial = generation;
    const std::weak_ptr<ComboState> weak = shared_from_this();
    auto guard = [weak, serial] {
      const auto state = weak.lock();
      return state && state->allowed() && state->generation == serial;
    };
    auto publish = display.prepare_selection(index, guard);
    if (!publish || !guard())
      return;
    session->live = false;
    if (key != Key::None)
      suppressed = key;
    pending = detail::OverlayComponentCommand::close_then_invoke(
        handle, node, true,
        [weak, serial, publish = std::move(publish)]() mutable {
          const auto state = weak.lock();
          if (!state || !state->allowed() || state->generation != serial)
            return;
          state->handle = {};
          publish();
        });
  }
};
class ComboAnchor final : public Component,
                          public detail::ThemeBinding,
                          public detail::OverlayCommandSource,
                          public detail::OverlayAnchorPolicy {
public:
  ComboAnchor(detail::ComboBoxAdapter adapter, std::string placeholder,
              ComboBoxStyle style, MenuItemStyle items)
      : state_(std::make_shared<ComboState>(std::move(adapter))),
        style_(std::move(style)) {
    state_->placeholder = std::move(placeholder);
    state_->item_style = std::move(items);
  }
  bool focusable() const noexcept override { return true; }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool dismiss_overlay_on_tab() const noexcept override { return true; }
  bool dismiss_overlay_when_read_only() const noexcept override { return true; }
  bool overlay_session_valid() const noexcept override {
    return !state_->session || state_->session->live;
  }
  Size measure(const std::vector<ChildMetrics> &) const override {
    const auto style = resolved();
    return {
        std::max(style.minimum_width,
                 TextService::measure(state_->text, anchor_text(style)).width +
                     2 * style.horizontal_padding),
        style.control_height};
  }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->node = context.node_id();
    state_->owner_guard = detail::InputMutationAccess::guard(context);
    state_->invalidate = context.invalidator();
    state_->layout = context.layout_invalidator();
    const std::weak_ptr<ComboState> weak = state_;
    state_->subscription = state_->adapter.observe([weak] {
      if (const auto state = weak.lock()) {
        state->refresh();
      }
    });
    state_->refresh();
  }
  void unmount(LifecycleContext &) override {
    state_->mounted = false;
    ++state_->generation;
    if (state_->session)
      state_->session->live = false;
    state_->subscription.reset();
    state_->pending.reset();
    state_->handle = {};
    state_->invalidate = {};
    state_->layout = {};
    state_->owner_guard = {};
  }
  void deactivate(LifecycleContext &context) override {
    interaction_.deactivate(context, false);
    focused_ = false;
    state_->suppressed = Key::None;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    focused_ = focused;
    interaction_.focus_changed(focused, context, false);
    context.invalidate();
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto state = state_;
    if (state->suppressed != Key::None && event.key == state->suppressed) {
      if (event.type == InputType::KeyUp)
        state->suppressed = Key::None;
      if (event.type == InputType::KeyDown || event.type == InputType::KeyUp)
        return EventResult::Handled;
    }
    if (!state->allowed()) {
      interaction_.cancel_pending_mutation(context, false);
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown && event.key == Key::Down)
      return open(context, Key::Down);
    const auto result = interaction_.input(event, context, true, false);
    context.invalidate();
    if (!result.activate)
      return result.result;
    return open(context,
                event.type == InputType::KeyDown ? event.key : Key::None);
  }
  std::optional<detail::OverlayComponentCommand>
  take_overlay_command() override {
    return std::exchange(state_->pending, {});
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::ComboBox;
    info.text_value = state_->text;
    info.name = state_->text;
    info.focusable = true;
    info.focused = focused_;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only() || !state_->adapter.valid();
    info.expanded = state_->handle.valid() ? SemanticExpandedState::Expanded
                                           : SemanticExpandedState::Collapsed;
    if (info.enabled)
      info.actions = {SemanticAction::Focus};
    if (info.enabled && !info.read_only)
      info.actions.push_back(SemanticAction::Expand);
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto style = resolved();
    const auto bounds = context.bounds();
    auto &painter = context.painter();
    painter.fill_rounded_rect(bounds, style.corner_radius, style.fill);
    painter.stroke_rounded_rect(bounds, style.corner_radius, style.border_width,
                                style.border);
    painter.text({bounds.x + bounds.w * .5f, bounds.y + bounds.h * .5f},
                 state_->text, anchor_text(style));
  }

private:
  EventResult open(InputContext &context, Key key) {
    const auto state = state_;
    if (!state->allowed() || state->handle.valid() || state->pending)
      return EventResult::Handled;
    const auto serial = state->generation;
    auto provider = state->adapter.snapshot;
    auto snapshot = provider();
    const auto selected = snapshot.selected_index();
    if (!state->allowed() || state->generation != serial ||
        !detail::InputMutationAccess::allowed(context))
      return EventResult::Handled;
    auto session = std::make_shared<detail::ChoicePopupSession>();
    session->rows = snapshot.rows;
    session->selected = selected;
    session->highlighted = selected;
    if (selected >= session->rows.size() || !session->rows[selected].enabled) {
      session->highlighted = detail::no_choice;
      session->highlighted = detail::choice_step(*session, 1);
    }
    session->style = state->item_style;
    session->suppressed = key;
    session->generation = serial;
    const std::weak_ptr<ComboState> weak = state;
    const std::weak_ptr<detail::ChoicePopupSession> weak_session = session;
    session->allowed = [weak, weak_session] {
      const auto model = weak.lock();
      const auto current = weak_session.lock();
      return model && current && model->session == current &&
             model->allowed() && model->generation == current->generation;
    };
    session->choose = [weak](std::size_t index, Key trigger) {
      if (const auto model = weak.lock())
        model->commit(index, trigger);
    };
    session->take_command = [weak] {
      const auto model = weak.lock();
      return model ? std::exchange(model->pending, {})
                   : std::optional<detail::OverlayComponentCommand>{};
    };
    OverlaySpec overlay;
    overlay.mode = OverlayMode::Modal;
    overlay.anchor = state->node;
    overlay.placement = OverlayPlacement::AnchorBelow;
    overlay.dismiss_on_escape = true;
    overlay.dismiss_on_outside_pointer_down = true;
    overlay.content = detail::choice_popup_spec(session);
    state->display = std::move(snapshot);
    state->session = session;
    state->refresh();
    context.invalidate_layout();
    context.invalidate();
    if (!state->allowed() || state->generation != serial)
      return EventResult::Handled;
    state->suppressed = key;
    state->pending = detail::OverlayComponentCommand::show(
        std::move(overlay), [weak, session, serial](OverlayHandle handle) {
          if (const auto model = weak.lock()) {
            if (!model->mounted || !model->allowed() ||
                model->generation != serial || model->session != session ||
                !session->live) {
              session->live = false;
              if (handle.valid())
                model->pending =
                    detail::OverlayComponentCommand::close_then_invoke(
                        handle, model->node, false, {});
              return;
            }
            model->handle = handle;
            if (!handle.valid()) {
              model->suppressed = Key::None;
              session->live = false;
            }
          }
        });
    return EventResult::Handled;
  }
  ResolvedComboBoxStyle resolved() const {
    VisualState visual;
    visual.enabled = effective_enabled();
    visual.read_only = effective_read_only();
    visual.focused = focused_;
    visual.hovered = interaction_.hovered();
    visual.pressed = interaction_.pressed();
    return resolve_combo_box_style(default_combo_box_style(current_theme()),
                                   style_, visual);
  }
  void retained_checkpoint() override { state_->refresh(); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    if (!state_->mutable_value && state_->session)
      state_->session->live = false;
  }
  std::shared_ptr<ComboState> state_;
  ComboBoxStyle style_;
  detail::PressActivationState interaction_;
  bool focused_{};
};
} // namespace
namespace detail {
std::size_t choice_step(const ChoicePopupSession &session, int direction) {
  const auto count = session.rows.size();
  if (!count)
    return no_choice;
  const auto current = session.highlighted;
  if (current == no_choice || current >= count) {
    for (std::size_t step = 0; step < count; ++step) {
      const auto index = direction > 0 ? step : count - step - 1;
      if (session.rows[index].enabled)
        return index;
    }
    return no_choice;
  }
  for (std::size_t step = 1; step <= count; ++step) {
    if (!session.wrap && ((direction > 0 && step >= count - current) ||
                          (direction < 0 && step > current)))
      return current;
    const auto index = direction > 0 ? (current + step) % count
                                     : (current + count - step % count) % count;
    if (session.rows[index].enabled)
      return index;
  }
  return no_choice;
}
void choice_highlight(const std::shared_ptr<ChoicePopupSession> &session,
                      std::size_t index) {
  if (!session || !session->live || session->highlighted == index)
    return;
  session->highlighted = index;
  if (session->invalidator)
    session->invalidator();
}
Spec choice_popup_spec(std::shared_ptr<ChoicePopupSession> session) {
  return Spec{[session] { return std::make_unique<ChoicePanel>(session); }, {}};
}
Spec combo_box_spec(ComboBoxAdapter adapter, std::string placeholder,
                    ComboBoxStyle style, MenuItemStyle items) {
  return Spec{[adapter = std::move(adapter),
               placeholder = std::move(placeholder), style = std::move(style),
               items = std::move(items)] {
                return std::make_unique<ComboAnchor>(adapter, placeholder,
                                                     style, items);
              },
              {}};
}
} // namespace detail
} // namespace ui
