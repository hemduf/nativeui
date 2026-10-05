# RadioButton<T> and RadioGroup<T>

**Status: existing — extraction required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Offer exclusive choices sharing a selection controller. RadioGroup is a model/focus object rather than a component with independent measurement.

NativeUI: [widgets_checkbox_radio.inc](../include/nativeui/detail/widgets_checkbox_radio.inc), RadioGroup<T>, RadioButton<T>, value registry, and detail::RadioButtonComponent core; [focus_group.hpp](../include/nativeui/detail/focus_group.hpp) defines retained participation.

MyGo: `ui/widgets.go`, `Radio[T]`; `ui/toggle.go`, `RadioGroup`. Focus grouping and typed choices already exist; preserve the API and actual constraints without imposing T=int.

## 2. Public API and composition

Current API to preserve; the following declarations are in `namespace ui`.

```cpp
template<class T>
class RadioGroup {
public:
  explicit RadioGroup(Binding<T> selected);
  explicit RadioGroup(State<T>& selected);
};
template<class T>
class RadioButton {
public:
  RadioButton(const RadioGroup<T>& group, T value, std::string label);
  RadioButton&& style(RadioStyle value) &&;
  Spec spec() &&;
};
```

Example using the current API:

```cpp
ui::State<int> size{1};
ui::RadioGroup<int> group{size};
auto small = ui::RadioButton(group, 1, "Small").spec();
auto large = ui::RadioButton(group, 2, "Large").spec();
```

The header preserves constructors, deduction, and State's equality condition; the typed registry produces is_selected/select/observe adapters for the non-template core.

RadioStyle remains the existing public style. Row/Column containers arrange radio Specs; no empty radio_group.cpp or additional public controller component.

## 3. State, ownership, and notifications

The group owns shared Binding<T> copies, focus token, and value registry; RadioButton copies its internal controls. The lexical RadioGroup object may leave scope after Specs are created without invalidating these copies.

T value is owned with stable registry identity. Duplicate group values are already rejected by the registry; preserve rejection and identity lifetime.

Selection absent from options: all faces unselected, but the first available participant provides Tab entry. An external write forces no default value at mount.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction, Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and observe() remains inactive. No implicit destruction notification: check valid at each dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not extend the lifetime of application models captured by a closure.

External observation invalidates presentation without simulating a user gesture. Synchronous State notifications: stable snapshot, additions on the next pass, removals skipped, and recursive writes coalesced. After an exception, the published value remains, the rest of that notification pass is interrupted, and dispatch must remain reusable.

## 4. Interactions

Click release and Space at KeyUp choose value; an already selected value generates no additional State notification. Enter does not activate in the current source.

Group focus: one Tab stop, entering at an available selected value; arrows/Home/End traverse and select available participants through the shared runtime.

ReadOnly prevents select() while allowing focus/reading. Disabled and hidden participants are excluded from navigation choices; disabled selected values remain observable.

Wheel ignored; PointerCancel, removal, or loss of focus cancel arming without deselection. Leaving and returning inside follows existing PressActivationState.

## 5. Measurement and layout

Measurement: outer diameter, leading_padding, label_gap, label measurement, and minimum_width; height control_height. Preserve all RadioStyle fields and results.

The logical group has no measurement; radios may be arranged in several containers, but focus follows valid retained order under the same token.

The central mark does not change width by default. Label clipping and disk center use the actual allocated space in logical units.

## 6. Presentation and invalidation

Outer/inner/mark colors and radii come from resolved style; selected determines the mark, pressed the transient interaction. Focus ring does not replace selected.

Invalidation classification accounts for mark visibility even without a color change. Selected patches with different metrics require layout.

Two groups sharing State can show the same value while retaining different focus tokens. No global group name.

## 7. Accessibility

Target: RadioButton with selected/checked and Select action, label name; expose the group relationship through an application-named Group container.

RadioGroup is not a Spec and introduces no independent semantic node. RadioButtonComponent currently provides no semantics in the reviewed source: publication must be added during extraction.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Value registry and token remain owned through shared references in Specs/components; removing an option releases its registration according to the existing registry.

The core keeps type-erased is_selected/select/observe callbacks and detached invalidation objects. Copy select and invoke it after capture ends; a reentrant observer must not rediscover a node through a recycled address.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: State, shared group focus, TextService, ThemeBinding. [segmented_control](segmented_control.md) is another exclusive presentation rather than a RadioGroup rewrite.

User T may throw during copy/equals: registration and mounting must be transactional. Equality failure during paint/layout does not publish a partial frame.

Empty/all-disabled: no navigable stop; removing selection does not rewrite Binding. Identical labels do not affect the value-based registry.

## 10. Files and compatibility

Target: `include/nativeui/radio_button.hpp` and `src/radio_button.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

Keep RadioGroup<T>, RadioButton<T>, guides/overloads, and typed registry in the header; move the real detail::RadioButtonComponent core and painting/interactions to the .cpp.

widgets_checkbox_radio.inc becomes a compatibility entry point without duplicate implementation; do not move already used public types into a private namespace.

Register `src/radio_button.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`radio_button_exclusive`: selects one choice; an identical value does not rerun observers.

`radio_button_roving`: one Tab stop; arrows/Home/End select while skipping disabled participants.

`radio_button_duplicates`: duplicate values rejected; duplicate labels with distinct values allowed.

`radio_button_group_lifetime`: Specs remain valid after lexical controller destruction.

`radio_button_generic_value`: a user type compiles without predefined explicit instantiations.

`radio_button_observer_copy_throw`: equality/copy/select exceptions and reentrant removal restore registry/focus and the next action.

Add `examples/features/radio_button.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.