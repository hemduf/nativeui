# Fieldset

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

Fieldset groups fields under a legend and inherited availability. NativeUI has Enabled/ReadOnly wrappers but no Fieldset. Foundations: [component_state.hpp](../include/nativeui/component_state.hpp).

MyGo `ui/form.go`: `Fieldset`. The legend names the Group and internal labels participate in the enclosing Form. NativeUI adopts this grouping without merging Field/Fieldset.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Proposed target API:

```cpp
template<class... Children> Fieldset(std::string legend, Children&&... fields);
Fieldset&& enabled(Binding<bool>) &&;
Fieldset&& enabled(State<bool>&) &&;
Fieldset&& read_only(Binding<bool>) &&;
Fieldset&& read_only(State<bool>&) &&;
Fieldset&& description(std::string) &&;
Fieldset&& style(FieldsetStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<bool> allowed{true};
ui::State<std::string> address{""};
auto shipping = ui::Fieldset{"Shipping",
    ui::Field{"Address",ui::TextInput{"",address}}}.enabled(allowed);
```

Defaults: enabled true/read_only false, with an owned legend. Border and spacing variants belong in FieldsetStyle rather than a separate “form section” component.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

The group owns legend/description/children; optional availability states use Binding. Without a binding, normal inheritance applies. Control values remain application-owned, with no global name map or “current form” state. Nested Field components contribute to the nearest Form context, except an internal Form, which starts its own context.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

The legend is neither a button nor a Tab stop. Tab/pointer input goes to descendants. False enabled blocks actions according to inheritance without clearing values; read-only preserves navigation/reading. No grouped toggle action when activating the legend. Escape/Enter remain with controls/Form.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

The legend sits above the child column with explicit gaps/padding. Within Form, do not start a new label column: retain the current alignment. A long legend wraps; an empty legend leaves any description/group but announces no empty text. Overflow requires an external ScrollView.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

FieldsetStyle: legend typography, spacing and optional border/padding. Do not apply uniform opacity for disabled state; controls resolve their own recipe. Classify repaint/layout according to actual legend/style/availability changes. No permanent animation.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Group with name=legend and an owned description. Specific field names are preserved rather than replaced by the legend. Effective disabled/read-only states propagate to descendants. A Group without a legend does not invent a label from the first Field.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Availability subscriptions and the Form context use per-instance RAII. Reentrant removal during measurement must remove the context contribution without retaining a label reference. Destruction calls no validation or enabled callback; cleanup is no-throw.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

[Field](field.md), [Form](form.md), [Enabled](enabled.md), [ReadOnly](read_only.md). Cases: an empty group, empty legend, nested fieldset, nested Form, expired availability state and removal during focus/capture. No automatic validation ordering or audio transfer.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/fieldset.hpp` and `src/fieldset.cpp`.

fieldset.hpp declares builder/style and child/State adapters; fieldset.cpp contains the retained Group, lexical Form participation, measurement/layout/availability and legend paint. A separate pair from field.cpp and form.cpp; do not move child styles into this model.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `fieldset_semantic_group`: legend name and child names preserved.
- `fieldset_form_column`: alignment with outer fields.
- `fieldset_nested_form`: a new isolated context when Form is nested.
- `fieldset_availability`: restrictive enabled/read-only behavior without clearing data.
- `fieldset_empty_legend`: coherent measurement and accessible readout.
- `fieldset_removal_fault`: context/subscriptions recover after an exception.

Create the future public example `examples/features/fieldset.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
