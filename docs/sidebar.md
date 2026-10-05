# Sidebar<Key>

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

Sidebar is single-selection navigation with collapsible sections, icons, and row accessories. NativeUI ListView does not yet express navigation sections; this component builds on list/disclosure rather than Router.

MyGo `ui/sidebar.go`: `Sidebar`, `SidebarSection`, `SidebarItem`, `keys`. One Tab stop, ID selection, typeahead, sections under headings, and scrolling when content overflows.

Version studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions indicate the present; the following requirements form the target contract.

## 2. Public API and composition

Proposed target API, members of Sidebar<Key>:

```cpp
explicit Sidebar(Binding<std::optional<Key>> selection);
explicit Sidebar(State<std::optional<Key>>& selection);
Sidebar&& section(std::string id,std::string title,Binding<bool> open) &&;
Sidebar&& section(std::string id,std::string title) &&;
template<class Accessory>
Sidebar&& item(Key key,std::string label,Accessory&& accessory,bool enabled=true) &&;
Sidebar&& item(Key key,std::string label,bool enabled=true) &&;
Sidebar&& on_navigate(std::function<void(const Key&)>) &&;
Sidebar&& style(SidebarStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<std::optional<std::string>> mailbox{std::string{"inbox"}};
ui::State<bool> open{true};
auto nav = ui::Sidebar<std::string>{mailbox}
    .section("mail","Mailboxes",open.binding())
    .item("inbox","Inbox").item("sent","Sent");
```

State<bool>& section overload. Without an open binding, the section is always open. An item attaches to the last declared section; items before sections form a root group without a header. Section/item keys are unique in their domains; accessory Spec may contain IconView/Badge, with no interactive business function by default.

All signatures belong to namespace `ui`. The builder is consumed by `Spec spec() &&`; children become owned `Spec` objects. Proposed declarations do not claim to be an already-delivered API. Signature blocks are fragments of members of the described type, not complete programs; `Key` or `T` corresponds to that type's template parameter where it exists.

## 3. State, ownership, and notifications

Authoritative single-selection binding; section open bindings own safe references without a global section controller. Static builder items/sections in v1; the application rebuilds to change the set; keys preserve identities through the retained recipe where possible. Navigate callback after a selected change, with an owned key copy; an unknown external key or collapsed section imposes no different selection. An invalid Binding retains readable selected text but refuses gesture mutation.

The component, its state, and its notifications are confined to the UI/main thread. Genuine historical direct borrows of `State<T>&` must remain alive; constructors delegating to `state.binding()` retain the safe control block, not State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored, and `observe()` is inactive; there is no automatic destruction notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

Up/Down/Home/End on visible enabled items; label typeahead with a 700 ms buffer. Item click selects and calls on_navigate once if selection is new; Enter on an already-selected item activates on_navigate without a new write. Header click toggles if it has an open binding; headers are focusable through the roving list and Left/Right close/open them. One main Tab stop and nonfocusable accessory controls; an explicitly interactive accessory must retain its own actions without a synthetic selection click. Closing a section under active/focus recovers to the header/neighbor without automatic business navigation.

Routing respects inherited availability, clipping, and runtime capture. No global shortcut or audio parameter access should be added for this component.

## 5. Measurement and layout

Column in ScrollView, variable headers/rows according to style, one-level indentation under sections. Parent-provided width (no global fixed width), preferred default220 DIP, min120 DIP through style. Single-line ellipsized labels; badges/accessories retain intrinsic size within row limits. Vertical overflow scrolls, horizontal clips. A collapsed header hides items and their height contribution.

Sizes and positions are NativeUI logical coordinates. The backend performs scale factor conversion exactly once; the component handles no native screen coordinates.

## 6. Presentation and invalidation

SidebarStyle: surface, padding, section typography, active/selected/hover row, indent/gap. Selected accent distinct from header hover. A visual Badge does not automatically change the label; description/accessibility may explicitly supply a count. Section arrows may animate locally for 150ms with reduced-motion handling, stopping while hidden/unmounted.

Invalidation distinguishes pixel changes from metric changes. An identical effective state is a no-op; the component does not force a whole-window repaint when its bounds suffice.

## 7. Accessibility

MyGo's Tree role does not exist in NativeUI: Group/Custom and expanded on headers, ListItem or Custom on destinations with eligible Select/Activate/Focus. Each section remains a logical grouping; owned labels and consistent selection key snapshots. Future native source-list/tree mapping is a separate extension.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to deferred T068; this specification validates neither VoiceOver, UIA, nor AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group`, or `Custom` as appropriate.

## 8. Lifecycle and recovery

Resolve the key on release and copy the callback before user code. Navigation may remove Sidebar or rebuild sections at the checkpoint; do not continue on a stale index. An expired open binding becomes unmodifiable, with the last open value readable until the next safe read. Teardown releases observers/timers/focus references, never business callbacks.

Subscriptions, invalidators, and captures belong to each instance and are released through RAII. A started callback that throws is never automatically replayed; invariants are restored before C++ propagation. Unmounting is no-throw and triggers no application destruction callback. UI owner destruction from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

[ListView](list_view.md), [Collapsible](collapsible.md), [ScrollView](scroll_view.md), optional IconView/Badge. Cases: no items, all disabled, unknown key, duplicate item/section key rejected initially, section without open, long title, wide accessory, and removal during navigation. No imposed Router; application-injected on_navigate.

Reuse the `Tree`, focus, availability, and invalidation services; do not create competing local copies. IME limits remain those of [DESIGN.md §17.4](../DESIGN.md): native committed-text transport is available; full preedit/IME and candidate rectangle transport are deferred. Do not equate this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/sidebar.hpp` and `src/sidebar.cpp`.

sidebar.hpp contains section/item models in the pair, key/accessory templates, and style. sidebar.cpp contains roving/typeahead, disclosure state, layout/paint, and activation. No standalone SidebarItem or SidebarSection file; owned type-erased keys and a non-template core.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained measurement/layout core and, where applicable, input/paint; no empty file or central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. No Pugl, Skia, OS, plugin, or automation type enters public signatures.

## 11. Tests and acceptance criteria

- `sidebar_sections_root`: attach item to current section or root group.
- `sidebar_selection_navigation`: correct change/Enter callback.
- `sidebar_typeahead_roving`: labels, disabled skipping, one Tab stop.
- `sidebar_close_active`: recovered focus without automatic business navigation.
- `sidebar_unknown_duplicate`: inert external unknown, duplicates refused.
- `sidebar_accessory_layout`: wide badge/icon and ellipsis.
- `sidebar_reentrant_navigation`: callback removal without UAF.
- `sidebar_open_binding_dead`: next access safe, no synthetic write.

Create the future public example `examples/features/sidebar.cpp`; `--self-test` runs this page's assertions, then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances, and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation verifies sources, signatures, and links; it reports no execution of these tests. Acceptance: all named cases pass, no historical API regressions, no global mutable dependency, and no NativeUI warnings.
