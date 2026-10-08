# Form

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

Form organizes Field/Fieldset with aligned labels and explicit submit commands. NativeUI currently has no Form. Foundations: [layout.hpp](../include/nativeui/layout.hpp), [command.hpp](../include/nativeui/command.hpp).

MyGo `ui/form.go`: `Form`, `alignLabels`, `alignBaselines`. Labels are right-aligned in a shared column against the control’s first baseline. The target adopts the composition without inferring a global business model from descendants.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Proposed target API:

```cpp
enum class FormLayout { Aligned, Stacked, Responsive };
template<class... Children> explicit Form(Children&&... fields);
Form&& layout(FormLayout) &&;
Form&& stacked_below(double logical_width) &&;
Form&& on_submit(std::function<void()> callback) &&;
Form&& on_cancel(std::function<void()> callback) &&;
Form&& style(FormStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<std::string> name{"Ada"};
auto form = ui::Form{ui::Field{"Name",ui::TextInput{"",name}}}
    .layout(ui::FormLayout::Responsive).stacked_below(360.0);
```

Defaults: Aligned, with a 360 DIP responsive threshold when Responsive is selected. Target foundations: `ChildMetrics::first_baseline` optional<float> and `Component::first_baseline(Size measured_size) const` defaulting to nullopt; ChildMetrics currently contains no baseline.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

Form does not own field values or rewrite their bindings. It owns children, styles and callbacks. The lexical alignment context belongs to the retained Form, obtained through a scoped Tree service without a static “current form”. Fieldset participates in the same context; a nested Form opens another context.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

Tab follows controls; label clicking belongs to Field. An explicit submit command calls on_submit; Enter in TextArea remains text insertion rather than implicit submission. Escape calls on_cancel only if descendants have not consumed the event/command. No validation callback during paint or measurement. A Form without a callback allows commands to bubble.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

- In Aligned, label-column width = the maximum preferred label width among participating Field components.
- Align the label to the control’s first_baseline when available; otherwise center it using its first line/height according to Field.
- In Stacked, the label is above the control and label widths are not shared.
- Responsive chooses the mode according to allocated width without changing the model value or remounting controls.
- Empty labels create no text padding but retain the required alignment.
- Measurement uses bounded passes: collect label metrics, resolve width, measure controls and then place them.
- Errors/descriptions sit below the control and contribute to height; hidden/collapsed fields respect availability.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

FormStyle contains row gap/column gap and label alignment rather than control visual recipes. Label-width changes cause participant layout; an isolated Field color change requires only paint. Responsive switching clears old pixels without losing focus/text selection.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Form targets an optionally named Group; Field establishes control names/descriptions without replacing explicit names. Rich labelled-by/error relationships require future neutral extensions; v1 can compose owned name/description strings. No native mapping is claimed here.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Register Field with the context through a monotonic/weak handle and RAII. Measurement uses a snapshot of participants; reentrant removal exposes no invalidated reference. A baseline/measurement failure preserves the old layout and permits a new generation. Submit/cancel callbacks have no automatic retry.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on [Field](field.md), [Fieldset](fieldset.md), the target baseline layout and CommandScope. No automatic business-data storage/validation or network I/O. Threshold must be finite/non-negative; non-Field children are accepted as column children without label participation. An empty Form is legal.

Append `Command::Submit` and `Command::Cancel` to the portable enum, with routing tests and explicit mappings supplied by the application/adapter. They are absent from the current API; Enter is not automatically mapped to Submit. A multiline field retains newline insertion.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/form.hpp` and `src/form.cpp`.

form.hpp exposes FormLayout/style/child templates; form.cpp contains the scoped context, metric collection and responsive layout/submit routing. The baseline foundation in Component is an explicit interface change to make before implementing alignment; do not present it as already existing.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `form_label_column`: labels share the maximum width; nested Form is isolated.
- `form_baseline_fallback`: a real baseline or nullopt without incoherent placement.
- `form_responsive_focus`: resizing switches mode without remounting or losing selection.
- `form_submit_textarea`: multiline Enter is not captured by submit.
- `form_fieldset_alignment`: Fieldset participates in the current context.
- `form_participant_remove`: safe stale handle and correct subsequent layout.
- `form_measure_fault`: failure followed by recovery without a global registry.

Create the future public example `examples/features/form.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
