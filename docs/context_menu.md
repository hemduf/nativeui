# ContextMenu

**Status: new — implementation required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Add a portable context menu to retained content. The content keeps its normal measurement and interactions until an explicit menu request.

NativeUI provides [combo_popup.hpp](../include/nativeui/combo_popup.hpp) and [overlay.hpp](../include/nativeui/overlay.hpp) but no ContextMenu decorator.

MyGo: `ui/menu.go`, method `Element.ContextMenu`, then `menuPress`, `menuRelease`, `menuKey`. It composes platform menus; the NativeUI target reuses PopupMenu panels in an overlay.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class ContextMenu {
public:
  ContextMenu(Spec child, PopupMenu::ItemsProvider items);
  ContextMenu(Spec child, std::vector<PopupMenuItem> items);
  ContextMenu&& item_style(MenuItemStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
auto text = ui::Label("Document").spec();
auto contextual = ui::ContextMenu(
    std::move(text),
    std::vector<ui::PopupMenuItem>{ui::PopupMenuItem::action("Copy", [] {})})
    .spec();
```

Items, checked, shortcut_label, and children follow [PopupMenu](popup_menu.md) exactly. The provider is called only when a context-menu request is accepted.

The decorator is a single-child container; it does not synthesize a new visual button. Preserve the child's accessible name.

## 3. State, ownership, and notifications

Own the child Spec, provider, and style; separate session per decorator and UI. The opening point is a copied logical Point, never a retained native event.

An open menu captures the current snapshot; rebuilding the item source does not retarget an already visible action. Unmounting content closes the menu for this anchor.

Identity used to close/restore focus belongs to the decorator and previously targeted descendant; if no valid descendant remains, return to normal Focus policy.

## 4. Interactions

Existing normalized request: InputType::ContextMenu carries logical position/modifiers; routing selects the hit-tested target and then bubbles if Ignored. It ends active capture before delivery without moving keyboard focus or arming a new PointerDown. The decorator consumes this request rather than a synthesized primary click.

Keyboard target: append Key::Menu and Key::F10 to enum Key without renumbering current values; normalize Menu/Shift+F10 to InputType::ContextMenu at the focused rectangle position through the runtime. These keys and their platform translation are not currently delivered. Plain F10 does not trigger the request.

Do not trigger the child's primary action from the same secondary click; consume only the accepted request. Primary click, selection/dragging, wheel, and Tab follow the child while the menu is closed.

Panel navigation and cancellation follow PopupMenu. A request refused by the decorator is Ignored; previous capture has nevertheless already been cancelled by existing normalized routing. An accepted request with an empty list opens the closable empty panel, as in PopupMenu.

## 5. Measurement and layout

Decorator measurement equals the child's, with no mandatory padding. Arrange its child within the same bounds; parent clipping and transforms apply to the opening point.

Place the menu at the request position or the keyboard descendant's edge; clamping/flipping use the retained overlay service (OverlaySpec/OverlayHandle, detail::OverlayService). The overlay does not contribute to document measurement.

Resizing and scrolling the child can invalidate anchor geometry; recalculate using safe identity, or close if the target disappears.

## 6. Presentation and invalidation

The decorator adds neither a border nor hover styling to the child. MenuItemStyle affects only the panel; content presentation still comes from its components.

Opening/closing invalidate overlay paint; rebuild content only when its data changes. Keep focus states consistent during the modal command menu.

Submenus and the 200 ms timer are those of PopupMenu; no second animation or menu stack.

## 7. Accessibility

Preserve child semantics; the decorator exposes Group/None according to its composition role, with menu availability in its description or a command action.

The current SemanticAction model has no ShowContextMenu; do not claim Activate is equivalent to the child's primary action. A ShowContextMenu extension may be added separately and routed to the same core.

The open panel publishes PopupMenu/MenuItem as specified by PopupMenu, without exposing native references.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Prepare opening commands before publication. No menu launches after unmounting, even if a deferred request is already queued.

Closure commits the terminal handle before the action. Provider or enqueue failure: remain closed, keep the child usable, and cancel the unaccepted command; no synchronous fallback opening.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: [popup_menu](popup_menu.md), Overlay, Focus, backend-neutral input. Translation of the existing secondary gesture and new keyboard keys must be covered at the input layer rather than in every decorator.

Nested decorators: the nearest mounted/enabled decorator with a provider accepts and takes ownership, even for an empty or inactive snapshot; only one menu opens. An unavailable decorator or null provider ignores the request, allowing the parent to receive it.

Empty, removed, or hidden content: no request; overlay bounds coordinates outside the viewport. MyGo's system editing commands are not added automatically.

## 10. Files and compatibility

Target: `include/nativeui/context_menu.hpp` and `src/context_menu.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

Reuse a private popup_menu.cpp core for the session and panels; context_menu.cpp contains the decorator, its routing, and anchor geometry. Both files contain distinct real behavior.

Register `src/context_menu.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`context_menu_secondary`: a secondary click opens at the logical point without activating content.

`context_menu_keyboard`: normalized Menu/Shift+F10 opens near focus; plain F10 does nothing.

`context_menu_nested`: only one decorator accepts; a parent can handle a refused request.

`context_menu_layout`: content measurement/clipping remain identical before and after opening.

`context_menu_stale_request`: a deferred request after removal becomes inert; provider/queue failure recovers.

`context_menu_action_throw`: the menu closes before the callback even if the anchor is removed or an exception occurs.

Add `examples/features/context_menu.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.