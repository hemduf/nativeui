# NumberInput

Status: **new — implementation required**.

[Component catalog](widgets.md)

Sources studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Edit a bounded double using text and arrows, with a temporarily incomplete draft. The numeric
value never receives NaN or invalid text.

NativeUI has TextInput and the float SliderDomain, but no numeric field. Reuse the editing core
without directly connecting its string draft to State<double>.

MyGo: `ui/number.go`, `NumberInput`. It publishes as soon as input parses within the range, adds
Up/Down and buttons, then formats on loss of focus. The target retains live-valid behavior and
formalizes external conflicts.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class NumberInput {
public:
  NumberInput(std::string label, Binding<double> value);
  NumberInput(std::string label, State<double>& value);
  NumberInput&& range(double minimum, double maximum) &&;
  NumberInput&& step(double value) &&;
  NumberInput&& precision(unsigned digits) &&;
  NumberInput&& on_submit(std::function<void(double)> callback) &&;
  NumberInput&& style(NumberInputStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<double> copies{1.0};
auto count = ui::NumberInput("Copies", copies).range(1.0, 99.0).step(1.0)
    .precision(0).on_submit([](double) {}).spec();
```

Defaults: range[0,100], step=1, precision derived from the step's shortest decimal representation,
bounded to 17 digits. Explicit precision(0..17) overrides this calculation; out-of-range values
are rejected.

NumberInputStyle contains TextInputStyle, StepperStyle, gap, and invalid message/color. Subpart
styles belong in the NumberInput pair; arbitrary parsing callbacks are not required for v1.

Locale-independent parser: trim ASCII whitespace, accept a sign, decimal point, and exponent;
reject trailing bytes, NaN/Inf, and overflow. A comma does not represent a decimal separator.

## 3. State, ownership, and notifications

State<double> is the validated value; the string draft, validity, and focus_baseline are local. A
draft that parses as finite and in range publishes the entered value live without snapping to the
step; granularity does not impose rounding during typing.

Texts “”, “-”, and “1e” are incomplete drafts without mutation. An out-of-range value remains an
invalid draft and displays an error; blur restores the last valid value's representation.

A different external change while focused replaces the draft and baseline with its effective value
and ends local composition; it wins over an old draft. An identical publication from editing does
not reformat typing.

Enter validates a valid draft, updates the baseline, formats, and calls submit once; Escape restores
the baseline with a write if necessary. Arrows reread the current value, then snap/clamp to the grid
as Stepper does.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction,
Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and
observe() remains inactive. There is no implicit destruction notification: check valid at every
dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not
extend the lifetime of application models captured by a closure.

An external observation invalidates presentation without simulating a user gesture. State
notifications are synchronous: a stable snapshot, additions on the next pass, skipped removals,
and coalesced recursive writes. After an exception, the published value remains, notifications
for the rest of the pass stop, and dispatch must remain reusable.

## 4. Interactions

Text editing follows TextInput: selection, clipboard, draft undo/redo. Up/Down change the value by
step and reformat; Home/End remain text navigation without jumping to the bounds.

The adjacent Stepper handles plus/minus and its 400/80 ms repeat; clicking keeps the field focused
to continue typing. A single Tab stop on the editor, with stepper actions through keyboard/semantics.

Invalid/incomplete Enter retains focus and the error without submit; invalid blur discards the draft
and displays the last valid value. Escape cancels to the baseline; Stepper PointerCancel retains
live increments already published.

ReadOnly permits selection/copying without mutating parsing or arrows; disabled blocks editing. The
wheel is ignored to prevent accidental numeric changes during scrolling.

Normal committed text follows TextInput; the absence of a native preedit bridge is explicit. While
synthetic composition is active, parse only after Commit, not on each preedit Update.

## 5. Measurement and layout

The field takes TextInputStyle width, a compact Stepper width, and gap; height is shared. Preserve a
width independent of draft length to avoid movement while typing.

The invalid message may be exposed as a description and painted in space reserved by the style,
without collapsing the field; Form/Field handles an external error line if needed.

Caret/selection clip to the text area. All coordinates are logical; long scientific notation
scrolls horizontally.

## 6. Presentation and invalidation

Invalid validity affects border/text/message, but not the value presented to Stepper. A blank
placeholder and the number zero are distinct content.

Formatting is locale-independent and fixed to the chosen digits; presentation rounding does not
republish a rounded value on blur.

Draft changes require paint and scroll updates; geometry/style metrics require layout; Stepper
repaints its availability near the bounds. No validator/parser is called from paint.

## 7. Accessibility

Target: Custom with numeric_value/value_range and draft text_value, label as name, and
Increment/Decrement/SetValue actions when mutable.

SpinButton does not exist in the current SemanticRole; invalid/error is a backend-neutral description
until a dedicated field is added. Arrows do not duplicate Tab stops.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role
specified here is a target contract: its presence in the enum does not prove that the current
component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI result is claimed; verify the
headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Prepare parsing and representation before commit; release capture/context, then set. Copy the submit
callback with a stable double after local state is consistent, without accessing this after the call.

An observer may replace the value, remove the field, or throw. Retain the already committed value,
resume the draft at the next checkpoint; do not automatically republish the draft value after failure.

Check the clipboard response generation and valid status. Unmounting cancels draft/composition and
Stepper repetition without submit or destructive rollback.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released
per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before
publishing a value or calling the application. A callback that has started and throws is never
replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback
must go through a deferred safe point; synchronous owner destruction safety is not promised.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a
monotonic identity; after removal they become inert, without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: [TextInput](text_input.md), [Stepper](stepper.md), a private double domain,
TextService/State. Do not change the float SliderDomain for this addition.

Domain min<=max and finite step>0; min==max leaves a constant field effectively ReadOnly for numeric
changes. External NaN/inf renders as min with an invalid description, without automatic correction.

Doubles at extreme bounds, exponent overflow, subnormal step, and precision17 must be tested without
overflowing addition that produces NaN. User paste “1,2” remains invalid instead of silently becoming 12.

## 10. Files and compatibility

Target: `include/nativeui/number_input.hpp` and `src/number_input.cpp`. The header exposes public
declarations and only the necessary template adapters; the .cpp must contain a real retained core,
interactions, measurement, and rendering, never an empty file.

The header exposes NumberInput/NumberInputStyle and callbacks; the .cpp owns draft/parser/formatter,
synchronization, TextInput/Stepper composition, and rendering.

Text/stepper cores are shared through private interfaces without copying their implementation into
a NumberInput header.

Register `src/number_input.cpp` in NativeUI::Core during implementation. Preserve historical aggregate
includes as compatible entry points; no Pugl, Skia, OS, or plugin types belong in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed by
this documentation batch.

## 11. Tests and acceptance criteria

Tests are required during implementation; this documentation reports no execution results.

`number_input_parse`: signed/decimal/exponent accepted; empty/partial/comma/NaN/trailing/overflow
input without mutation.

`number_input_live_valid`: valid typing publishes without snapping; blur formatting does not change
the double.

`number_input_external_conflict`: an external write wins over an active draft and becomes the
Escape baseline.

`number_input_submit_blur`: invalid Enter stays in editing without submit; invalid blur restores
the last value.

`number_input_stepper`: increments snap/clamp, repeat, no wheel, and one Tab stop.

`number_input_composition`: Update does not parse, Commit parses once; stale clipboard is ignored.

`number_input_throw_remove`: an observer/submit removes/throws and the next action works.

Add `examples/features/number_input.cpp`, compilable by a public consumer, with a `--self-test` mode
verifying the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared with stable geometry,
two independent instances work, historical includes compile, and new sources are warning-free.
