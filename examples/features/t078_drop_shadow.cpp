#include "example_support.hpp"

#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

class DropShadowDemoComponent final : public ui::Component {
public:
    DropShadowDemoComponent()
        : shadow_(ui::Effect::drop_shadow(
              {8.0f, 8.0f}, 6.0f, {0.0f, 0.0f, 0.0f, 0.55f})) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {420.0f, 190.0f};
    }

    [[nodiscard]] ui::VisualOutset visual_outset() const noexcept override {
        return shadow_.visual_outset();
    }

    void paint(ui::PaintContext& context) const override {
        auto& painter = context.painter();
        const auto bounds = context.bounds();
        painter.fill_rounded_rect(bounds, 12.0f, {0.055f, 0.06f, 0.075f, 1.0f});

        const ui::Rect card{bounds.x + 58.0f, bounds.y + 42.0f, 112.0f, 70.0f};
        {
            auto layer = painter.scoped_layer(card, shadow_);
            painter.fill_rounded_rect(card, 14.0f, {0.18f, 0.52f, 0.95f, 1.0f});
        }

        const ui::Rect mask{bounds.x + 252.0f, bounds.y + 42.0f, 92.0f, 70.0f};
        {
            auto layer = painter.scoped_layer(
                mask,
                ui::Effect::drop_shadow_only(
                    {10.0f, 6.0f}, 7.0f, {1.0f, 0.24f, 0.10f, 0.8f}));
            painter.fill_rounded_rect(mask, 14.0f, {1.0f, 1.0f, 1.0f, 1.0f});
        }

        painter.text({114.0f, 148.0f}, "DropShadow",
                     12.0f, ui::colors::textMuted, ui::TextAlign::Center);
        painter.text({298.0f, 148.0f}, "DropShadowOnly",
                     12.0f, ui::colors::textMuted, ui::TextAlign::Center);
    }

private:
    ui::Effect shadow_;
};

class DropShadowDemo {
public:
    ui::Spec spec() && {
        return ui::Spec{
            [] { return std::make_unique<DropShadowDemoComponent>(); },
            {}};
    }
};

const char* self_test() {
    ui::UI tree{DropShadowDemo{}};
    ui::HeadlessRenderer renderer{{420.0f, 190.0f}, 1.0f};
    if (!renderer.render(tree)) return "drop-shadow demo render failed";

    const auto source = renderer.pixel(110, 76);
    const auto shadow = renderer.pixel(176, 96);
    if (!(source.b > shadow.b + 40)) return "DropShadow source/shadow contrast is missing";

    const auto only_shadow = renderer.pixel(354, 92);
    const auto only_source = renderer.pixel(270, 76);
    if (!(only_shadow.r > only_shadow.g + 8)) return "DropShadowOnly tint is missing";
    if (only_source.r > 180 && only_source.g > 180 && only_source.b > 180) {
        return "DropShadowOnly unexpectedly composited its white source";
    }
    return nullptr;
}

int platform_smoke() {
    const char* stage = "application";
    try {
        ui::Application application;
        if (!application.valid()) {
            return example::fail(application.last_error().empty()
                                     ? "T078 platform application is invalid"
                                     : application.last_error());
        }

        stage = "standalone";
        auto standalone_ui = std::make_unique<ui::UI>(DropShadowDemo{});
        ui::StandaloneWindow standalone{
            application,
            *standalone_ui,
            ui::WindowDesc{
                .title = "NativeUI T078 platform smoke",
                .size = {460.0f, 250.0f},
                .resizable = true}};
        if (!standalone.valid() || !standalone.native_handle()) {
            return example::fail(standalone.last_error().empty()
                                     ? "T078 standalone window is invalid"
                                     : standalone.last_error());
        }

        stage = "embedded";
        auto embedded_ui = std::make_unique<ui::UI>(DropShadowDemo{});
        ui::EmbeddedView embedded{
            *embedded_ui, standalone.native_handle(), {420.0f, 190.0f}};
        if (!embedded.native_handle()) {
            return example::fail(embedded.last_error().empty()
                                     ? "T078 embedded view is invalid"
                                     : embedded.last_error());
        }

        stage = "native-paint";
        for (int i = 0; i < 12; ++i) {
            (void)application.poll(0.0);
            (void)embedded.poll();
        }
        if (!standalone.last_error().empty()) return example::fail(standalone.last_error());
        if (!embedded.last_error().empty()) return example::fail(embedded.last_error());
        return 0;
    } catch (const std::exception& error) {
        return example::fail(std::string{"T078 platform smoke "} + stage + ": " + error.what());
    }
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) {
        if (const char* error = self_test()) return example::fail(error);
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--platform-smoke") {
        return platform_smoke();
    }

    auto tree = std::make_unique<ui::UI>(DropShadowDemo{});
    return example::run_window(
        *tree, "NativeUI T078 - Drop Shadows", {460.0f, 250.0f});
}
