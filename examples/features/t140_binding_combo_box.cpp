#include "example_support.hpp"

namespace {

struct DemoState {
    ui::State<int> selection{1};
};

ui::UI make_ui(DemoState& state, int first, int second) {
    return ui::UI{
        ui::Column{
            ui::Header{"T140 — Binding-backed ComboBox"},
            ui::Label{
                "ComboBox retains Binding<T> internally while State<T>& remains source compatible."
            }.size(12.0f),
            ui::ComboBox<int>{
                state.selection.binding(),
                {{first, "First", true}, {second, "Second", true}}},
        }.gap(12.0f).padding(16.0f)};
}

ui::InputEvent key_down(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyDown;
    event.key = key;
    return event;
}

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

int self_test() {
    DemoState left;
    DemoState right;
    right.selection.set(10);

    int left_notifications = 0;
    int right_notifications = 0;
    auto left_binding = left.selection.binding();
    auto right_binding = right.selection.binding();
    auto left_subscription = left_binding.observe(
        [&left_notifications](const int&) { ++left_notifications; });
    auto right_subscription = right_binding.observe(
        [&right_notifications](const int&) { ++right_notifications; });

    auto left_tree = make_ui(left, 1, 2);
    auto right_tree = make_ui(right, 10, 20);
    example::Platform left_platform;
    example::Platform right_platform;
    left_tree.resize({620.0f, 260.0f});
    right_tree.resize({620.0f, 260.0f});
    left_tree.activate(left_platform);
    right_tree.activate(right_platform);

    ui::HeadlessRenderer left_renderer{{620.0f, 260.0f}, 1.0f};
    ui::HeadlessRenderer right_renderer{{620.0f, 260.0f}, 1.0f};
    if (!left_renderer.render(left_tree)) return example::fail("initial left ComboBox render failed");
    if (!right_renderer.render(right_tree)) return example::fail("initial right ComboBox render failed");

    left_tree.dispatch(key_down(ui::Key::Down), left_platform);
    left_tree.dispatch(key_up(ui::Key::Down), left_platform);
    left_tree.dispatch(key_down(ui::Key::Down), left_platform);
    left_tree.dispatch(key_down(ui::Key::Enter), left_platform);
    if (left.selection.get() != 2 || right.selection.get() != 10) {
        return example::fail("Binding ComboBox commit crossed independent State sources");
    }
    if (left_notifications != 1 || right_notifications != 0) {
        return example::fail("Binding ComboBox commit notification count/isolation failed");
    }

    if (!left_renderer.render(left_tree)) return example::fail("committed ComboBox render failed");
    if (left_tree.layout_dirty() || left_tree.paint_dirty()) {
        return example::fail("ComboBox render did not settle invalidation state");
    }
    if (right_tree.layout_dirty() || right_tree.paint_dirty()) {
        return example::fail("independent ComboBox became dirty before external mutation");
    }

    left_binding.set(1);
    if (left.selection.get() != 1 || left_notifications != 2 || right_notifications != 0) {
        return example::fail("external Binding ComboBox update notification/isolation failed");
    }
    if (!left_tree.layout_dirty() || !left_tree.paint_dirty()) {
        return example::fail("external ComboBox Binding update lost layout+paint invalidation");
    }
    if (right_tree.layout_dirty() || right_tree.paint_dirty()) {
        return example::fail("external ComboBox Binding update dirtied independent UI");
    }

    ui::State<int> legacy_selection{1};
    [[maybe_unused]] auto legacy_combo = ui::ComboBox<int>{
        legacy_selection,
        {{1, "One", true}, {2, "Two", true}}}.spec();

    auto stale_combo = [] {
        ui::State<int> temporary_selection{1};
        return ui::ComboBox<int>{
            temporary_selection.binding(),
            {{1, "One", true}, {2, "Two", true}}};
    }();
    ui::UI stale_tree{std::move(stale_combo)};
    example::Platform stale_platform;
    stale_tree.resize({320.0f, 160.0f});
    stale_tree.activate(stale_platform);
    ui::HeadlessRenderer stale_renderer{{320.0f, 160.0f}, 1.0f};
    if (!stale_renderer.render(stale_tree)) {
        return example::fail("ComboBox retained Binding could not read after State destruction");
    }
    stale_tree.dispatch(key_down(ui::Key::Down), stale_platform);
    stale_tree.dispatch(key_up(ui::Key::Down), stale_platform);
    stale_tree.dispatch(key_down(ui::Key::Down), stale_platform);
    stale_tree.dispatch(key_down(ui::Key::Enter), stale_platform);

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state, 1, 2);
    return example::run_window(tree, "NativeUI T140 Binding ComboBox", {620.0f, 260.0f});
}
