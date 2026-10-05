# Meter

**Status: existing — enhancements required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Display a bounded value and, as an extension, normal/warning/critical state according to two thresholds. Display only; no input or indeterminate progress.

Available in [widgets_progress_meter.inc](../include/nativeui/detail/widgets_progress_meter.inc) through Meter and BoundedDisplayComponent; same ranges/orientations/formatter as ProgressBar, with a tighter fill radius. Style in [progress_style.hpp](../include/nativeui/progress_style.hpp).

MyGo: `ui/indicators.go`, `Meter` and `MeterLevels`. Warning/Critical define high-is-bad or low-is-bad according to their order; equality disables thresholds. The target adds this behavior without importing Theme.Success/Warning/Danger colors absent from NativeUI ThemePalette.

## 2. Public API and composition

Exact current API: `Meter(State<float>&, float minimum=0, float maximum=1)`, rvalue `orientation(ProgressOrientation)`, `formatter(Formatter)`, `style(MeterStyle)`, and `spec() &&`. Formatter takes float and returns std::string.

Verified example using the existing API:

```cpp
ui::State<float> level{0.7f};
auto display = ui::Meter{level, 0.0f, 1.0f}
    .formatter([](float value) { return std::to_string(value); })
    .spec();
```

Target additions: `Meter(Binding<float>, float minimum=0, float maximum=1)`; `struct MeterLevels { double warning; double critical; }`; `levels(MeterLevels) &&`, `threshold_colors(Color warning, Color critical) &&`. Thresholds absent by default; defaults amber {1,0.65,0,1}, critical {0.85,0.15,0.15,1}. Existing API remains float.

## 3. State, ownership, and notifications

Current source borrows State<float>&, which must live until unmounting. The target immediately converts the State constructor to Binding, preserving signatures and removing the internal direct borrow.

Observation never rewrites the model: clamped view, non-finite=min. Removed Binding source becomes invalid, get retains the last value/set ignored/observe inactive without destruction notification; no Meter change callback.

Immutable threshold options apply to the effective displayed value rather than a raw out-of-range value. Normal/warning/critical are derived and do not form a second public State.

## 4. Interactions

No focus, capture, active pointer interaction, dragging, wheel, keyboard, or text input; input Ignored as today.

ReadOnly retains read-only styling and description. Disabled takes its disabled color even at a critical threshold; no implicit click-to-acknowledge alert.

Application confirmation/cancellation does not apply. An action such as loading/resetting a counter belongs to a neighboring Button.

## 5. Measurement and layout

Style preferred sizes preserve horizontal/vertical and formatted variants. Formatter is not called in measurement; no relayout at the rate of text updates.

Horizontal from the left, vertical from the bottom; historical fill radius preserved. Double fraction calculation prevents overflow from subtracting two finite floats.

Bounded view and zero dimensions: no fill outside the track. Thresholds change neither range nor length, only presentation/description; clipping follows bounds.

## 6. Presentation and invalidation

Without levels, current MeterStyle colors/precedence are unchanged. With levels and critical>warning, value>=critical is critical, then >=warning is warning; otherwise <=critical is critical, then <=warning is warning.

Equality warning==critical disables thresholds; normal=resolved MeterStyle.fill. Threshold colors replace only fill for warning/critical states after normal style; disabled style ultimately wins. Bounds/text/radius retain existing resolution.

No animation or implicitly audio-specific smoothing; updates affect paint/semantics. Formatter receives the effective float value. Level name is included in description without forcing visible text.

## 7. Accessibility

Target contract: Meter with a composition-supplied name and effective numeric_value/range; normal/warning/critical description when thresholds are enabled. No Action::SetValue.

Inclusive comparisons match pixels. External non-finite values display minimum, and the description may indicate an invalid source without announcing a reliable domain-specific state.

Meter role and hooks exist; actual current publication is not assumed. T068 bridges are deferred; no native support is already claimed.

## 8. Lifecycle and recovery

UI/main thread; per-instance RAII subscription and invalidator, disarmed at unmounting. No timer or global history/peak.

A reentrant/throwing formatter must keep paint scopes/dispatch guards balanced, and a started invocation is never replayed. Structural changes are deferred to the retained checkpoint.

Prepare range/thresholds/styles before publication. Destruction is no-throw with no application callback; a vanished Binding source causes neither UAF nor fictitious cancellation.

## 9. Dependencies and edge cases

Reuses domain/Theme/Painter and Binding; bounded helpers may be privately shared with ProgressBar, with each component retaining a real cpp.

Non-finite/reversed/equal min/max range causes invalid_argument at historical instantiation. Non-finite levels or levels outside [min,max] cause invalid_argument for the target addition; in-range equality is valid and disables thresholds.

Min==Max is not a zero counter: invalid domain as before. External NaN/inf/out-of-range values are never corrected. Empty formatting allowed; no sRGB color converted to a native integer in the API.

## 10. Files and compatibility

Target: `include/nativeui/meter.hpp` and `src/meter.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: `widgets_progress_meter.inc` and progress_style.hpp. Preserve MeterStyle/ProgressStylePatch and ProgressOrientation in their historical entry points. Extract the actual Meter core into meter.cpp without duplicate runtime in .inc.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `meter_legacy_display`: absent thresholds preserve pixels, orientation, and formatter.
- `meter_high_bad`: inclusive 80/95 thresholds determine warning/critical.
- `meter_low_bad`: inclusive 20/5 thresholds reverse logic without changing fraction.
- `meter_equal_invalid`: equality disables; non-finite/out-of-domain rejected before publication.
- `meter_disabled_priority`: Disabled style overrides critical fill and ReadOnly remains visible.
- `meter_formatter_throw`: an exception and the next value remain recoverable.

Create `examples/features/meter.cpp` and target `nativeui_example_meter`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Reuse `examples/features/t033_progress_meter.cpp` and existing wide-domain evidence. Add thresholds, snapshots, and Binding without removing float contracts.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.