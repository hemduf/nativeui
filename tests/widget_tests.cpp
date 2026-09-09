#include "test_support.hpp"

#include <algorithm>
#include <limits>

namespace {

bool pixels_match(ui::Rgba8 lhs, ui::Rgba8 rhs, int tolerance = 4) {
    const auto close = [tolerance](std::uint8_t a, std::uint8_t b) {
        return std::abs(static_cast<int>(a) - static_cast<int>(b)) <= tolerance;
    };
    return close(lhs.r, rhs.r) && close(lhs.g, rhs.g) && close(lhs.b, rhs.b) &&
           close(lhs.a, rhs.a);
}

void value_widgets_respect_effective_read_only_state() {
    ui::State<bool> read_only{true};
    ui::State<float> drive{0.50f};
    ui::State<bool> bypass{false};

    ui::UI tree{
        ui::ReadOnly{read_only,
            ui::Row{
                ui::Knob{"Drive", drive},
                ui::Toggle{"Bypass", bypass},
            }.gap(8.0f)}
    };

    test::MockPlatform platform;
    tree.resize({420.0f, 220.0f});
    tree.activate(platform);

    // Read-only remains focusable/targetable, but value mutations are consumed
    // by the value controls without changing their bound State.
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK_NEAR(drive.get(), 0.50f, 0.0001f);

    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(!bypass.get());
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(!bypass.get());

    read_only.set(false);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(bypass.get());

    tree.dispatch(test::key(ui::Key::Tab, true), platform);
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK_NEAR(drive.get(), 0.51f, 0.0001f);
}

void progress_meter_numeric_contract() {
    const ui::detail::BoundedDisplayDomain domain{-1.0f, 1.0f};
    NUI_CHECK_NEAR(domain.effective(-2.0f), -1.0f, 0.0001f);
    NUI_CHECK_NEAR(domain.effective(2.0f), 1.0f, 0.0001f);
    NUI_CHECK_NEAR(domain.effective(std::numeric_limits<float>::quiet_NaN()), -1.0f, 0.0001f);
    NUI_CHECK_NEAR(domain.effective(std::numeric_limits<float>::infinity()), -1.0f, 0.0001f);
    NUI_CHECK_NEAR(domain.fraction(-1.0f), 0.0f, 0.0001f);
    NUI_CHECK_NEAR(domain.fraction(0.0f), 0.5f, 0.0001f);
    NUI_CHECK_NEAR(domain.fraction(1.0f), 1.0f, 0.0001f);

    const ui::Rect bounds{10.0f, 20.0f, 200.0f, 100.0f};
    const auto horizontal = domain.fill_rect(bounds, 0.0f, ui::ProgressOrientation::Horizontal);
    NUI_CHECK_NEAR(horizontal.x, 10.0f, 0.0001f);
    NUI_CHECK_NEAR(horizontal.y, 20.0f, 0.0001f);
    NUI_CHECK_NEAR(horizontal.w, 100.0f, 0.0001f);
    NUI_CHECK_NEAR(horizontal.h, 100.0f, 0.0001f);

    const auto vertical = domain.fill_rect(bounds, 0.0f, ui::ProgressOrientation::Vertical);
    NUI_CHECK_NEAR(vertical.x, 10.0f, 0.0001f);
    NUI_CHECK_NEAR(vertical.y, 70.0f, 0.0001f);
    NUI_CHECK_NEAR(vertical.w, 200.0f, 0.0001f);
    NUI_CHECK_NEAR(vertical.h, 50.0f, 0.0001f);

    bool rejected = false;
    try {
        [[maybe_unused]] ui::detail::BoundedDisplayDomain invalid{1.0f, 1.0f};
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    NUI_CHECK(rejected);

    rejected = false;
    try {
        [[maybe_unused]] ui::detail::BoundedDisplayDomain invalid{
            0.0f, std::numeric_limits<float>::infinity()};
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    NUI_CHECK(rejected);
}

void progress_meter_contract() {
    ui::State<float> progress{0.50f};
    ui::State<float> meter{0.25f};
    auto probe_state = std::make_shared<test::ProbeState>();

    ui::UI tree{
        ui::Column{
            ui::ProgressBar{progress, 0.0f, 1.0f},
            ui::Meter{meter, -1.0f, 1.0f}.orientation(ui::ProgressOrientation::Vertical),
            test::Probe{probe_state},
        }
    };

    test::MockPlatform platform;
    tree.resize({320.0f, 240.0f});
    tree.activate(platform);

    // T033 widgets are display-only and never join focus traversal. The probe
    // is the first focusable node despite coming after both display widgets.
    NUI_CHECK(probe_state->focus_in == 1);
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK(probe_state->key_events == 1);
    NUI_CHECK_NEAR(progress.get(), 0.50f, 0.0001f);
    NUI_CHECK_NEAR(meter.get(), 0.25f, 0.0001f);

    // External out-of-range/non-finite values are presentation-clamped only;
    // mounting/painting must never normalize application state in place.
    progress.set(2.0f);
    meter.set(std::numeric_limits<float>::quiet_NaN());
    NUI_CHECK_NEAR(progress.get(), 2.0f, 0.0001f);
    NUI_CHECK(std::isnan(meter.get()));
}

void progress_meter_visual_and_idle_contract() {
    // Compare semantic raster states captured by the same Skia surface rather
    // than converting linear Color floats to bytes in the test. This keeps the
    // golden contract independent of backend color-space encoding while still
    // proving min/mid/max/out-of-range/NaN and fill direction.
    {
        ui::State<float> value{0.0f};
        ui::UI tree{ui::ProgressBar{value}};
        ui::HeadlessRenderer renderer{{200.0f, 40.0f}, 1.0f};

        NUI_CHECK(renderer.render(tree));
        const auto empty_left = renderer.pixel(40, 20);
        const auto empty_right = renderer.pixel(160, 20);
        NUI_CHECK(pixels_match(empty_left, empty_right));

        value.set(1.0f);
        NUI_CHECK(tree.paint_dirty());
        NUI_CHECK(!tree.layout_dirty());
        NUI_CHECK(renderer.render(tree));
        const auto filled_left = renderer.pixel(40, 20);
        const auto filled_right = renderer.pixel(160, 20);
        NUI_CHECK(pixels_match(filled_left, filled_right));
        NUI_CHECK(!pixels_match(filled_left, empty_left));

        value.set(0.5f);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixels_match(renderer.pixel(40, 20), filled_left));
        NUI_CHECK(pixels_match(renderer.pixel(160, 20), empty_right));

        value.set(2.0f);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixels_match(renderer.pixel(40, 20), filled_left));
        NUI_CHECK(pixels_match(renderer.pixel(160, 20), filled_right));
        NUI_CHECK_NEAR(value.get(), 2.0f, 0.0001f);

        value.set(std::numeric_limits<float>::quiet_NaN());
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixels_match(renderer.pixel(40, 20), empty_left));
        NUI_CHECK(pixels_match(renderer.pixel(160, 20), empty_right));
        NUI_CHECK(std::isnan(value.get()));
    }

    {
        ui::State<float> value{0.0f};
        ui::UI tree{ui::Meter{value}.orientation(ui::ProgressOrientation::Vertical)};
        ui::HeadlessRenderer renderer{{40.0f, 200.0f}, 1.0f};

        NUI_CHECK(renderer.render(tree));
        const auto empty_bottom = renderer.pixel(20, 160);
        const auto empty_top = renderer.pixel(20, 40);
        NUI_CHECK(pixels_match(empty_bottom, empty_top));

        value.set(1.0f);
        NUI_CHECK(renderer.render(tree));
        const auto filled_bottom = renderer.pixel(20, 160);
        const auto filled_top = renderer.pixel(20, 40);
        NUI_CHECK(pixels_match(filled_bottom, filled_top));
        NUI_CHECK(!pixels_match(filled_bottom, empty_bottom));

        value.set(0.5f);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixels_match(renderer.pixel(20, 160), filled_bottom));
        NUI_CHECK(pixels_match(renderer.pixel(20, 40), empty_top));

        value.set(2.0f);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixels_match(renderer.pixel(20, 160), filled_bottom));
        NUI_CHECK(pixels_match(renderer.pixel(20, 40), filled_top));
        NUI_CHECK_NEAR(value.get(), 2.0f, 0.0001f);

        value.set(std::numeric_limits<float>::quiet_NaN());
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixels_match(renderer.pixel(20, 160), empty_bottom));
        NUI_CHECK(pixels_match(renderer.pixel(20, 40), empty_top));
        NUI_CHECK(std::isnan(value.get()));
    }

    // Meter owns no animation/timer source: ignored input does not invalidate;
    // the only observed invalidation here is an external State update.
    {
        ui::State<float> value{0.2f};
        ui::UI tree{ui::Meter{value}};
        test::MockPlatform platform;
        tree.resize({200.0f, 40.0f});
        tree.activate(platform);
        int invalidations = 0;
        tree.set_invalidation_callback([&invalidations](ui::Rect) { ++invalidations; });
        tree.dispatch(test::pointer(ui::InputType::PointerMove, 30.0f, 20.0f), platform);
        tree.dispatch(test::key(ui::Key::Right), platform);
        NUI_CHECK(invalidations == 0);
        value.set(0.8f);
        NUI_CHECK(invalidations > 0);
    }

    // Formatter is presentation-only; invoking it during paint cannot write
    // normalized data back into the application state.
    {
        ui::State<float> value{2.0f};
        int formatter_calls = 0;
        ui::UI tree{ui::ProgressBar{value}.formatter([&formatter_calls](float effective) {
            ++formatter_calls;
            return std::to_string(effective);
        })};
        ui::HeadlessRenderer renderer{{200.0f, 40.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(formatter_calls == 1);
        NUI_CHECK_NEAR(value.get(), 2.0f, 0.0001f);
    }
}

void suite() {
    ui::State<float> drive{0.50f};
    ui::State<float> tone{0.25f};
    ui::State<bool> bypass{false};

    ui::UI tree{
        ui::Row{
            ui::Knob{"Drive", drive},
            ui::Knob{"Tone", tone},
            ui::Toggle{"Bypass", bypass},
        }.gap(8.0f)
    };

    test::MockPlatform platform;
    tree.resize({720.0f, 220.0f});
    tree.activate(platform);

    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK_NEAR(drive.get(), 0.51f, 0.0001f);

    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Right, true), platform);
    NUI_CHECK(tone.get() > 0.25f);
    NUI_CHECK(tone.get() < 0.26f);

    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(bypass.get());
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(!bypass.get());

    // Existing layout builders remain constructible and resizable.
    ui::State<bool> enabled{true};
    ui::UI layout_tree{
        ui::Padding{8.0f,
            ui::Stack{
                ui::Column{
                    ui::Header{"Overlay"},
                    ui::Spacer{12.0f},
                    ui::Toggle{"Enabled", enabled},
                }
            }
        }
    };
    layout_tree.resize({320.0f, 240.0f});
    layout_tree.activate(platform);
    NUI_CHECK(layout_tree.dirty());

    value_widgets_respect_effective_read_only_state();
    progress_meter_numeric_contract();
    progress_meter_contract();
    progress_meter_visual_and_idle_contract();
}

} // namespace

int main() { return test::run("widgets", &suite); }
