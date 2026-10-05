# HistoryButton

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

HistoryButton represents Back or Forward with optional history for multistep jumps. No dedicated NativeUI component; the application must inject actions.

MyGo `ui/router.go`: `BackButton`, `ForwardButton`, `historyButton`. A click calls Go(±1); a context menu lists up to fifteen destinations; Router is a MyGo dependency not ported by this widget.

Version studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions indicate the present; the following requirements form the target contract.

## 2. Public API and composition

Proposed target API:

```cpp
enum class HistoryDirection { Backward, Forward };
struct HistoryEntry {
    std::string key;
    std::string title;
    bool operator==(const HistoryEntry&) const=default;
};
HistoryButton(HistoryDirection direction,Binding<bool> can_navigate,
              std::function<void(int signed_steps)> navigate);
HistoryButton(HistoryDirection direction,State<bool>& can_navigate,
              std::function<void(int signed_steps)> navigate);
HistoryButton&& entries(Binding<std::vector<HistoryEntry>>) &&;
HistoryButton&& entries(State<std::vector<HistoryEntry>>&) &&;
HistoryButton&& maximum_menu_entries(std::size_t count) &&;
HistoryButton&& label(std::string text) &&;
HistoryButton&& style(HistoryButtonStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<bool> can_back{true};
auto back = ui::HistoryButton{ui::HistoryDirection::Backward,can_back,
    [](int steps){(void)steps;}};
```

Default menu cap15, “Back”/“Forward” labels, nearest-first entries. signed_steps negative backward/positive forward. Entries summarize destinations in this direction only; nonempty unique keys, titles may be equal.

All signatures belong to namespace `ui`. The builder is consumed by `Spec spec() &&`; children become owned `Spec` objects. Proposed declarations do not claim to be an already-delivered API. Signature blocks are fragments of members of the described type, not complete programs; `Key` or `T` corresponds to that type's template parameter where it exists.

## 3. State, ownership, and notifications

External can_navigate Binding; effective button enabled = can true AND inherited availability AND callback present. Optional entries Binding, owned snapshot for menu. The component maintains neither history nor current index: the application supplies the list and handles every write. Invalid Binding forbids navigation, no synthetic callback. Empty titles display the key as an explicit fallback.

The component, its state, and its notifications are confined to the UI/main thread. Genuine historical direct borrows of `State<T>&` must remain alive; constructors delegating to `state.binding()` retain the safe control block, not State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored, and `observe()` is inactive; there is no automatic destruction notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

Primary click/Enter/Space call navigate(±1) once on release. A context request or keyboard context command opens a portable menu if entries are nonempty/can is true. Choice remaps the key in current entries, then calls direction*(index+1), rather than the old menu index. Escape cancels press/closes menu. Long press is not required in v1. Wheel ignored, no global Alt-Left/Right shortcut captured by this widget; the application composes its commands.

This control's target contract treats ReadOnly as forbidding navigation because it triggers an application mutation; focus and label reading remain allowed. Revalidate can_navigate, enabled, and read_only on release and menu choice.

Routing respects inherited availability, clipping, and runtime capture. No global shortcut or audio parameter access should be added for this component.

## 5. Measurement and layout

Intrinsic Button chevron icon + padding, style-resolved minimum hit target, optional visible label if chosen by style. Menu anchored to bounds through the overlay service, existing viewport/menu scrolling constraints. No dedicated size based on total history length and no UI page allocation.

Sizes and positions are NativeUI logical coordinates. The backend performs scale factor conversion exactly once; the component handles no native screen coordinates.

## 6. Presentation and invalidation

HistoryButtonStyle based on the button/icon recipe: hover/pressed/disabled/focus and dimensions. Can false clears pressed state and closes the menu without navigation. Entries updates may reflow the menu but do not request button layout if label/icon is unchanged. Do not assume an already-existing History theme slot.

Invalidation distinguishes pixel changes from metric changes. An identical effective state is a no-op; the component does not force a whole-window repaint when its bounds suffice.

## 7. Accessibility

Button role, descriptive Back/Forward name, Focus/Activate when eligible; PopupMenu/MenuItems named by destinations. Direction alone is not the only visible/accessibility information. Disabled blocks mutating actions; do not invent a Router role or native history.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to deferred T068; this specification validates neither VoiceOver, UIA, nor AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group`, or `Custom` as appropriate.

## 8. Lifecycle and recovery

Navigation may close/destroy the owner through a safe deferred checkpoint; the button disarms and releases capture before callback. Entries changed during menu: revalidate key/can before signed step; absent key is a no-op and closes the menu. A throwing callback is not retried; popup handles terminalized/RAII cleanup. Unmounting does not call navigate.

Subscriptions, invalidators, and captures belong to each instance and are released through RAII. A started callback that throws is never automatically replayed; invariants are restored before C++ propagation. Unmounting is no-throw and triggers no application destruction callback. UI owner destruction from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

[Button](button.md), [IconView](icon_view.md), [PopupMenu](popup_menu.md)/[ContextMenu](context_menu.md), Overlay/Focus services. Cases: history beginning/end, empty entries, cap zero disables menu, duplicate keys rejected before snapshot, index > INT_MAX rejects menu action without overflow, expired can and absent callback. No Router or global history registry.

Reuse the `Tree`, focus, availability, and invalidation services; do not create competing local copies. IME limits remain those of [DESIGN.md §17.4](../DESIGN.md): native committed-text transport is available; full preedit/IME and candidate rectangle transport are deferred. Do not equate this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/history_button.hpp` and `src/history_button.cpp`.

history_button.hpp contains direction/entry/style/builders; history_button.cpp contains retained activation, menu snapshot mapping, and paint/layout. Backward/Forward are variants of this pair, rather than separate headers/cpp files. No Router type or OS API in signatures.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained measurement/layout core and, where applicable, input/paint; no empty file or central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. No Pugl, Skia, OS, plugin, or automation type enters public signatures.

## 11. Tests and acceptance criteria

- `history_directions_steps`: main clicks ±1 and menu jump ±N.
- `history_can_gating`: disabled/read-only/expired bindings, no callback.
- `history_menu_cap`: nearest-first capped at 15, cap0, equal labels with distinct keys.
- `history_menu_rebase`: changed entries remap the key before action.
- `history_destination_removed`: stale key is a no-op with the menu closed.
- `history_press_cancel`: release outside/cancel and safe closing.
- `history_navigate_throw`: at-most-once callback, valid next interaction.
- `history_no_router`: example works with injected callbacks alone.

Create the future public example `examples/features/history_button.cpp`; `--self-test` runs this page's assertions, then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances, and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation verifies sources, signatures, and links; it reports no execution of these tests. Acceptance: all named cases pass, no historical API regressions, no global mutable dependency, and no NativeUI warnings.
