# ReadOnly

Status: **existing — enhancements required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`ReadOnly` keeps values readable and, depending on the control, permits focus/copy/navigation while blocking mutation. Source: [component_state.hpp](../include/nativeui/component_state.hpp), `ReadOnly`, `ReadOnlyComponent`.

MyGo expresses this property through Element states; this NativeUI wrapper has no independent catalog equivalent. The current State-only code must be extracted and enhanced with Binding.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Current API:

```cpp
template<class Child> ReadOnly(State<bool>& state, Child&& child);
Spec spec() &&;
```

Verified existing example:

```cpp
ui::State<bool> locked{true};
ui::State<std::string> text{"Value"};
auto field = ui::ReadOnly{locked, ui::TextInput{"Value", text}};
```

Target: the same constructor taking Binding<bool>. Widget handlers remain responsible for defining their non-mutating actions; the wrapper replaces no value or callback.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

Effective ReadOnly is restrictive: a true ancestor imposes read-only behavior. Do not freeze the child’s value Binding: external application changes remain visible. No mode-change callback is added; observing the input state is sufficient.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

- Text: reading, selection, copying and focus are permitted; mutating insertion/cut/paste is rejected.
- Value controls: navigation/focus is permitted where their contract allows it; adjustment is rejected.
- Actions without a mutable value follow the widget’s own contract; do not treat all buttons as disabled.
- Transition midway through a gesture: recover the mutating gesture without a new write.
- The wrapper has no focus/capture or direct Escape/wheel handling.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Measurement/minimum/placement are transparent; no space is removed. If a read-only visual recipe changes metrics, the child requests layout; the wrapper does not assume every recipe has the same dimensions.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No painting. Preserve readability distinct from disabled behavior; do not automatically reduce opacity. Descendants publish their resolved read-only appearance. External value changes request appropriate paint/layout without a user gesture callback.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Wrapper `None`; descendants remain readable with `read_only=true`. Reject SetValue/Increment/Decrement and other advertised mutations; Focus/reading/navigation remain available when eligible. The bridge must never bypass state through a direct write.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Reentrant mode replacement during editing preserves committed data; do not restore an old value through a late rollback. An expired binding produces no stale access. Restore editing guards after an exception and let the runtime recover focus/capture.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Inherited availability and child implementations; [Enabled](enabled.md) remains a separate property. Cases: locking during slider drag, a text draft, a reentrant callback, external changes while locked and nested scopes. Unavailable IME preedit is not added here.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/read_only.hpp` and `src/read_only.cpp`.

read_only.hpp/cpp extract the non-template read-only core; component_state.hpp remains a facade. Keep ReadOnly State& and add Binding without transforming widget value models or their historical overloads.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `readonly_text_copy`: selection/copy works; paste makes no change.
- `readonly_value_gesture`: the slider is focusable but writing is rejected.
- `readonly_external_update`: a new external value is visible.
- `readonly_inheritance`: a true parent cannot be overridden.
- `readonly_mid_drag`: locking during capture, with no writes after locking.
- `readonly_throw_recovery`: exception followed by a valid next interaction.

Create the future public example `examples/features/read_only.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
