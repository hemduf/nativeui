#include "detail/widget_menu_popup.hpp"
#include <nativeui/context_menu.hpp>
namespace ui {
namespace {
class ContextMenuComponent final : public Component,
                                   public detail::OverlayCommandSource,
                                   public detail::OverlayAnchorPolicy {
public:
  ContextMenuComponent(PopupMenu::ItemsProvider provider, MenuItemStyle style,
                       bool has_child)
      : provider_(std::move(provider)), style_(std::move(style)),
        runtime_(std::make_shared<detail::MenuAnchorRuntime>()),
        has_child_(has_child) {}
  bool pointer_targetable() const noexcept override { return has_child_; }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool dismiss_overlay_on_tab() const noexcept override { return true; }
  bool overlay_handles_escape() const noexcept override { return true; }
  bool overlay_session_valid() const noexcept override {
    return detail::menu_anchor_live(runtime_);
  }
  std::optional<Rect> overlay_anchor_bounds() const noexcept override {
    return runtime_->anchor_bounds;
  }
  Size measure(const std::vector<ChildMetrics> &children) const override {
    return children.empty() ? Size{} : children[0].preferred;
  }
  Size minimum_size(const std::vector<ChildMetrics> &children) const override {
    return children.empty() ? Size{} : children[0].minimum;
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    if (!children.empty())
      children[0].bounds = bounds;
    if (runtime_->anchor_offset)
      runtime_->anchor_bounds =
          Rect{bounds.x + runtime_->anchor_offset->x,
               bounds.y + runtime_->anchor_offset->y, 0, 0};
  }
  void mount(MountContext &context) override {
    runtime_->node_id = context.node_id();
    runtime_->mounted = true;
    runtime_->permission = detail::InputMutationAccess::action_guard(context);
    runtime_->invalidate = context.invalidator();
  }
  void unmount(LifecycleContext &) override {
    runtime_->mounted = false;
    detail::retire_menu_anchor(runtime_);
    runtime_->permission = {};
    runtime_->invalidate = {};
  }
  void activate(LifecycleContext &) override { runtime_->active = true; }
  void deactivate(LifecycleContext &) override {
    runtime_->active = false;
    detail::retire_menu_anchor(runtime_);
  }
  void retained_checkpoint() override {
    detail::reconcile_menu_anchor(runtime_);
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    if (event.type != InputType::ContextMenu || !has_child_ || !provider_)
      return EventResult::Ignored;
    const auto runtime = runtime_;
    const Point point = event.position;
    const auto bounds = context.bounds();
    runtime->anchor_offset = Point{point.x - bounds.x, point.y - bounds.y};
    return detail::open_menu(runtime, provider_, style_, context, event.key,
                             Rect{point.x, point.y, 0, 0});
  }
  std::optional<detail::OverlayComponentCommand>
  take_overlay_command() override {
    return detail::take_menu_command(runtime_);
  }
  bool has_pending_overlay_command() const noexcept override {
    return !runtime_->commands.empty();
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Group;
    info.enabled = effective_enabled();
    if (provider_ && has_child_)
      info.description = "Menu contextuel disponible";
    return info;
  }
  void paint(PaintContext &) const override {}

private:
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    runtime_->interactive = after.interactive();
    if (!after.interactive())
      detail::retire_menu_anchor(runtime_);
  }
  PopupMenu::ItemsProvider provider_;
  MenuItemStyle style_;
  std::shared_ptr<detail::MenuAnchorRuntime> runtime_;
  bool has_child_{};
};
} // namespace
ContextMenu::ContextMenu(Spec child, PopupMenu::ItemsProvider provider)
    : child_(std::move(child)), provider_(std::move(provider)) {}
ContextMenu::ContextMenu(Spec child, std::vector<PopupMenuItem> items)
    : ContextMenu(std::move(child),
                  [items = std::move(items)] { return items; }) {}
ContextMenu &&ContextMenu::item_style(MenuItemStyle value) && {
  item_style_ = std::move(value);
  return std::move(*this);
}
Spec ContextMenu::spec() && {
  const bool has_child = bool(child_.factory);
  std::vector<Spec> children;
  if (has_child)
    children.push_back(child_);
  return Spec{[provider = provider_, style = item_style_, has_child] {
                return std::make_unique<ContextMenuComponent>(provider, style,
                                                              has_child);
              },
              std::move(children)};
}
} // namespace ui
