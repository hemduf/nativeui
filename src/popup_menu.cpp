#include "detail/widget_menu_popup.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
namespace ui {
PopupMenuItem PopupMenuItem::action(std::string label,
                                    std::function<void()> callback,
                                    bool enabled) {
  PopupMenuItem result;
  result.label = std::move(label);
  result.callback = std::move(callback);
  result.enabled = enabled;
  return result;
}
PopupMenuItem PopupMenuItem::separator() {
  PopupMenuItem result;
  result.kind = Kind::Separator;
  result.enabled = false;
  return result;
}
bool PopupMenuItem::actionable() const noexcept {
  return kind == Kind::Action && enabled &&
         (bool(callback) || !children.empty());
}
namespace detail {
namespace {
TextStyle item_text(const ResolvedMenuItemStyle &style,
                    TextAlign align = TextAlign::Left) {
  TextStyle text;
  text.size = style.text_size;
  text.color = style.text;
  text.align = align;
  text.family = style.font_family;
  text.fallback_families = style.fallback_families;
  text.weight = style.text_weight;
  text.slant = style.text_slant;
  return text;
}
TextStyle anchor_text(const ResolvedComboBoxStyle &style) {
  TextStyle text;
  text.size = style.text_size;
  text.color = style.text;
  text.align = TextAlign::Center;
  text.family = style.font_family;
  text.fallback_families = style.fallback_families;
  text.weight = style.text_weight;
  text.slant = style.text_slant;
  return text;
}
void validate(const std::vector<PopupMenuItem> &items, std::size_t depth) {
  if (!items.empty() && depth > 16)
    throw std::invalid_argument("PopupMenu depth exceeds 16");
  std::unordered_set<std::string> keys;
  for (const auto &item : items) {
    if (!item.key.empty() && !keys.insert(item.key).second)
      throw std::invalid_argument("PopupMenu sibling keys must be unique");
    if ((!item.children.empty() && item.callback) ||
        (item.kind == PopupMenuItem::Kind::Separator &&
         (!item.children.empty() || item.callback)))
      throw std::invalid_argument(
          "PopupMenu children require a submenu without an action");
    validate(item.children, depth + 1);
  }
}
void cancel_hover(const std::shared_ptr<MenuPopupSession> &session) noexcept {
  const auto timer = std::exchange(session->hover_timer, {});
  session->hover_due = false;
  session->hover_index = kNoPopupIndex;
  if (timer.valid()) {
    try {
      (void)session->dispatcher.cancel(timer);
    } catch (...) {
    }
  }
}
void release_contact(std::shared_ptr<MenuPopupSession> session) noexcept {
  session->pointer_armed = false;
  auto release = std::exchange(session->release, {});
  if (release) {
    try {
      release();
    } catch (...) {
    }
  }
}
void stop_session(std::shared_ptr<MenuPopupSession> session) noexcept {
  if (!session)
    return;
  session->live = false;
  cancel_hover(session);
  release_contact(session);
  stop_session(session->child);
}
void discard_session(std::shared_ptr<MenuPopupSession> session) noexcept {
  if (!session)
    return;
  stop_session(session);
  discard_session(session->child);
  session->child.reset();
  session->commands.clear();
}
std::shared_ptr<MenuPopupSession>
root_of(const std::shared_ptr<MenuPopupSession> &session) noexcept {
  auto root = session->root.lock();
  return root ? root : session;
}
bool identity_live(const std::shared_ptr<MenuPopupSession> &session) noexcept {
  const auto root = root_of(session);
  const auto anchor = session->anchor;
  return session->live && root->live && anchor && anchor->mounted &&
         anchor->active && anchor->interactive &&
         anchor->generation == session->generation &&
         (anchor->current.expired() || anchor->current.lock() == root);
}
bool allowed(const std::shared_ptr<MenuPopupSession> &session) {
  return identity_live(session) &&
         (!session->anchor->permission || session->anchor->permission());
}
std::optional<OverlayComponentCommand>
take(std::deque<OverlayComponentCommand> &commands) {
  if (commands.empty())
    return {};
  auto command = std::move(commands.front());
  commands.pop_front();
  return command;
}
Spec panel_spec(const std::shared_ptr<MenuPopupSession> &session) {
  Spec result{
      [session] { return std::make_unique<MenuPopupComponent>(session); }, {}};
  result.children_factory = [](Component &component) {
    return static_cast<MenuPopupComponent &>(component).initial_children();
  };
  return result;
}
void close_level(const std::shared_ptr<MenuPopupSession> &session,
                 InputContext *context = nullptr) {
  if (!identity_live(session))
    return;
  const auto parent = session->parent.lock();
  auto command = OverlayComponentCommand::close_then_invoke(
      session->handle, session->anchor->node_id, false, {});
  session->commands.push_back(std::move(command));
  session->level_close_requested = true;
  stop_session(session);
  if (parent && parent->child == session)
    parent->child.reset();
  if (context)
    context->invalidate();
}
void open_child(const std::shared_ptr<MenuPopupSession> &session) {
  const auto index = session->highlighted;
  if (!allowed(session) || index >= session->items.size() ||
      !session->items[index].actionable() ||
      session->items[index].children.empty() || index >= session->rows.size())
    return;
  if (session->child && session->child->live)
    return;
  const auto generation = session->generation;
  auto child = std::make_shared<MenuPopupSession>();
  child->show_pending = true;
  child->anchor = session->anchor;
  child->items = session->items[index].children;
  child->item_style = session->item_style;
  child->generation = generation;
  child->root = root_of(session);
  child->parent = session;
  child->highlighted =
      first_popup_index(child->items.size(), [&](std::size_t i) {
        return child->items[i].actionable();
      });
  if (!allowed(session) || session->generation != generation ||
      session->highlighted != index)
    return;
  OverlaySpec overlay;
  overlay.mode = OverlayMode::Modal;
  overlay.anchor = session->node_id;
  overlay.placement = OverlayPlacement::AnchorRight;
  overlay.dismiss_on_outside_pointer_down = true;
  overlay.content = panel_spec(child);
  const std::weak_ptr<MenuPopupSession> weak = session;
  auto command = OverlayComponentCommand::show(
      std::move(overlay), [weak, child, generation](OverlayHandle handle) {
        const auto parent = weak.lock();
        child->show_pending = false;
        if (!parent || !identity_live(parent) ||
            parent->generation != generation || parent->child != child) {
          child->live = false;
          if (parent && handle.valid())
            parent->commands.push_back(
                OverlayComponentCommand::close_then_invoke(
                    handle, parent->anchor->node_id, false, {}));
          return;
        }
        child->handle = handle;
        if (!handle.valid()) {
          stop_session(child);
          parent->child.reset();
        }
      });
  if (session->invalidate)
    session->invalidate();
  if (!allowed(session) || session->generation != generation ||
      session->highlighted != index)
    return;
  session->commands.push_back(std::move(command));
  session->submenu_anchor = session->rows[index];
  session->child = std::move(child);
  cancel_hover(session);
}
struct Completion final {
  std::weak_ptr<MenuAnchorRuntime> anchor;
  std::shared_ptr<MenuPopupSession> root;
  std::uint64_t generation{};
  std::function<void()> callback;
  bool started{};
  void invoke() {
    if (started)
      return;
    const auto owner = anchor.lock();
    const auto session = root;
    if (!owner || !session || !owner->mounted || !owner->active ||
        !owner->interactive || owner->generation != generation ||
        owner->current.lock() != session ||
        (owner->permission && !owner->permission()))
      return;
    started = true;
    auto action = std::move(callback);
    owner->handle = {};
    owner->current.reset();
    if (action)
      action();
  }
};
void choose(const std::shared_ptr<MenuPopupSession> &session, std::size_t index,
            InputContext &context, Key trigger = Key::None) {
  if (!allowed(session) || index >= session->items.size() ||
      !session->items[index].actionable())
    return;
  if (!session->items[index].children.empty()) {
    session->highlighted = index;
    open_child(session);
    return;
  }
  const auto root = root_of(session);
  if (root->completion_queued)
    return;
  const auto generation = session->generation;
  auto completion = std::make_shared<Completion>();
  completion->anchor = session->anchor;
  completion->root = root;
  completion->generation = generation;
  completion->callback = session->items[index].callback;
  auto command = OverlayComponentCommand::close_then_invoke(
      root->handle, session->anchor->node_id, false,
      [completion] { completion->invoke(); });
  release_contact(session);
  cancel_hover(session);
  context.invalidate();
  if (!allowed(session) || session->generation != generation ||
      root->completion_queued)
    return;
  session->commands.push_back(std::move(command));
  root->completion_queued = true;
  if (trigger != Key::None)
    session->anchor->suppress_until_key_up = trigger;
  stop_session(root);
}
class MenuSemanticRow final : public Component {
public:
  MenuSemanticRow(std::shared_ptr<MenuPopupSession> session, std::size_t index)
      : session_(std::move(session)), index_(index) {}
  bool pointer_targetable() const noexcept override { return false; }
  Size measure(const std::vector<ChildMetrics> &) const override { return {}; }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    const auto &item = session_->items[index_];
    if (item.kind == PopupMenuItem::Kind::Separator)
      return info;
    info.role = SemanticRole::MenuItem;
    info.name = item.label;
    info.description = item.shortcut_label;
    info.enabled = item.actionable() && identity_live(session_);
    info.selected = session_->highlighted == index_;
    if (item.checked)
      info.checked = *item.checked ? SemanticCheckedState::Checked
                                   : SemanticCheckedState::Unchecked;
    if (!item.children.empty())
      info.expanded = session_->child && session_->highlighted == index_
                          ? SemanticExpandedState::Expanded
                          : SemanticExpandedState::Collapsed;
    if (info.enabled)
      info.actions = {SemanticAction::Activate};
    return info;
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    if (action != SemanticAction::Activate || !allowed(session_))
      return EventResult::Ignored;
    choose(session_, index_, context);
    return EventResult::Handled;
  }
  void paint(PaintContext &) const override {}

private:
  std::shared_ptr<MenuPopupSession> session_;
  std::size_t index_;
};
} // namespace
void validate_menu_items(const std::vector<PopupMenuItem> &items) {
  validate(items, 1);
}
void retire_menu_anchor(
    const std::shared_ptr<MenuAnchorRuntime> &anchor) noexcept {
  ++anchor->generation;
  anchor->opening = false;
  discard_session(anchor->current.lock());
  anchor->current.reset();
  anchor->handle = {};
  anchor->suppress_until_key_up = Key::None;
  anchor->commands.clear();
}
bool menu_anchor_live(
    const std::shared_ptr<MenuAnchorRuntime> &anchor) noexcept {
  const auto current = anchor->current.lock();
  return !current || current->live || current->completion_queued;
}
void reconcile_menu_anchor(const std::shared_ptr<MenuAnchorRuntime> &anchor) {
  const auto current = anchor->current.lock();
  if (current && !anchor->handle.valid() && !anchor->opening &&
      anchor->commands.empty() && !current->show_pending &&
      !current->completion_queued)
    retire_menu_anchor(anchor);
}
std::optional<OverlayComponentCommand>
take_menu_command(const std::shared_ptr<MenuAnchorRuntime> &anchor) {
  return take(anchor->commands);
}
EventResult open_menu(const std::shared_ptr<MenuAnchorRuntime> &anchor,
                      const PopupMenu::ItemsProvider &provider,
                      const MenuItemStyle &style, InputContext &context,
                      Key opening_key, std::optional<Rect> bounds) {
  reconcile_menu_anchor(anchor);
  if (!provider || !anchor->mounted || !anchor->active ||
      !anchor->interactive || !InputMutationAccess::action_allowed(context) ||
      (anchor->permission && !anchor->permission()))
    return EventResult::Ignored;
  if (anchor->opening || anchor->handle.valid() || !anchor->commands.empty())
    return EventResult::Handled;
  const auto serial = ++anchor->generation;
  anchor->opening = true;
  try {
    auto owned_provider = provider;
    if (!anchor->mounted || !anchor->active || !anchor->interactive ||
        anchor->generation != serial ||
        !InputMutationAccess::action_allowed(context) ||
        (anchor->permission && !anchor->permission())) {
      anchor->opening = false;
      return EventResult::Ignored;
    }
    auto session = std::make_shared<MenuPopupSession>();
    session->show_pending = true;
    session->anchor = anchor;
    session->item_style = style;
    session->generation = serial;
    session->root = session;
    session->items = owned_provider();
    validate_menu_items(session->items);
    session->highlighted =
        first_popup_index(session->items.size(), [&](std::size_t i) {
          return session->items[i].actionable();
        });
    OverlaySpec overlay;
    overlay.anchor = anchor->node_id;
    overlay.mode = OverlayMode::Modal;
    overlay.placement = OverlayPlacement::AnchorBelow;
    overlay.dismiss_on_outside_pointer_down = true;
    overlay.content = panel_spec(session);
    const std::weak_ptr<MenuAnchorRuntime> weak = anchor;
    auto command = OverlayComponentCommand::show(
        std::move(overlay), [weak, session, serial](OverlayHandle handle) {
          const auto owner = weak.lock();
          session->show_pending = false;
          if (!owner || !owner->mounted || !owner->active ||
              !owner->interactive || owner->generation != serial ||
              owner->current.lock() != session ||
              (owner->permission && !owner->permission())) {
            stop_session(session);
            if (owner && handle.valid())
              owner->commands.push_back(
                  OverlayComponentCommand::close_then_invoke(
                      handle, owner->node_id, false, {}));
            return;
          }
          owner->handle = handle;
          session->handle = handle;
          if (!handle.valid())
            retire_menu_anchor(owner);
        });
    context.invalidate();
    if (!anchor->mounted || !anchor->active || !anchor->interactive ||
        anchor->generation != serial ||
        !InputMutationAccess::action_allowed(context) ||
        (anchor->permission && !anchor->permission())) {
      anchor->opening = false;
      return EventResult::Ignored;
    }
    anchor->commands.push_back(std::move(command));
    anchor->current = std::move(session);
    anchor->anchor_bounds = bounds;
    anchor->suppress_until_key_up = opening_key;
    anchor->opening = false;
    return EventResult::Handled;
  } catch (...) {
    anchor->opening = false;
    throw;
  }
}
MenuPopupComponent::MenuPopupComponent(
    std::shared_ptr<MenuPopupSession> session)
    : session_(std::move(session)) {
  if (session_->root.expired()) {
    session_->root = session_;
    session_->generation = session_->anchor->generation;
    session_->anchor->current = session_;
  }
}
bool MenuPopupComponent::focusable() const noexcept { return true; }
bool MenuPopupComponent::clips_children() const noexcept { return true; }
bool MenuPopupComponent::uses_retained_checkpoint() const noexcept {
  return true;
}
bool MenuPopupComponent::dismiss_overlay_on_tab() const noexcept {
  return true;
}
bool MenuPopupComponent::overlay_handles_escape() const noexcept {
  return true;
}
bool MenuPopupComponent::overlay_session_valid() const noexcept {
  const auto root = root_of(session_);
  const auto anchor = session_->anchor;
  if (root->completion_queued && anchor && anchor->mounted && anchor->active &&
      anchor->interactive && anchor->generation == root->generation &&
      anchor->current.lock() == root)
    return true;
  return identity_live(session_) && (!session_->child || session_->child->live);
}
std::optional<Rect> MenuPopupComponent::overlay_anchor_bounds() const noexcept {
  return session_->submenu_anchor;
}
ResolvedMenuItemStyle MenuPopupComponent::resolved(std::size_t index) const {
  VisualState visual;
  visual.enabled =
      index < session_->items.size() && session_->items[index].actionable();
  visual.selected = session_->highlighted == index;
  visual.hovered = visual.selected;
  visual.pressed = visual.selected && session_->pointer_armed;
  visual.focused = session_->focused;
  return resolve_menu_item_style(default_menu_item_style(current_theme()),
                                 session_->item_style, visual);
}
Size MenuPopupComponent::measure(const std::vector<ChildMetrics> &) const {
  auto base = resolve_menu_item_style(default_menu_item_style(current_theme()),
                                      session_->item_style, {});
  float label_width{}, shortcut_width{}, height{};
  bool checked{}, children{};
  for (std::size_t i = 0; i < session_->items.size(); ++i) {
    const auto &item = session_->items[i];
    const auto style = resolved(i);
    if (item.kind == PopupMenuItem::Kind::Separator) {
      height += std::max(0.f, base.separator_height);
      continue;
    }
    label_width = std::max(
        label_width, TextService::measure(item.label, item_text(style)).width);
    shortcut_width = std::max(
        shortcut_width,
        TextService::measure(item.shortcut_label, item_text(style)).width);
    checked = checked || item.checked.has_value();
    children = children || !item.children.empty();
    height += std::max(0.f, style.row_height);
  }
  const float columns =
      (checked ? base.text_size + base.horizontal_padding : 0.f) +
      (children ? base.text_size + base.horizontal_padding : 0.f) +
      (shortcut_width > 0 ? shortcut_width + base.horizontal_padding : 0.f);
  return {std::max(current_theme().controls.minimum_width,
                   label_width + columns + 2 * base.horizontal_padding),
          std::max(height, std::max(0.f, base.row_height))};
}
ChildMetrics MenuPopupComponent::measure_constrained(
    const Constraints &constraints,
    const std::vector<ChildMetrics> &children) const {
  const auto preferred = constraints.constrain(measure(children));
  return {constraints.constrain({0, 0}), preferred};
}
void MenuPopupComponent::layout_children(
    Rect bounds, const std::vector<ChildMetrics> &,
    std::vector<ChildPlacement> &children) const {
  auto base = resolve_menu_item_style(default_menu_item_style(current_theme()),
                                      session_->item_style, {});
  auto &session = *session_;
  session.bounds = bounds;
  session.content_height = 0;
  for (std::size_t i = 0; i < session.items.size(); ++i)
    session.content_height +=
        session.items[i].kind == PopupMenuItem::Kind::Separator
            ? std::max(0.f, base.separator_height)
            : std::max(0.f, resolved(i).row_height);
  session.scroll = std::clamp(session.scroll, 0.f,
                              std::max(0.f, session.content_height - bounds.h));
  session.rows.resize(session.items.size());
  float y = bounds.y - session.scroll;
  for (std::size_t i = 0; i < session.items.size(); ++i) {
    const float height = session.items[i].kind == PopupMenuItem::Kind::Separator
                             ? std::max(0.f, base.separator_height)
                             : std::max(0.f, resolved(i).row_height);
    session.rows[i] = {bounds.x, y, bounds.w, height};
    if (i < children.size())
      children[i].bounds = session.rows[i];
    y += height;
  }
  if (session.child && session.highlighted < session.rows.size())
    session.submenu_anchor = session.rows[session.highlighted];
}
void MenuPopupComponent::mount(MountContext &context) {
  session_->mounted = true;
  session_->node_id = context.node_id();
  session_->invalidate = context.invalidator();
  if (session_->structure)
    session_->structure();
}
void MenuPopupComponent::unmount(LifecycleContext &) {
  if (!session_->parent.expired() && !session_->level_close_requested &&
      !root_of(session_)->completion_queued)
    stop_session(root_of(session_));
  session_->mounted = false;
  session_->invalidate = {};
  session_->structure = {};
  discard_session(session_);
}
void MenuPopupComponent::activate(LifecycleContext &context) {
  session_->dispatcher = context.dispatcher();
}
void MenuPopupComponent::deactivate(LifecycleContext &) {
  const auto root = root_of(session_);
  // A deliberate one-level close must leave the parent session usable.
  // Availability changes and owner teardown still retire the entire menu.
  if (session_->level_close_requested && session_ != root)
    discard_session(session_);
  else
    discard_session(root);
}
void MenuPopupComponent::retained_checkpoint() {
  if (session_->child && !session_->child->show_pending &&
      !session_->child->handle.valid() && session_->commands.empty()) {
    stop_session(session_->child);
    session_->child.reset();
  }
  if (session_->hover_due) {
    session_->hover_due = false;
    open_child(session_);
  }
}
void MenuPopupComponent::focus_changed(bool focused, FocusContext &context) {
  session_->focused = focused;
  if (!focused)
    cancel_hover(session_);
  context.invalidate();
}
void MenuPopupComponent::ensure_visible(std::size_t index,
                                        InputContext &context) {
  if (index >= session_->rows.size())
    return;
  const auto row = session_->rows[index], bounds = session_->bounds;
  auto scroll = session_->scroll;
  if (row.y < bounds.y)
    scroll -= bounds.y - row.y;
  if (row.y + row.h > bounds.y + bounds.h)
    scroll += row.y + row.h - bounds.y - bounds.h;
  scroll = std::clamp(scroll, 0.f,
                      std::max(0.f, session_->content_height - bounds.h));
  if (scroll != session_->scroll) {
    session_->scroll = scroll;
    context.invalidate_layout();
  }
}
void MenuPopupComponent::highlight(std::size_t index, InputContext &context) {
  if (session_->highlighted == index)
    return;
  cancel_hover(session_);
  if (session_->child && session_->child->live)
    close_level(session_->child);
  session_->highlighted = index;
  ensure_visible(index, context);
  context.invalidate();
}
EventResult MenuPopupComponent::input(const InputEvent &event,
                                      InputContext &context) {
  const auto session = session_;
  const auto anchor = session->anchor;
  if (anchor->suppress_until_key_up != Key::None &&
      event.key == anchor->suppress_until_key_up) {
    if (event.type == InputType::KeyUp) {
      anchor->suppress_until_key_up = Key::None;
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown)
      return EventResult::Handled;
  }
  if (!allowed(session))
    return EventResult::Ignored;
  if (event.type == InputType::KeyDown) {
    switch (event.key) {
    case Key::Up:
    case Key::Down:
      highlight(step_popup_index(session->items.size(), session->highlighted,
                                 event.key == Key::Down ? 1 : -1,
                                 [&](std::size_t i) {
                                   return session->items[i].actionable();
                                 }),
                context);
      return EventResult::Handled;
    case Key::Home:
    case Key::End:
      highlight(event.key == Key::Home
                    ? first_popup_index(session->items.size(),
                                        [&](std::size_t i) {
                                          return session->items[i].actionable();
                                        })
                    : last_popup_index(session->items.size(),
                                       [&](std::size_t i) {
                                         return session->items[i].actionable();
                                       }),
                context);
      return EventResult::Handled;
    case Key::Enter:
    case Key::Space:
      choose(session, session->highlighted, context, event.key);
      return EventResult::Handled;
    case Key::Right:
      open_child(session);
      return EventResult::Handled;
    case Key::Left:
      if (!session->parent.expired())
        close_level(session, &context);
      return EventResult::Handled;
    case Key::Escape:
      close_level(session, &context);
      return EventResult::Handled;
    default:
      break;
    }
  }
  const auto at = [&] {
    if (!context.bounds().contains(event.position))
      return kNoPopupIndex;
    for (std::size_t i = 0; i < session->rows.size(); ++i)
      if (session->rows[i].contains(event.position))
        return i;
    return kNoPopupIndex;
  };
  if (event.type == InputType::PointerWheel) {
    cancel_hover(session);
    session->scroll =
        std::clamp(session->scroll - event.delta.y, 0.f,
                   std::max(0.f, session->content_height - context.bounds().h));
    context.invalidate_layout();
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerMove ||
      event.type == InputType::PointerDown) {
    const auto index = at();
    highlight(index < session->items.size() &&
                      session->items[index].actionable()
                  ? index
                  : kNoPopupIndex,
              context);
    if (!allowed(session))
      return EventResult::Handled;
    if (event.type == InputType::PointerDown &&
        session->highlighted != kNoPopupIndex) {
      session->release = context.pointer_releaser();
      session->pointer_armed = true;
      context.capture_pointer();
      context.invalidate();
    } else if (event.type == InputType::PointerMove &&
               !session->pointer_armed &&
               session->highlighted < session->items.size() &&
               !session->items[session->highlighted].children.empty() &&
               (!session->child || !session->child->live) &&
               session->dispatcher.valid() && !session->hover_timer.valid()) {
      const auto index_copy = session->highlighted;
      const auto generation = session->generation;
      const std::weak_ptr<MenuPopupSession> weak = session;
      session->hover_index = index_copy;
      session->hover_timer = session->dispatcher.schedule_after(
          std::chrono::milliseconds{200}, [weak, index_copy, generation] {
            const auto current = weak.lock();
            if (!current || !identity_live(current) ||
                current->generation != generation ||
                current->highlighted != index_copy)
              return;
            current->hover_timer = {};
            current->hover_due = true;
            if (current->invalidate)
              current->invalidate();
          });
    }
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerUp) {
    const auto index = at();
    const bool activate =
        session->pointer_armed && index == session->highlighted;
    release_contact(session);
    if (activate)
      choose(session, index, context);
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerCancel) {
    release_contact(session);
    cancel_hover(session);
    context.invalidate();
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerLeave) {
    cancel_hover(session);
    return EventResult::Handled;
  }
  if (event.type == InputType::Tick)
    return EventResult::Handled;
  return EventResult::Ignored;
}
std::optional<OverlayComponentCommand>
MenuPopupComponent::take_overlay_command() {
  return take(session_->commands);
}
bool MenuPopupComponent::has_pending_overlay_command() const noexcept {
  return !session_->commands.empty();
}
std::vector<std::string> MenuPopupComponent::desired_keys() const {
  std::vector<std::string> result;
  for (std::size_t i = 0; i < session_->items.size(); ++i)
    result.push_back(session_->items[i].key.empty()
                         ? "index:" + std::to_string(i)
                         : "key:" + session_->items[i].key);
  return result;
}
std::vector<DynamicChildSpec> MenuPopupComponent::desired_children() const {
  auto keys = desired_keys();
  std::vector<DynamicChildSpec> result;
  for (std::size_t i = 0; i < keys.size(); ++i)
    result.push_back({keys[i], Spec{[session = session_, i] {
                                      return std::make_unique<MenuSemanticRow>(
                                          session, i);
                                    },
                                    {}}});
  return result;
}
void MenuPopupComponent::set_structure_invalidator(
    std::function<void()> callback) {
  session_->structure = std::move(callback);
}
std::vector<Spec> MenuPopupComponent::initial_children() const {
  std::vector<Spec> result;
  for (auto &item : desired_children()) {
    item.spec.retained_key = item.key;
    result.push_back(std::move(item.spec));
  }
  return result;
}
SemanticInfo MenuPopupComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::PopupMenu;
  info.name = "Menu";
  info.focusable = true;
  info.focused = session_->focused;
  info.enabled = identity_live(session_);
  return info;
}
void MenuPopupComponent::paint(PaintContext &context) const {
  const auto bounds = context.bounds();
  auto &painter = context.painter();
  auto clip = painter.scoped_clip(bounds);
  painter.fill_rounded_rect(bounds, current_theme().radii.medium,
                            current_theme().palette.surface);
  painter.stroke_rounded_rect(bounds, current_theme().radii.medium,
                              current_theme().controls.border_width,
                              current_theme().palette.border);
  bool checked{}, children{};
  float shortcut_width{};
  for (std::size_t i = 0; i < session_->items.size(); ++i) {
    const auto &item = session_->items[i];
    checked = checked || item.checked.has_value();
    children = children || !item.children.empty();
    shortcut_width =
        std::max(shortcut_width, TextService::measure(item.shortcut_label,
                                                      item_text(resolved(i)))
                                     .width);
  }
  for (std::size_t i = 0;
       i < session_->items.size() && i < session_->rows.size(); ++i) {
    const auto row = session_->rows[i];
    if (row.y + row.h <= bounds.y || row.y >= bounds.y + bounds.h)
      continue;
    const auto &item = session_->items[i];
    const auto style = resolved(i);
    if (item.kind == PopupMenuItem::Kind::Separator) {
      painter.line({row.x + style.separator_inset, row.y + row.h * .5f},
                   {row.x + row.w - style.separator_inset, row.y + row.h * .5f},
                   style.separator_width, style.separator);
      continue;
    }
    painter.fill_rounded_rect(row, style.corner_radius, style.fill);
    const float check_width =
        checked ? style.text_size + style.horizontal_padding : 0.f;
    const float chevron_width =
        children ? style.text_size + style.horizontal_padding : 0.f;
    const float shortcuts =
        shortcut_width > 0 ? shortcut_width + style.horizontal_padding : 0.f;
    const float center = row.y + row.h * .5f;
    if (item.checked && *item.checked)
      painter.text({row.x + style.horizontal_padding, center}, "✓",
                   item_text(style));
    {
      auto text_clip = painter.scoped_clip(
          {row.x + check_width, row.y,
           std::max(0.f, row.w - check_width - chevron_width - shortcuts),
           row.h});
      painter.text({row.x + style.horizontal_padding + check_width, center},
                   item.label, item_text(style));
    }
    if (!item.shortcut_label.empty())
      painter.text(
          {row.x + row.w - style.horizontal_padding - chevron_width, center},
          item.shortcut_label, item_text(style, TextAlign::Right));
    if (!item.children.empty())
      painter.text({row.x + row.w - style.horizontal_padding, center}, "›",
                   item_text(style, TextAlign::Right));
  }
}
PopupMenuComponent::PopupMenuComponent(
    std::string label, PopupMenu::ItemsProvider provider, ComboBoxStyle style,
    MenuItemStyle item_style, std::shared_ptr<MenuAnchorRuntime> runtime)
    : label_(std::move(label)), provider_(std::move(provider)),
      style_(std::move(style)), item_style_(std::move(item_style)),
      runtime_(std::move(runtime)) {}
bool PopupMenuComponent::focusable() const noexcept { return true; }
bool PopupMenuComponent::roving_focus_target() const noexcept { return true; }
bool PopupMenuComponent::cancel_capture_on_read_only() const noexcept {
  return false;
}
bool PopupMenuComponent::uses_retained_checkpoint() const noexcept {
  return true;
}
bool PopupMenuComponent::dismiss_overlay_on_tab() const noexcept {
  return true;
}
bool PopupMenuComponent::overlay_handles_escape() const noexcept {
  return true;
}
bool PopupMenuComponent::overlay_session_valid() const noexcept {
  return menu_anchor_live(runtime_);
}
ResolvedComboBoxStyle PopupMenuComponent::resolved() const {
  VisualState visual;
  visual.enabled = effective_enabled();
  visual.read_only = effective_read_only();
  visual.focused = focused_;
  visual.hovered = press_.hovered();
  visual.pressed = press_.pressed();
  return resolve_combo_box_style(default_combo_box_style(current_theme()),
                                 style_, visual);
}
Size PopupMenuComponent::measure(const std::vector<ChildMetrics> &) const {
  const auto style = resolved();
  return {std::max(style.minimum_width,
                   TextService::measure(label_, anchor_text(style)).width +
                       2 * style.horizontal_padding),
          style.control_height};
}
void PopupMenuComponent::mount(MountContext &context) {
  runtime_->node_id = context.node_id();
  runtime_->mounted = true;
  runtime_->permission = InputMutationAccess::action_guard(context);
  runtime_->invalidate = context.invalidator();
}
void PopupMenuComponent::unmount(LifecycleContext &context) {
  runtime_->mounted = false;
  retire_menu_anchor(runtime_);
  runtime_->permission = {};
  runtime_->invalidate = {};
  press_.deactivate(context, false);
}
void PopupMenuComponent::activate(LifecycleContext &) {
  runtime_->active = true;
}
void PopupMenuComponent::deactivate(LifecycleContext &context) {
  runtime_->active = false;
  retire_menu_anchor(runtime_);
  press_.deactivate(context, false);
  focused_ = false;
}
void PopupMenuComponent::retained_checkpoint() {
  reconcile_menu_anchor(runtime_);
}
void PopupMenuComponent::focus_changed(bool focused, FocusContext &context) {
  focused_ = focused;
  press_.focus_changed(focused, context, false);
  context.invalidate();
}
EventResult PopupMenuComponent::input(const InputEvent &event,
                                      InputContext &context) {
  const auto runtime = runtime_;
  if (runtime->suppress_until_key_up != Key::None &&
      event.key == runtime->suppress_until_key_up) {
    if (event.type == InputType::KeyUp) {
      runtime->suppress_until_key_up = Key::None;
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown)
      return EventResult::Handled;
  }
  if (!runtime->interactive || !InputMutationAccess::action_allowed(context))
    return EventResult::Ignored;
  if (event.type == InputType::KeyDown && event.key == Key::Down)
    return open_menu(runtime, provider_, item_style_, context, Key::Down);
  PressActivationResult result;
  try {
    result = press_.input(event, context, true);
  } catch (...) {
    press_ = {};
    throw;
  }
  if (!result.activate)
    return result.result;
  return open_menu(runtime, provider_, item_style_, context,
                   event.type == InputType::KeyDown ? event.key : Key::None);
}
EventResult PopupMenuComponent::semantic_action(SemanticAction action,
                                                InputContext &context) {
  if (action == SemanticAction::Activate)
    return open_menu(runtime_, provider_, item_style_, context);
  return EventResult::Ignored;
}
std::optional<OverlayComponentCommand>
PopupMenuComponent::take_overlay_command() {
  return take_menu_command(runtime_);
}
bool PopupMenuComponent::has_pending_overlay_command() const noexcept {
  return !runtime_->commands.empty();
}
SemanticInfo PopupMenuComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::Button;
  info.name = label_;
  info.focusable = true;
  info.focused = focused_;
  info.enabled = effective_enabled();
  const auto current = runtime_->current.lock();
  info.expanded = current && current->live ? SemanticExpandedState::Expanded
                                           : SemanticExpandedState::Collapsed;
  if (info.enabled && provider_)
    info.actions = {SemanticAction::Activate, SemanticAction::Focus};
  return info;
}
void PopupMenuComponent::paint(PaintContext &context) const {
  const auto style = resolved();
  const auto bounds = context.bounds();
  auto &painter = context.painter();
  auto clip = painter.scoped_clip(bounds);
  painter.fill_rounded_rect(bounds, style.corner_radius, style.fill);
  painter.stroke_rounded_rect(bounds, style.corner_radius, style.border_width,
                              style.border);
  painter.text({bounds.x + bounds.w * .5f, bounds.y + bounds.h * .5f}, label_,
               anchor_text(style));
}
void PopupMenuComponent::effective_availability_changed(
    const ComponentAvailability &,
    const ComponentAvailability &after) noexcept {
  runtime_->interactive = after.interactive();
  if (!after.interactive()) {
    retire_menu_anchor(runtime_);
    press_ = {};
  }
}
} // namespace detail
PopupMenu::PopupMenu(std::string label, std::vector<PopupMenuItem> items)
    : PopupMenu(std::move(label),
                [items = std::move(items)] { return items; }) {}
PopupMenu::PopupMenu(std::string label, ItemsProvider provider)
    : label_(std::move(label)), provider_(std::move(provider)) {}
PopupMenu &&PopupMenu::style(ComboBoxStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
PopupMenu &&PopupMenu::item_style(MenuItemStyle value) && {
  item_style_ = std::move(value);
  return std::move(*this);
}
Spec PopupMenu::spec() && {
  return Spec{[label = label_, provider = provider_, style = style_,
               item = item_style_] {
                return std::make_unique<detail::PopupMenuComponent>(
                    label, provider, style, item,
                    std::make_shared<detail::MenuAnchorRuntime>());
              },
              {}};
}
} // namespace ui
