#include "example_support.hpp"

#include <cmath>
#include <memory>
#include <vector>

namespace {

class TransformTrackingDemoComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {360.0f, 180.0f};
    }

    void paint(ui::PaintContext& context) const override {
        auto& painter = context.painter();
        painter.fill_rounded_rect(context.bounds(), 12.0f, ui::colors::panel);

        {
            auto state = painter.scoped_state();
            painter.translate(92.0f, 78.0f);
            painter.rotate(0.24f);
            painter.scale(1.3f, 0.8f);

            const auto tracked = painter.current_transform();
            const auto origin = tracked.map_point({0.0f, 0.0f});
            const bool tracker_ok = std::abs(origin.x - 92.0f) < 1.0e-4f &&
                                    std::abs(origin.y - 78.0f) < 1.0e-4f &&
                                    tracked.inverse().has_value();

            painter.fill_rounded_rect(
                {-42.0f, -26.0f, 84.0f, 52.0f},
                10.0f,
                tracker_ok ? ui::colors::accent : ui::Color{1.0f, 0.0f, 0.0f, 1.0f});
        }

        // Leaving the scope must restore exact logical identity for the next
        // independent draw.
        const auto restored = painter.current_transform();
        const auto restored_origin = restored.map_point({0.0f, 0.0f});
        const bool restored_ok = std::abs(restored_origin.x) < 1.0e-6f &&
                                 std::abs(restored_origin.y) < 1.0e-6f;
        painter.fill_rounded_rect(
            {250.0f, 58.0f, 64.0f, 40.0f},
            8.0f,
            restored_ok ? ui::colors::accent : ui::Color{1.0f, 0.0f, 0.0f, 1.0f});

        painter.text({180.0f, 142.0f},
                     "Tracked affine transform + exact scope restore",
                     12.0f,
                     ui::colors::textMuted,
                     ui::TextAlign::Center);
    }
};

class TransformTrackingDemo {
public:
    ui::Spec spec() && {
        return ui::Spec{
            [] { return std::make_unique<TransformTrackingDemoComponent>(); },
            {}};
    }
};

bool accent_like(ui::Rgba8 pixel) {
    return pixel.r > 180 && pixel.g > 90 && pixel.b < 120 && pixel.a > 220;
}

} // namespace

int main(int argc, char** argv) {
    auto make_ui = [] {
        return std::make_unique<ui::UI>(TransformTrackingDemo{});
    };

    if (example::self_test_requested(argc, argv)) {
        auto tree = make_ui();
        ui::HeadlessRenderer renderer{{360.0f, 180.0f}, 1.0f};
        if (!renderer.render(*tree)) return example::fail("headless render failed");
        if (!accent_like(renderer.pixel(92, 78))) {
            return example::fail("tracked transformed shape did not render at scene origin");
        }
        if (!accent_like(renderer.pixel(282, 78))) {
            return example::fail("scope restore did not recover logical identity");
        }
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree,
                               "NativeUI T098 - Transform Tracking",
                               {400.0f, 240.0f});
}
