# Field

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

Field associates a label, control, help text and error message; within Form, its label participates in shared alignment. NativeUI currently has no Field; composing Label/TextInput does not automatically link their semantics.

MyGo `ui/form.go`: `Field`, `fieldControl`, `focusIn`, `namesItself`, `Description`, `Error`. The target uses the first eligible control by default, with an explicit target option for ambiguous compositions.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Proposed target API:

```cpp
template<class Child> Field(std::string label, Child&& control);
Field&& description(std::string text) &&;
Field&& description(Binding<std::string> text) &&;
Field&& error(std::string text) &&;
Field&& error(Binding<std::string> text) &&;
Field&& target(std::string descendant_key) &&;
Field&& required(bool value=true) &&;
Field&& style(FieldStyle) &&;
Spec spec() &&;
```

State<string>& overloads for description/error delegate to Binding. Future example:

```cpp
ui::State<std::string> email{""};
ui::State<std::string> error{""};
auto field = ui::Field{"Email",ui::TextInput{"",email}}
    .description("For the receipt").error(error.binding()).required();
```

`target` names a stable application-defined retained key, never an address or index; when absent, select the first eligible focusable descendant or recognized control group.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

Field owns the label/Spec and value-form strings; Binding forms remain observed. It owns neither the control value nor a business validation rule. Required is a semantic/visual indication rather than a validator. An empty Error means none; a string change invalidates metrics and description only when the effective value changes.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

Clicking the label focuses the eligible target. For checkbox/toggle/radio, delegate Activate/Toggle/Select to the target only when the action is advertised and permitted; do not synthesize pointer events. No TextInput activation beyond focus. No Tab stop for a decorative label. Escape/Enter/wheel input remains with the control. Respect the target’s disabled/read-only state at request time.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Outside Form, stack the label/control/descriptions; within Form, place the label on the left according to the context and the control on the right. Use Form’s target baseline hook; otherwise center the label’s first line against the control height. Description and error sit below the control rather than the combined width. Long labels/errors wrap to the available width and do not override control minima.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

FieldStyle contains gaps and label/error/help typography and colors. An error is expressed through text and a visual indication rather than red alone; error decoration does not override the widget’s own styles. Removing an error requests layout and clearing the old text. No implicit blinking or global alert.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Composable name: if the control already has an explicit name, preserve it and give the label to the group; otherwise provide the label as the name. Append owned help+error text to the description without replacing an explicit description. Requires scoped semantic enrichment in Tree; no current native labelled-by relationship is claimed. Rich required/error semantics may require future neutral fields; fall back to a text description.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Resolve target through stable identity at click time rather than retaining Component*. Target removed/hidden/disabled: label clicking is a deterministic no-op; the next layout may resolve another default target. An expired help/error Binding becomes absent text without UAF. Release the subscription before teardown.

Because Binding does not notify State destruction, apply the “help/error absent after expiration” policy on the next safe measure/input/paint read, then invalidate if the displayed effect changes. Never dereference State or promise an immediate destruction notification.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

[Form](form.md), [Fieldset](fieldset.md), UI-thread focus/semantic actions and target scoped overrides. An empty Field or one without a focusable child remains legal presentation; a missing explicit target does not unexpectedly activate another control. No automatic field parsing/validation.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/field.hpp` and `src/field.cpp`.

field.hpp declares style/builder/text overloads; field.cpp contains target resolution, semantic assistance, baseline/layout/label input and text paint. Field remains a separate component from Form and Fieldset; no entirely inline implementation in a helper.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `field_label_focus`: the label targets the first control or explicit key.
- `field_checkbox_label_action`: one permitted action, zero when disabled/read-only.
- `field_existing_name`: preserve the explicit name and compose the description.
- `field_error_wrap`: a long error, empty error and removal without artifacts.
- `field_target_removed`: absent target is a no-op, with no stale address.
- `field_binding_expired`: help text remains safe after expiration.
- `field_reentrant_action_throw`: no automatic second activation.

Create the future public example `examples/features/field.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
