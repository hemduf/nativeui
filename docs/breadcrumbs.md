# Breadcrumbs

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

Breadcrumbs displays a path of navigable ancestors followed by the current destination. NativeUI has no such widget; injected actions use the button/popup foundation without Router.

MyGo `ui/feedback.go`: `Breadcrumbs`. Ancestors are links, the final component is nonclickable text, and long labels ellipsize. The target uses stable keys instead of MyGo's chosen index.

Version studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions indicate the present; the following requirements form the target contract.

## 2. Public API and composition

Proposed target API:

```cpp
struct BreadcrumbItem {
    std::string key;
    std::string label;
    bool enabled=true;
    bool operator==(const BreadcrumbItem&) const=default;
};
Breadcrumbs(Binding<std::vector<BreadcrumbItem>> path);
Breadcrumbs(State<std::vector<BreadcrumbItem>>& path);
Breadcrumbs&& on_navigate(std::function<void(const std::string&)>) &&;
Breadcrumbs&& label(std::string accessible_name) &&;
Breadcrumbs&& style(BreadcrumbsStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<std::vector<ui::BreadcrumbItem>> path{{
    {"root","Disk"},{"home","Ada"},{"docs","Documents"}}};
auto crumbs = ui::Breadcrumbs{path}.label("Path")
    .on_navigate([](const std::string&){});
```

A callback is required for activatable ancestors; without one, presentation only. No additional selection: the last key is the destination, and the application replaces path after navigation. Item model and separators remain in the pair.

All signatures belong to namespace `ui`. The builder is consumed by `Spec spec() &&`; children become owned `Spec` objects. Proposed declarations do not claim to be an already-delivered API. Signature blocks are fragments of members of the described type, not complete programs; `Key` or `T` corresponds to that type's template parameter where it exists.

## 3. State, ownership, and notifications

The path Binding owns a safe reference, with an owned snapshot per generation. Nonempty unique keys. The last item is nonnavigable even if enabled; enabled ancestors are activatable when a callback is present. The callback does not write path itself; the application decides navigation and updates. Binding expiration retains the last path and refuses every navigation action from potentially stale data.

The component, its state, and its notifications are confined to the UI/main thread. Genuine historical direct borrows of `State<T>&` must remain alive; constructors delegating to `state.binding()` retain the safe control block, not State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored, and `observe()` is inactive; there is no automatic destruction notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

Click release/Enter/Space activate an ancestor once; down-out/cancel disarm. Tab traverses visible enabled ancestors; final text is outside Tab navigation. Escape closes the overflow popup or cancels a press; the wheel does not change destination. Path change between down/up: revalidate that the key exists and remains an enabled ancestor, otherwise no-op. Right-click opens no system menu automatically.

Routing respects inherited availability, clipping, and runtime capture. No global shortcut or audio parameter access should be added for this component.

## 5. Measurement and layout

Row with chevrons between items, no divider around the path. Fit by intrinsic sizes; intra-label ellipsis first. If the sum of minima does not fit, keep first and last and replace the intermediate group with a “…” popup; a small viewport retains last + overflow if the first does not fit. Hidden items are available in the popup by key, not as separate semantically duplicated objects. Finite bounds/labels, layout without loading an external path.

Sizes and positions are NativeUI logical coordinates. The backend performs scale factor conversion exactly once; the component handles no native screen coordinates.

## 6. Presentation and invalidation

BreadcrumbsStyle: font, padding, minimum item width, chevron/gap, and focus/hover colors. Slightly accented destination, theme-compatible textual enabled/disabled distinction. Changed path/layout invalidates metrics, hover only pixels. Decorative separator icons are not interactive.

Invalidation distinguishes pixel changes from metric changes. An identical effective state is a no-op; the component does not force a whole-window repaint when its bounds suffice.

## 7. Accessibility

Group named through label; target Custom/Button ancestors while the Link role is absent, Text destination. Overflow PopupMenu exposes hidden ancestors; avoid publishing a hidden item simultaneously in the bar and visible menu. The complete name retains the label even when ellipsized. No native URL relationship or implicitly invoked browser.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to deferred T068; this specification validates neither VoiceOver, UIA, nor AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group`, or `Custom` as appropriate.

## 8. Lifecycle and recovery

Prepare the path snapshot before publication. Initial duplicate/empty key throws invalid_argument; an invalid update retains the last accepted generation and a diagnostic. An open popup retains owned keys and revalidates the dataset on choice; removing a key during the popup closes/removes the stale entry without a callback. A throwing callback disarms press before propagation and does not replay navigation.

Subscriptions, invalidators, and captures belong to each instance and are released through RAII. A started callback that throws is never automatically replayed; invariants are restored before C++ propagation. Unmounting is no-throw and triggers no application destruction callback. UI owner destruction from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

[Button](button.md), [PopupMenu](popup_menu.md), [Popover](popover.md) overlay services as needed, row layout/text shaping. Cases: empty path, singleton, all actions disabled, wide Unicode label, shrinking bounds, and a callback replacing path. No file I/O or URL resolution by the component.

Reuse the `Tree`, focus, availability, and invalidation services; do not create competing local copies. IME limits remain those of [DESIGN.md §17.4](../DESIGN.md): native committed-text transport is available; full preedit/IME and candidate rectangle transport are deferred. Do not equate this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/breadcrumbs.hpp` and `src/breadcrumbs.cpp`.

breadcrumbs.hpp declares BreadcrumbItem/style/builder; breadcrumbs.cpp contains fitting/overflow, retained items, activation, and paint/layout. No file per item or exposed Router type. Popup items are owned models and overlays follow the existing service.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained measurement/layout core and, where applicable, input/paint; no empty file or central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. No Pugl, Skia, OS, plugin, or automation type enters public signatures.

## 11. Tests and acceptance criteria

- `breadcrumbs_last_not_action`: singleton/final item not focusable.
- `breadcrumbs_key_navigation`: ancestor callback gets the exact key, not an index.
- `breadcrumbs_overflow`: very long path, root/destination priorities, and menu.
- `breadcrumbs_changed_during_press`: key becoming leaf/removed = no-op.
- `breadcrumbs_menu_stale`: dataset changes before choice, no stale key.
- `breadcrumbs_semantic_label`: complete label despite ellipsis.
- `breadcrumbs_callback_throw`: press released, next action possible.

Create the future public example `examples/features/breadcrumbs.cpp`; `--self-test` runs this page's assertions, then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances, and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation verifies sources, signatures, and links; it reports no execution of these tests. Acceptance: all named cases pass, no historical API regressions, no global mutable dependency, and no NativeUI warnings.
