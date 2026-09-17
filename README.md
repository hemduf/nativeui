# NativeUI POC — Pugl + Skia

A C++20 UI-only framework proof of concept for standalone applications and embedded/plugin views.

## Architecture

- **Pugl**: native windowing, parent/child embedding, input, clipboard and event pump.
- **Skia Ganesh/OpenGL**: window rendering.
- **Skia raster** can be used for future headless/golden tests.
- **CMake + CPM.cmake**: all dependency acquisition.
- **`hemduf/skia-builder`**: prebuilt static Skia binaries, forked from `olilarkin/skia-builder`. NativeUI never builds Skia.
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

`State<T>` is a generic UI observable. A plugin may bridge its own parameter system to it externally, but NativeUI has no plugin parameter concept. The bound `State<T>` objects must outlive the `ui::UI` tree that references them. `State<T>` and the retained `ui::UI` API are intentionally UI/main-thread confined; they are not audio-thread synchronization primitives. A VST3/CLAP adapter must use an explicitly thread-safe bridge before applying updates in the UI domain.


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

// Call from the host/plugin UI-thread idle/timer mechanism. Never blocks.
view.poll();

// When the host grants a new logical size, also on the UI/main thread:
view.set_size({800, 500});
```

Pugl is created with `PUGL_MODULE` for embedded views and `puglUpdate(..., 0.0)` is used by `EmbeddedView::poll()`. Construction, use and destruction of the platform view are UI/main-thread operations.

## Dependencies with CPM

The project bootstraps CPM.cmake, then:

1. fetches Pugl source at the pinned commit `b7637149ebe53124e5be90559e02a0185bbcbd73`; Windows/Linux compile the normal generic platform sources, while macOS shares only Pugl's portable C core and compiles the Cocoa/OpenGL bridge per final consumer;
2. downloads the pinned `hemduf/skia-builder` `chrome/m149` release ZIP for the current production-supported platform path and imports its static `skia` library.

The Skia artifacts are checksum-pinned. Pugl is source-pinned by commit. No GN/Ninja Skia build is part of NativeUI.

### macOS consumer-scoped platform bridge

The pinned Pugl macOS backend contains Objective-C runtime classes, whose names are process-global. T053 therefore does **not** build one generic Cocoa/OpenGL archive with a framework-level prefix. Instead, `NativeUI::Core` and Pugl's portable C core remain generic, while `mac.m`, `mac_gl.m` and NativeUI's Cocoa IME bridge are compiled into a small static bridge for each final consumer target.

Each final application/module/shared-library consumer supplies one stable `CONSUMER_ID`. NativeUI derives the Objective-C prefix in exactly one CMake function from the exact UTF-8 identity bytes using the frozen `NUI_<fragment>_<sha256-12>_` algorithm, then renames every Pugl runtime class in that consumer bridge. NativeUI's own source-tree examples and smoke tests register distinct identities internally, so a normal source checkout no longer needs a global/manual `NATIVEUI_OBJC_RUNTIME_PREFIX` cache variable.

CI builds two independently identified macOS consumers in one configure, audits both bridge archives for prefixed class **and metaclass** symbols, rejects every unprefixed `Pugl*` Objective-C runtime class/metaclass, and loads both final modules in one process to prove runtime coexistence.

The installed/public low-level `nativeui_attach_platform(TARGET ... CONSUMER_ID ...)` helper is owned by T047. Until that package ticket lands, source-tree application/test targets use NativeUI's internal consumer attachment primitive; do not copy the prefix derivation or restore a manual global prefix path.

### Default assets

- macOS: `skia-build-mac-universal-gpu-release.zip`
- Linux x64: `skia-build-linux-x64-gpu-release.zip`
- Linux ARM64 CI: `skia-build-linux-arm64-gpu-release.zip` on the native `ubuntu-24.04-arm` runner.
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

T041 adds native standalone and embedded smoke executables. T053 assigns separate consumer identities to these targets automatically on macOS:

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

The embedded smoke creates a `PUGL_PROGRAM` parent then a real `PUGL_MODULE` child attached through the parent's native handle. `EmbeddedView::poll()` remains non-blocking. `last_error()` on both window wrappers exposes runtime Pugl/renderer errors after successful construction.

## Build

The normal source-tree build is the same on all supported platforms; macOS consumer identities for NativeUI-owned executable targets are registered internally:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/nativeui_demo
```
