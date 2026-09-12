#include "example_support.hpp"

namespace {

int combo_anchor_contract() {
    constexpr ui::Size size{260.0f, 96.0f};
    example::Platform platform;

    {
        ui::State<int> selected{1};
        ui::ComboBoxStyle style;
        const ui::Color border{0.28f, 0.46f, 0.66f, 1.0f};
        style.base.border = border;
        style.hovered.border = border;

        ui::UI tree{ui::ComboBox<int>{selected, {{1, "One", true}, {2, "Two", true}}}.style(style)};
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("ComboBox baseline render failed");

        tree.dispatch(example::pointer(ui::InputType::PointerMove, 20.0f, 20.0f), platform);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("equal resolved ComboBox hover style invalidated the tree");
        }
    }

    {
        ui::State<int> selected{1};
        ui::ComboBoxStyle style;
        style.hovered.control_height = 74.0f;

        ui::UI tree{ui::ComboBox<int>{selected, {{1, "One", true}, {2, "Two", true}}}.style(style)};
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("ComboBox layout baseline render failed");

        tree.dispatch(example::pointer(ui::InputType::PointerMove, 20.0f, 20.0f), platform);
        if (!tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail("layout-affecting ComboBox hover style did not invalidate layout + paint");
        }
    }

    return 0;
}

int tabs_contract() {
    constexpr ui::Size size{360.0f, 180.0f};
    example::Platform platform;

    {
        ui::State<int> selected{1};
        ui::TabsStyle style;
        const ui::Color fill{0.16f, 0.22f, 0.30f, 1.0f};
        const ui::Color text{0.82f, 0.84f, 0.88f, 1.0f};
        style.base.tab_fill = fill;
        style.base.text = text;
        style.hovered.tab_fill = fill;
        style.hovered.text = text;

        ui::UI tree{ui::Tabs<int>{selected}
            .tab(1, "One", ui::Spacer{280.0f, 64.0f})
            .tab(2, "Two", ui::Spacer{280.0f, 64.0f})
            .style(style)};
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Tabs hover baseline render failed");

        tree.dispatch(example::pointer(ui::InputType::PointerMove, 270.0f, 20.0f), platform);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("equal resolved Tabs hover style invalidated the tree");
        }
    }

    {
        ui::State<int> selected{1};
        ui::TabsStyle style;
        style.hovered.header_height = 72.0f;

        ui::UI tree{ui::Tabs<int>{selected}
            .tab(1, "One", ui::Spacer{280.0f, 64.0f})
            .tab(2, "Two", ui::Spacer{280.0f, 64.0f})
            .style(style)};
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Tabs hover-layout baseline render failed");

        tree.dispatch(example::pointer(ui::InputType::PointerMove, 270.0f, 20.0f), platform);
        if (!tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail("layout-affecting Tabs hover style did not invalidate layout + paint");
        }
    }

    return 0;
}

int availability_contract() {
    constexpr ui::Size size{320.0f, 120.0f};
    example::Platform platform;
    ui::State<float> value{0.5f};
    ui::State<bool> enabled{true};
    ui::ProgressBarStyle style;
    style.disabled.horizontal_size = ui::Size{224.0f, 52.0f};

    ui::UI tree{ui::Enabled{
        enabled,
        ui::ProgressBar{value}.style(style)}};
    tree.resize(size);
    tree.activate(platform);
    ui::HeadlessRenderer renderer{size, 1.0f};
    if (!renderer.render(tree)) {
        return example::fail("availability-style baseline render failed");
    }

    enabled.set(false);
    if (!tree.layout_dirty() || !tree.paint_dirty()) {
        return example::fail(
            "layout-affecting disabled style did not invalidate layout + paint");
    }
    return 0;
}

int button_availability_contract() {
    constexpr ui::Size size{320.0f, 120.0f};
    example::Platform platform;

    {
        ui::State<bool> enabled{true};
        ui::ButtonStyle style;
        style.disabled.control_height = 72.0f;

        ui::UI tree{ui::Enabled{
            enabled,
            ui::Button{"Action", [] {}}.style(style)}};
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("Button availability-layout baseline render failed");
        }

        enabled.set(false);
        if (!tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail(
                "layout-affecting Button disabled style did not invalidate layout + paint");
        }
    }

    {
        ui::State<bool> enabled{true};
        ui::ButtonStyle style;
        style.disabled.fill = ui::Color{0.12f, 0.18f, 0.24f, 1.0f};

        ui::UI tree{ui::Enabled{
            enabled,
            ui::Button{"Action", [] {}}.style(style)}};
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("Button paint-only availability baseline render failed");
        }

        enabled.set(false);
        if (tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail(
                "paint-only Button disabled style invalidated layout or missed repaint");
        }
    }

    return 0;
}

int self_test() {
    if (const int result = combo_anchor_contract(); result != 0) return result;
    if (const int result = tabs_contract(); result != 0) return result;
    if (const int result = availability_contract(); result != 0) return result;
    if (const int result = button_availability_contract(); result != 0) return result;
    return 0;
}

ui::UI make_demo() {
    return ui::UI{ui::Column{
        ui::Header{"T038 — Closure invalidation"},
        ui::Label{"Final state-aware invalidation checks for ComboBox, Tabs and availability variants."}.size(12.0f),
    }.gap(12.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    auto tree = make_demo();
    return example::run_window(tree, "NativeUI T038 Closure Invalidation", {620.0f, 220.0f});
}
