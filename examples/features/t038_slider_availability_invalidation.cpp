#include "example_support.hpp"

namespace {

int slider_contract() {
    constexpr ui::Size size{220.0f, 72.0f};

    {
        ui::State<float> value{0.5f};
        ui::State<bool> enabled{true};
        ui::SliderStyle style;
        style.disabled.thumb_diameter = 40.0f;

        ui::UI tree{ui::Enabled{
            enabled,
            ui::Slider{value}.range(0.0f, 1.0f).style(style)
        }};
        tree.resize(size);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Slider disabled-layout baseline failed");

        enabled.set(false);
        if (!tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail(
                "layout-affecting Slider disabled style did not invalidate layout + paint");
        }
    }

    {
        ui::State<float> value{0.5f};
        ui::State<bool> read_only{false};
        ui::SliderStyle style;
        style.read_only.thumb = ui::Color{0.73f, 0.28f, 0.61f, 1.0f};

        ui::UI tree{ui::ReadOnly{
            read_only,
            ui::Slider{value}.range(0.0f, 1.0f).style(style)
        }};
        tree.resize(size);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Slider read-only baseline failed");

        read_only.set(true);
        if (tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail(
                "paint-only Slider read-only style did not remain paint-only");
        }
    }

    return 0;
}

int range_slider_contract() {
    constexpr ui::Size size{220.0f, 72.0f};

    {
        ui::State<ui::RangeValue> value{ui::RangeValue{0.25f, 0.75f}};
        ui::State<bool> enabled{true};
        ui::SliderStyle style;
        style.disabled.focus_ring_width = 9.0f;

        ui::UI tree{ui::Enabled{
            enabled,
            ui::RangeSlider{value}.range(0.0f, 1.0f).style(style)
        }};
        tree.resize(size);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("RangeSlider disabled-layout baseline failed");
        }

        enabled.set(false);
        if (!tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail(
                "layout-affecting RangeSlider disabled style did not invalidate layout + paint");
        }
    }

    {
        ui::State<ui::RangeValue> value{ui::RangeValue{0.25f, 0.75f}};
        ui::State<bool> read_only{false};
        ui::SliderStyle style;
        style.read_only.active = ui::Color{0.17f, 0.68f, 0.42f, 1.0f};

        ui::UI tree{ui::ReadOnly{
            read_only,
            ui::RangeSlider{value}.range(0.0f, 1.0f).style(style)
        }};
        tree.resize(size);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("RangeSlider read-only baseline failed");
        }

        read_only.set(true);
        if (tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail(
                "paint-only RangeSlider read-only style did not remain paint-only");
        }
    }

    return 0;
}

int self_test() {
    if (const int result = slider_contract(); result != 0) return result;
    return range_slider_contract();
}

ui::UI make_demo() {
    return ui::UI{ui::Column{
        ui::Header{"T038 — Slider Availability Invalidation"},
        ui::Label{"Disabled/read-only style variants classify paint vs layout changes."}.size(12.0f),
    }.gap(12.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    auto tree = make_demo();
    return example::run_window(
        tree,
        "NativeUI T038 Slider Availability Invalidation",
        {660.0f, 220.0f});
}
