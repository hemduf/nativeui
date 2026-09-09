# NativeUI Design — C++20 Retained-Mode UI with Pugl + Skia

**Status:** active architecture baseline  
**Updated:** September 7, 2026  
**Targets:** Windows, macOS, Linux/X11  
**Language:** C++20  
**Distribution:** static libraries  
**Windowing / embedding:** Pugl (Win32 / Cocoa / X11)  
**Rendering:** Skia Ganesh/OpenGL for native views + Skia raster for headless rendering  

This document records **durable architectural decisions** for NativeUI. Execution status, ticket dependencies and short-lived implementation notes belong in `CONTEXT.md`, `ROADMAP.md` and GitHub Issues.

---

## 1. Mission and scope

NativeUI is a generic C++20 retained-mode UI toolkit for:

- standalone desktop applications;
- native child views embedded by an external plug-in or host adapter;
- deterministic headless UI tests and rendering;
- static-library integration on Windows, macOS and Linux/X11.

NativeUI owns:

- declarative C++ composition;
- component-tree ownership and lifecycle;
- constraints and layout;
- input routing, focus, pointer interaction and commands;
- generic observable UI state;
- drawing primitives and reusable widgets;
- text editing and text rendering abstractions;
- resource abstractions and caches;
- invalidation and bounded redraw;
- platform-neutral public behavior;
- packaging and validation infrastructure.

NativeUI deliberately does **not** own:

- CLAP, VST3, AU or AAX APIs;
- DSP or audio processing;
- plug-in parameter IDs or normalized parameter models;
- automation semantics;
- host-specific begin/end edit gestures;
- real-time audio-thread synchronization;
- a custom Win32/Cocoa/X11 windowing implementation.

Plug-in support means that NativeUI can create and manage a **native child view** inside a native parent supplied by an external adapter. The plug-in API itself remains outside NativeUI.

---

## 2. Architectural invariants

The following rules are architectural constraints, not implementation preferences:

1. Pugl owns normal native window creation, embedding and event delivery.
2. Skia owns normal rendering.
3. Widgets and layout code do not include Pugl, Win32, AppKit or Xlib headers.
4. Component geometry is expressed in logical pixels.
5. Native/Pugl geometry and the Skia framebuffer are physical pixels.
6. Embedded event polling never blocks the host.
7. A normal new widget does not require a central component enum or switch.
8. Plug-in parameter and audio semantics remain external.
9. Mutable instance-dependent global, singleton or `thread_local` state is forbidden.
10. Ordinary retained UI mutation and `State<T>` mutation are UI/main-thread confined unless an API explicitly states otherwise.
11. Dependencies are acquired through CMake + CPM.
12. Skia is consumed from pinned `olilarkin/skia-builder` binaries; NativeUI does not maintain a Skia build pipeline.
13. Pugl is pinned to an exact source commit and compiled statically by NativeUI.
14. NativeUI does not add SDL, GLFW, Qt, JUCE, NanoVG or GTK as a second UI/windowing stack.
15. Headless tests must not require Pugl, OpenGL or a display server.

Any ticket that changes one of these invariants is an architecture change and must update this document.

---

## 3. Current architecture

```text
                         Application / plug-in adapter
                                    |
                                    v
+------------------------------------------------------------------+
|                         NativeUI public API                       |
|                                                                  |
| DSL   Components   Layout   Focus/Input   State   Resources       |
+-------------------------------+----------------------------------+
                                |
                                v
                       Runtime Component Tree
                    /          |           \
                   /           |            \
             layout/input   invalidation    paint
                   |                          |
                   |                          v
                   |                    NativeUI Painter
                   |                          |
                   |                          v
                   |                         Skia
                   |                    Ganesh / raster
                   |                          |
                   +------------+-------------+
                                |
                                v
                       Pugl native view/event bridge
                         /        |         \
                      Win32     Cocoa       X11
```

The major layering rule is:

```text
widgets / layout / core
    |
    +----> NativeUI geometry, input, state and services
    +----> NativeUI Painter / CanvasContext2D

platform integration
    |
    +----> Pugl

render integration
    |
    +----> Skia
    +----> Pugl OpenGL backend for native views
```

Pugl and Skia are implementation dependencies. Application-level widget code should not need either API directly.

---

## 4. Current implementation snapshot

As of September 7, 2026, the implemented baseline includes:

- observable `State<T>`;
- declarative DSL compiled into a runtime component tree;
- stable component/node lifecycle foundations;
- constraints, alignment, justification and flex growth/shrink;
- `Row`, `Column`, `Stack`, `Padding`, `Spacer`, `Flex`, `Grid`, `Clip` and scroll layout primitives;
- bounded paint/layout invalidation;
- logical pointer routing and toolkit pointer capture;
- focus scopes and keyboard traversal;
- gesture, command and drop primitives;
- transforms, clipping, paths and gradient paint styles;
- backend-neutral decoded image resources with `ImageFit::Fill`, `Contain` and `Cover`;
- `ResourceProvider` and reusable image caching;
- headless Skia raster rendering and golden-image tests;
- `Header`, `Label`/`TextLabel`, `Knob`, `Toggle`, `TextInput`, multiline `TextArea` and interactive `Canvas`;
- `TextEditModel` with UTF-8-aware single-line and multiline editing behavior;
- platform-neutral font selection/fallback and embedded-font registration;
- `StandaloneWindow` and `EmbeddedView` Pugl/OpenGL integration;
- clipboard bridging and platform smoke-test infrastructure;
- dedicated feature examples with `--self-test` coverage.

SVG/icon resources are being implemented separately in T023/PR #60 and are not part of the merged baseline described above until that PR lands.

A known macOS defect remains open in issue #64: multiple simultaneous `StandaloneWindow` instances currently expose a Pugl `PUGL_PROGRAM` application-world lifecycle problem. Multiple independent `EmbeddedView` instances are a separate path and are currently validated successfully.

---

## 5. Windowing and events: Pugl

NativeUI does **not** implement parallel Win32, Cocoa and X11 windowing backends.

Pugl provides the portability layer for:

- native view creation/destruction;
- standalone and embedded modes;
- parent/child embedding;
- keyboard events and committed Unicode text;
- pointer motion/buttons/wheel;
- focus;
- cursor management;
- clipboard and drag-and-drop;
- timers;
- scale factor;
- expose/invalidation;
- platform event pumping;
- OpenGL context integration.

Pugl is especially suitable because it is intentionally small, embeddable, static-link friendly and designed for applications and plug-in GUIs.

### 5.1 Pinned dependency

NativeUI currently pins:

```text
repository: hemduf/pugl
commit:     d12d63815b8cfe3f36293d3791a418e8f558ff1b
license:    ISC
```

The exact commit is mandatory. Pugl is acquired as source with CPM and NativeUI compiles only the required common, platform and OpenGL backend sources into `NativeUI::Pugl`.

The consumer is not required to install or invoke Meson.

### 5.2 Pugl world modes

Standalone views use:

```cpp
puglNewWorld(PUGL_PROGRAM, 0);
```

Embedded views use:

```cpp
puglNewWorld(PUGL_MODULE, 0);
```

Embedding uses Pugl's native parent mechanism internally.

The event-pump contract is different:

```cpp
// standalone: waiting is allowed
puglUpdate(world, -1.0);

// embedded: never block the host
puglUpdate(world, 0.0);
```

`EmbeddedView::poll()` is therefore always non-blocking.

### 5.3 No unnecessary platform abstraction layer

Pugl is wrapped internally, but NativeUI does not add a second virtual Win32/Cocoa/X11 interface merely to hide Pugl from itself.

A second platform backend should be introduced only if a real second implementation exists. Until then, an abstract platform hierarchy would add complexity without increasing portability.

---

## 6. Rendering: Skia

NativeUI uses Skia as the rendering engine and `olilarkin/skia-builder` as the binary distribution source.

Current pin:

```text
repository: olilarkin/skia-builder
release:    chrome/m149
```

NativeUI does not run GN, Ninja or depot_tools and does not maintain parallel Skia build arguments.

### 6.1 Native-window renderer

The v1 native renderer is:

```text
Pugl native view
      |
      v
Pugl OpenGL backend
      |
      v
current OpenGL context
      |
      v
Skia Ganesh / OpenGL
      |
      v
SkSurface / SkCanvas
```

This avoids custom platform presenters such as:

- Win32 `CreateDIBSection` presentation;
- custom CoreGraphics/CALayer raster presentation;
- X11 `XImage` / MIT-SHM presentation;
- NativeUI-owned platform OpenGL context creation.

The Pugl expose callback is the normal render entry point. NativeUI renders into the current view framebuffer through Skia Ganesh and lets Pugl own platform context enter/leave/swap behavior.

### 6.2 Headless renderer

Headless tests use Skia raster surfaces:

```cpp
auto surface = SkSurfaces::Raster(...);
```

This keeps layout, widget and golden tests deterministic and executable without a display server, Pugl or OpenGL.

### 6.3 Future GPU path

Ganesh/OpenGL is the portable baseline, not a permanent API commitment.

After the v1 architecture is stable, a future Graphite/Dawn path can be evaluated:

```text
macOS   -> Metal / Dawn / Graphite
Windows -> D3D12 / Dawn / Graphite
Linux   -> Vulkan / Dawn / Graphite
```

That change must not require rewriting the component model, DSL, layout or widget APIs.

---

## 7. Logical coordinates and high DPI

All public component geometry uses **logical pixels**.

Pugl/native geometry and the OpenGL framebuffer use physical pixels. NativeUI converts platform geometry using the Pugl scale factor before layout and scales the Skia drawing transform once for the frame.

```text
Pugl physical geometry
        |
        | / scaleFactor
        v
NativeUI logical layout
        |
        v
Skia canvas scaled once
        |
        v
physical framebuffer
```

Rules:

- layout never stores physical framebuffer coordinates as public component geometry;
- input coordinates are normalized before toolkit dispatch;
- scale handling is per view/instance;
- dirty regions and clipping must preserve the logical/physical distinction;
- plug-in host resize negotiation stays outside NativeUI.

---

## 8. Declarative public API

The declarative syntax remains intentionally small and C++-native.

A representative current composition is:

```cpp
ui::State<float> drive{0.5f};
ui::State<float> tone{0.5f};
ui::State<float> mix{0.5f};
ui::State<bool> bypass{false};
ui::State<std::string> preset_name{"Init"};

ui::UI ui{
    ui::Column{
        ui::Header{"LIVING INSTRUMENTS"},

        ui::Row{
            ui::Knob{"Drive", drive},
            ui::Knob{"Tone", tone},
            ui::Knob{"Mix", mix},
        },

        ui::Toggle{"Bypass", bypass},
        ui::TextInput{"Preset name", preset_name}
            .placeholder("Name")
    }
    .padding(24.0f)
    .gap(18.0f)
};
```

The DSL is a construction layer, not a template-heavy runtime representation.

---

## 9. Runtime component tree

Declarative builders produce `Spec` objects that are compiled into a runtime tree of independently owned components.

Conceptually:

```text
Column<Row<Knob, Knob>, Toggle, TextInput>
                 |
                 | compile
                 v
               Tree
          /      |       \
       Node     Node     Node
        |        |        |
   Component Component Component
```

The retained tree owns:

- component lifetime;
- child relationships;
- stable node identity;
- measurement and placement;
- focus state;
- input routing;
- invalidation state;
- paint traversal.

Benefits:

- declarative construction remains pleasant;
- dynamic component ownership is explicit;
- runtime behavior is not coupled to giant template types;
- the tree can be introspected for debugging and future accessibility work;
- normal custom components do not need registration in a central switch.

---

## 10. Generic UI state

`ui::State<T>` is intentionally a **generic observable UI state**, not a plug-in parameter abstraction.

Current semantics include:

```cpp
ui::State<float> value{0.5f};

auto subscription = value.observe([](const float& new_value) {
    // UI-domain observer
});

value.set(0.75f);
```

`State<T>` has no concept of:

- parameter ID;
- normalization;
- host automation;
- begin/end edit;
- plug-in gesture ownership;
- audio processing;
- lock-free real-time synchronization.

A plug-in may build an external bridge:

```text
plug-in parameter <---- external adapter ----> ui::State<float>
```

That bridge is not part of NativeUI.

`State<T>` and ordinary retained-tree mutation are UI/main-thread confined. Real-time adapters must use an explicitly designed handoff mechanism such as atomics, bounded queues or immutable snapshots rather than touching UI state directly from the audio thread.

---

## 11. Components and extension model

A reusable custom widget is implemented as a `Component` plus, when useful, a declarative builder.

Component code works through NativeUI abstractions for:

- constraints/measurement;
- layout bounds;
- `PaintContext` / `Painter`;
- `InputEvent` / `InputContext`;
- invalidation;
- focus and pointer capture;
- text metrics and platform services.

A normal custom component must not require changes to:

- Pugl integration;
- the Skia renderer;
- a central component registry;
- platform backends.

A genuinely new low-level capability may require core work, for example:

- a new input-event family;
- a new layout primitive;
- a generic painter operation;
- accessibility semantics;
- a new platform service.

The generic primitive is implemented first; widgets consume it afterward.

---

## 12. Layout system

NativeUI uses a retained `measure -> place/layout` model with explicit constraints rather than a CSS engine.

The merged baseline includes:

- minimum and preferred sizing;
- constraints;
- alignment and justification;
- flex grow/shrink factors;
- `Row`;
- `Column`;
- `Stack`;
- `Padding`;
- `Spacer`;
- `Flex`;
- `Grid`;
- `Clip`;
- scroll layout/state primitives.

Representative syntax:

```cpp
ui::Column{
    ui::Flex{content}.grow(1.0f),
    ui::Row{left, right}
        .gap(8.0f)
        .align(ui::Align::Center)
}
.gap(12.0f)
.justify(ui::Justify::Start);
```

NativeUI intentionally avoids introducing a complete CSS/Flexbox implementation unless concrete product requirements justify that complexity.

Layout invalidation and paint invalidation remain distinct so a visual-only change does not force unnecessary layout work.

---

## 13. Input, focus, commands and pointer interaction

Pugl events are translated immediately into platform-neutral NativeUI events before they enter the component tree.

The input system owns:

- hit testing;
- handled/bubbling semantics;
- focus order and focus scopes;
- `Tab` / `Shift+Tab` traversal;
- pointer hover/press state;
- logical pointer capture;
- wheel normalization;
- gesture helpers;
- command routing;
- drag-and-drop primitives;
- text-input activation/deactivation.

### 13.1 Toolkit pointer capture

Pointer capture is primarily a toolkit concept: once a component captures a pointer interaction, subsequent pointer events received by the view are routed to that component until release/cancel.

The toolkit must always release capture on:

- pointer up;
- explicit cancel;
- deactivation;
- relevant teardown paths.

OS-level capture may be added behind the platform layer if a verified platform problem requires it; widgets must not call Win32/AppKit/X11 capture APIs directly.

### 13.2 `Canvas`

`Canvas` is the generic escape hatch for application-specific rendering and interaction, not a second widget system.

Its drawing callback receives local coordinates through `CanvasContext2D`. Its input callback receives local pointer coordinates and can use the normal toolkit invalidation/focus/capture services.

Reusable standard controls should eventually become first-class components rather than permanent Canvas-only implementations.

---

## 14. Event mapping from Pugl

The internal bridge follows this conceptual mapping:

```text
PUGL_KEY_PRESS/RELEASE -> KeyDown / KeyUp
PUGL_TEXT              -> committed TextInput
PUGL_BUTTON_*          -> PointerDown / PointerUp
PUGL_MOTION            -> PointerMove
PUGL_SCROLL            -> PointerWheel
PUGL_FOCUS_*           -> focus activation/deactivation
PUGL_CONFIGURE         -> logical resize / scale handling
PUGL_EXPOSE            -> native frame render
PUGL_TIMER             -> timer/animation work
PUGL_DATA_*            -> clipboard / drop flow
```

Keyboard command/navigation events and committed text are deliberately separate. `KeyDown` is not used as a substitute for text insertion.

---

## 15. Painting API

Skia is the renderer, but component authors normally paint through NativeUI's `Painter`, `PaintContext` and `CanvasContext2D` abstractions.

The merged baseline includes:

- rectangles and rounded rectangles;
- lines, circles and arcs;
- paths;
- stroke styles;
- linear and radial gradients;
- clipping;
- save/restore and transforms;
- text;
- decoded images;
- source-rectangle image drawing;
- image fitting (`Fill`, `Contain`, `Cover`).

Skia-specific ownership is kept private to implementation objects wherever practical. The existing low-level `UI::paint(SkCanvas&, ...)` integration seam is an implementation-facing boundary, not a requirement for normal widget/application code.

The long-term rule remains: ordinary public widget code should be renderer-independent even while Skia is the single supported renderer.

---

## 16. Images and resources

Storage policy stays outside widgets.

`ResourceProvider` supplies encoded bytes using application-defined identifiers. A provider may source data from:

- embedded byte arrays;
- files in a standalone/dev application;
- a macOS bundle;
- Win32 resources;
- archives;
- generated memory;
- an external callback/backend.

Widgets never hard-code filesystem paths.

The merged image design provides:

- a copyable platform-neutral `Image` handle;
- decode from encoded in-memory bytes;
- Skia-owned decoded lifetime hidden from public API;
- `ImageCache` for successful and failed lookups;
- explicit cache ownership rather than a process-global registry.

Cache lifetime and ownership must remain plug-in safe: clearing or replacing a cache owned by one UI instance must not invalidate data owned by another unrelated instance.

SVG/icon resources are the next rendering-resource extension and follow the same rule: parsing/loading is resource-preparation work, not a real-time audio callback operation.

---

## 17. Text and font system

NativeUI separates editing behavior, text styling/measurement and platform/window input.

### 17.1 Text editing model

`TextEditModel` is headless and independent of Pugl. It owns UTF-8 editing state, including:

- text;
- caret and selection anchor;
- codepoint-aligned byte offsets;
- insertion and deletion;
- word/document navigation;
- selection;
- bounded undo/redo;
- maximum length in codepoints.

The multiline work used by `TextArea` extends this with:

- line-aware navigation;
- cross-line selection;
- vertical movement;
- viewport scrolling;
- selection and caret painting behavior.

### 17.2 Text widgets

Current text-oriented controls include:

- `Label` / `TextLabel`;
- `TextInput`;
- multiline `TextArea`;
- `Header`.

`TextInput` supports the normal committed-text editing path, selection, clipboard behavior, placeholder text, max length, horizontal scrolling and submit behavior.

### 17.3 Fonts

The font service is platform-neutral at the public boundary and supports:

- system font resolution;
- family/fallback selection;
- weight and slant;
- embedded font registration;
- Skia-backed measurement/rendering.

Platform-specific CoreText, DirectWrite and Fontconfig construction remains private.

Text measurement and painting repair malformed UTF-8 identically: each invalid
byte becomes U+FFFD before reaching Skia. Repaired bytes are owned by the local
resolved layout, and run offsets refer to that buffer. Valid input keeps its
original bytes without a repair allocation; there is no shared repair state.

`ui::text::utf8_prefix(value, max_bytes)` borrows a complete valid prefix or
returns `nullopt` when it encounters malformed input before the limit. It uses
that same decoder, with no allocation or font initialization. This helper
checks encoding, not file type or text-preview policy; a NUL scalar is valid
UTF-8, for example. Suffix bytes beyond the inspected prefix are not validated.

Embedded-font registration owns/copies the supplied font bytes. Shared registry data must use immutable/safely published state and must not become implicit instance-dependent mutable global state.

### 17.4 Committed text and advanced IME

Pugl `PUGL_TEXT` provides committed Unicode text and is the normal insertion path, including dead-key/input-method sequences that produce committed text.

Full IME pre-edit/composition remains a separate future platform-extension feature. This includes marked/pre-edit text and candidate-rectangle behavior that the pinned Pugl API does not fully expose.

If required, advanced IME should be implemented through a very small platform extension layer. It is **not** a reason to replace Pugl or rebuild the complete windowing stack.

---

## 18. Clipboard and drag/drop

Clipboard and drop operations are bridged through Pugl and exposed through NativeUI platform services.

Important portability rules:

- normal text clipboard writes use canonical `text/plain`;
- parameterized MIME strings such as `text/plain;charset=utf-8` must not be used on the pinned macOS Pugl path because of its MIME-to-UTI behavior;
- clipboard data must not be retained beyond the lifetime guaranteed by the platform event/API;
- drop acceptance/rejection remains platform-neutral to widgets.

The pinned Pugl fork implements `puglRejectOffer()` on Cocoa, Windows and X11, so NativeUI calls it directly. Drop types and `PUGL_ACCEPT_DROP` are registered before realization. On macOS the backend child forwards the complete drag-destination lifecycle to its owning wrapper; accepted data is delivered once at the actual drop boundary.

External drag sources such as Finder own keyboard focus. `DropOffer`/`DropData` therefore target a **mounted** tree even while it is inactive, without activating keyboard focus, component lifecycle or IME. Hit testing still respects clipping, inherited visibility/enabled state and normal bubbling. Unmounted trees reject delivery.

Reading a dropped file and choosing an optional preview are application-layer
decisions. T018 decodes local file URIs and reads the first regular file without
an extension allow-list. It inspects up to 64 KiB plus three UTF-8 lookahead
bytes, and previews up to 120 character-aligned bytes if the inspected content
is text. Binary/non-UTF-8 content is reported as received without a text preview,
not as a rejected drop. The shared UTF-8 decoder supplies encoding validation;
the example's control-byte check only decides preview availability. NativeUI's
drop API delivers MIME type and owned payload bytes, never file-type policy.

---

## 19. Standalone windows

The current public API owns a UI tree by reference and a window description:

```cpp
ui::UI ui{root};

ui::StandaloneWindow window{
    ui,
    ui::WindowDesc{
        .title = "NativeUI Demo",
        .size = {800.0f, 500.0f},
        .resizable = true,
    }
};

return window.run();
```

`StandaloneWindow` provides:

- `run()`;
- `poll(timeout)`;
- `request_close()`;
- logical `size()` and `set_size()`;
- `scale_factor()`;
- native view handle access for integration;
- platform services for text input, clipboard and drops.

The standalone loop may wait between events. Active animation/timer work must use bounded waits or Pugl timers rather than a permanent 60 Hz redraw loop.

### 19.1 Known macOS multi-window limitation

Issue #64 currently tracks a crash when multiple `StandaloneWindow` objects create multiple `PUGL_PROGRAM` worlds in the same macOS process.

The architecture requirement is not to hide this behind a mutable singleton. The fix must establish explicit application/world ownership while preserving:

- per-window state;
- independent destruction;
- multiple simultaneous windows;
- plug-in-safe process coexistence;
- deterministic lifecycle.

A likely design direction is an explicit application/context owner or another ownership model consistent with upstream Pugl's `PUGL_PROGRAM` lifecycle, but the issue must be validated before this document commits to one exact implementation.

---

## 20. Embedded views

The current public API creates an embedded view with the UI, parent native handle and logical size:

```cpp
ui::UI ui{root};

ui::EmbeddedView view{
    ui,
    parent_native_handle,
    {640.0f, 420.0f}
};

view.poll(); // always non-blocking
```

The external CLAP/VST3/AU adapter is responsible for obtaining the correct native parent handle and integrating polling/idle behavior with its host API.

NativeUI does not know which plug-in format owns the parent.

`NativeParentHandle`/`NativeViewHandle` are currently represented as `std::uintptr_t` in the public windowing boundary. Platform adapters are responsible for safe conversion to/from the host's native handle type.

Embedded lifecycle requirements include:

- repeated construction/destruction;
- multiple simultaneous independent instances;
- destroy instance A while instance B remains functional;
- non-blocking polling;
- no ownership of the host's parent native view/window;
- graphics resources released while the relevant Pugl/OpenGL context is valid;
- focus, pointer capture, text input, timers and callbacks disconnected before destruction.

---

## 21. Invalidation and animation

NativeUI does not continuously redraw an idle UI.

The intended flow is:

```text
input / state / model change
          |
          v
component invalidation
          |
          v
Tree dirty/layout state
          |
          v
platform redraw request
          |
          v
PUGL_EXPOSE
          |
          v
Skia render
```

The core aggregates dirty regions and distinguishes layout invalidation from paint-only invalidation.

For native views, `PUGL_EXPOSE` is the normal drawing entry point.

Animation/timer work exists only while active. Embedded integrations must not install a permanent blocking or self-owned host event loop.

---

## 22. Threading and real-time boundaries

NativeUI is a UI/main-thread toolkit unless a specific API explicitly documents a different contract.

Rules:

- mutate the retained component tree on the UI/main thread;
- mutate ordinary `State<T>` on the UI/main thread;
- create/mutate native views on the host UI/main thread;
- perform AppKit/Win32/X11 UI operations only through the proper platform/UI thread path;
- do not call NativeUI UI APIs directly from an audio process callback;
- do not use `thread_local` as instance ownership;
- do not make process-global mutable state stand in for a UI instance.

An adapter bridging audio/worker data to the UI must use a separately reviewed handoff mechanism.

Real-time paths must avoid:

- allocation/deallocation;
- blocking/contended locks;
- filesystem/network I/O;
- UI calls;
- process-wide initialization;
- Objective-C/AppKit work;
- waiting/sleeping;
- unbounded logging.

NativeUI itself remains independent of the plug-in audio thread.

---

## 23. Multi-instance and plug-in-host safety

NativeUI can be statically linked into plug-ins that coexist with many other plug-ins and with multiple instances of the same plug-in in one process.

Every implementation must therefore be designed for:

- per-instance ownership;
- repeated open/close;
- repeated attach/detach where supported;
- simultaneous instances;
- destruction during active focus/input/timer/drop state;
- module unload/reload scenarios where relevant;
- absence of hidden dependence on static destruction order.

Mutable process-global state is forbidden by default.

A process-shared cache/service is acceptable only when sharing is intentional, synchronized, collision-safe, documented and unable to let one instance silently replace or invalidate another instance's data.

The mandatory review policy is defined in `CODE_REVIEW.md` and applies to every code-changing ticket.

---

## 24. Objective-C runtime safety

macOS plug-in processes have a process-global Objective-C runtime namespace. C/C++ hidden symbol visibility does not isolate Objective-C runtime class names.

NativeUI therefore follows these rules:

- avoid defining Objective-C runtime classes in reusable static-library code when possible;
- never add generic runtime names such as `View`, `PluginView`, `EditorView`, `WindowController` or `AppDelegate`;
- if a runtime class is required in plug-in code, its real runtime name must contain a consumer/plugin-specific collision-resistant prefix;
- a NativeUI-only prefix is insufficient when the same static library can appear in multiple plug-in bundles;
- avoid categories on Apple/framework classes;
- do not use method swizzling, `+load` or host-wide `NSApplication` mutation for normal NativeUI work;
- keep AppKit work on the UI/main thread;
- explicitly audit ARC/bridging/block ownership.

Any Objective-C/Objective-C++ change must pass the Objective-C runtime section of `CODE_REVIEW.md`.

---

## 25. Platform support

### 25.1 Windows

Pugl uses Win32 internally. NativeUI's normal path does not implement its own `CreateWindowExW`/`WndProc` stack.

Current build characteristics include:

- OpenGL through CMake `FindOpenGL`;
- Win32 system libraries required by Pugl;
- Skia x64 MSVC packages;
- `/MD` default, `/MT` selectable through `NATIVEUI_SKIA_WINDOWS_CRT`;
- `NOMINMAX` protection on NativeUI's C++ platform bridge.

### 25.2 macOS

Pugl uses Cocoa/AppKit internally. NativeUI does not create a parallel custom NSWindow/NSView windowing backend.

The pinned Skia package currently uses the universal macOS GPU Release asset:

```text
skia-build-mac-universal-gpu-release.zip
```

OpenGL is the v1 native renderer baseline even though Apple deprecates OpenGL; a later Graphite/Metal path may replace it without changing toolkit APIs.

### 25.3 Linux

The v1 native backend is X11 through Pugl and OpenGL/GLX.

The pinned `chrome/m149` package currently supports Linux x64 Release in NativeUI's dependency configuration.

### 25.4 Wayland

Native Wayland is not part of v1 because the pinned Pugl backend does not provide it.

When Wayland becomes a release requirement, evaluate in this order:

1. upstream/contribute a Pugl Wayland backend;
2. if that is not viable, introduce an alternative host implementation behind the existing public NativeUI window API.

Do not pre-emptively duplicate the complete platform layer before a concrete Wayland requirement exists.

---

## 26. Build and dependency model

CMake + CPM is mandatory.

Current project configuration starts with:

```cmake
cmake_minimum_required(VERSION 3.24)
project(NativeUI VERSION 0.1.0 LANGUAGES C CXX)
```

Current major options are:

```cmake
NATIVEUI_BUILD_PLATFORM=ON
NATIVEUI_BUILD_EXAMPLES=ON
NATIVEUI_BUILD_TESTS=ON
NATIVEUI_ENABLE_SANITIZERS=OFF
NATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS=OFF

NATIVEUI_PUGL_SOURCE=
NATIVEUI_PUGL_COMMIT=d12d63815b8cfe3f36293d3791a418e8f558ff1b

NATIVEUI_SKIA_ROOT=
NATIVEUI_SKIA_TAG=chrome/m149
NATIVEUI_SKIA_WINDOWS_CRT=MD
NATIVEUI_SKIA_CONFIG=Release
```

### 26.1 Target layering

The current build separates headless/core functionality from platform integration:

```text
NativeUI::Core
    |
    +---- NativeUI retained runtime
    +---- Skia core services
    +---- headless renderer
    |
    |    no Pugl / OpenGL dependency
    |
NativeUI::NativeUI
    |
    +---- NativeUI::Core
    +---- NativeUI::Pugl
    +---- NativeUI::OpenGL
```

This separation is important because core tests and public API compile probes can run without a display.

### 26.2 Pugl acquisition

Pugl is source-only and compiled statically by NativeUI:

```cmake
CPMAddPackage(
  NAME pugl_src
  GITHUB_REPOSITORY lv2/pugl
  GIT_TAG ${NATIVEUI_PUGL_COMMIT}
  DOWNLOAD_ONLY YES
)
```

Only the required common, platform and OpenGL sources are compiled.

### 26.3 Skia acquisition

Skia is downloaded as a pinned binary archive from `olilarkin/skia-builder` using CPM URL + SHA256 verification, unless `NATIVEUI_SKIA_ROOT` points at an already extracted package.

NativeUI supports both CPM's flattened archive layout and the original manually extracted `build/` archive layout.

NativeUI does not rebuild Skia.

---

## 27. Consumer packaging target

The intended installed-package experience is:

```cmake
find_package(NativeUI CONFIG REQUIRED)
target_link_libraries(MyTarget PRIVATE NativeUI::NativeUI)
```

The consumer should not need to understand Pugl source lists or the internal Skia static-library graph.

Install/export packaging is still a release-path deliverable (T047 and downstream work). Until that work is complete, `NativeUI::NativeUI` is already the in-tree target that represents the complete platform-enabled toolkit.

Because NativeUI is primarily a static C++ library, it does not promise a stable C++ ABI across incompatible compilers/runtimes. Skia and consumer runtime/toolchain choices must be compatible.

---

## 28. Testing architecture

Testing is part of the framework architecture, not an afterthought.

### 28.1 Core tests

Core tests cover behavior such as:

- `State<T>`;
- component lifetime;
- measure/layout;
- constraints/alignment/flex/grid/scroll;
- invalidation;
- hit testing/routing;
- focus;
- pointer capture;
- gestures;
- commands;
- drop primitives;
- transforms/paths/paint styles;
- images/resources;
- text edit behavior;
- widgets;
- font services.

### 28.2 Headless/golden rendering

Golden rendering uses the Skia raster backend and must remain display-less and deterministic.

Updating golden baselines is explicit rather than an automatic side effect of a test failure.

### 28.3 Platform smoke tests

Native platform validation covers standalone and embedded Pugl/OpenGL integration, including:

- create/show/resize/destroy;
- embedded child creation;
- non-blocking polling;
- multiple instances;
- repeated lifecycle;
- logical/physical scale conversion;
- keyboard/text;
- pointer/wheel;
- clipboard/drop;
- timers/invalidation;
- OpenGL + Skia frame creation;
- teardown during active interaction.

### 28.4 Feature examples

Every feature ticket ships a dedicated example executable:

```text
examples/features/tNNN_<feature>.cpp
```

Each feature example must:

- demonstrate the public API interactively;
- support `--self-test`;
- return non-zero on self-test failure;
- compile against `NativeUI::Core` in display-less CI so public API regressions are caught even when platform windowing cannot run.

This rule keeps features independently demonstrable and testable.

---

## 29. Unsupported shortcuts

The following approaches should not be introduced without an explicit architecture decision.

### Do not rewrite Pugl windowing

Do not create parallel Win32/Cocoa/X11 backends merely because a small capability is missing.

Evaluate, in order:

1. existing Pugl API;
2. a small local platform extension;
3. an upstream Pugl contribution;
4. only then a larger alternative architecture if the requirement genuinely cannot be met.

### Do not expose platform APIs to widgets

Widgets must not include or call Pugl, Win32, AppKit or Xlib directly.

### Do not put plug-in APIs in the core

No CLAP/VST3/AU parameter, host or DSP logic belongs in NativeUI core.

### Do not rebuild Skia

NativeUI consumes `skia-builder` release artifacts. It does not maintain GN args, depot_tools or a second Skia CI pipeline.

### Do not create multiple graphics stacks in v1

Ganesh/OpenGL is the native-view baseline; Skia raster is the headless baseline. Graphite/Dawn is a future renderer evolution, not a parallel v1 requirement.

### Do not use mutable globals to solve lifecycle problems

Application-level shared ownership, when required, must be explicit and reviewed. A convenience singleton is not an acceptable fix for multi-window or multi-instance lifecycle problems.

---

## 30. Near-term architecture work

The next major capabilities extend the current architecture rather than replacing it:

- finish SVG/icon resources;
- complete the standard widget set (`Button`, `Slider`, `RangeSlider`, meters, menus, scroll/list/tabs, etc.);
- introduce typed theming/style inheritance and bounded animation helpers;
- implement advanced IME composition where required;
- harden native resize/scale, pointer-capture and multi-instance lifecycle behavior;
- finish CMake install/export packaging and dependency diagnostics;
- add benchmark/release validation;
- design accessibility without coupling widgets to platform APIs;
- evaluate Wayland after X11 v1 is stable;
- evaluate Graphite/Dawn when it provides a concrete benefit over Ganesh/OpenGL.

These items are scheduled through GitHub Issues and `ROADMAP.md`; this document defines the architectural constraints they must preserve.

---

## 31. v1 success criteria

NativeUI v1 is architecturally successful when:

1. the same application-level C++ UI compiles without platform `#ifdef` logic on Windows, macOS and Linux/X11;
2. standalone windows work through Pugl on all supported desktop platforms;
3. embedded child views work through `PUGL_MODULE`/native parenting;
4. embedded polling is non-blocking;
5. pointer, keyboard, focus, committed Unicode text and clipboard behavior are reliable;
6. idle UIs do not continuously redraw;
7. high-DPI/scale conversion is correct;
8. multiple independent embedded/UI instances can coexist safely;
9. standalone multi-window ownership is explicit and robust on all supported platforms;
10. repeated open/close/create/destroy lifecycle does not leak or corrupt state;
11. Skia Ganesh/OpenGL renders correctly in the Pugl context;
12. headless/golden tests use Skia raster without a display;
13. the standard widget/layout/text/resource layers cover normal application and plug-in editor needs;
14. the installed package can be consumed through `NativeUI::NativeUI`;
15. no plug-in/audio SDK is required by the framework core;
16. Pugl and Skia dependencies are pinned and reproducible;
17. instance-dependent mutable global/singleton/thread-local state is absent;
18. macOS runtime code is safe when multiple independently built plug-ins coexist in one host process.

---

## 32. Final architecture statement

The core architectural decision remains deliberately simple:

```text
                    Declarative C++ DSL
                           |
                           v
                    Component Tree
                 /         |         \
              Layout      Input      Paint
                            |          |
                            v          v
                       Pugl events    Skia
                            |          |
                            +----+-----+
                                 v
                         Pugl OpenGL View
                        /       |       \
                     Win32    Cocoa     X11
```

> **NativeUI builds a UI toolkit, not a windowing system. Pugl owns native windowing/embedding; Skia owns rendering; NativeUI owns UI behavior.**

NativeUI's value is concentrated in:

- declarative C++ composition;
- retained component ownership;
- layout;
- input/focus/interaction;
- generic UI state;
- text editing;
- resources;
- widgets;
- styling and invalidation;
- predictable standalone/embedded lifecycle;
- simple static-library consumption.

The two structural third-party dependencies remain:

- `lv2/pugl` at an exact pinned commit, vendored/compiled statically;
- `olilarkin/skia-builder` as the source of pinned prebuilt Skia static binaries.

The main platform risks remain isolated rather than allowed to distort the toolkit architecture:

1. native Wayland support is not provided by the current Pugl baseline;
2. full IME pre-edit/composition may require narrow platform extensions;
3. macOS `PUGL_PROGRAM` ownership for multiple standalone windows requires an explicit lifecycle fix.

None of these currently justifies reimplementing Win32, Cocoa and X11 windowing inside NativeUI.

---

## 33. References

- Pugl — https://github.com/lv2/pugl
- Pugl documentation — https://lv2.gitlab.io/pugl/c/html/
- DPF/DGL — https://github.com/DISTRHO/DPF
- iPlug2 / IGraphics — https://github.com/iPlug2/iPlug2
- skia-builder — https://github.com/olilarkin/skia-builder
- Skia build documentation — https://skia.org/docs/user/build/
- NativeUI workflow — `AGENTS.md`
- NativeUI mandatory review policy — `CODE_REVIEW.md`
- NativeUI current recovery context — `CONTEXT.md`
- NativeUI execution roadmap — `ROADMAP.md`
