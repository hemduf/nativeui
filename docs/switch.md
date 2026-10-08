# Switch<T>

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Switch<T>` selects a subtree by value equality; it does not represent a toggle switch. Source: [dynamic.hpp](../include/nativeui/dynamic.hpp), `Switch<T>`, `SwitchBranch<T>`, `SwitchComponent<T>`.

MyGo uses a Go switch during composition; no independent widget needs porting. Preserve this name for conditional composition and [Toggle](toggle.md) for the visual switch.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API to preserve in full:

```cpp
explicit Switch(Binding<T> state);
explicit Switch(State<T>& state);
template<class Child> Switch& when(T value, Child&& child) &;
template<class Child> Switch&& when(T value, Child&& child) &&;
template<class Child> Switch& otherwise(Child&& child) &;
template<class Child> Switch&& otherwise(Child&& child) &&;
Spec spec() &&;
```

Verified existing example:

```cpp
ui::State<int> page{0};
auto view = ui::Switch<int>{page}.when(0, ui::Label{"Home"})
    .when(1, ui::Label{"Options"}).otherwise(ui::Label{"Unknown"});
```

Preserve the implicit State/Binding deduction guides, lvalue/rvalue overloads and T equality constraints. The first equal branch wins even when several `.when` calls declare the same value; `otherwise` replaces the previous fallback.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

Branch recipes and values are owned. Binding is observed; branch keys currently remain `switch:<index>` and fallback `switch:fallback`. Changing to another value that selects the same branch preserves that branch. Inactive branches are not mounted.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

No input/focus of its own. A branch change within a callback is deferred to Tree; capture/focus of the removed branch is recovered. No special Escape, switching gesture or on_select event. The application observes its state for business notifications.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Measurement/preferred size comes from the sole active branch; no match without a fallback = zero. Inactive branches contribute neither size nor gaps. All placements use the host bounds; no implicit layout transition between branches.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

The host does not paint. An unchanged branch is not unmounted for an equivalent state notification. Clear old pixels on replacement. Any optional animation should be an explicit component; no global transition layer is added here.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

The host is `None`, with only active-branch descendants. Reinserting an unmounted branch recreates retained IDs; do not recycle a defunct semantic identity. The fallback receives no special role.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Adapt T to branch selection in the header, then pass index/key and Spec to the type-erased core. A throwing comparison/factory does not leave a half-switched host. Preserve the dynamic runtime’s quarantine/explicit retry rules; a started callback is not replayed.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on dynamic sources and Tree reconciliation. T must remain copyable as required by existing uses and comparable; do not require hashing or enum types only. Cases: no match, duplicates, replaced fallback, reentrant state, throwing equality and an empty switch.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/switch.hpp` and `src/switch.cpp`.

switch.hpp retains only typed comparison adapters and template constructors; switch.cpp hosts non-template DynamicHost, type-erased observation and branch publication. Do not replace this with predefined T instantiations. Preserve dynamic.hpp and historical public signatures.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `switch_first_equal`: the first duplicate branch wins.
- `switch_fallback_replace`: the last otherwise is selected; no fallback leaves empty content.
- `switch_lvalue_overloads`: compile when/otherwise on named and temporary builders.
- `switch_custom_key_type`: a user type that is neither an enum nor hashable.
- `switch_same_branch_identity`: no remount for the same branch.
- `switch_compare_factory_fault`: recovery after an equality/factory throw.

Create the future public example `examples/features/switch.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
