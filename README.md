# NativeUI POC — Pugl + Skia

A C++20 UI-only framework proof of concept for standalone applications and embedded/plugin views.

## Architecture

- **Pugl**: native windowing, parent/child embedding, input, clipboard and event pump.
- **Skia Ganesh/OpenGL**: window rendering.
- **Skia raster** can be used for future headless/golden tests.
- **CMake + CPM.cmake**: all dependency acquisition.
- **`olilarkin/skia-builder`**: prebuilt static Skia binaries. NativeUI never builds Skia.
- No CLAP/VST3/AU types, parameter IDs, automation or audio code.

## Declarative API

```cpp
ui::State<float> drive{0.35f};
ui::State<float> tone{0.5f};
ui::State<float> mix{0.75f};
ui::State<bool> bypass{false};
ui::State<std::string> preset{"Init"};

ui::UI ui {
    ui::Column {
        ui::Header{"LIVING INSTRUMENTS"},
        ui::Row {
            ui::Knob{"Drive", drive},
            ui::Knob{"Tone", tone},
            ui::Knob{"Mix", mix},
        },
        ui::Toggle{"Bypass", bypass},
        ui::TextInput{"Preset", preset}.placeholder("Preset name")
    }.padding(24).gap(18)
};
```

`State<T>` is a generic UI observable. A plugin may bridge its own parameter system to it externally, but NativeUI has no plugin parameter concept. The bound `State<T>` objects must outlive the `ui::UI` tree that references them.


## Headless text editing model

Text editing state is reusable independently of the `TextInput` widget and platform services:

```cpp
ui::TextEditModel edit{"one two"};
edit.move_left(ui::TextMotion::Word);
edit.select_word_at(edit.cursor());
edit.insert("three");
edit.undo();
edit.redo();
```

`TextEditModel` owns UTF-8-safe cursor/selection byte offsets, codepoint/word/document motion, insertion/deletion, max-length enforcement and bounded undo/redo history. `TextInput` keeps only view concerns such as hit-testing, horizontal scrolling, focus/clipboard integration and painting.

## Interactive Canvas

`ui::Canvas` is a generic custom-drawing component. The draw callback receives a local 2D context; `(0, 0)` is the top-left of the canvas regardless of where it is placed in the component tree.

```cpp
ui::Canvas{
    ui::Size{640, 150},
    [&](ui::CanvasContext2D& g) {
        g.fill_rounded_rect({0, 0, g.width(), g.height()}, 12, ui::colors::panel);
        g.line({10, 40}, {630, 40}, 1, ui::colors::border);
        g.text({16, 20}, "STEP SEQUENCER", 13, ui::colors::text);
    }}
    .on_input([&](const ui::InputEvent& event, ui::CanvasInputContext& ctx) {
        // Pointer coordinates are local to the Canvas.
        if (event.type == ui::InputType::PointerDown) {
            // update model...
            ctx.capture_pointer();
            ctx.invalidate();
        }
        if (event.type == ui::InputType::PointerUp) {
            ctx.release_pointer();
        }
    });
```

Providing `on_input(...)` automatically makes the canvas focusable. Mouse clicks focus it, keyboard events are then routed to it, and pointer capture keeps drag interactions alive until release. The included standalone demo uses this API for a 16-step sequencer: click toggles a step, dragging paints the same add/remove state across several steps, Left/Right selects a step, Space/Enter toggles it, Up adds it, and Down/Backspace/Delete removes it.

### Canvas transforms

`CanvasContext2D` supports a protected local transform stack:

```cpp
g.save();
g.translate(120.0f, 60.0f);
g.rotate(ui::kPi * 0.25f);
g.scale(1.2f);
g.fill_rect({-20, -10, 40, 20}, ui::colors::accent);
g.restore();
```

Transforms are local to the Canvas even when layout places it away from the window origin, and component paint state is automatically isolated from siblings.

## Standalone

```cpp
ui::StandaloneWindow window{
    ui,
    {.title = "Demo", .size = {720, 560}, .resizable = true}
};
return window.run();
```

## Embedded view

The external plugin adapter passes the native parent handle as `uintptr_t`:

```cpp
ui::EmbeddedView view{ui, parentHandle, {640, 420}};

// Call from the host/plugin idle/timer mechanism. Never blocks.
view.poll();

// When the host grants a new logical size:
view.set_size({800, 500});
```

Pugl is created with `PUGL_MODULE` for embedded views and `puglUpdate(..., 0.0)` is used by `EmbeddedView::poll()`.

## Dependencies with CPM

The project bootstraps CPM.cmake, then:

1. fetches Pugl source at the pinned commit `b7637149ebe53124e5be90559e02a0185bbcbd73` and compiles only its native core + OpenGL backend as a static target;
2. downloads the pinned `skia-builder` `chrome/m149` release ZIP for the current platform and imports its static `skia` library.

The Skia artifacts are checksum-pinned. Pugl is source-pinned by commit. No GN/Ninja Skia build is part of NativeUI.

### Default assets

- macOS: `skia-build-mac-universal-gpu-release.zip`
- Linux x64: `skia-build-linux-x64-gpu-release.zip`
- Windows x64: `/MD` Release by default; `/MT` selectable.

### Focus scopes

Focus scopes are focus-only subtree boundaries; rendering and visibility remain separate concerns:

```cpp
ui::State<bool> dialogFocus{false};

ui::FocusScope{dialogFocus,
    ui::Column{
        ui::Toggle{"Cancel", cancel},
        ui::Toggle{"OK", ok}
    }}
    .trap(true)
    .default_focus(1);
```

When `dialogFocus` becomes true, focus moves to the requested focusable descendant. A trapping scope wraps `Tab` and `Shift+Tab` inside the subtree. When it becomes false, NativeUI restores the focus that was active before the scope opened when possible. Inactive scope descendants are excluded from focus targeting/traversal but are not automatically hidden.

### Input bubbling

NativeUI first selects one authoritative input target (focused node, hit-tested pointer target, or captured pointer target). If that component returns `ui::EventResult::Ignored`, the event bubbles through its ancestors until one returns `Handled` or the root is reached.

```cpp
ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
    if (consume(event)) return ui::EventResult::Handled;
    return ui::EventResult::Ignored; // parent may handle it
}
```

`Tab` and `Shift+Tab` remain tree/focus-manager commands and are never delivered twice through the bubble path.

### Scroll layout primitive

`ui::Scroll` provides viewport measurement, clipping and externally controlled offset without imposing wheel/scrollbar behavior:

```cpp
ui::ScrollState scroll{ui::ScrollAxis::Vertical};

ui::UI ui {
    ui::Scroll{scroll, longContent}
};

scroll.set_offset({0.0f, 120.0f});
scroll.scroll_by({0.0f, 24.0f});

auto content = scroll.content_size();
auto viewport = scroll.viewport_size();
auto maximum = scroll.max_offset();
```

Use `Horizontal`, `Vertical`, or `Both`. Offsets are clamped whenever content or viewport metrics change. Interactive wheel/scrollbar behavior belongs to the future `ScrollView` widget.

### Clipping / overflow

Use `ui::Clip` when a child is allowed to keep its natural size but must render and receive pointer input only inside a viewport:

```cpp
ui::Clip {
    ui::Canvas{ui::Size{320, 120}, drawTimeline}
}
```

Clips compose by intersection. Pointer hit testing follows the same clip chain as painting, so content outside a clip cannot be clicked.

## Golden rendering tests

Geometry rendering regressions are covered by versioned PPM baselines. Normal tests never modify them:

```bash
ctest --test-dir build -R nativeui_golden_tests --output-on-failure
```

Update baselines only when intentionally reviewing rendering changes:

```bash
./build/nativeui_golden_tests --update-goldens
# or
cmake --build build --target nativeui_update_goldens
```

On mismatch, NativeUI emits `actual.ppm` and `diff.ppm` artifacts plus pixel statistics. Text pixels are excluded from cross-platform golden regions unless a dedicated tolerance has been established.


## Platform lifecycle smoke tests

T041 adds native standalone and embedded smoke executables:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DNATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS=ON
cmake --build build -j
ctest --test-dir build -L smoke --output-on-failure
```

Targets can also be run directly:

```bash
./build/nativeui_smoke_standalone
./build/nativeui_smoke_embedded
```

The embedded smoke creates a `PUGL_PROGRAM` parent and a real `PUGL_MODULE` child attached through the parent's native handle. `EmbeddedView::poll()` remains non-blocking. `last_error()` on both window wrappers exposes runtime Pugl/renderer errors after successful construction.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/nativeui_demo
```

### skia-builder archive layout

`CPMAddPackage(URL ...)` uses CMake/FetchContent extraction semantics. Since the
`skia-builder` ZIPs contain a single top-level `build/` directory, CMake normally
strips that directory while extracting into `_deps/skia_prebuilt-src`.
NativeUI therefore auto-detects both supported layouts:

```text
# CPM / FetchContent extraction
_deps/skia_prebuilt-src/
├── include/include/core/SkCanvas.h
└── mac-gpu/...

# Manual unzip retaining the original archive root
<root>/build/
├── include/include/core/SkCanvas.h
└── mac-gpu/...
```

Do not hard-code `build/` below `skia_prebuilt_SOURCE_DIR`.

### Offline/local dependency overrides

The normal build always uses CPM. For local/offline validation you can point to already extracted dependency trees:

```bash
cmake -S . -B build \
  -DNATIVEUI_PUGL_SOURCE=/path/to/pugl \
  -DNATIVEUI_SKIA_ROOT=/path/to/extracted/skia-builder-archive
```

## Platform build requirements

CPM manages the source/binary dependencies, but the native SDK development packages still come from the platform:

- **macOS**: Xcode/Command Line Tools (Cocoa, OpenGL, CoreText/CoreGraphics are system frameworks).
- **Windows**: Windows SDK + OpenGL + DirectWrite; select the Skia `/MD` or `/MT` package with `NATIVEUI_SKIA_WINDOWS_CRT`.
- **Linux/X11**: X11, OpenGL/GLX and Fontconfig development packages. On Debian/Ubuntu this is typically `libx11-dev libgl1-mesa-dev libfontconfig1-dev`.

NativeUI deliberately disables optional Pugl Xcursor/XRandR/XSync integration in this POC to keep the baseline dependency set small.

## High-DPI model

Pugl reports native geometry in physical pixels. NativeUI converts input and view geometry to logical coordinates by dividing by `puglGetScaleFactor()`. Skia wraps the physical framebuffer directly and scales the canvas exactly once. Public `WindowDesc::size` and component geometry are therefore logical coordinates.

## Input implemented

- Tab / Shift+Tab focus
- mouse click/drag
- knob arrow-key editing (Shift = fine)
- toggle via Space/Enter/click
- text input with UTF-8 committed text
- caret and selection
- Home/End and word navigation
- copy/cut/paste
- undo/redo
- double/triple click selection
- horizontal text scrolling
- placeholder, max length, submit, Escape/revert
- Pugl clipboard bridge

Advanced IME pre-edit/candidate positioning remains a later platform extension because Pugl currently exposes committed text but not a complete composition/pre-edit API.

## Current platform scope

- macOS / Cocoa through Pugl
- Windows / Win32 through Pugl
- Linux / X11 through Pugl
- Wayland is not part of this POC


## Git repository

Version the sources, tests (including `tests/golden/baselines/*.ppm`), examples,
CMake files, `.github/` workflows and project documentation. `.gitignore` excludes
builds, downloaded dependency caches, generated test images, local IDE/environment
settings, `tickets/`, `TICKETS.md` and `nativeui_T*.zip` recovery snapshots. Development
tickets are maintained in [GitHub Issues](https://github.com/hemduf/nativeui/issues?q=is%3Aissue).
`.gitattributes` normalizes text
line endings and preserves binary golden images byte-for-byte.

Use out-of-source builds as shown above. Keep machine-specific dependency paths in
the ignored `CMakeUserPresets.json` or pass the documented CMake overrides locally;
shared `CMakePresets.json` files can be versioned.

Clone the source repository, then use the build commands above:

```bash
git clone https://github.com/hemduf/nativeui.git
cd nativeui
```

Recovery ZIPs remain separate artifacts; Git stores the files required to rebuild
the project.

## Project continuation / agent recovery

The backlog is available as [52 GitHub issues](https://github.com/hemduf/nativeui/issues?q=is%3Aissue),
organized by priority, status and [roadmap milestone](https://github.com/hemduf/nativeui/milestones?state=all).
GitHub is the source of truth for ticket descriptions, status, dependencies and
discussion. Local `TICKETS.md` and `tickets/` copies may exist for offline recovery,
but are ignored by Git and are not required after cloning.

The repository contains the continuation documentation:

- `AGENTS.md` — mandatory development/TDD/review/recovery workflow;
- `CONTEXT.md` — compact current-state context for resuming without chat history;
- `ROADMAP.md` — milestone roadmap from POC to reusable toolkit;
- `PLAN.md` — implementation sequencing and rationale;
- [GitHub Issues](https://github.com/hemduf/nativeui/issues?q=is%3Aissue) — actionable tickets with status, dependencies, acceptance criteria and tests.

Any agent resuming the project should start with `AGENTS.md`, then `CONTEXT.md`, and follow the numeric ticket order and current execution frontier. Read the selected GitHub issue for updates; synchronize optional local recovery copies if present.


## Feature examples

Every feature ticket ships a dedicated executable, not only unit tests. Current examples:

```text
nativeui_example_t007_constraints
nativeui_example_t008_alignment
nativeui_example_t009_flex
nativeui_example_t010_grid
nativeui_example_t011_clipping
nativeui_example_t012_scroll
nativeui_example_t013_bubbling
nativeui_example_t014_focus_scopes
nativeui_example_t015_pointer_capture
```

Run interactively on a desktop, or run the executable self-check without opening a window:

```bash
./build/nativeui_example_t015_pointer_capture --self-test
ctest --test-dir build -R nativeui_example_ --output-on-failure
```

Feature-example sources are also compiled against `NativeUI::Core` in display-less builds to catch public API regressions.
