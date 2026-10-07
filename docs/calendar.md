# Calendar

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A monthly Gregorian calendar selecting an optional civil date, without time or time zone. Reusable inline and in DateInput.

Absent from NativeUI; the [State/Binding](../include/nativeui/state.hpp), focus, and Component foundations exist. No native calendar service is imported.

MyGo: `ui/date.go`, `Calendar`, `calendarGrid`, `setDay`, and `sameDay`. Displays six weeks starting Monday; arrows change inline selection, Home/End navigate the month, and PageUp/PageDown change the displayed month. MyGo retains the time/time zone of time.Time; the target deliberately uses a civil date.

## 2. Public API and composition

Proposed target API:

```cpp
class Calendar {
public:
    using Value = std::optional<std::chrono::sys_days>;
    Calendar(std::string label, Binding<Value> selection);
    Calendar(std::string label, State<Value>& selection);
    Calendar&& range(Value minimum, Value maximum) &&;
    Calendar&& reference_day(std::chrono::sys_days value) &&;
    Calendar&& today(Value value) &&;
    Calendar&& commit_on_navigation(bool value = true) &&;
    Calendar&& on_change(std::function<void(Value)> callback) &&;
    Calendar&& style(CalendarStyle value) &&;
    Spec spec() &&;
};
```

Defaults: Monday first, six rows of seven days, reference_day = sys_days{} (1970-01-01), no today, open bounds, and commit_on_navigation=true. CalendarStyle describes cell sizes, text, selected/cursor/today colors, and spacing.

Proposed target example:

```cpp
ui::State<ui::Calendar::Value> selected{std::nullopt};
auto picker = ui::Calendar{"Day", selected}
    .reference_day(std::chrono::sys_days{
        std::chrono::year{2026}/10/4})
    .spec();
```

## 3. State, ownership, and notifications

The selection model is external; the Binding is copied, and the State overload is converted to Binding in the constructor. Source destruction: Binding invalid, last value still readable, set ignored, and observe inactive; no automatic notification. Revalidate valid() before editing, then do not notify on_change if mutation is impossible. Cursor/displayed month are private to the instance.

on_change belongs to user commits that change the effective value; an external write updates cursor/month and paint but does not call this callback. No model normalization on mount.

commit_on_navigation=false keeps the cursor separate; click or Enter commits the cursor. A new external value replaces the draft and its month; no old movement rewrites this update.

## 4. Interactions

One Tab stop on the grid, arrows +/-1 and +/-7 days; Home/End = first/last day of the month, PageUp/PageDown = previous/next month with the day clamped to the target month. Navigation saturates at selectable bounds.

PointerDown/Up on a valid cell commits once; cancellation/removal before Up chooses nothing. Month buttons are keyboard accessible; Enter/Space choose the cursor in draft mode.

No wheel or implicit scrolling. ReadOnly allows reading navigation but no commit. Escape abandons a local press; in Popover, the parent handles closing. Disabled does not take focus.

## 5. Measurement and layout

Fixed grid of seven columns and six weeks, without height changes by month. All visible days outside the month are selectable if within bounds; their keys are sys_days dates, rather than cell indices.

Measurement = header/navigation + weekday row + six rows + gaps/padding, in logical units. A narrow width bounds cells and clips without overlapping month buttons.

On resize, cell rectangles and hit-testing share the same layout. Unbounded constraints use style sizes; a zero-sized view allows no hit or division by zero.

## 6. Presentation and invalidation

New CalendarStyle, with defaults resolved from existing palette/typography/spacing. Selection, cursor, today marker, days outside the month, and unavailable days are visually distinct.

External date and cursor movement = paint/semantics; month change = content and semantics; style metrics = layout + paint. Transitions require no animation.

The injected today marker does not affect selection. Text defaults to English (months/weekdays), fixed for v1; do not introduce a global locale service.

## 7. Accessibility

Target contract: Group named by label, Button cells named by full date, selected and enabled according to choice/bounds. The grid retains focus and exposes its cursor through snapshot/description; Role::Table does not currently exist.

Cell Activate/Select actions and month button Activate. Do not claim an already-available native activeDescendant action; logical date identity remains testable.

Backend-neutral hooks are available; T068 bridges are deferred. No text editor/IME in Calendar.

## 8. Lifecycle and recovery

UI/main thread; RAII selection subscription and lifetime-safe invalidators. On unmounting, cancel press/capture and discard the local cursor without writing.

Prepare cells/layout/snapshot before publication; a construction error retains neither half a month nor an orphan subscription.

Before on_change, finish the logical mutation, then snapshot callback/value. Reentrancy that removes the component or changes selection causes neither a second commit nor replay after an exception. No-throw destruction; deferred top-level destruction.

## 9. Dependencies and edge cases

Depends on Button, TextService, Binding, focus, and chrono. Key::PageUp/PageDown are absent from the studied input.hpp: an additive extension at the enum's end is required, preserving historical values, with Pugl/platform normalization and tests at the src backend boundary, never in Calendar. [DateInput](date_input.md) composes this core with commit_on_navigation=false; it does not duplicate calendar rules.

Minimum > maximum bounds = invalid_argument before mount. An external out-of-bounds value is displayed as unavailable; cursor clamped, model intact. min==max gives one selectable day.

Representable year: Gregorian chrono year 1..9999 for v1 display; bounds/reference outside the domain are rejected; an external out-of-domain value displays invalid absence without conversion overflow. Absence starts at reference; no implicit system date.

## 10. Files and compatibility

Target: `include/nativeui/calendar.hpp` and `src/calendar.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Origin to extract or reuse: existing composition/focus/Binding and C++20 chrono. CalendarStyle/Value stay in the pair; DateInput reuses the grid core without a public backend template.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, without an empty file or central widget switch. No Pugl, Skia, OS, or plugin SDK types belong in this API.

This delivery is documentation only: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests required during implementation:

- `calendar_leap_month`: February 2024/2100 and month-end transitions follow the calendar.
- `calendar_cursor_commit`: inline and draft modes have distinct notifications.
- `calendar_range_identity`: equal/out-of-range bounds and date keys remain consistent.
- `calendar_external_draft`: an external write replaces the cursor without a user callback.
- `calendar_remove_cell`: month change/removal during a press does not choose a different cell.
- `calendar_throw_recover`: a reentrant callback that throws leaves another choice possible.

Create `examples/features/calendar.cpp` and the `nativeui_example_calendar` target, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery after the faults described above under ASan/UBSan where lifetime is involved.

Acceptance: the named tests pass, no capture/registration remains after unmounting, and the published API matches these contracts. Verification performed here: reading declarations and sources; no C++ or interactive tests executed.
