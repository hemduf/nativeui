# PopupMenu

**Status: existing — enhancements required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Open a menu anchored to a button; an action can be invoked by pointer or keyboard. The menu is a portable overlay within the same UI.

NativeUI: [combo_popup.hpp](../include/nativeui/combo_popup.hpp), PopupMenu/PopupMenuItem, MenuPopupComponent, and overlay commands. Actions, separators, disabled items, and button/menu styles are present.

MyGo: `ui/widgets.go`, `MenuButton`; `ui/menu.go`, `Menu.Item`, `Separator`, `Submenu`, `MenuItem.Checked`, `Shortcut`. Target: checked state, displayed shortcuts, and submenus without importing MyGo system menus.

## 2. Public API and composition

Current API to preserve; the following declarations are in `namespace ui`.

```cpp
using ItemsProvider = std::function<std::vector<PopupMenuItem>()>;
PopupMenu(std::string label, std::vector<PopupMenuItem> items);
PopupMenu(std::string label, ItemsProvider items_provider);
PopupMenu&& style(ComboBoxStyle value) &&;
PopupMenu&& item_style(MenuItemStyle value) &&;
Spec spec() &&;
```

Example using the current API:

```cpp
auto menu = ui::PopupMenu("Actions", std::vector<ui::PopupMenuItem>{
    ui::PopupMenuItem::action("Open", [] {}),
    ui::PopupMenuItem::separator(),
    ui::PopupMenuItem::action("Delete", [] {}, false)}).spec();
```

Current PopupMenuItem owns kind(Action/Separator), label, enabled, and a std::function<void()> callback; action() and separator() retain their behavior.

Owned target extensions: optional string key unique among siblings, optional<bool> checked, string shortcut_label, and vector<PopupMenuItem> children. An item with non-empty children is a submenu without a direct callback; displayed shortcuts do not register shortcuts globally.

Provider is called again on each opening, never from paint. It produces an owned snapshot for the entire session; changes become visible in the next opened menu.

## 3. State, ownership, and notifications

The provider and each callback are owned. The anchor runtime carries retained identity, overlay handle, and opening-key suppression; actions store no raw node pointer.

The session keeps an immutable item snapshot, a local highlighted item, and a submenu stack. Checked visually reads the snapshot; the application action publishes the new value for the next opening.

Close, then invoke the callback: this is the commit order. The anchor may disappear during the action without prolonging the menu or invoking the same item twice.

## 4. Interactions

Opening: click on release, initial Enter, Space on release; Down arrow opens. The opening key remains consumed until KeyUp to avoid activating the first item.

Within the panel: Up/Down traverse enabled actions, Home/End select first/last; Enter/Space confirm. Separators, disabled items, and actions without callbacks are skipped.

Submenu target: Right opens the highlighted item's children; Left closes the current level; Escape closes one level and then the entire session. Hover opens a submenu after 200 ms; use a cancellable delay, never a Node pointer.

Tab and outside click close according to the existing overlay. PointerCancel stops arming without choosing. The wheel scrolls an over-height panel; focus returns to an anchor that is still valid or follows runtime policy.

## 5. Measurement and layout

Button width is unchanged: measured label, minimum, and padding from ComboBoxStyle. The panel measures labels, the checked column, shortcut_label, and the children indicator.

Anchor in logical coordinates; the overlay service places the menu below it, then flips/clamps within the viewport. Each submenu computes right or left placement without exceeding the edge.

A large menu has bounded height and a scrollable viewport; do not mount closed subpanels. Removal of an invalid anchor closes the session.

## 6. Presentation and invalidation

Preserve ComboBoxStyle/MenuItemStyle. MenuItemStyle extensions for checked/shortcut/chevron columns remain within the menu model; no OS menu or fictitious Theme slot.

Highlight and focus do not reevaluate the provider. Item metric changes invalidate measurement/panel; color changes alone invalidate paint.

A checked item does not change width between true/false: reserve the column. No submenu timer while closed, hidden, or unmounted.

## 7. Accessibility

Target: SemanticRole::Button anchor with expanded state; PopupMenu panel, MenuItem items, None separators, and applicable checked state for marked items.

Highlight and submenu relationships use snapshots/identities; announce the name and shortcut_label as description. combo_popup.hpp currently provides no semantics override: publication remains to be completed.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Prepare the snapshot and panels before publishing the handle. If provider or mounting throws, the anchor remains closed or the old session remains consistent; no partially open handle.

Every action starts after logical closure and focus restoration at the intended checkpoint. Callback exception: the session remains closed; no reopening or retry.

Overlay enqueue rejection/exception: leave a durable command for the next checkpoint or report refusal without publication; do not synchronously run an action that requires deferred closure.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: [combo_box](combo_box.md), retained overlay service (OverlaySpec/OverlayHandle, detail::OverlayService) and overlay commands, Focus, ThemeBinding, and menu styles. ContextMenu reuses the same panel engine.

An empty list is allowed: current open_popup publishes a panel without a selectable action; Escape, Tab, or outside click close it. Preserve this empty opening without inventing an item/callback. The session snapshot does not change when the application changes its list.

Invalid children, duplicate keys, or depth greater than 16 are rejected before opening with invalid_argument for the direct C++ API. Callbacks and styles remain opaque to the menu engine.

## 10. Files and compatibility

Target: `include/nativeui/popup_menu.hpp` and `src/popup_menu.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

Preserve combo_popup.hpp as a compatible header also importing ComboBox. PopupMenuItem and menu variants remain declared in popup_menu.hpp.

Extract non-template anchor/panel cores from combo_popup.hpp; share them with ContextMenu through a private interface, without a central widget switch or duplicating the overlay service.

Register `src/popup_menu.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`popup_menu_snapshot`: provider runs on opening; later source mutation does not change the session.

`popup_menu_navigation`: disabled items/separators are skipped; opening key is suppressed; nested Right/Left/Escape navigation.

`popup_menu_placement`: a long menu scrolls and submenus flip near edges at multiple scales.

`popup_menu_action_once`: closure is committed before action; a callback removes the anchor or throws without a second action.

`popup_menu_open_failure`: provider, mount, and enqueue failures leave handles/focus recoverable.

`popup_menu_legacy_api`: historical action, separator, style, and item_style compile unchanged.

Add `examples/features/popup_menu.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.