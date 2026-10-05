# FocusScope

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`FocusScope` only defines the focus domain; active=false does not hide its children. Source: [focus.hpp](../include/nativeui/focus.hpp), `FocusScope`, `FocusScopeComponent`; routing uses existing Tree services.

MyGo `ui/scope.go`, `enterScope`, `arrangeFocus`, `restoreFocus`, provides modality tied to overlays. Do not import those services: use NativeUI’s scope and overlays.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API:

```cpp
template<class Child> FocusScope(Binding<bool> active, Child&& child);
template<class Child> FocusScope(State<bool>& active, Child&& child);
FocusScope&& trap(bool value=true) &&;
FocusScope&& default_focus(std::size_t focusable_descendant_index) &&;
Spec spec() &&;
```

Verified existing example:

```cpp
ui::State<bool> active{true};
auto scope = ui::FocusScope{active, ui::Button{"OK", []{}}}
    .trap(true).default_focus(0);
```

Trap defaults to true and the default index is zero. FocusScopeComponent remains public, including the is_focus_scope/focus_scope_active/traps/default_index hooks.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

The active Binding is copied, with immutable trap/index values in the recipe. The observer invalidates focus and paint according to the runtime without changing child availability or models. No global “current scope”. Focusable descendant order is determined during recovery rather than through fixed pointers.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

- active=false removes descendants from focus targets but leaves paint/visibility and pointer input according to the runtime.
- trap=true keeps Tab/Shift-Tab within the active domain.
- default_focus is an index among eligible focusable descendants rather than immediate children.
- Out-of-range index: deterministic recovery to the first eligible descendant; an empty domain has no focus.
- The scope does not consume Escape and is not modal on its own.
- Popups/dialogs establish their own policy through existing services.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Transparent wrapper: the first child provides minimum/preferred size and receives the bounds. No extra space for a focus ring. Descendants’ semantic/focus rectangles remain their own placements.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No painting or backdrop/outline of its own. Descendants draw focus-visible. An active toggle must clear old focus rings without turning the region Hidden. No scope animation.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Role `None`, with descendant reading independent of focus-active. The runtime remains authoritative for accessible focus. Focus requests into an inactive domain are rejected without breaking value snapshots.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Retain the active Binding after the recipe is consumed and release the subscription on unmount. Disabling a scope within a focus callback must leave a durable recovery checkpoint and avoid recursive transfers. Focus-restoration identities are stable/weak, with no retained Node*.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on Tree focus/domains, [Visibility](visibility.md) and [Dialog](dialog.md). Cases: nested traps, initially false active state, no control, an out-of-range index, a disabled child and removal of the current scope. Do not copy MyGo’s focus manager.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/focus_scope.hpp` and `src/focus_scope.cpp`.

focus_scope.hpp/cpp extract non-template hooks and measurement; focus.hpp remains compatible and FocusScopeComponent public. No native type or global registry in the API.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `focus_scope_inactive_visible`: pixels remain present; Tab does not target children.
- `focus_scope_trap_cycle`: Tab/Shift-Tab cycle correctly.
- `focus_scope_default_index`: index among eligible descendants, with out-of-range fallback.
- `focus_scope_nested`: nested domains and restoration.
- `focus_scope_remove_focused`: safe removal with active focus.
- `focus_scope_focus_callback_throw`: the next transfer remains possible.

Create the future public example `examples/features/focus_scope.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
