# RichText

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Compose a paragraph from owned fragments with fonts, colors, decorations, highlights, and inline actions. Text remains non-editable; rich editing and HTML markup are outside this component's scope.

Absent from the NativeUI public catalog. [text.hpp](../include/nativeui/text.hpp) exposes TextStyle and TextService but no public multi-run layout; [Label](label.md) is a simple display.

MyGo: `ui/richtext.go`, `Span`, `RichText`, `encodeSpans`, `spanPaint.runs`, `Painter.RichText`. Its fragments wrap as a paragraph; Link children can retain interaction on their words. Reproduce this behavior without importing the Go engine.

## 2. Public API and composition

Proposed target API:

```cpp
struct RichTextSpan {
    std::string id;
    std::string text;
    std::optional<TextStyle> style;
    std::optional<Color> background;
    bool underline{};
    bool strikethrough{};
    std::function<void()> on_activate;
};
class RichText {
public:
    explicit RichText(std::vector<RichTextSpan> spans);
    RichText&& style(TextStyle value) &&;
    RichText&& wrap(bool value = true) &&;
    Spec spec() &&;
};
```

A supplied span style replaces the entire base TextStyle for that run; absence inherits it in full. Decoration color equals text color. An id is required and unique only for an interactive span.

Proposed target example:

```cpp
auto paragraph = ui::RichText{std::vector<ui::RichTextSpan>{
    {.text = "Read "},
    {.id = "guide", .text = "the guide", .underline = true,
     .on_activate = [] {}},
    {.text = " before starting."}
}}.wrap().spec();
```

## 3. State, ownership, and notifications

The component owns fragments, styles, and callbacks. No pointer or string_view into the user model is retained. No text Binding or selection in this initial API.

Per-instance paragraph cache indexed by text/style/width; offsets refer to the owned repaired UTF-8 buffer. Interactive keys remain stable through reflow.

Activation does not write text. Snapshot the callback before invocation; the pressed state is terminal before user code runs.

## 4. Interactions

Interactive span: primary press/release in the same run region; capture, then cancellation on PointerCancel, disabling, or removal. A run spanning several lines accepts all its rectangles.

Tab/Shift+Tab traverse only inline actions. Enter/Space activate the focused action once; keyboard repeats do not duplicate a press.

Ordinary text ignores events and the wheel. No selection, clipboard, input, content dragging, or confirmation. Escape cancels the current press without triggering an action.

## 5. Measurement and layout

Measure a complete paragraph without naively adding independent measurements that would break ligatures, bidi, or clusters. Explicit line breaks separate paragraphs; wrapping is enabled by default and uses the constrained width.

Unbounded width: one line per paragraph. Zero width: no non-finite geometry or reflow loop. Overlong word: breaks are allowed at grapheme boundaries, never inside a UTF-8 sequence.

Rendering and hit testing consume the same layout result and run rectangles. Clip to retained bounds; all measurements and geometric offsets are logical.

## 6. Presentation and invalidation

Base TextStyle and complete overrides; backgrounds behind glyphs, underlines/strikethroughs based on font metrics rather than arbitrary per-span coordinates.

Fixed visual contract: an inline action is underlined when hovered/pressed, and its run rectangles receive a focus ring in the run's text color on keyboard focus. Preserve the explicit background. No RichTextStyle type or additional Theme slot in this v1.

Style or width change means reflow and repaint; action/id alone affects semantic structure and interactions. Painting remains pure and does not invoke action callbacks.

## 7. Accessibility

Target contract: Text for an ordinary paragraph and Button/Custom for an inline action according to the approved mapping. SemanticRole::Link does not exist in the reviewed version; do not publish it as if it did.

Action name equals span text; logical bounds are the union of its rectangles, with stable identity. Textual reading does not duplicate every run already described by the overall value.

The native T068 bridge is deferred. Non-editable text: no active IME; if selection/editing are added later, they must respect the committed/preedit separation in DESIGN17.4.

## 8. Lifecycle and recovery

UI/main thread; no subscriptions. Do not retain Painter, contexts, or retained nodes in the paragraph cache.

Prepare reflow and then publish it atomically. If it fails, keep the previous valid result; if no result exists, propagate the C++ exception after cleanup.

An action that removes its subtree or throws is never replayed. Release capture and clear the pressed flag before invocation; destruction is no-throw. Top-level UI destruction is deferred to a safe checkpoint.

## 9. Dependencies and edge cases

Structural dependency: add a private multi-run layout adapter for the existing text system within this file pair, with a private backend and a common measurement/painting contract; no new public Skia API.

Empty spans are ignored in layout but do not create invisible actions. Empty/duplicate interactive ids cause invalid_argument before publication. No implicit markup parsing.

Invalid UTF-8 is repaired identically in all runs; combining clusters crossing a run boundary remain geometrically consistent. Removing an active span cancels its action; a new id does not reuse its focus.

## 10. Files and compatibility

Target: `include/nativeui/rich_text.hpp` and `src/rich_text.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: existing TextService and Painter. `RichTextSpan` and the cache must not be added to widgets_basic.inc; the component uses existing services and a non-template core.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `rich_text_mixed_runs`: styles, background, and decorations follow each fragment.
- `rich_text_unicode_reflow`: bidi, emoji, and clusters remain intact at reduced width.
- `rich_text_inline_hit`: an action spanning two lines hit-tests only its own rectangles.
- `rich_text_remove_pressed`: removing the id during a press suppresses activation.
- `rich_text_callback_throw`: after a reentrant action throws, the next works without replaying the first.

Create `examples/features/rich_text.cpp` and target `nativeui_example_rich_text`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery from the failures described above under ASan/UBSan when lifetime is involved.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.