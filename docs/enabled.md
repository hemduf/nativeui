# Enabled

Status: **existing — enhancements required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Enabled` restricts descendants’ interaction eligibility while preserving their presence and size. Source: [component_state.hpp](../include/nativeui/component_state.hpp), `Enabled`, `EnabledComponent`.

The current signature takes only State<bool>. MyGo provides the capability through Disabled on elements; no independent family needs copying. The target adds Binding and extraction without changing inheritance rules.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Current API:

```cpp
template<class Child> Enabled(State<bool>& state, Child&& child);
Spec spec() &&;
```

Verified existing example:

```cpp
ui::State<bool> allowed{true};
auto controls = ui::Enabled{allowed, ui::Button{"Apply", []{}}};
```

Additional target API: `template<class Child> Enabled(Binding<bool>, Child&&)`. State delegates to Binding; no on_enabled callback of its own, as the application observes its state.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

Effective Enabled = a restrictive combination of ancestor/descendant states. A false parent cannot be overridden by a true child. The model remains unchanged when disabled. The new Binding subscription uses per-instance RAII; no global listener.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

False prevents activation and mutation and removes ineligible descendants from focus targeting according to Tree. Availability recovery ends an active gesture; re-enabling triggers no activation. The wrapper is neither independently focusable nor targetable. Do not turn disable into ReadOnly: field navigation/copy follows each field’s disabled contract.

- Re-enabling a widget does not automatically restore a key or pointer that remains physically pressed.
- An old pointer-up after disable must not activate the re-enabled control.
- Removing focus does not authorize the wrapper to initiate form business validation.
- Disabled nodes retain their readable data even when their hit targets are excluded.
- A shortcut routed to another scope follows its own constraints; the wrapper does not change a global shortcut table.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Minimum/preferred size and placement are transparent; disabled preserves layout space. A child style may change its metrics according to state, but the wrapper never reduces size by itself. Hit-test bounds are those of the descendant, subject to effective availability.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No painting. Descendants resolve disabled appearance; do not blindly apply uniform opacity to the subtree. Paint or layout only when the descendant’s recipe actually changes; no full-viewport invalidation from the wrapper.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Wrapper `None`. Descendants remain present where relevant with `enabled=false` and reject activation/mutation actions. Do not automatically remove their names/values from the snapshot solely because they are disabled.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

During a reentrant disable within activation, finish the started invocation at most once, then recover gestures at the checkpoint. Invalid target Binding: safe inert state without dereferencing State*. Unmounting disconnects the availability invalidator before owner loss.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Availability runtime, [ReadOnly](read_only.md), [Visibility](visibility.md) and descendant styles. Cases: disable on pointer-down, a held key, a listener recursively changing enabled and two instances with separate bindings. No system call to gray out a control.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/enabled.hpp` and `src/enabled.cpp`.

enabled.hpp/cpp contain the wrapper and availability core; component_state.hpp re-exports them for compatibility. No change to existing State& signatures and no relocation of state into Theme.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `enabled_inheritance`: a parent restriction cannot be overridden.
- `enabled_preserves_layout`: identical bounds and model.
- `enabled_armed_button`: disable before up does not trigger a click.
- `enabled_semantics`: readable value, mutating actions rejected.
- `enabled_binding_lifetime`: state expires without UAF.
- `enabled_reentrant_notify`: reentrant/throwing observer followed by recovery.

Create the future public example `examples/features/enabled.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
