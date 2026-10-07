# TokenField

Status: **new — implementation required**.

[Component catalog](widgets.md)

Sources studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Edit an ordered list of tags/recipients as chips and a draft field, with optional suggestions.

NativeUI has TextInput, layouts, and State<vector<T>>, but no TokenField. Chips and remove buttons
remain subparts of one component, rather than an additional family of files.

MyGo: `ui/combobox.go`, `TokenField`. Enter/comma add text, Backspace with an empty draft removes the
last token, suggestions exclude existing tokens, and the input retains its identity despite
preceding chips.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class TokenField {
public:
  TokenField(std::string label, Binding<std::vector<std::string>> tokens,
             std::vector<std::string> suggestions = {});
  TokenField(std::string label, State<std::vector<std::string>>& tokens,
             std::vector<std::string> suggestions = {});
  TokenField&& allow_custom(bool value = true) &&;
  TokenField&& maximum_tokens(std::size_t value) &&;
  TokenField&& placeholder(std::string value) &&;
  TokenField&& style(TokenFieldStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<std::vector<std::string>> tags{std::vector<std::string>{"Audio"}};
auto field = ui::TokenField("Tags", tags, {"Audio", "Synthesis", "Effects"})
    .allow_custom().maximum_tokens(20).spec();
```

Defaults: allow_custom=true, maximum_tokens=0 meaning unlimited. The suggestions vector is owned;
remote/asynchronous models are outside v1; reconstruct the source through Spec.

Target TokenFieldStyle: TextInputStyle, chip text/background/border/padding, remove ButtonStyle,
gaps, and suggestion MenuItemStyle. The v1 addition separator is an ASCII comma, without implicit
CSV parsing/quoting.

Token order is vector order. No additional on_change: every operation publishes a complete vector
through the Binding.

## 3. State, ownership, and notifications

Binding<vector<string>> is the list; draft, popup generation, and active chip are local. The editor
is a child with a fixed identity independent of token count/order.

Adding trims ASCII whitespace and rejects empty text and exact duplicates; preserve casing/Unicode.
allow_custom=false requires an exact suggestion match; reaching the maximum refuses addition without
clearing the draft.

A paste operation containing commas prepares the entire new list and publishes one value: complete
segments are added, the final segment remains draft. Duplicates/empty segments are ignored; tokens
exceeding the maximum remain in the draft joined with commas.

An external token write does not destroy the draft, but recalculates suggestions and cancels an
armed removal/choice from the old generation. External duplicate values are rendered without silent
correction, with local occurrence identities.

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

Enter on a suggestion adds it; otherwise it adds the draft if allowed, then clears the draft on
successful addition or an already-present duplicate. Comma commits complete segments; default
clipboard behavior follows single-line TextInput.

Backspace with an empty draft immediately removes the last token; Delete removes the active chip.
For keyboard accessibility, Left at the draft's beginning can activate the last chip, then
Left/Right traverse chips and draft; Escape returns to the draft without mutation.

The remove button acts on click release and retains draft focus. Buttons do not create a Tab stop
per chip: TokenField has one stop, with internal arrow navigation.

Suggestions filter as in Autocomplete and exclude existing tokens; Up/Down/Enter, Tab/blur/Escape,
and PointerCancel follow the suggestion engine. The wheel only scrolls the panel.

ReadOnly forbids addition/removal but exposes text and the list; disabled blocks actions.
Composition Update does not interpret a preedit comma: split only on Commit.

## 5. Measurement and layout

Wrapping layout of chips and editor, with a minimum draft width; when space runs out, move an entire
chip to the next line and increase field height.

An overlong chip takes the available width, ellipsizes text, and keeps remove within its area; all
positions are logical. The popup anchors to the whole frame rather than only the last small text area.

Token addition/removal invalidates layout; the draft at a stable minimum size needs paint/scroll,
without remounting the editor. Parent scrolling handles a very tall list, with no claimed
virtualization.

## 6. Presentation and invalidation

Each chip has a distinct face and label; the active chip carries a local ring/selection, and remove
has a hover affordance. The textual value is not the identity of a mutable Node.

The parent focus ring includes the field; the chip's internal shape must not produce a second
simultaneous focus. External data is taken at the layout snapshot.

No continuous animation or drag reordering in v1. Color/font/padding change invalidation according
to their metrics.

## 7. Accessibility

Target: a named Group, TextInput/ComboBox editor, and ListItem/Text chips with Remove buttons named
“Remove <token>”. Semantic order follows the list, with no mutating action in ReadOnly.

There is no TokenField role; chips remain in the backend-neutral model. The single input retains
its identity even if the list becomes empty.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role
specified here is a target contract: its presence in the enum does not prove that the current
component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI result is claimed; verify the
headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Prepare a newly owned vector, end gesture/capture and update draft state, then Binding.set. Do not
retain a pointer to a string in an old vector in the remove callback.

Removal of an external duplicate targets its occurrence and generation; if the model changes,
cancel the action instead of removing a token with the same text but a new position.

An observer exception leaves the vector committed and the editor usable; the draft is available
at the checkpoint, without replaying an automatic addition. A throwing chip mount does not publish
a partial list.

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

Dependencies: [TextInput](text_input.md), [Autocomplete](autocomplete.md), a private wrapping layout
core in token_field.cpp, Overlay, and State.

External empty/duplicate tokens are faithfully displayed; removal is allowed by occurrence identity.
User additions do not create new empty/duplicate tokens. A new maximum below the existing size does
not truncate the model.

No suggestions with allow_custom=false makes addition unavailable, but removal remains possible.
Trimming/equality are explicit; no hidden email/network normalizer.

## 10. Files and compatibility

Target: `include/nativeui/token_field.hpp` and `src/token_field.cpp`. The header exposes public
declarations and only the necessary template adapters; the .cpp must contain a real retained core,
interactions, measurement, and rendering, never an empty file.

TokenFieldStyle, the private chip model, and options remain in the pair; the .cpp contains the
retained keyed list, wrapping, draft, vector operations, and popup through the shared suggestion core.

Register `src/token_field.cpp` in NativeUI::Core during implementation. Preserve historical aggregate
includes as compatible entry points; no Pugl, Skia, OS, or plugin types belong in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed by
this documentation batch.

## 11. Tests and acceptance criteria

Tests are required during implementation; this documentation reports no execution results.

`token_field_add_batch`: Enter/comma/paste produce trimmed tokens without duplicates and one batch
notification.

`token_field_limits`: maximum/custom restrictions refuse without draft loss or external truncation.

`token_field_remove_keyboard`: correct remove, empty Backspace, chip Left/Right/Delete, and Escape.

`token_field_input_identity`: addition/removal/reordering retain the same editor/caret; suggestions
exclude tokens.

`token_field_external_duplicates`: faithful duplicate snapshot rendering; the armed occurrence is
canceled after model change.

`token_field_composition`: a preedit comma adds nothing; Commit performs one operation.

`token_field_throw_recovery`: an observer, chip mount, or callback removing the field keeps the
list/flags consistent.

Add `examples/features/token_field.cpp`, compilable by a public consumer, with a `--self-test` mode
verifying the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared with stable geometry,
two independent instances work, historical includes compile, and new sources are warning-free.
