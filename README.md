# NativeUI

NativeUI is a C++20 retained-mode UI toolkit for standalone desktop applications, native embedded child views, and deterministic headless UI tests on macOS, Windows, and Linux/X11.

The stable v1 API is backend-neutral. Application and widget code includes NativeUI headers; rendering and native-window implementation dependencies remain private implementation details.

## Public C++ API

Use the umbrella header for normal applications:

```cpp
#include <nativeui/nativeui.hpp>
```

A minimal standalone application owns its event loop explicitly:

```cpp
ui::State<bool> enabled{true};
ui::UI app_ui{
    ui::Column{
        ui::Header{"NativeUI"},
        ui::Toggle{"Enabled", enabled},
    }.padding(16.0f).gap(8.0f)
};

ui::Application app;
ui::StandaloneWindow window{
    app,
    app_ui,
    {.title = "NativeUI", .size = {640.0f, 420.0f}}
};

if (!app.valid() || !window.valid()) return 1;
return app.run();
```

`Application` is the sole standalone event-loop owner. `Application::poll()` is available for integrations that need bounded/manual standalone pumping. `StandaloneWindow` does not own an independent event loop.

Embedded views keep their separate host-owned lifecycle and non-blocking polling contract:

```cpp
ui::EmbeddedView view{app_ui, parent_handle, {640.0f, 420.0f}};
if (!view.poll()) {
    // Inspect view.last_error() or the host close state as appropriate.
}
```

Construction, normal UI mutation, polling, callbacks, and destruction are confined to the UI/main thread unless a specific API documents otherwise.

## Custom drawing

Custom components and `ui::Canvas` draw through NativeUI-owned APIs only. Consumers do not include rendering-backend headers.

```cpp
class Panel final : public ui::Component {
public:
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f, 80.0f};
    }

    void paint(ui::PaintContext& context) const override {
        context.painter().fill_rounded_rect(
            context.bounds(), 8.0f, ui::Color{0.12f, 0.14f, 0.18f, 1.0f});
    }
};
```

`ui::CanvasContext2D` exposes the same backend-neutral drawing model for declarative custom views, including transforms, clipping, paths, text, images, and gradients.

## Installed CMake contract

The low-level v1 package contract is:

```cmake
find_package(NativeUI CONFIG REQUIRED)

target_link_libraries(MyTarget PRIVATE NativeUI::Core)
nativeui_attach_platform(
    TARGET MyTarget
    CONSUMER_ID com.example.target
)
```

For standalone executables, `nativeui_add_application()` provides the high-level T054 application helper. `CONSUMER_ID` is a stable identity for the final target and is required by the native attachment contract.

The package ships the implementation assets required by its pinned backend versions, but those assets are not consumer compile-interface headers or public C++ backend APIs.

## Resources, services, and dispatch

`ResourceManager`, `Image`, `SvgIcon`, embedded resources, and font registration provide NativeUI-owned resource types. `Dispatcher` schedules UI-domain work and timers. `DesktopServices` exposes platform-neutral file dialogs, URL opening, reveal-in-file-manager, and related desktop operations through NativeUI types.

Application adapters must bridge audio/worker threads to the UI domain explicitly. `State<T>` is an observable UI value, not an audio-thread synchronization primitive.

## Public headers and `detail/`

Normal v1 API is declared by headers directly under `include/nativeui/` and by `nativeui/nativeui.hpp`. `nativeui/detail/*` is implementation-only and unsupported for direct consumer use even when a detail file must be physically installed to support inline public headers.

The complete v1 source contract, ownership/threading rules, and API-family inventory are recorded in `docs/V1_API.md`.

## Source compatibility and SemVer

NativeUI 1.x treats the documented normal C++ API and the documented CMake target/helper contract as source-stable. Removing or renaming a stable API, adding a new required argument, or incompatibly changing documented semantics requires the next major version unless the previous behavior was explicitly undefined or is being corrected as a bug.

Public aggregate structs require the same source-compatibility care: adding fields can change aggregate initialization and is treated as API evolution rather than a harmless implementation change.

NativeUI does not promise C++ binary ABI compatibility across compiler, standard-library, runtime, architecture, or toolchain changes. Implementation/detail symbols and headers are not stable API.

## Implementation dependency pins

NativeUI acquires implementation dependencies through CMake/CPM. The source tree currently pins:

- `hemduf/pugl` commit `723474fa43a5d1b08be2446966a4db9007b749c6`;
- `olilarkin/skia-builder` release `chrome/m149`.

These pins define NativeUI's implementation build, not a direct consumer API.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Core-only/display-less validation:

```bash
cmake -S . -B build-core \
  -DNATIVEUI_BUILD_PLATFORM=OFF \
  -DNATIVEUI_BUILD_EXAMPLES=OFF
cmake --build build-core -j
ctest --test-dir build-core --output-on-failure
```

Platform smoke tests can be enabled with `-DNATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS=ON`. Golden baselines are updated only through the explicit `nativeui_update_goldens` target after intentional visual review.
