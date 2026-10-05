# Toolbar

**Status: new — implementation required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A horizontal command bar with group navigation and overflow for controls that no longer fit. Commands remain the same in the bar and menu.

NativeUI has Row, PopupMenu, Button, and group focus but no public Toolbar in [widgets.hpp](../include/nativeui/widgets.hpp).

MyGo: `ui/toolbar.go`, `Toolbar`, `layoutToolbar`, `appendToolbarItems`. End controls are collapsed and then described in a menu. Target: explicit owned metadata without retaining or clicking hidden node pointers.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
struct ToolbarItem {
  std::string key;
  Spec content;
  std::optional<PopupMenuItem> overflow_item;
};
class Toolbar {
public:
  Toolbar(std::string label, std::vector<ToolbarItem> items);
  Toolbar&& style(ToolbarStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
std::vector<ui::ToolbarItem> items;
items.push_back({"save", ui::Button("Save", [] {}).spec(),
    ui::PopupMenuItem::action("Save", [] {})});
auto bar = ui::Toolbar("Document", std::move(items)).spec();
```

Callbacks in content and overflow_item must invoke the same application action; do not attempt synthetic activation of a hidden Node. Actions are empty in the minimal example; in real usage share an owned closure.

An absent overflow_item marks a control that cannot collapse; a ToggleGroup provides a children submenu of options with snapshot checked state. Represent a Spacer using Spacer content and no overflow_item, with flexible width.

Target ToolbarStyle: padding, gap, minimum height, Toolbar control styles, and the “More” button label. Item snapshots do not change in place: replace Spec at a checkpoint when application metadata changes.

## 3. State, ownership, and notifications

Non-empty unique keys identify items within a generation; labels never provide identity. ToolbarItem.content and overflow_item form an immutable generation snapshot. Changing items/metadata requires replacing Spec at a checkpoint; no implicit in-place update or preservation of transient state between generations is promised.

The bar retains mounted components when they move into overflow to preserve State and identity. They become hidden and non-interactive; active capture/IME are ended before visual removal.

Overflow contains copies of command metadata. An obsolete command is blocked by its application owner token; it must not reactivate content with a recycled identity.

## 4. Interactions

Tab enters at a single visible participant or More; Left/Right wrap, Home/End choose first/last. TextInput or subgroup arrows are handled by the descendant before outer navigation.

Clicks/Enter/Space are delegated to controls; the More menu follows PopupMenu. The bar does not capture the wheel or commands without an owner.

If the focused control collapses, end its interaction then transfer focus to More without writing a default value. On widening, do not steal focus from the open menu; close that menu if overflow no longer exists.

ReadOnly/Disabled are inherited by controls; one-time actions follow the Button contract, values follow ToggleButton/ComboBox.

## 5. Measurement and layout

Measure intrinsic items, gap, and padding; if their sum exceeds width, first reserve More, then collapse overflowable items from the end until visible items fit.

Preserve order of retained items and overflow commands. Groups are indivisible for arrangement, and their submenu describes the children without splitting a ToggleGroup in the middle.

Non-collapsible items stay present: if their width exceeds the viewport, clip the bar and allow the parent to scroll; silently lose no command. Flexible Spacers take only remaining space.

Make overflow decisions in layout, never in paint. Logical coordinates and font styles supply measurements; adjust after scale/font/resize changes.

## 6. Presentation and invalidation

Controls are transparent at rest, with a visible face on hover/press; Toolbar style applies through a local scope. Checked bools persist visually even in the More menu.

More occupies no width when everything fits. An overflow transition invalidates layout, focus, and semantic structure; colors alone invalidate paint.

No global animation loop or remount on every resize. Hidden item invalidations are visually suspended until reappearance while retaining current data.

## 7. Accessibility

Current SemanticRole does not contain Toolbar: use a named Group and preserve visible controls' actions. The More menu is Button/PopupMenu; collapsed items are not duplicated in the accessible tree.

For checked state or a submenu, use PopupMenuItem metadata. Icon-only buttons have an explicit command label in content and overflow_item.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Prepare a complete overflow plan, then publish geometry, visibility, and navigation together. Measurement/factory failure: retain the previously committed layout, with no item halfway between bar and menu.

The menu copies actions before invocation and closes first. A callback removing the item/bar or throwing does not produce a second action from the old node.

Focus/overlay enqueue rejection is recoverable through a durable command at the checkpoint; do not use direct calls on a hidden participant.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: [row](row.md), [spacer](spacer.md), [focus_scope](focus_scope.md), [popup_menu](popup_menu.md), [toggle_group](toggle_group.md). No Router or system menu.

Empty items: minimum-size bar without More. Duplicate keys are rejected before publication. An overflow_item without a callback remains visible in the menu according to its actual enabled/actionable state.

An application updating enabled/checked must supply new metadata at the same checkpoint as content. No ad hoc inspection of hidden State or audio access.

## 10. Files and compatibility

Target: `include/nativeui/toolbar.hpp` and `src/toolbar.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

ToolbarItem, ToolbarStyle, and group/spacer models remain in the Toolbar file pair; the .cpp contains measurement/collapsing, style scope, and focus, with a shared menu engine.

No Pugl/Skia type scanning in ToolbarItem or Node*-to-callback conversion; keys and metadata are the only exported command identities.

Register `src/toolbar.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`toolbar_overflow`: reducing width collapses from the end, reserves More, and preserves order.

`toolbar_groups`: an indivisible group becomes a submenu; checked and disabled states display correctly.

`toolbar_same_command`: a menu action invokes exactly the same command as the visible control.

`toolbar_resize_focus`: collapsing focus/drag cancels cleanly; widening causes no remount or focus theft.

`toolbar_nonoverflowable`: a control without metadata remains visible/clipped; no item is lost.

`toolbar_stale_throw`: a stale command or throwing callback reactivates no removed node.

`toolbar_layout_failure`: a measurement exception publishes no partial geometry/visibility.

Add `examples/features/toolbar.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.