#include "example_support.hpp"

#include <memory>
#include <vector>

namespace {

class ScopedClippingDemoComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {420.0f, 160.0f};
    }

    void paint(ui::PaintContext& context) const override {
        auto& painter = context.painter();
        const auto bounds = context.bounds();
        painter.fill_rounded_rect(bounds, 12.0f, ui::colors::panel);

        const ui::Rect rect_clip{bounds.x + 20.0f, bounds.y + 24.0f, 100.0f, 100.0f};
        {
            auto clip = painter.scoped_clip(rect_clip);
            painter.circle({rect_clip.x + 50.0f, rect_clip.y + 50.0f},
                           68.0f,
                           ui::colors::accent);
        }

        const ui::Rect rounded_clip{bounds.x + 160.0f, bounds.y + 24.0f, 100.0f, 100.0f};
        {
            auto clip = painter.scoped_clip(rounded_clip, 22.0f);
            painter.circle({rounded_clip.x + 50.0f, rounded_clip.y + 50.0f},
                           68.0f,
                           ui::colors::accent);
        }

        ui::Path triangle;
        triangle.move_to({bounds.x + 350.0f, bounds.y + 20.0f})
                .line_to({bounds.x + 404.0f, bounds.y + 124.0f})
                .line_to({bounds.x + 296.0f, bounds.y + 124.0f})
                .close();
        {
            auto clip = painter.scoped_clip(triangle);
            painter.circle({bounds.x + 350.0f, bounds.y + 78.0f},
                           70.0f,
                           ui::colors::accent);
        }

        painter.text({bounds.x + 70.0f, bounds.y + 146.0f},
                     "Rect", 11.0f, ui::colors::textMuted, ui::TextAlign::Center);
        painter.text({bounds.x + 210.0f, bounds.y + 146.0f},
                     "Rounded", 11.0f, ui::colors::textMuted, ui::TextAlign::Center);
        painter.text({bounds.x + 350.0f, bounds.y + 146.0f},
                     "Path", 11.0f, ui::colors::textMuted, ui::TextAlign::Center);
    }
};

class ScopedClippingDemo {
public:
    ui::Spec spec() && {
        return ui::Spec{
            [] { return std::make_unique<ScopedClippingDemoComponent>(); },
            {}};
    }
};

bool accent_like(ui::Rgba8 pixel) {
    return pixel.r > 180 && pixel.g > 90 && pixel.b < 120 && pixel.a > 220;
}

bool panel_like(ui::Rgba8 pixel) {
    return pixel.r < 80 && pixel.g < 80 && pixel.b < 80 && pixel.a > 220;
}

} // namespace

int main(int argc, char** argv) {
    auto make_ui = [] {
        return std::make_unique<ui::UI>(ScopedClippingDemo{});
    };

    if (example::self_test_requested(argc, argv)) {
        auto tree = make_ui();
        ui::HeadlessRenderer renderer{{420.0f, 160.0f}, 1.0f};
        if (!renderer.render(*tree)) return example::fail("headless render failed");

        if (!accent_like(renderer.pixel(70, 74))) {
            return example::fail("rect scoped clip did not render its center");
        }
        if (!accent_like(renderer.pixel(210, 74))) {
            return example::fail("rounded scoped clip did not render its center");
        }
        if (!accent_like(renderer.pixel(350, 78))) {
            return example::fail("path scoped clip did not render its center");
        }
        if (!panel_like(renderer.pixel(8, 8))) {
            return example::fail("scoped clip leaked outside its bounds");
        }
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree,
                               "NativeUI T075 - Scoped Clipping",
                               {460.0f, 220.0f});
}
