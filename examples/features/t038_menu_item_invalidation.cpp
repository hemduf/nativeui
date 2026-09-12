#include "example_support.hpp"

#include <cmath>
#include <limits>

namespace {

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

bool pixel_near(ui::Rgba8 pixel, ui::Color color, int tolerance = 5) {
    const auto channel = [](float value) {
        return static_cast<int>(std::lround(value * 255.0f));
    };
    return std::abs(static_cast<int>(pixel.r) - channel(color.r)) <= tolerance &&
           std::abs(static_cast<int>(pixel.g) - channel(color.g)) <= tolerance &&
           std::abs(static_cast<int>(pixel.b) - channel(color.b)) <= tolerance;
}

std::pair<int, int> marker_vertical_extent(
    const ui::HeadlessRenderer& renderer,
    ui::Size size,
    ui::Color marker) {
    int minimum_y = std::numeric_limits<int>::max();
    int maximum_y = -1;
    for (int y = 0; y < static_cast<int>(size.h); ++y) {
        for (int x = 0; x < static_cast<int>(size.w); ++x) {
            if (!pixel_near(renderer.pixel(x, y), marker)) continue;
            minimum_y = std::min(minimum_y, y);
            maximum_y = std::max(maximum_y, y);
        }
    }
    return {minimum_y, maximum_y};
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

int geometry_contract() {
    constexpr ui::Size size{300.0f, 180.0f};
    example::Platform platform;
    ui::State<int> selected{1};
    ui::MenuItemStyle style;
    const ui::Color marker{0.91f, 0.12f, 0.73f, 1.0f};
    style.base.row_height = 32.0f;
    style.selected.row_height = 68.0f;
    style.selected.fill = marker;

    ui::UI tree{
        ui::ComboBox<int>{selected, {{1, "One", true}, {2, "Two", true}}}.item_style(style)};
    tree.resize(size);
    tree.activate(platform);
    if (const int result = open_combo(tree, platform); result != 0) return result;

    ui::HeadlessRenderer renderer{size, 1.0f};
    if (!renderer.render(tree)) return example::fail("MenuItem geometry baseline render failed");
    const auto before = marker_vertical_extent(renderer, size, marker);
    if (before.second < before.first) {
        return example::fail("selected MenuItem marker was not rendered");
    }

    if (!ui::handled(tree.dispatch(example::key(ui::Key::Down), platform))) {
        return example::fail("MenuItem highlight did not move");
    }
    if (!renderer.render(tree)) {
        return example::fail("MenuItem geometry change did not schedule repaint");
    }
    const auto after = marker_vertical_extent(renderer, size, marker);
    if (after.second < after.first || after.first <= before.first) {
        return example::fail("resolved MenuItem row geometry did not follow the new highlight");
    }
    const int before_height = before.second - before.first + 1;
    const int after_height = after.second - after.first + 1;
    if (std::abs(after_height - before_height) > 2) {
        return example::fail("selected MenuItem row height changed unexpectedly while moving highlight");
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
    if (const int result = geometry_contract(); result != 0) return result;
    return equal_style_contract();
}

ui::UI make_demo() {
    return ui::UI{ui::Column{
        ui::Header{"T038 — MenuItem invalidation"},
        ui::Label{"Deterministic popup row geometry and equal-style invalidation checks."}.size(12.0f),
    }.gap(12.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    auto tree = make_demo();
    return example::run_window(tree, "NativeUI T038 MenuItem Invalidation", {620.0f, 220.0f});
}
