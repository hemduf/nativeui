#include "test_support.hpp"

#include <cmath>
#include <limits>

namespace {

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

void slider_pointer_keyboard_contract() {
    test::MockPlatform platform;

    {
        ui::State<float> value{0.0f};
        ui::UI tree{ui::Slider{value}.range(0.0f, 1.0f).step(0.25f)};
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 100.0f, 30.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK_NEAR(value.get(), 0.5f, 0.0001f);
        tree.dispatch(test::pointer(ui::InputType::PointerMove, 200.0f, 30.0f), platform);
        NUI_CHECK_NEAR(value.get(), 1.0f, 0.0001f);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 200.0f, 30.0f), platform);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);

        value.set(0.5f);
        tree.dispatch(test::key(ui::Key::Right, true), platform);
        NUI_CHECK_NEAR(value.get(), 0.75f, 0.0001f);
        tree.dispatch(test::key(ui::Key::Left), platform);
        NUI_CHECK_NEAR(value.get(), 0.5f, 0.0001f);
        tree.dispatch(test::key(ui::Key::Home), platform);
        NUI_CHECK_NEAR(value.get(), 0.0f, 0.0001f);
        tree.dispatch(test::key(ui::Key::End), platform);
        NUI_CHECK_NEAR(value.get(), 1.0f, 0.0001f);
    }

    {
        ui::State<float> value{50.0f};
        ui::UI tree{ui::Slider{value}.range(0.0f, 100.0f)};
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);
        tree.dispatch(test::key(ui::Key::Right), platform);
        NUI_CHECK_NEAR(value.get(), 51.0f, 0.0001f);
        tree.dispatch(test::key(ui::Key::Right, true), platform);
        NUI_CHECK_NEAR(value.get(), 51.1f, 0.0001f);
    }

    {
        ui::State<float> value{0.0f};
        ui::UI tree{ui::Slider{value}
                        .range(-1.0f, 1.0f)
                        .orientation(ui::SliderOrientation::Vertical)};
        tree.resize({60.0f, 200.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 0.0f), platform);
        NUI_CHECK_NEAR(value.get(), 1.0f, 0.0001f);
        tree.dispatch(test::pointer(ui::InputType::PointerMove, 30.0f, 200.0f), platform);
        NUI_CHECK_NEAR(value.get(), -1.0f, 0.0001f);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 200.0f), platform);
    }
}

void slider_t059_contract() {
    test::MockPlatform platform;

    {
        ui::State<float> value{0.5f};
        ui::State<bool> read_only{true};
        int writes = 0;
        auto observer = value.observe([&](const float&) { ++writes; });
        ui::UI tree{ui::ReadOnly{read_only, ui::Slider{value}.range(0.0f, 1.0f)}};
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 180.0f, 30.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK_NEAR(value.get(), 0.5f, 0.0001f);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK_NEAR(value.get(), 0.5f, 0.0001f);
        NUI_CHECK(writes == 0);
    }

    {
        ui::State<float> value{0.5f};
        ui::State<bool> enabled{true};
        ui::UI tree{ui::Enabled{enabled, ui::Slider{value}.range(0.0f, 1.0f)}};
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 100.0f, 30.0f), platform);
        enabled.set(false);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) ==
                  ui::EventResult::Ignored);
    }
}

void range_slider_pointer_keyboard_contract() {
    test::MockPlatform platform;

    {
        ui::State<ui::RangeValue> value{ui::RangeValue{0.25f, 0.75f}};
        ui::UI tree{ui::RangeSlider{value}.range(0.0f, 1.0f).step(0.25f)};
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 100.0f, 30.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK_NEAR(value.get().low, 0.5f, 0.0001f);
        NUI_CHECK_NEAR(value.get().high, 0.75f, 0.0001f);

        tree.dispatch(test::pointer(ui::InputType::PointerMove, 190.0f, 30.0f), platform);
        NUI_CHECK_NEAR(value.get().low, 0.75f, 0.0001f);
        NUI_CHECK_NEAR(value.get().high, 0.75f, 0.0001f);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 190.0f, 30.0f), platform);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);

        value.set(ui::RangeValue{0.25f, 0.75f});
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 150.0f, 30.0f), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 150.0f, 30.0f), platform);
        value.set(ui::RangeValue{0.25f, 0.75f});
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 100.0f, 30.0f), platform);
        NUI_CHECK_NEAR(value.get().low, 0.25f, 0.0001f);
        NUI_CHECK_NEAR(value.get().high, 0.5f, 0.0001f);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 100.0f, 30.0f), platform);

        tree.dispatch(test::key(ui::Key::Home), platform);
        NUI_CHECK_NEAR(value.get().low, 0.25f, 0.0001f);
        NUI_CHECK_NEAR(value.get().high, 0.25f, 0.0001f);
        tree.dispatch(test::key(ui::Key::End), platform);
        NUI_CHECK_NEAR(value.get().high, 1.0f, 0.0001f);
        tree.dispatch(test::key(ui::Key::Left, true), platform);
        NUI_CHECK_NEAR(value.get().high, 0.75f, 0.0001f);
    }

    {
        ui::State<ui::RangeValue> value{ui::RangeValue{-0.5f, 0.5f}};
        ui::UI tree{ui::RangeSlider{value}
                        .range(-1.0f, 1.0f)
                        .orientation(ui::SliderOrientation::Vertical)};
        tree.resize({60.0f, 200.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 0.0f), platform);
        NUI_CHECK_NEAR(value.get().low, -0.5f, 0.0001f);
        NUI_CHECK_NEAR(value.get().high, 1.0f, 0.0001f);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 0.0f), platform);
    }
}

void range_slider_t059_contract() {
    test::MockPlatform platform;

    {
        ui::State<ui::RangeValue> value{ui::RangeValue{0.25f, 0.75f}};
        ui::State<bool> read_only{true};
        int writes = 0;
        auto observer = value.observe([&](const ui::RangeValue&) { ++writes; });
        ui::UI tree{ui::ReadOnly{read_only,
            ui::RangeSlider{value}.range(0.0f, 1.0f).step(0.25f)}};
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 100.0f, 30.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK_NEAR(value.get().low, 0.25f, 0.0001f);
        NUI_CHECK_NEAR(value.get().high, 0.75f, 0.0001f);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(writes == 0);
    }

    {
        ui::State<ui::RangeValue> value{ui::RangeValue{0.25f, 0.75f}};
        ui::State<bool> enabled{true};
        ui::UI tree{ui::Enabled{enabled,
            ui::RangeSlider{value}.range(0.0f, 1.0f)}};
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 50.0f, 30.0f), platform);
        enabled.set(false);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) ==
                  ui::EventResult::Ignored);
    }
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

    const float extreme = std::numeric_limits<float>::max();
    const ui::detail::BoundedDisplayDomain wide{-extreme, extreme};
    NUI_CHECK_NEAR(wide.fraction(-extreme), 0.0f, 0.0001f);
    NUI_CHECK_NEAR(wide.fraction(0.0f), 0.5f, 0.0001f);
    NUI_CHECK_NEAR(wide.fraction(extreme), 1.0f, 0.0001f);

    const ui::Rect bounds{10.0f, 20.0f, 200.0f, 100.0f};
    const auto wide_full = wide.fill_rect(bounds, extreme, ui::ProgressOrientation::Horizontal);
    NUI_CHECK(std::isfinite(wide_full.w));
    NUI_CHECK_NEAR(wide_full.w, 200.0f, 0.0001f);

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

    NUI_CHECK(probe_state->focus_in == 1);
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK(probe_state->key_events == 1);
    NUI_CHECK_NEAR(progress.get(), 0.50f, 0.0001f);
    NUI_CHECK_NEAR(meter.get(), 0.25f, 0.0001f);

    progress.set(2.0f);
    meter.set(std::numeric_limits<float>::quiet_NaN());
    NUI_CHECK_NEAR(progress.get(), 2.0f, 0.0001f);
    NUI_CHECK(std::isnan(meter.get()));
}

void progress_meter_visual_and_idle_contract() {
    {
        ui::State<float> value{0.0f};
        ui::UI tree{ui::ProgressBar{value}};
        ui::HeadlessRenderer renderer{{200.0f, 40.0f}, 1.0f};

        NUI_CHECK(renderer.render(tree));
        const auto empty_frame = renderer.rgba_pixels();

        value.set(1.0f);
        NUI_CHECK(tree.paint_dirty());
        NUI_CHECK(!tree.layout_dirty());
        NUI_CHECK(renderer.render(tree));
        const auto full_frame = renderer.rgba_pixels();
        NUI_CHECK(full_frame != empty_frame);

        value.set(0.5f);
        NUI_CHECK(renderer.render(tree));
        const auto half_frame = renderer.rgba_pixels();
        NUI_CHECK(half_frame != empty_frame);
        NUI_CHECK(half_frame != full_frame);

        value.set(2.0f);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(renderer.rgba_pixels() == full_frame);
        NUI_CHECK_NEAR(value.get(), 2.0f, 0.0001f);

        value.set(std::numeric_limits<float>::quiet_NaN());
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(renderer.rgba_pixels() == empty_frame);
        NUI_CHECK(std::isnan(value.get()));
    }

    {
        ui::State<float> value{0.0f};
        ui::UI tree{ui::Meter{value}.orientation(ui::ProgressOrientation::Vertical)};
        ui::HeadlessRenderer renderer{{40.0f, 200.0f}, 1.0f};

        NUI_CHECK(renderer.render(tree));
        const auto empty_frame = renderer.rgba_pixels();

        value.set(1.0f);
        NUI_CHECK(renderer.render(tree));
        const auto full_frame = renderer.rgba_pixels();
        NUI_CHECK(full_frame != empty_frame);

        value.set(0.5f);
        NUI_CHECK(renderer.render(tree));
        const auto half_frame = renderer.rgba_pixels();
        NUI_CHECK(half_frame != empty_frame);
        NUI_CHECK(half_frame != full_frame);

        value.set(2.0f);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(renderer.rgba_pixels() == full_frame);
        NUI_CHECK_NEAR(value.get(), 2.0f, 0.0001f);

        value.set(std::numeric_limits<float>::quiet_NaN());
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(renderer.rgba_pixels() == empty_frame);
        NUI_CHECK(std::isnan(value.get()));
    }

    {
        ui::State<float> value{0.2f};
        ui::UI tree{ui::Meter{value}};
        test::MockPlatform platform;
        tree.resize({200.0f, 40.0f});
        tree.activate(platform);
        ui::HeadlessRenderer renderer{{200.0f, 40.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(!tree.paint_dirty());
        NUI_CHECK(!tree.layout_dirty());

        int invalidations = 0;
        tree.set_invalidation_callback([&invalidations](ui::Rect) { ++invalidations; });
        tree.dispatch(test::pointer(ui::InputType::PointerMove, 30.0f, 20.0f), platform);
        tree.dispatch(test::key(ui::Key::Right), platform);
        NUI_CHECK(invalidations == 0);
        NUI_CHECK(!tree.paint_dirty());
        NUI_CHECK(!tree.layout_dirty());

        value.set(0.8f);
        NUI_CHECK(invalidations == 1);
        NUI_CHECK(tree.paint_dirty());
        NUI_CHECK(!tree.layout_dirty());
    }

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
    slider_pointer_keyboard_contract();
    slider_t059_contract();
    range_slider_pointer_keyboard_contract();
    range_slider_t059_contract();
    progress_meter_numeric_contract();
    progress_meter_contract();
    progress_meter_visual_and_idle_contract();
}

} // namespace

int main() { return test::run("widgets", &suite); }
