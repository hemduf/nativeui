# NativeUI keyboard routing on macOS

NativeUI's embedded Cocoa editors dispatch keyboard input to their retained UI
first. Events that the UI ignores continue through the native responder chain
to the host. This applies to every DAW and every format using the shared NativeUI
editor; Space is not hard-coded as a transport command.

Text input enables Cocoa's input context only while a text editor is active.
Typing, marked-text composition and editing commands remain local when consumed.
Press/release ownership prevents a handled press from leaking its release to the
DAW. Unhandled events retain their original key code, modifiers and repeat flag.
Embedded editors also leave Command-Q to the host instead of closing themselves.

`performKeyEquivalent:` reports consumption to AppKit without forwarding an
unhandled event immediately. A subsequent `keyDown:` for the same native event
does not dispatch into the UI again. Only the focused instance claims key
equivalents, so AppKit's traversal cannot activate another plugin's controls.

The implementation follows the routing principles in JUCE's
[Cocoa peer](https://github.com/juce-framework/JUCE/blob/master/modules/juce_gui_basics/native/juce_NSViewComponentPeer_mac.mm)
and its [key-equivalent correction](https://github.com/juce-framework/JUCE/commit/55d1585445c7ff56bedc702054aed3e7c0b89617).
It does not add JUCE as a dependency. For Ableton's process only, a rejected
keyDown temporarily selects the host parent as first responder, following the
[JUCE AU workaround](https://github.com/juce-framework/JUCE/blob/master/modules/juce_audio_plugin_client/juce_audio_plugin_client_AU_1.mm).
This fallback is never executed during a key-equivalent probe, consistent with
JUCE's [2025 crash fix](https://github.com/juce-framework/JUCE/commit/7f4176e259fa27fe8751ee5d553039a9e250182d).
Forwarding is guarded against recursion; focus restoration checks the surviving
attachment, visibility and responder after the host callback.
The temporary Live handoff preserves the retained control's focus. Real host
focus changes, hiding and detachment still update NativeUI normally.

The Cocoa backend also guards text commits received outside keyboard events,
where querying `keyCode` would raise an AppKit exception. A view closed from a
keyboard callback retires its borrowed Pugl pointer before any further dispatch.

NativeUI consumes an exact Pugl source commit. Cocoa focus/lifetime/text safety
is implemented in Pugl itself, without source rewriting during configuration.
Configuration upgrades known defaults cached by older NativeUI versions. Custom
Pugl revisions and source overrides remain selectable; an incompatible Cocoa
backend fails configuration with guidance to update the selected source.
The bridge installs overrides only on each consumer-scoped Pugl view subclass;
it never changes host or AppKit classes. All bookkeeping is per view and confined
to the host UI/main thread. If a retained keyboard/text callback throws after a
local effect, the native boundary reports the event as consumed and follows the
existing view-close policy, preventing a second DAW action.

Focus transitions also prepare complete layout geometry before publishing a new
focus owner or refreshing its text-input area. Failed layout leaves the transition
pending; teardown or unavailable targets retire it safely. Availability and scope
reconciliation stop when layout callbacks deactivate the retained tree.

The Cocoa regression tests run the same native event-delivery contract in a
generic host fixture and a test-only `com.ableton.live` bundle. They cover ignored
and consumed keys, press/release ownership, repeats, modifiers, key equivalents,
text editing, simulated marked-text composition, independent instances,
visibility, reparenting, local/host callback closure, and a local effect followed
by an exception. WindowServer access is required; display-less runs skip with 77.

```sh
CMAKE_BUILD_PARALLEL_LEVEL=1 cmake --build build --target nativeui_embedded_keyboard_tests
ctest --test-dir build -R nativeui_embedded_keyboard --output-on-failure
```

These are Cocoa host fixtures. Actual DAW transport/menu behavior still needs a
rebuilt consumer and a check in that DAW. No Windows/Linux keyboard behavior is
claimed from these macOS tests.
