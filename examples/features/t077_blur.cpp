#include "example_support.hpp"

#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

class BlurDemoComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {420.0f, 180.0f};
    }

    void paint(ui::PaintContext& context) const override {
        auto& painter = context.painter();
        const auto bounds = context.bounds();
        painter.fill_rounded_rect(bounds, 12.0f, {0.05f, 0.055f, 0.07f, 1.0f});

        const ui::Rect glow_bounds{bounds.x + 28.0f, bounds.y + 28.0f, 160.0f, 92.0f};
        {
            ui::PaintOptions options;
            options.opacity = 0.9f;
            auto glow = painter.scoped_layer(
                glow_bounds, ui::Effect::gaussian_blur(10.0f, 10.0f), options);
            painter.fill_rounded_rect(
                {glow_bounds.x + 42.0f, glow_bounds.y + 28.0f, 76.0f, 36.0f},
                18.0f,
                {0.10f, 0.75f, 1.0f, 1.0f});
        }

        const ui::Rect asymmetric{bounds.x + 232.0f, bounds.y + 28.0f, 150.0f, 92.0f};
        {
            auto streak = painter.scoped_layer(
                asymmetric, ui::Effect::gaussian_blur(14.0f, 2.0f));
            painter.fill_rounded_rect(
                {asymmetric.x + 48.0f, asymmetric.y + 30.0f, 54.0f, 32.0f},
                10.0f,
                {1.0f, 0.34f, 0.18f, 1.0f});
        }

        painter.text({108.0f, 150.0f}, "Gaussian 10 / 10",
                     12.0f, ui::colors::textMuted, ui::TextAlign::Center);
        painter.text({307.0f, 150.0f}, "Gaussian 14 / 2",
                     12.0f, ui::colors::textMuted, ui::TextAlign::Center);
    }
};

class BlurDemo {
public:
    ui::Spec spec() && {
        return ui::Spec{
            [] { return std::make_unique<BlurDemoComponent>(); },
            {}};
    }
};

const char* self_test() {
    ui::UI tree{BlurDemo{}};
    ui::HeadlessRenderer renderer{{420.0f, 180.0f}, 1.0f};
    if (!renderer.render(tree)) return "blur demo render failed";

    // Samples are deliberately inside the fully repainted component region.
    const auto background = renderer.pixel(30, 74);
    const auto halo = renderer.pixel(62, 74);
    const auto core = renderer.pixel(108, 74);
    if (!(halo.b > background.b + 8)) return "Gaussian halo is not visible";
    if (!(core.b > halo.b + 20)) return "Gaussian core/halo contrast is missing";

    const auto horizontal = renderer.pixel(266, 74);
    const auto vertical = renderer.pixel(307, 48);
    if (!(horizontal.r > vertical.r + 6)) return "asymmetric blur did not widen horizontally";
    return nullptr;
}

int platform_smoke() {
    const char* stage = "application";
    try {
        ui::Application application;
        if (!application.valid()) {
            return example::fail(application.last_error().empty()
                                     ? "T077 platform application is invalid"
                                     : application.last_error());
        }

        stage = "standalone";
        auto standalone_ui = std::make_unique<ui::UI>(BlurDemo{});
        ui::StandaloneWindow standalone{
            application,
            *standalone_ui,
            ui::WindowDesc{
                .title = "NativeUI T077 platform smoke",
                .size = {460.0f, 240.0f},
                .resizable = true}};
        if (!standalone.valid() || !standalone.native_handle()) {
            return example::fail(standalone.last_error().empty()
                                     ? "T077 standalone window is invalid"
                                     : standalone.last_error());
        }

        stage = "embedded";
        auto embedded_ui = std::make_unique<ui::UI>(BlurDemo{});
        ui::EmbeddedView embedded{
            *embedded_ui, standalone.native_handle(), {420.0f, 180.0f}};
        if (!embedded.native_handle()) {
            return example::fail(embedded.last_error().empty()
                                     ? "T077 embedded view is invalid"
                                     : embedded.last_error());
        }

        // Pump both real native renderers long enough to force the T077 effect
        // through Skia Ganesh/OpenGL in standalone and embedded contexts.
        stage = "native-paint";
        for (int i = 0; i < 12; ++i) {
            (void)application.poll(0.0);
            (void)embedded.poll();
        }
        if (!standalone.last_error().empty()) return example::fail(standalone.last_error());
        if (!embedded.last_error().empty()) return example::fail(embedded.last_error());
        return 0;
    } catch (const std::exception& error) {
        return example::fail(std::string{"T077 platform smoke "} + stage + ": " + error.what());
    }
}

} // namespace

int main(int argc, char** argv) {
    auto make_ui = [] {
        return std::make_unique<ui::UI>(BlurDemo{});
    };

    if (example::self_test_requested(argc, argv)) {
        if (const char* error = self_test()) return example::fail(error);
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--platform-smoke") {
        return platform_smoke();
    }

    auto tree = make_ui();
    return example::run_window(*tree,
                               "NativeUI T077 - Gaussian Blur",
                               {460.0f, 240.0f});
}
