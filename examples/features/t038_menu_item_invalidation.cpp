#include "example_support.hpp"

namespace {

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

int open_combo(ui::UI& tree, example::Platform& platform) {
    if (!ui::handled(tree.dispatch(example::key(ui::Key::Down), platform))) {
        return example::fail("ComboBox popup did not open");
    }
    if (!ui::handled(tree.dispatch(key_up(ui::Key::Down), platform))) {
        return example::fail("ComboBox opening key-up was not consumed");
    }
    return 0;
}

int layout_contract() {
    constexpr ui::Size size{300.0f, 180.0f};
    example::Platform platform;
    ui::State<int> selected{1};
    ui::MenuItemStyle style;
    style.selected.row_height = 68.0f;

    ui::UI tree{
        ui::ComboBox<int>{selected, {{1, "One", true}, {2, "Two", true}}}.item_style(style)};
    tree.resize(size);
    tree.activate(platform);
    if (const int result = open_combo(tree, platform); result != 0) return result;

    ui::HeadlessRenderer renderer{size, 1.0f};
    if (!renderer.render(tree)) return example::fail("MenuItem layout baseline render failed");
    if (!ui::handled(tree.dispatch(example::key(ui::Key::Down), platform))) {
        return example::fail("MenuItem highlight did not move");
    }
    if (!tree.layout_dirty() || !tree.paint_dirty()) {
        return example::fail("layout-affecting MenuItem highlight did not invalidate layout + paint");
    }
    return 0;
}

int equal_style_contract() {
    constexpr ui::Size size{300.0f, 180.0f};
    example::Platform platform;
    ui::State<int> selected{1};
    ui::MenuItemStyle style;
    const ui::Color stable{0.0f, 0.0f, 0.0f, 0.0f};
    style.base.fill = stable;
    style.selected.fill = stable;
    style.hovered.fill = stable;

    ui::UI tree{
        ui::ComboBox<int>{selected, {{1, "One", true}, {2, "Two", true}}}.item_style(style)};
    tree.resize(size);
    tree.activate(platform);
    if (const int result = open_combo(tree, platform); result != 0) return result;

    ui::HeadlessRenderer renderer{size, 1.0f};
    if (!renderer.render(tree)) return example::fail("MenuItem equal-style baseline render failed");
    if (!ui::handled(tree.dispatch(example::key(ui::Key::Down), platform))) {
        return example::fail("MenuItem equal-style highlight did not move");
    }
    if (tree.layout_dirty() || tree.paint_dirty()) {
        return example::fail("equal resolved MenuItem highlight invalidated the tree");
    }
    return 0;
}

int self_test() {
    if (const int result = layout_contract(); result != 0) return result;
    return equal_style_contract();
}

ui::UI make_demo() {
    return ui::UI{ui::Column{
        ui::Header{"T038 — MenuItem invalidation"},
        ui::Label{"Deterministic popup row invalidation closure checks."}.size(12.0f),
    }.gap(12.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    auto tree = make_demo();
    return example::run_window(tree, "NativeUI T038 MenuItem Invalidation", {620.0f, 220.0f});
}
