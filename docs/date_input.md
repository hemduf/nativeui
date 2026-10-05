# DateInput

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A compact optional civil date field displaying ISO YYYY-MM-DD and opening a Calendar. This first port follows the MyGo picker; it is not a freeform date editor.

Absent from the NativeUI toolkit. [Overlay](../include/nativeui/overlay.hpp), focus, and buttons exist; [Calendar](calendar.md) and [Popover](popover.md) are target components.

MyGo: `ui/date.go`, `DateInput`, and `calendarGrid` in moveChooses=false mode. Click/Enter/Space open it, navigation changes the cursor, click/Enter choose and return focus to the field. MyGo retains time/time zone; the target does not carry them.

## 2. Public API and composition

Proposed target API:

```cpp
class DateInput {
public:
    using Value = std::optional<std::chrono::sys_days>;
    DateInput(std::string label, Binding<Value> value);
    DateInput(std::string label, State<Value>& value);
    DateInput&& range(Value minimum, Value maximum) &&;
    DateInput&& reference_day(std::chrono::sys_days value) &&;
    DateInput&& placeholder(std::string value) &&;
    DateInput&& clearable(bool value = true) &&;
    DateInput&& on_change(std::function<void(Value)> callback) &&;
    DateInput&& style(DateInputStyle value) &&;
    Spec spec() &&;
};
```

Defaults: absence allowed, “Choose a date” placeholder, clearable=true, open bounds, reference_day=sys_days{}. DateInputStyle covers chrome, text, and preferred width; CalendarStyle is inherited in a consistent internal configuration.

Proposed target example: `ui::DateInput{"Deadline", deadline}.clearable().spec()`; deadline is a State<Value> converted to Binding on construction; its subsequent destruction retains the last snapshot without possible mutation.

## 3. State, ownership, and notifications

External Value through Binding; the State overload is converted immediately. Source destruction: Binding invalid, get returns the last value, set ignored/observe inactive without automatic notification. Revalidate before edit/open and close the popup at the next access checkpoint if unavailable, without on_change. Open, cursor, and overlay handle are private. On opening, copy the valid value or bounded reference_day into the cursor without writing Value.

A valid choice calls Binding.set, then on_change once if the accepted value differs. Clear chooses nullopt through the same path. An external update while open replaces selection/cursor without on_change.

Closing without choosing preserves the current external value. Avoid a second public State for open; the parent reuses only the lifetime-safe Overlay handle.

## 4. Interactions

Trigger: completed primary click, Enter, and Space open/close once per press; Tab enters normal navigation. Calendar receives focus at the cursor on opening.

Popup: arrows/Home/End/PageUp/PageDown move the draft; Enter/Space or click commit and close. Escape and outside click close without commit. Clear is a real named action, disabled if the value is absent/ReadOnly.

ReadOnly allows neither mutating opening nor clear; the value remains readable. PointerCancel cancels a pressed trigger. The wheel is ignored; no TextInput parsing in v1.

## 5. Measurement and layout

Measure the trigger by ISO text or placeholder, icon and clear, and style preferred width. The grid retains its natural width: do not force it to match a very wide field.

Popup anchored by NodeId, Overlay service Auto placement, viewport bounds. Calendar header and grid bound their content; the parent does not clone placement policy.

Resize/DPR/parent scroll reevaluate the anchor. A removed or unavailable anchor closes it; no centered fallback for an orphan date picker.

## 6. Presentation and invalidation

New DateInputStyle: surface/border/text/placeholder/icon/focus and metrics. Render dates with tabular digits if the font allows, without a font-features API that does not exist in TextStyle.

Binding change = text + paint/semantics; placeholder/date transition that can change size = layout. Popup open = Overlay structure invalidation.

The application label serves as the name, not necessarily the button text. No animation required. Draw the icon with primitives/IconView, without a hardcoded resource path.

## 7. Accessibility

Target contract: Button with label as name, ISO text_value or absence, expanded according to popup, Activate/Focus. Do not claim Role::DateInput exists.

Calendar exposes cells/buttons; focus returns to a still-valid anchor after closing. Clear action named “Clear date”. An invalid value is explained in the description without misrepresenting the model.

T068 bridges are deferred. No text entry or preedit at this stage; future text editing depends on DESIGN17.4 and does not change chrono choices.

## 8. Lifecycle and recovery

UI/main thread; RAII subscription, open handle, and popup callbacks protected by token/generation. Idempotent closing and unmounting remove only this instance's overlay.

Prepare content before opening; creation/scheduling failure leaves the field closed and retryable. No capture/focus restoration to a stale NodeId.

Commit/clear action is terminal before on_change; a callback opening another popup does not close that new popup. An exception never replays the notification; no-throw destruction, deferred top-level UI destruction.

## 9. Dependencies and edge cases

Depends on [Calendar](calendar.md), [Popover](popover.md), Button, chrono, and Binding. PageUp/PageDown navigation requires the Key additions and backend normalization described by Calendar; no Pugl specifics in the widget. Same 1..9999 domain and inclusive bounds as Calendar.

An absent date uses the placeholder; an external out-of-domain/out-of-range value is displayed as invalid without correction. Opening chooses a bounded reference, rather than a hidden current day.

Inconsistent bounds are rejected before mount. Removing the model while the popup is open is safe for Binding; content becomes nonmutating on revalidation without a fictitious change notification. An external update at the last moment takes precedence over a stale draft.

## 10. Files and compatibility

Target: `include/nativeui/date_input.hpp` and `src/date_input.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Origin to extract or reuse: Button/Overlay and future Calendar. Value type identical to Calendar; no native date wrapper, implicit locale parsing, or new popup stack.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, without an empty file or central widget switch. No Pugl, Skia, OS, or plugin SDK types belong in this API.

This delivery is documentation only: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests required during implementation:

- `date_input_open_cancel`: navigation then Escape/outside do not modify the date.
- `date_input_choose_clear`: choice/clear write once and return focus to the anchor.
- `date_input_external_open`: an external update while open replaces the draft without on_change.
- `date_input_anchor_remove`: removal/Hidden/Disabled during popup removes every handle.
- `date_input_invalid_date`: absence and out-of-domain/out-of-range values stay readable without correction.
- `date_input_reentrant_popup`: a callback opening something else or throwing leaves the service usable.

Create `examples/features/date_input.cpp` and the `nativeui_example_date_input` target, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery after the faults described above under ASan/UBSan where lifetime is involved.

Acceptance: the named tests pass, no capture/registration remains after unmounting, and the published API matches these contracts. Verification performed here: reading declarations and sources; no C++ or interactive tests executed.
