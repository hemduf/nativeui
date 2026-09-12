#include "example_support.hpp"

namespace {

struct DemoState {
    ui::State<bool> stable{false};
    ui::State<bool> resizing{false};
};

ui::ToggleStyle stable_toggle_style() {
    ui::ToggleStyle style;
    const ui::Color border{0.34f, 0.52f, 0.72f, 1.0f};
    style.base.border = border;
    style.base.border_width = 1.0f;
    style.hovered.border = border;
    style.focused.border = border;
    style.focused.border_width = 1.0f;
    return style;
}

ui::ToggleStyle resizing_toggle_style() {
    ui::ToggleStyle style;
    style.hovered.control_height = 72.0f;
    style.checked.control_width = 260.0f;
    return style;
}

ui::UI make_ui(DemoState& state) {
    return ui::UI{ui::Column{
        ui::Header{"T038 — Toggle Style Invalidation"},
        ui::Label{
            "Equal visual states stay clean while explicit geometry variants request layout."
        }.size(12.0f),
        ui::Toggle{"Equal hover presentation", state.stable}.style(stable_toggle_style()),
        ui::Toggle{"Hover / checked geometry", state.resizing}.style(resizing_toggle_style())
    }.gap(14.0f)};
}

int self_test() {
    constexpr ui::Size size{280.0f, 90.0f};

    // Equal resolved interaction presentation is a strict no-op. The raw
    // hovered bit changes, but explicit focus/hover patches make the effective
    // presentation identical before and after the pointer transition.
    {
        ui::State<bool> value{false};
        ui::UI tree{ui::Toggle{"Stable", value}.style(stable_toggle_style())};
        example::Platform platform;
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("Toggle equal-style baseline render failed");
        }
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("Toggle baseline render did not clear invalidation");
        }

        tree.dispatch(
            example::pointer(ui::InputType::PointerMove, 20.0f, 20.0f),
            platform);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("equal resolved Toggle hover style invalidated the tree");
        }
    }

    // A visual-state patch is allowed to change measurement. Hover must then
    // invalidate layout rather than issuing only a paint invalidation.
    {
        ui::State<bool> value{false};
        ui::UI tree{ui::Toggle{"Hover layout", value}.style(resizing_toggle_style())};
        example::Platform platform;
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("Toggle hover-layout baseline render failed");
        }

        tree.dispatch(
            example::pointer(ui::InputType::PointerMove, 20.0f, 20.0f),
            platform);
        if (!tree.layout_dirty()) {
            return example::fail("layout-affecting Toggle hover style did not invalidate layout");
        }
    }

    // Application-owned checked state uses the same classification seam: a
    // checked variant that changes intrinsic width must invalidate layout.
    {
        ui::State<bool> value{false};
        ui::UI tree{ui::Toggle{"Checked layout", value}.style(resizing_toggle_style())};
        tree.resize(size);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("Toggle checked-layout baseline render failed");
        }

        value.set(true);
        if (!tree.layout_dirty()) {
            return example::fail("layout-affecting Toggle checked style did not invalidate layout");
        }
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(
        tree, "NativeUI T038 Toggle Style Invalidation", {640.0f, 300.0f});
}
