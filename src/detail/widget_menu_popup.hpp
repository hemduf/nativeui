#pragma once
#include <deque>
#include <limits>
#include <memory>
#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/detail/overlay_commands.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/popup_menu.hpp>
namespace ui::detail {
inline constexpr std::size_t kNoPopupIndex =
    std::numeric_limits<std::size_t>::max();
template <class Predicate>
std::size_t first_popup_index(std::size_t count, Predicate predicate) {
  for (std::size_t i = 0; i < count; ++i)
    if (predicate(i))
      return i;
  return kNoPopupIndex;
}
template <class Predicate>
std::size_t last_popup_index(std::size_t count, Predicate predicate) {
  while (count)
    if (predicate(--count))
      return count;
  return kNoPopupIndex;
}
template <class Predicate>
std::size_t step_popup_index(std::size_t count, std::size_t current,
                             int direction, Predicate predicate) {
  if (!count)
    return kNoPopupIndex;
  auto index = current < count ? current : (direction > 0 ? count - 1 : 0);
  for (std::size_t attempt = 0; attempt < count; ++attempt) {
    index = direction > 0 ? (index + 1) % count : (index + count - 1) % count;
    if (predicate(index))
      return index;
  }
  return kNoPopupIndex;
}
struct MenuPopupSession;
struct MenuAnchorRuntime final {
  NodeId node_id{kInvalidNodeId};
  OverlayHandle handle;
  Key suppress_until_key_up{Key::None};
  bool mounted{}, active{true}, interactive{true}, opening{};
  std::uint64_t generation{};
  std::optional<Rect> anchor_bounds;
  std::optional<Point> anchor_offset;
  std::function<bool()> permission;
  std::function<void()> invalidate;
  std::weak_ptr<MenuPopupSession> current;
  std::deque<OverlayComponentCommand> commands;
};
struct MenuPopupSession final : std::enable_shared_from_this<MenuPopupSession> {
  std::shared_ptr<MenuAnchorRuntime> anchor;
  std::vector<PopupMenuItem> items;
  MenuItemStyle item_style;
  OverlayHandle handle;
  std::size_t highlighted{kNoPopupIndex};
  bool completion_queued{}, live{true}, mounted{}, focused{}, pointer_armed{},
      show_pending{}, level_close_requested{};
  std::uint64_t generation{};
  NodeId node_id{kInvalidNodeId};
  std::weak_ptr<MenuPopupSession> root, parent;
  std::shared_ptr<MenuPopupSession> child;
  std::optional<Rect> submenu_anchor;
  std::vector<Rect> rows;
  Rect bounds{};
  float scroll{}, content_height{};
  Dispatcher dispatcher;
  TimerHandle hover_timer;
  bool hover_due{};
  std::size_t hover_index{kNoPopupIndex};
  std::function<void()> invalidate, structure, release;
  std::deque<OverlayComponentCommand> commands;
};
void validate_menu_items(const std::vector<PopupMenuItem> &items);
void retire_menu_anchor(
    const std::shared_ptr<MenuAnchorRuntime> &anchor) noexcept;
void reconcile_menu_anchor(const std::shared_ptr<MenuAnchorRuntime> &anchor);
[[nodiscard]] bool
menu_anchor_live(const std::shared_ptr<MenuAnchorRuntime> &anchor) noexcept;
[[nodiscard]] EventResult
open_menu(const std::shared_ptr<MenuAnchorRuntime> &anchor,
          const PopupMenu::ItemsProvider &provider, const MenuItemStyle &style,
          InputContext &context, Key opening_key = Key::None,
          std::optional<Rect> bounds = {});
[[nodiscard]] std::optional<OverlayComponentCommand>
take_menu_command(const std::shared_ptr<MenuAnchorRuntime> &anchor);
class MenuPopupComponent final : public Component,
                                 public ThemeBinding,
                                 public OverlayCommandSource,
                                 public OverlayAnchorPolicy,
                                 public DynamicChildrenSource {
public:
  explicit MenuPopupComponent(std::shared_ptr<MenuPopupSession> session);
  bool focusable() const noexcept override;
  bool clips_children() const noexcept override;
  bool uses_retained_checkpoint() const noexcept override;
  bool dismiss_overlay_on_tab() const noexcept override;
  bool overlay_handles_escape() const noexcept override;
  bool overlay_session_valid() const noexcept override;
  std::optional<Rect> overlay_anchor_bounds() const noexcept override;
  Size measure(const std::vector<ChildMetrics> &) const override;
  ChildMetrics
  measure_constrained(const Constraints &,
                      const std::vector<ChildMetrics> &) const override;
  void layout_children(Rect, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &) const override;
  void mount(MountContext &) override;
  void unmount(LifecycleContext &) override;
  void activate(LifecycleContext &) override;
  void deactivate(LifecycleContext &) override;
  void retained_checkpoint() override;
  void focus_changed(bool, FocusContext &) override;
  EventResult input(const InputEvent &, InputContext &) override;
  std::optional<OverlayComponentCommand> take_overlay_command() override;
  bool has_pending_overlay_command() const noexcept override;
  std::vector<std::string> desired_keys() const override;
  std::vector<DynamicChildSpec> desired_children() const override;
  void set_structure_invalidator(std::function<void()>) override;
  std::vector<Spec> initial_children() const;
  SemanticInfo semantics() const override;
  void paint(PaintContext &) const override;

private:
  std::shared_ptr<MenuPopupSession> session_;
  ResolvedMenuItemStyle resolved(std::size_t) const;
  void highlight(std::size_t, InputContext &);
  void ensure_visible(std::size_t, InputContext &);
};
class PopupMenuComponent final : public Component,
                                 public ThemeBinding,
                                 public OverlayCommandSource,
                                 public OverlayAnchorPolicy {
public:
  PopupMenuComponent(std::string label, PopupMenu::ItemsProvider provider,
                     ComboBoxStyle style, MenuItemStyle item_style,
                     std::shared_ptr<MenuAnchorRuntime> runtime);
  bool focusable() const noexcept override;
  bool roving_focus_target() const noexcept override;
  bool cancel_capture_on_read_only() const noexcept override;
  bool uses_retained_checkpoint() const noexcept override;
  bool dismiss_overlay_on_tab() const noexcept override;
  bool overlay_handles_escape() const noexcept override;
  bool overlay_session_valid() const noexcept override;
  Size measure(const std::vector<ChildMetrics> &) const override;
  void mount(MountContext &) override;
  void unmount(LifecycleContext &) override;
  void activate(LifecycleContext &) override;
  void deactivate(LifecycleContext &) override;
  void retained_checkpoint() override;
  void focus_changed(bool, FocusContext &) override;
  EventResult input(const InputEvent &, InputContext &) override;
  EventResult semantic_action(SemanticAction, InputContext &) override;
  std::optional<OverlayComponentCommand> take_overlay_command() override;
  bool has_pending_overlay_command() const noexcept override;
  SemanticInfo semantics() const override;
  void paint(PaintContext &) const override;

private:
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &) noexcept override;
  ResolvedComboBoxStyle resolved() const;
  std::string label_;
  PopupMenu::ItemsProvider provider_;
  ComboBoxStyle style_;
  MenuItemStyle item_style_;
  std::shared_ptr<MenuAnchorRuntime> runtime_;
  PressActivationState press_;
  bool focused_{};
};
} // namespace ui::detail
