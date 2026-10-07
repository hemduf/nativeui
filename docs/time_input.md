# TimeInput

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Optional civil time of day as HH:MM segments, with optional seconds. No day, time zone, timestamp, or DST handling.

Absent from NativeUI; committed Input paths and the State model are available. The component handles digits and segments without a rich editing engine.

MyGo: `ui/timeinput.go`, `TimeInput`, hours/minutes segments. Up/down wrap each segment, digits form two-digit values, and left/right move focus. MyGo retains date and seconds/time zone; the target retains seconds as an integer number of seconds since midnight.

## 2. Public API and composition

Proposed target API:

```cpp
class TimeInput {
public:
    using Value = std::optional<std::chrono::seconds>;
    TimeInput(std::string label, Binding<Value> value);
    TimeInput(std::string label, State<Value>& value);
    TimeInput&& show_seconds(bool value = true) &&;
    TimeInput&& clearable(bool value = true) &&;
    TimeInput&& on_change(std::function<void(Value)> callback) &&;
    TimeInput&& style(TimeInputStyle value) &&;
    Spec spec() &&;
};
```

Defaults: 24-hour format, HH:MM, hidden but preserved seconds, absent value allowed, clearable=true. Domain of every valid value: seconds{0}..seconds{86399}. TimeInputStyle describes text, separators, selection/focus, padding, and segment widths.

Proposed target example:

```cpp
ui::State<ui::TimeInput::Value> alarm{
    std::chrono::seconds{9 * 3600 + 30 * 60}};
auto field = ui::TimeInput{"Alarm", alarm}
    .show_seconds(false).spec();
```

## 3. State, ownership, and notifications

Binding carries the integer value; the State overload is converted immediately. Source destruction: valid=false, get returns the last value, set ignored/observe inactive without automatic notification; revalidate before edit and do not notify on_change. The active segment and 1–2 digit buffer belong to the instance. No global digit timer.

Every accepted edit updates the complete value, then on_change once if actually changed. Focus and segment movement do not modify the model. External changes replace segments/buffer without on_change.

Absence is not midnight: display --:--. The first modification starts from 00:00:00, then applies the digit or step; clear resets nullopt. Hiding seconds never rounds them.

## 4. Interactions

Click chooses the segment, Tab/Shift+Tab traverse segments then neighboring controls. Left/right move to the adjacent segment without focus wrapping. Up/down increment/decrement with independent modulo 24 or 60, without carrying between segments.

Committed ASCII digits: the first digit starts the buffer; the second validates if within the domain, otherwise it becomes a new first digit. After two digits, or a digit that cannot start a valid number, advance to the next segment; remain at the last one.

Enter validates the local buffer and retains the model; Escape discards only an incomplete buffer, not values already published. Wheel/drag ignored. ReadOnly reads/navigates but consumes edits without writing; Disabled is not focusable.

## 5. Measurement and layout

Each segment retains a width of at most two digits, measured with TextService, padding, and noninteractive separators. Minimize 09→10 jumps; width does not follow the buffer.

Bounded space: clip segments within bounds without overlapping clear. Zero space prevents clicks and produces no division.

Logical coordinates; focus and segment rectangles come from the same layout. Show_seconds changes structure/focus at the checkpoint, never during active traversal.

## 6. Presentation and invalidation

New TimeInputStyle: group chrome, inactive text, selected segment, separators, focus ring, and disabled/read-only states. Label name independent of representation.

External change/digit = paint/semantics. Buffer-only editing = segment paint. Show_seconds and style metrics = structure/layout. Do not require an absent font-features API; choose a deterministic test font.

Segments require no blinking cursor or continuous animation/timer. An externally invalid value gives an invalid --:-- presentation and description without writeback.

## 7. Accessibility

Target contract: Group and two/three Custom children named “hours”, “minutes”, “seconds”, with numeric_value/range and Increment/Decrement/SetValue/Focus actions.

SemanticRole::Stepper is absent; do not claim it is delivered. Segment SetValue contract bounded to 0..23/59, without modifying other segments or nonexistent date/time zone.

Committed TextInput is available; preedit/candidate rectangles are not delivered under DESIGN17.4. Snapshots are testable, but T068 native bridges remain deferred.

## 8. Lifecycle and recovery

UI/main thread; RAII subscription and buffer state reset on unmount/loss of focus. Cancel native text input on unmounting if the segment activates it.

Before Binding.set, snapshot value/options; reentrant callbacks can replace model/visibility, so revalidate lifetime before moving focus. A started on_change is never replayed.

An exception restores guards/segment to a consistent state without automatically completing digits. No-throw, callback-silent destruction. No global timer/read-only state or buffer shared between instances.

## 9. Dependencies and edge cases

Depends on chrono, Binding, committed Input, Focus, and TextService; no native dialog or system locale parser.

Negative external value or >=86400: domain diagnostic and invalid absence display, source intact. A new edit starts from safe zero. Fractional seconds are absent by choice of the seconds type.

An external change during buffering overwrites the buffer, even if caused by Binding normalization. Arbitrary clipboard text and AM/PM are not accepted in v1; only committed digits are edits.

## 10. Files and compatibility

Target: `include/nativeui/time_input.hpp` and `src/time_input.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Origin to extract or reuse: existing Input/State/Focus/TextService. Value is optional<chrono::seconds>, never time_point; segmentation helpers remain in the non-template pair.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, without an empty file or central widget switch. No Pugl, Skia, OS, or plugin SDK types belong in this API.

This delivery is documentation only: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests required during implementation:

- `time_input_segment_wrap`: 23→00 and 59→00 do not modify other segments.
- `time_input_digits`: 0/9/2/4 and 59/60 follow buffer rules exactly.
- `time_input_hidden_seconds`: modifying HH:MM preserves hidden seconds.
- `time_input_absent_invalid`: absence and out-of-domain values are distinct from midnight.
- `time_input_external_focus`: external changes and removal of the active segment clear buffer/focus correctly.
- `time_input_throw_recover`: a throwing observer does not duplicate the edit and allows the next digit.

Create `examples/features/time_input.cpp` and the `nativeui_example_time_input` target, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery after the faults described above under ASan/UBSan where lifetime is involved.

Acceptance: the named tests pass, no capture/registration remains after unmounting, and the published API matches these contracts. Verification performed here: reading declarations and sources; no C++ or interactive tests executed.
