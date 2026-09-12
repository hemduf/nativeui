#include "example_support.hpp"

namespace {

struct DemoState {
    ui::State<std::string> stable{"stable\nmultiline"};
    ui::State<std::string> resizing{"resizing\nmultiline"};
};

ui::TextAreaStyle stable_text_area_style() {
    ui::TextAreaStyle style;
    const ui::Color border{0.34f, 0.52f, 0.72f, 1.0f};
    style.base.border = border;
    style.hovered.border = border;
    return style;
}

ui::TextAreaStyle resizing_text_area_style() {
    ui::TextAreaStyle style;
    style.hovered.control_height = 220.0f;
    return style;
}

ui::UI make_ui(DemoState& state) {
    return ui::UI{ui::Column{
        ui::Header{"T038 — TextArea Style Invalidation"},
        ui::Label{
            "Equal resolved hover presentation stays clean; geometry variants request layout."
        }.size(12.0f),
        ui::TextArea{"Equal hover presentation", state.stable}.style(stable_text_area_style()),
        ui::TextArea{"Hover geometry", state.resizing}.style(resizing_text_area_style())
    }.gap(14.0f)};
}

int self_test() {
    constexpr ui::Size size{460.0f, 220.0f};

    // Equal effective hover presentation is a strict no-op. The logical hover
    // bit changes, but the resolved style is identical before/after.
    {
        ui::State<std::string> value{"stable\nmultiline"};
        ui::UI tree{ui::TextArea{"Stable", value}.style(stable_text_area_style())};
        example::Platform platform;
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("TextArea equal-style baseline render failed");
        }
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("TextArea baseline render did not clear invalidation");
        }

        tree.dispatch(
            example::pointer(ui::InputType::PointerMove, 20.0f, 80.0f),
            platform);
        if (tree.layout_dirty() || tree.paint_dirty()) {
            return example::fail("equal resolved TextArea hover style invalidated the tree");
        }
    }

    // A visual-state variant may explicitly change intrinsic geometry. Hover
    // must then request layout rather than issuing only a paint invalidation.
    {
        ui::State<std::string> value{"resize\nmultiline"};
        ui::UI tree{ui::TextArea{"Hover layout", value}.style(resizing_text_area_style())};
        example::Platform platform;
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) {
            return example::fail("TextArea hover-layout baseline render failed");
        }

        tree.dispatch(
            example::pointer(ui::InputType::PointerMove, 20.0f, 80.0f),
            platform);
        if (!tree.layout_dirty()) {
            return example::fail("layout-affecting TextArea hover style did not invalidate layout");
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
        tree, "NativeUI T038 TextArea Style Invalidation", {640.0f, 520.0f});
}
