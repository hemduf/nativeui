#include "example_support.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace {

[[nodiscard]] bool pixel_matches(ui::Rgba8 pixel, ui::Color color, int tolerance = 3) {
    const auto channel = [](float value) {
        return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    return std::abs(static_cast<int>(pixel.r) - channel(color.r)) <= tolerance &&
           std::abs(static_cast<int>(pixel.g) - channel(color.g)) <= tolerance &&
           std::abs(static_cast<int>(pixel.b) - channel(color.b)) <= tolerance;
}

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

int list_view_contract() {
    constexpr ui::Size retained_size{240.0f, 96.0f};
    constexpr ui::Size virtual_size{240.0f, 64.0f};
    const ui::Color stable_row{0.16f, 0.22f, 0.30f, 1.0f};

    {
        example::Platform platform;
        ui::State<std::optional<int>> selected{std::nullopt};
        ui::ListViewStyle style;
        style.base.row_fill = stable_row;
        style.hovered.row_fill = stable_row;
        style.pressed.row_fill = stable_row;

        ui::UI tree{ui::ListView<int>{selected}
            .item(1, ui::Spacer{220.0f, 32.0f})
            .item(2, ui::Spacer{220.0f, 32.0f})
            .style(style)};
        tree.resize(retained_size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{retained_size, 1.0f};
        if (!renderer.render(tree)) return example::fail("retained ListView baseline render failed");

        (void)tree.dispatch(example::key(ui::Key::Tab), platform);
        if (!renderer.render(tree)) return example::fail("retained ListView focus settle failed");
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("retained ListView focus settle left the tree dirty");
        }

        (void)tree.dispatch(
            example::pointer(ui::InputType::PointerMove, 20.0f, 16.0f), platform);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("equal resolved retained ListView hover invalidated the tree");
        }

        (void)tree.dispatch(
            example::pointer(ui::InputType::PointerDown, 20.0f, 16.0f), platform);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("equal resolved retained ListView press invalidated the tree");
        }
    }

    {
        example::Platform platform;
        ui::State<std::optional<int>> selected{std::nullopt};
        ui::ListViewStyle style;
        style.base.row_fill = stable_row;
        style.hovered.row_fill = ui::Color{0.20f, 0.52f, 0.32f, 1.0f};

        ui::UI tree{ui::ListView<int>{selected}
            .item(1, ui::Spacer{220.0f, 32.0f})
            .item(2, ui::Spacer{220.0f, 32.0f})
            .style(style)};
        tree.resize(retained_size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{retained_size, 1.0f};
        if (!renderer.render(tree)) return example::fail("retained ListView hover baseline render failed");

        (void)tree.dispatch(example::key(ui::Key::Tab), platform);
        if (!renderer.render(tree)) return example::fail("retained ListView hover focus settle failed");

        (void)tree.dispatch(
            example::pointer(ui::InputType::PointerMove, 20.0f, 16.0f), platform);
        if (tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail("changed retained ListView hover missed paint or dirtied layout");
        }
    }

    using VirtualState = ui::VirtualListState<int>;

    {
        example::Platform platform;
        ui::State<std::optional<int>> selected{std::nullopt};
        VirtualState state{
            selected,
            32.0f,
            [](const VirtualState::Item&) { return ui::Spacer{220.0f, 32.0f}; }};
        if (!state.replace({
                VirtualState::Item{1, "One"},
                VirtualState::Item{2, "Two"}})) {
            return example::fail("virtual ListView equal-style dataset replacement failed");
        }

        ui::ListViewStyle style;
        const ui::Color base_surface{0.04f, 0.10f, 0.18f, 1.0f};
        const ui::Color disabled_surface{0.72f, 0.24f, 0.12f, 1.0f};
        style.base.surface_fill = base_surface;
        style.disabled.surface_fill = disabled_surface;
        style.base.row_fill = stable_row;
        style.hovered.row_fill = stable_row;
        style.pressed.row_fill = stable_row;

        ui::UI tree{ui::ListView<int>{state}.style(style)};
        tree.resize(virtual_size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{virtual_size, 1.0f};
        if (!renderer.render(tree)) return example::fail("virtual ListView baseline render failed");
        if (!pixel_matches(renderer.pixel(3, 16), base_surface)) {
            return example::fail("enabled virtual ListView resolved disabled surface style");
        }

        (void)tree.dispatch(example::key(ui::Key::Tab), platform);
        if (!renderer.render(tree)) return example::fail("virtual ListView focus settle failed");
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("virtual ListView focus settle left the tree dirty");
        }

        (void)tree.dispatch(
            example::pointer(ui::InputType::PointerMove, 20.0f, 16.0f), platform);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("equal resolved virtual ListView hover invalidated the tree");
        }

        (void)tree.dispatch(
            example::pointer(ui::InputType::PointerDown, 20.0f, 16.0f), platform);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("equal resolved virtual ListView press invalidated the tree");
        }
    }

    {
        example::Platform platform;
        ui::State<std::optional<int>> selected{std::nullopt};
        VirtualState state{
            selected,
            32.0f,
            [](const VirtualState::Item&) { return ui::Spacer{220.0f, 32.0f}; }};
        if (!state.replace({
                VirtualState::Item{1, "One"},
                VirtualState::Item{2, "Two"}})) {
            return example::fail("virtual ListView press dataset replacement failed");
        }

        ui::ListViewStyle style;
        style.base.row_fill = stable_row;
        style.hovered.row_fill = stable_row;
        style.pressed.row_fill = ui::Color{0.18f, 0.30f, 0.82f, 1.0f};

        ui::UI tree{ui::ListView<int>{state}.style(style)};
        tree.resize(virtual_size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{virtual_size, 1.0f};
        if (!renderer.render(tree)) return example::fail("virtual ListView press baseline render failed");

        (void)tree.dispatch(example::key(ui::Key::Tab), platform);
        if (!renderer.render(tree)) return example::fail("virtual ListView press focus settle failed");
        (void)tree.dispatch(
            example::pointer(ui::InputType::PointerMove, 20.0f, 16.0f), platform);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("paint-equal virtual ListView hover invalidated the tree");
        }

        (void)tree.dispatch(
            example::pointer(ui::InputType::PointerDown, 20.0f, 16.0f), platform);
        if (tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail("changed virtual ListView press missed paint or dirtied layout");
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

int choice_availability_contract() {
    constexpr ui::Size size{320.0f, 120.0f};
    example::Platform platform;

    {
        ui::State<bool> checked{false};
        ui::State<bool> enabled{true};
        ui::CheckboxStyle style;
        style.disabled.control_height = 72.0f;

        ui::UI tree{ui::Enabled{
            enabled,
            ui::Checkbox{checked, "Choice"}.style(style)}};
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("Checkbox availability-layout baseline render failed");
        }

        enabled.set(false);
        if (!tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail(
                "layout-affecting Checkbox disabled style did not invalidate layout + paint");
        }
    }

    {
        ui::State<bool> checked{false};
        ui::State<bool> enabled{true};
        ui::CheckboxStyle style;
        style.disabled.box_fill = ui::Color{0.12f, 0.18f, 0.24f, 1.0f};

        ui::UI tree{ui::Enabled{
            enabled,
            ui::Checkbox{checked, "Choice"}.style(style)}};
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("Checkbox paint-only availability baseline render failed");
        }

        enabled.set(false);
        if (tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail(
                "paint-only Checkbox disabled style invalidated layout or missed repaint");
        }
    }

    {
        ui::State<int> selected{1};
        ui::RadioGroup<int> group{selected};
        ui::State<bool> enabled{true};
        ui::RadioStyle style;
        style.disabled.control_height = 72.0f;

        ui::UI tree{ui::Enabled{
            enabled,
            ui::RadioButton{group, 1, "Choice"}.style(style)}};
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("Radio availability-layout baseline render failed");
        }

        enabled.set(false);
        if (!tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail(
                "layout-affecting Radio disabled style did not invalidate layout + paint");
        }
    }

    {
        ui::State<int> selected{1};
        ui::RadioGroup<int> group{selected};
        ui::State<bool> enabled{true};
        ui::RadioStyle style;
        style.disabled.outer_fill = ui::Color{0.12f, 0.18f, 0.24f, 1.0f};

        ui::UI tree{ui::Enabled{
            enabled,
            ui::RadioButton{group, 1, "Choice"}.style(style)}};
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("Radio paint-only availability baseline render failed");
        }

        enabled.set(false);
        if (tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail(
                "paint-only Radio disabled style invalidated layout or missed repaint");
        }
    }

    return 0;
}

int self_test() {
    if (const int result = combo_anchor_contract(); result != 0) return result;
    if (const int result = list_view_contract(); result != 0) return result;
    if (const int result = tabs_contract(); result != 0) return result;
    if (const int result = availability_contract(); result != 0) return result;
    if (const int result = button_availability_contract(); result != 0) return result;
    if (const int result = choice_availability_contract(); result != 0) return result;
    return 0;
}

ui::UI make_demo() {
    return ui::UI{ui::Column{
        ui::Header{"T038 — Closure invalidation"},
        ui::Label{"Final state-aware invalidation checks for ComboBox, ListView, Tabs and availability variants."}.size(12.0f),
    }.gap(12.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    auto tree = make_demo();
    return example::run_window(tree, "NativeUI T038 Closure Invalidation", {620.0f, 220.0f});
}
