#include "example_support.hpp"

namespace {

int checkbox_contract() {
    constexpr ui::Size size{220.0f, 72.0f};
    example::Platform platform;

    {
        ui::State<bool> checked{false};
        ui::CheckboxStyle style;
        const ui::Color border{0.25f, 0.45f, 0.70f, 1.0f};
        style.base.box_border = border;
        style.hovered.box_border = border;

        ui::UI tree{ui::Checkbox{checked, "Stable"}.style(style)};
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Checkbox baseline render failed");

        tree.dispatch(example::pointer(ui::InputType::PointerMove, 20.0f, 20.0f), platform);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("equal resolved Checkbox hover style invalidated the tree");
        }
    }

    {
        ui::State<bool> checked{false};
        ui::CheckboxStyle style;
        const ui::Color stable_fill{0.18f, 0.24f, 0.32f, 1.0f};
        style.base.box_fill = stable_fill;
        style.checked.box_fill = stable_fill;
        style.base.checkmark_width = 0.0f;
        style.checked.checkmark_width = 0.0f;

        ui::UI tree{ui::Checkbox{checked, "Equal checked"}.style(style)};
        tree.resize(size);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Checkbox equal-checked baseline failed");

        checked.set(true);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("equal resolved Checkbox checked state invalidated the tree");
        }
    }

    {
        ui::State<bool> checked{false};
        ui::UI tree{ui::Checkbox{checked, "Checked paint"}};
        tree.resize(size);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Checkbox checked-paint baseline failed");

        checked.set(true);
        if (tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail("visual Checkbox checked state missed paint or dirtied layout");
        }
    }

    {
        ui::State<bool> checked{false};
        ui::CheckboxStyle style;
        style.checked.control_height = 58.0f;

        ui::UI tree{ui::Checkbox{checked, "Checked layout"}.style(style)};
        tree.resize(size);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Checkbox checked-layout baseline failed");

        checked.set(true);
        if (!tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail("layout-affecting Checkbox checked style did not invalidate layout + paint");
        }
    }

    return 0;
}

int radio_contract() {
    constexpr ui::Size size{220.0f, 72.0f};
    example::Platform platform;

    {
        ui::State<int> selected{2};
        ui::RadioGroup<int> group{selected};
        ui::RadioStyle style;
        const ui::Color outer{0.20f, 0.34f, 0.52f, 1.0f};
        style.base.outer_fill = outer;
        style.hovered.outer_fill = outer;

        ui::UI tree{ui::RadioButton{group, 1, "Stable"}.style(style)};
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Radio baseline render failed");

        tree.dispatch(example::pointer(ui::InputType::PointerMove, 20.0f, 20.0f), platform);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("equal resolved Radio hover style invalidated the tree");
        }
    }

    {
        ui::State<int> selected{2};
        ui::RadioGroup<int> group{selected};
        ui::UI tree{ui::RadioButton{group, 1, "Unrelated selection"}};
        tree.resize(size);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Radio unrelated-selection baseline failed");

        selected.set(3);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("unchanged Radio selected state invalidated the tree");
        }
    }

    {
        ui::State<int> selected{2};
        ui::RadioGroup<int> group{selected};
        ui::RadioStyle style;
        style.base.mark_radius = 0.0f;
        style.selected.mark_radius = 0.0f;

        ui::UI tree{ui::RadioButton{group, 1, "Equal selected"}.style(style)};
        tree.resize(size);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Radio equal-selected baseline failed");

        selected.set(1);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("equal resolved Radio selected state invalidated the tree");
        }
    }

    {
        ui::State<int> selected{2};
        ui::RadioGroup<int> group{selected};
        ui::UI tree{ui::RadioButton{group, 1, "Selected paint"}};
        tree.resize(size);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Radio selected-paint baseline failed");

        selected.set(1);
        if (tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail("visual Radio selected state missed paint or dirtied layout");
        }
    }

    {
        ui::State<int> selected{2};
        ui::RadioGroup<int> group{selected};
        ui::RadioStyle style;
        style.selected.control_height = 58.0f;

        ui::UI tree{ui::RadioButton{group, 1, "Selected layout"}.style(style)};
        tree.resize(size);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("Radio selected-layout baseline failed");

        selected.set(1);
        if (!tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail("layout-affecting Radio selected style did not invalidate layout + paint");
        }
    }

    return 0;
}

int self_test() {
    if (const int result = checkbox_contract(); result != 0) return result;
    return radio_contract();
}

ui::UI make_demo() {
    return ui::UI{ui::Column{
        ui::Header{"T038 — Choice State Invalidation"},
        ui::Label{"Deterministic no-op and checked/selected geometry closure checks."}.size(12.0f),
    }.gap(12.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    auto tree = make_demo();
    return example::run_window(tree, "NativeUI T038 Choice State Invalidation", {620.0f, 220.0f});
}
