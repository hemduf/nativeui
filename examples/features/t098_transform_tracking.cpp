#include "example_support.hpp"

#include <cmath>
#include <memory>
#include <vector>

namespace {

[[nodiscard]] bool near(float a, float b, float epsilon = 1.0e-4f) noexcept {
    return std::abs(a - b) <= epsilon;
}

[[nodiscard]] bool same_transform(const ui::Transform2D& a,
                                  const ui::Transform2D& b,
                                  float epsilon = 1.0e-4f) noexcept {
    return near(a.m00, b.m00, epsilon) &&
           near(a.m01, b.m01, epsilon) &&
           near(a.m02, b.m02, epsilon) &&
           near(a.m10, b.m10, epsilon) &&
           near(a.m11, b.m11, epsilon) &&
           near(a.m12, b.m12, epsilon);
}

class TransformTrackingDemoComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {360.0f, 180.0f};
    }

    void paint(ui::PaintContext& context) const override {
        auto& painter = context.painter();
        painter.fill_rounded_rect(context.bounds(), 12.0f, ui::colors::panel);

        const auto entry_transform = painter.current_transform();
        {
            auto state = painter.scoped_state();
            painter.translate(92.0f, 78.0f);
            painter.rotate(0.24f);
            painter.scale(1.3f, 0.8f);

            const auto tracked = painter.current_transform();
            const auto expected =
                entry_transform *
                ui::Transform2D::translation(92.0f, 78.0f) *
                ui::Transform2D::rotation(0.24f) *
                ui::Transform2D::scaling(1.3f, 0.8f);
            const bool tracker_ok = same_transform(tracked, expected) &&
                                    tracked.inverse().has_value();

            painter.fill_rounded_rect(
                {-42.0f, -26.0f, 84.0f, 52.0f},
                10.0f,
                tracker_ok ? ui::colors::accent : ui::Color{1.0f, 0.0f, 0.0f, 1.0f});
        }

        // Leaving the scope restores the exact transform that was active when
        // this component entered, including any parent/component transform.
        const bool restored_ok = same_transform(
            painter.current_transform(), entry_transform, 1.0e-6f);
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
            return example::fail("scope restore did not recover entry transform");
        }
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree,
                               "NativeUI T098 - Transform Tracking",
                               {400.0f, 240.0f});
}
