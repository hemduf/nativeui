#include <climits>
#include "detail/widget_menu_popup.hpp"
#include <nativeui/history_button.hpp>
#include <unordered_set>
namespace ui {
namespace {
using Entries = std::vector<HistoryEntry>;
TextStyle button_text(const ResolvedButtonStyle &style) {
  TextStyle text;
  text.size = style.text_size;
  text.color = style.text;
  text.align = TextAlign::Center;
  text.weight = style.text_weight;
  text.slant = style.text_slant;
  text.family = style.font_family;
  text.fallback_families = style.fallback_families;
  return text;
}
bool valid_entries(const Entries &entries) {
  std::unordered_set<std::string> keys;
  for (const auto &entry : entries)
    if (entry.key.empty() || !keys.insert(entry.key).second)
      return false;
  return true;
}
struct HistoryRuntime : std::enable_shared_from_this<HistoryRuntime> {
  HistoryRuntime(HistoryDirection direction, Binding<bool> can,
                 std::optional<Binding<Entries>> entries,
                 std::function<void(int)> navigate)
      : can(std::move(can)), entries(std::move(entries)),
        navigate(std::move(navigate)), direction(direction),
        seen_can(this->can.get()), seen_valid(this->can.valid()),
        menu_anchor(std::make_shared<detail::MenuAnchorRuntime>()) {
    if (this->entries) {
      auto initial = this->entries->snapshot();
      if (!valid_entries(initial))
        throw std::invalid_argument(
            "History entries require unique nonempty keys");
      accepted = std::make_shared<const Entries>(std::move(initial));
      entries_revision = this->entries->revision();
    } else {
      accepted = std::make_shared<const Entries>();
    }
  }
  Binding<bool> can;
  std::optional<Binding<Entries>> entries;
  std::function<void(int)> navigate;
  HistoryDirection direction;
  bool seen_can{}, seen_valid{}, mounted{}, mutable_value{true}, live{},
      enter_down{};
  std::shared_ptr<const Entries> accepted;
  std::shared_ptr<detail::MenuAnchorRuntime> menu_anchor;
  std::shared_ptr<detail::MenuPopupSession> menu;
  std::optional<detail::OverlayComponentCommand> pending;
  std::function<bool()> guard;
  std::function<void()> invalidate, availability, release;
  std::uint64_t generation{}, armed_generation{}, menu_epoch{},
      entries_revision{};
  std::string diagnostic;
  detail::PressActivationState press;
  bool allowed() const {
    return mounted && mutable_value && can.valid() && can.get() && navigate &&
           (!guard || guard());
  }
  void stop() noexcept {
    press = {};
    enter_down = false;
    auto old = std::move(release);
    if (old)
      try {
        old();
      } catch (...) {
      }
  }
  void close() {
    if (!live && !menu_anchor->handle.valid() && !pending)
      return;
    ++menu_epoch;
    live = false;
    const auto old = menu_anchor->handle;
    menu_anchor->handle = {};
    pending =
        old.valid()
            ? std::optional{detail::OverlayComponentCommand::close_then_invoke(
                  old, menu_anchor->node_id, true, {})}
            : std::nullopt;
    if (invalidate)
      invalidate();
  }
  void sync() {
    if (!mounted)
      return;
    const bool current = can.get();
    const bool valid = can.valid();
    if (current != seen_can || valid != seen_valid) {
      seen_can = current;
      seen_valid = valid;
      ++generation;
      stop();
      if (!can.valid() || !current)
        close();
      if (availability)
        availability();
      if (invalidate)
        invalidate();
    }
    if (entries && entries->revision() != entries_revision) {
      const auto revision = entries->revision();
      auto next = entries->snapshot();
      if (!mounted || entries->revision() != revision)
        return;
      const bool valid = valid_entries(next);
      if (valid) {
        auto owned = std::make_shared<const Entries>(std::move(next));
        accepted = std::move(owned);
        diagnostic.clear();
      } else {
        diagnostic = "Invalid history keys";
      }
      entries_revision = revision;
      if (invalidate)
        invalidate();
    }
    if (entries && !entries->valid())
      close();
    if (live && !menu_anchor->handle.valid() &&
        (!menu || !menu->completion_queued) && !pending) {
      live = false;
      ++menu_epoch;
    }
  }
  void invoke(std::optional<std::string> key, std::uint64_t epoch,
              const std::function<bool()> &permission = {}) {
    const auto keep = shared_from_this();
    sync();
    if (key) {
      if (!live || menu_epoch != epoch)
        return;
      live = false;
      epoch = ++menu_epoch;
      menu_anchor->handle = {};
    }
    if (!allowed() || (permission && !permission()))
      return;
    const auto can_revision = can.revision();
    const auto serial = generation;
    std::uint64_t revision{};
    int count = 1;
    if (key) {
      if (!entries || !entries->valid())
        return;
      revision = entries->revision();
      const auto current = entries->snapshot();
      if (entries->revision() != revision || !valid_entries(current))
        return;
      const auto found =
          std::find_if(current.begin(), current.end(),
                       [&](const auto &entry) { return entry.key == *key; });
      if (found == current.end()) {
        return;
      }
      const auto index = static_cast<std::size_t>(found - current.begin());
      if (index >= static_cast<std::size_t>(INT_MAX))
        return;
      count = static_cast<int>(index + 1);
    }
    auto callback = navigate;
    const int signed_count =
        direction == HistoryDirection::Backward ? -count : count;
    if (invalidate)
      invalidate();
    if (!allowed() || generation != serial || can.revision() != can_revision ||
        (key && (menu_epoch != epoch || !entries || !entries->valid() ||
                 entries->revision() != revision)) ||
        (permission && !permission()))
      return;
    if (callback)
      callback(signed_count);
  }
};
class HistoryFrame final : public Component,
                           public detail::ThemeBinding,
                           public detail::OverlayCommandSource,
                           public detail::OverlayAnchorPolicy {
public:
  HistoryFrame(HistoryDirection direction, Binding<bool> can,
               std::optional<Binding<Entries>> entries,
               std::function<void(int)> navigate, std::size_t maximum,
               std::string label, HistoryButtonStyle style)
      : state_(std::make_shared<HistoryRuntime>(direction, std::move(can),
                                                std::move(entries),
                                                std::move(navigate))),
        maximum_(maximum), label_(std::move(label)), style_(std::move(style)) {}
  bool focusable() const noexcept override { return true; }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool clips_children() const noexcept override { return true; }
  bool cancel_capture_on_read_only() const noexcept override { return true; }
  bool dismiss_overlay_on_tab() const noexcept override { return true; }
  bool dismiss_overlay_when_read_only() const noexcept override { return true; }
  bool overlay_session_valid() const noexcept override { return state_->live; }
  ComponentAvailability local_availability() const noexcept override {
    return {VisibilityMode::Visible,
            state_->seen_can && state_->can.valid() && bool(state_->navigate),
            false};
  }
  Size measure(const std::vector<ChildMetrics> &) const override {
    const auto style = resolved();
    return {std::max(style.minimum_width,
                     TextService::measure(glyph(), button_text(style)).width +
                         2 * style.horizontal_padding),
            style.control_height};
  }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->menu_anchor->mounted = true;
    state_->menu_anchor->node_id = context.node_id();
    state_->guard = detail::InputMutationAccess::guard(context);
    state_->invalidate = context.invalidator();
    state_->availability = context.availability_invalidator();
    const std::weak_ptr<HistoryRuntime> weak = state_;
    can_subscription_ = state_->can.observe([weak](bool) {
      if (const auto state = weak.lock())
        state->sync();
    });
    if (state_->entries)
      entries_subscription_ = state_->entries->observe([weak](const auto &) {
        if (const auto state = weak.lock())
          state->sync();
      });
  }
  void unmount(LifecycleContext &) override {
    state_->mounted = false;
    state_->menu_anchor->mounted = false;
    ++state_->generation;
    ++state_->menu_epoch;
    state_->live = false;
    state_->stop();
    state_->pending.reset();
    state_->menu_anchor->handle = {};
    state_->invalidate = {};
    state_->availability = {};
    state_->guard = {};
    can_subscription_.reset();
    entries_subscription_.reset();
  }
  void deactivate(LifecycleContext &) override {
    state_->stop();
    state_->close();
    focused_ = false;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    focused_ = focused;
    if (!focused)
      state_->stop();
    context.invalidate();
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto state = state_;
    state->sync();
    if (state->menu_anchor->suppress_until_key_up != Key::None &&
        event.key == state->menu_anchor->suppress_until_key_up) {
      if (event.type == InputType::KeyUp)
        state->menu_anchor->suppress_until_key_up = Key::None;
      return EventResult::Handled;
    }
    if (!state->allowed()) {
      state->stop();
      return EventResult::Handled;
    }
    if (event.type == InputType::ContextMenu)
      return open(context);
    if (event.type == InputType::KeyDown && event.key == Key::Escape) {
      state->stop();
      state->close();
      context.invalidate();
      return EventResult::Handled;
    }
    bool activate{};
    if (event.key == Key::Enter &&
        (event.type == InputType::KeyDown || event.type == InputType::KeyUp)) {
      if (event.type == InputType::KeyDown) {
        if (!state->enter_down)
          state->armed_generation = state->generation;
        state->enter_down = true;
      } else {
        activate = std::exchange(state->enter_down, false);
      }
      context.invalidate();
    } else {
      if (event.type == InputType::PointerDown ||
          (event.type == InputType::KeyDown && event.key == Key::Space)) {
        if (!state->press.pressed())
          state->armed_generation = state->generation;
        if (event.type == InputType::PointerDown)
          state->release = context.pointer_releaser();
      }
      detail::PressActivationResult result;
      try {
        result = state->press.input(event, context, false);
      } catch (...) {
        state->stop();
        throw;
      }
      if (event.type == InputType::PointerUp ||
          event.type == InputType::PointerCancel)
        state->release = {};
      activate = result.activate;
      if (!activate)
        return result.result;
    }
    auto permission = detail::InputMutationAccess::guard(context);
    if (activate && state->generation == state->armed_generation)
      state->invoke({}, 0, permission);
    return EventResult::Handled;
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    if (action == SemanticAction::Expand)
      return open(context);
    if (action == SemanticAction::Activate) {
      auto permission = detail::InputMutationAccess::guard(context);
      state_->stop();
      state_->invoke({}, 0, permission);
      return EventResult::Handled;
    }
    return EventResult::Ignored;
  }
  std::optional<detail::OverlayComponentCommand>
  take_overlay_command() override {
    return std::exchange(state_->pending, {});
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Button;
    info.name = label_;
    info.focusable = true;
    info.focused = focused_;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only();
    info.description = state_->diagnostic;
    if (info.enabled)
      info.actions = {SemanticAction::Focus};
    if (info.enabled && !info.read_only) {
      info.actions.push_back(SemanticAction::Activate);
      if (maximum_ && state_->entries && !state_->accepted->empty())
        info.actions.push_back(SemanticAction::Expand);
    }
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto style = resolved();
    const auto bounds = context.bounds();
    auto &p = context.painter();
    auto clip = p.scoped_clip(bounds);
    p.fill_rounded_rect(bounds, style.corner_radius, style.fill);
    p.stroke_rounded_rect(bounds, style.corner_radius, style.border_width,
                          style.border);
    p.text({bounds.x + bounds.w * .5f, bounds.y + bounds.h * .5f}, glyph(),
           button_text(style));
  }

private:
  EventResult open(InputContext &context) {
    const auto state = state_;
    state->sync();
    if (!state->allowed() || !maximum_ || !state->entries ||
        !state->entries->valid() || !state->diagnostic.empty() ||
        state->accepted->empty() || state->live || state->pending)
      return EventResult::Handled;
    const auto epoch = ++state->menu_epoch;
    const auto revision = state->entries->revision();
    const auto source = state->accepted;
    auto menu = std::make_shared<detail::MenuPopupSession>();
    menu->anchor = state->menu_anchor;
    menu->item_style = style_.item;
    menu->highlighted = 0;
    const std::weak_ptr<HistoryRuntime> weak = state;
    const auto count = std::min(maximum_, source->size());
    for (std::size_t index = 0; index < count; ++index) {
      const auto &entry = (*source)[index];
      menu->items.push_back(
          PopupMenuItem::action(entry.title.empty() ? entry.key : entry.title,
                                [weak, epoch, key = entry.key] {
                                  if (const auto current = weak.lock())
                                    current->invoke(key, epoch);
                                }));
    }
    OverlaySpec overlay;
    overlay.anchor = state->menu_anchor->node_id;
    overlay.mode = OverlayMode::Modal;
    overlay.placement = OverlayPlacement::AnchorBelow;
    overlay.dismiss_on_escape = true;
    overlay.dismiss_on_outside_pointer_down = true;
    overlay.content = Spec{
        [menu] { return std::make_unique<detail::MenuPopupComponent>(menu); },
        {}};
    context.invalidate();
    if (!state->allowed() || state->menu_epoch != epoch ||
        state->entries->revision() != revision ||
        !detail::InputMutationAccess::allowed(context))
      return EventResult::Handled;
    state->live = true;
    state->menu = menu;
    state->pending = detail::OverlayComponentCommand::show(
        std::move(overlay), [weak, menu, epoch](OverlayHandle handle) {
          const auto current = weak.lock();
          if (!current)
            return;
          if (!current->allowed() || current->menu_epoch != epoch ||
              !current->live) {
            if (handle.valid())
              current->pending =
                  detail::OverlayComponentCommand::close_then_invoke(
                      handle, current->menu_anchor->node_id, true, {});
            return;
          }
          current->menu_anchor->handle = handle;
          menu->handle = handle;
          if (!handle.valid())
            current->live = false;
        });
    return EventResult::Handled;
  }
  std::string glyph() const {
    const std::string arrow =
        state_->direction == HistoryDirection::Backward ? "‹" : "›";
    return style_.show_label ? arrow + " " + label_ : arrow;
  }
  ResolvedButtonStyle resolved() const {
    VisualState visual;
    visual.enabled = effective_enabled();
    visual.read_only = effective_read_only();
    visual.focused = focused_;
    visual.hovered = state_->press.hovered();
    visual.pressed = state_->press.pressed() || state_->enter_down;
    return resolve_button_style(default_button_style(current_theme()),
                                style_.button, visual);
  }
  void retained_checkpoint() override { state_->sync(); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    if (!state_->mutable_value) {
      state_->stop();
      state_->live = false;
    }
  }
  std::shared_ptr<HistoryRuntime> state_;
  Binding<bool>::Subscription can_subscription_;
  std::optional<Binding<Entries>::Subscription> entries_subscription_;
  std::size_t maximum_{};
  std::string label_;
  HistoryButtonStyle style_;
  bool focused_{};
};
} // namespace
HistoryButton::HistoryButton(HistoryDirection direction, Binding<bool> can,
                             std::function<void(int)> navigate)
    : direction_(direction), can_(std::move(can)),
      navigate_(std::move(navigate)),
      label_(direction == HistoryDirection::Backward ? "Back" : "Forward") {}
HistoryButton::HistoryButton(HistoryDirection direction, State<bool> &can,
                             std::function<void(int)> navigate)
    : HistoryButton(direction, can.binding(), std::move(navigate)) {}
HistoryButton &&
HistoryButton::entries(Binding<std::vector<HistoryEntry>> value) && {
  entries_ = std::move(value);
  return std::move(*this);
}
HistoryButton &&
HistoryButton::entries(State<std::vector<HistoryEntry>> &value) && {
  entries_ = value.binding();
  return std::move(*this);
}
HistoryButton &&HistoryButton::maximum_menu_entries(std::size_t value) && {
  maximum_ = value;
  return std::move(*this);
}
HistoryButton &&HistoryButton::label(std::string value) && {
  label_ = std::move(value);
  return std::move(*this);
}
HistoryButton &&HistoryButton::style(HistoryButtonStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec HistoryButton::spec() && {
  if (entries_ && !valid_entries(entries_->snapshot()))
    throw std::invalid_argument("History entries require unique nonempty keys");
  return Spec{[direction = direction_, can = can_, entries = entries_,
               navigate = navigate_, maximum = maximum_, label = label_,
               style = style_] {
                return std::make_unique<HistoryFrame>(
                    direction, can, entries, navigate, maximum, label, style);
              },
              {}};
}
} // namespace ui
