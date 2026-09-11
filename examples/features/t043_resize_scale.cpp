#include "example_support.hpp"

#include <cmath>
#include <exception>
#include <iostream>

namespace {

ui::UI make_ui() {
    return ui::UI{
        ui::Column{
            ui::Header{"T043 / RESIZE + SCALE"},
            ui::Label{"NativeUI layout uses logical pixels; the native framebuffer uses physical pixels."},
            ui::Canvas{520.0f, 90.0f, [](ui::CanvasContext2D& g) {
                g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 10.0f, ui::colors::panel);
                g.text({14.0f, 32.0f}, "Resize or move this window between scaled displays.", 14.0f, ui::colors::text);
                g.text({14.0f, 60.0f}, "Scale and client size are reported below on each poll.", 12.0f, ui::colors::textMuted);
            }}
        }.padding(18.0f).gap(12.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) {
        auto app = make_ui();
        const auto preferred = app.measure().preferred;
        if (!std::isfinite(preferred.w) || !std::isfinite(preferred.h) ||
            preferred.w < 0.0f || preferred.h < 0.0f) {
            return example::fail("preferred logical size is invalid");
        }
        app.resize({640.0f, 320.0f});
        if (!app.dirty()) return example::fail("resize did not invalidate the retained tree");
        return 0;
    }

#ifdef NATIVEUI_EXAMPLE_SELF_TEST_ONLY
    return example::fail("window mode is disabled in self-test-only validation builds");
#else
    try {
        auto app = make_ui();
        ui::Application application;
        if (!application.valid()) {
            std::cerr << "Application error: " << application.last_error() << '\n';
            return 1;
        }

        ui::StandaloneWindow window{
            application,
            app,
            ui::WindowDesc{.title = "NativeUI T043 - Resize + Scale",
                           .size = {640.0f, 320.0f},
                           .resizable = true}};
        if (!window.valid()) {
            std::cerr << "Window error: " << window.last_error() << '\n';
            return 1;
        }

        window.set_preferred_size_callback([](ui::Size preferred) {
            std::cout << "preferred logical size: " << preferred.w << " x " << preferred.h << '\n';
        });

        ui::Size previous{};
        float previous_scale = 0.0f;
        while (!application.quit_requested()) {
            if (!application.poll(0.016) && !application.quit_requested()) {
                std::cerr << "Application error: " << application.last_error() << '\n';
                return 1;
            }

            const auto size = window.size();
            const float scale = window.scale_factor();
            if (size.w != previous.w || size.h != previous.h || scale != previous_scale) {
                std::cout << "logical client: " << size.w << " x " << size.h
                          << "  scale: " << scale << '\n';
                previous = size;
                previous_scale = scale;
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "T043 resize/scale example failed: " << error.what() << '\n';
        return 1;
    }
#endif
}
