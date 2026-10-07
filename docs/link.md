# Link

**Status: new — implementation required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Present text that triggers application navigation. The component does not open a browser or automatically interpret routes.

NativeUI has TextService and Button but no public Link in [widgets.hpp](../include/nativeui/widgets.hpp). Reuse text and activation foundations rather than porting the MyGo engine.

MyGo: `ui/widgets.go`, function `Link`; it opens a URL or pushes a path into Router, underlines on hover, and integrates into rich text. NativeUI target: owned destination and an injected callback; the application decides the URL, route, and validation.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class Link {
public:
  using NavigateCallback = std::function<void(const std::string&)>;
  Link(std::string label, std::string destination, NavigateCallback navigate);
  Link&& style(LinkStyle value) &&;
  Link&& wrap(bool value = true) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
auto help = ui::Link("Help", "/help", [](const std::string&) {}).spec();
```

Target LinkStyle contains TextStyle, normal/hovered/disabled colors, underlining at rest and on hover, and a focus metric. wrap(false) is the default; no new Theme slot is assumed.

For a span in RichText, the run engine must consume equivalent destination and action data; this page does not require a separate retained component for each word.

## 3. State, ownership, and notifications

Text, destination, callback, and style are copied/moved into the Spec. Hover, capture, and focus are local; no address of temporary text is retained.

Changing destination through compatible reconstruction updates the callback at the checkpoint. An already armed interaction is cancelled if its link identity is replaced.

The component keeps no history and observes no global route. The callback receives a destination snapshot even if the action rebuilds the page.

## 4. Interactions

Primary click: activate on release inside after capture. Leaving bounds or PointerCancel cancel; the wheel bubbles to the parent.

Tab can assign focus; Enter activates on the first KeyDown, with repeats suppressed until KeyUp. Space uses activation on release to remain keyboard-accessible without concurrent scrolling.

Disabled makes the action unavailable; ReadOnly does not prohibit navigation that leaves the link's value unchanged. No URL menu, download, automatic visit, or system gesture is added.

Text selection belongs to RichText; standalone Link is an action region and does not select its label on drag.

## 5. Measurement and layout

Without wrap, intrinsic text width and line height, including ring space. With wrap, measure through TextService within the allocated width, without converting logical coordinates to pixels in the widget.

A narrow parent clips or wraps according to wrap. Hit testing follows the component's allocated box; RichText fragments have their own geometry in the run engine.

Changing font, text, or width with wrap enabled invalidates layout; destination alone does not change geometry.

## 6. Presentation and invalidation

Theme action color and underline on hover; visible ring for keyboard focus. Visited state is not implicitly recorded: an application may provide a suitable style.

Paint the underline using text metrics, without replacing content with a rectangular button. Color/hover trigger paint alone.

No icon loading or network opening in paint; invoke the callback only on completed activation.

## 7. Accessibility

SemanticRole does not currently contain Link. Use Custom with a name, destination description, and Activate action; adding a Link role is a separate semantic-model evolution.

Destination is not the name: a “Help” label and its target must be exposed separately. A link without a callback or with an empty destination keeps its text but does not advertise Activate.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Copy destination and callback, disarm activation, then call the application. If navigate throws, keep the link usable; no navigation is considered replayable.

Removing the link during a press or reconstructing a RichText cancels the gesture; a link with the same text does not inherit a stale identity.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: TextService, Button/PressActivationState, shared availability and focus; RichText uses only the navigation contract.

Empty destination or callback: no navigation activation, no exception. Destination characters are opaque to the toolkit; scheme filtering, permissions, and relative URIs belong to the application.

Identical labels pointing to different destinations must remain separate instances. No global “last visited link” or ported MyGo Router.

## 10. Files and compatibility

Target: `include/nativeui/link.hpp` and `src/link.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

LinkStyle and NavigateCallback remain in the Link file pair. Injected callback code does not enter the platform layer; the URL-opening adapter remains external.

Register `src/link.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`link_destination`: the callback receives exactly the owned destination; the label is distinct.

`link_keyboard`: Enter and Space activate once; Tab and wheel follow the group contract.

`link_cancel_replace`: PointerCancel and replacement of an armed destination cancel without incorrect navigation.

`link_wrap`: long, multibyte text preserves measurement, clipping, and focus during resizing.

`link_navigate_throw`: a callback throws or removes its subtree; captures and the next action recover.

Add `examples/features/link.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.