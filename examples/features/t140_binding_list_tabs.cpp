#include "example_support.hpp"

#include <optional>
#include <utility>

namespace {

ui::UI make_list(ui::Binding<std::optional<int>> selection) {
    return ui::UI{
        ui::ListView<int>{std::move(selection)}
            .item(1, ui::Spacer{160.0f, 30.0f})
            .item(2, ui::Spacer{160.0f, 30.0f})};
}

ui::UI make_tabs(ui::Binding<int> selection) {
    return ui::UI{
        ui::Tabs<int>{std::move(selection)}
            .tab(1, "One", ui::Spacer{240.0f, 60.0f})
            .tab(2, "Two", ui::Spacer{240.0f, 60.0f})};
}

int self_test() {
    ui::State<std::optional<int>> left_list_selection{1};
    ui::State<std::optional<int>> right_list_selection{10};
    ui::State<int> left_tab_selection{1};
    ui::State<int> right_tab_selection{10};

    auto left_list_binding = left_list_selection.binding();
    auto right_list_binding = right_list_selection.binding();
    auto left_tab_binding = left_tab_selection.binding();
    auto right_tab_binding = right_tab_selection.binding();

    int left_list_notifications = 0;
    int right_list_notifications = 0;
    int left_tab_notifications = 0;
    int right_tab_notifications = 0;
    auto left_list_subscription = left_list_binding.observe(
        [&](const std::optional<int>&) { ++left_list_notifications; });
    auto right_list_subscription = right_list_binding.observe(
        [&](const std::optional<int>&) { ++right_list_notifications; });
    auto left_tab_subscription = left_tab_binding.observe(
        [&](const int&) { ++left_tab_notifications; });
    auto right_tab_subscription = right_tab_binding.observe(
        [&](const int&) { ++right_tab_notifications; });

    auto left_list = make_list(left_list_binding);
    auto right_list = ui::UI{
        ui::ListView<int>{right_list_binding}
            .item(10, ui::Spacer{160.0f, 30.0f})
            .item(20, ui::Spacer{160.0f, 30.0f})};
    auto left_tabs = make_tabs(left_tab_binding);
    auto right_tabs = ui::UI{
        ui::Tabs<int>{right_tab_binding}
            .tab(10, "Ten", ui::Spacer{240.0f, 60.0f})
            .tab(20, "Twenty", ui::Spacer{240.0f, 60.0f})};

    example::Platform left_list_platform;
    example::Platform right_list_platform;
    example::Platform left_tabs_platform;
    example::Platform right_tabs_platform;
    left_list.resize({160.0f, 80.0f});
    right_list.resize({160.0f, 80.0f});
    left_tabs.resize({240.0f, 120.0f});
    right_tabs.resize({240.0f, 120.0f});
    left_list.activate(left_list_platform);
    right_list.activate(right_list_platform);
    left_tabs.activate(left_tabs_platform);
    right_tabs.activate(right_tabs_platform);

    ui::HeadlessRenderer left_list_renderer{{160.0f, 80.0f}, 1.0f};
    ui::HeadlessRenderer right_list_renderer{{160.0f, 80.0f}, 1.0f};
    ui::HeadlessRenderer left_tabs_renderer{{240.0f, 120.0f}, 1.0f};
    ui::HeadlessRenderer right_tabs_renderer{{240.0f, 120.0f}, 1.0f};
    if (!left_list_renderer.render(left_list) || !right_list_renderer.render(right_list) ||
        !left_tabs_renderer.render(left_tabs) || !right_tabs_renderer.render(right_tabs)) {
        return example::fail("initial Binding ListView/Tabs render failed");
    }

    if (left_list.dispatch(example::key(ui::Key::Down), left_list_platform) !=
            ui::EventResult::Handled ||
        left_list_selection.get() != std::optional<int>{2} ||
        right_list_selection.get() != std::optional<int>{10}) {
        return example::fail("Binding ListView navigation/isolation failed");
    }
    if (left_list_notifications != 1 || right_list_notifications != 0) {
        return example::fail("Binding ListView did not notify exactly once in isolation");
    }

    if (left_tabs.dispatch(example::key(ui::Key::Right), left_tabs_platform) !=
            ui::EventResult::Handled ||
        left_tab_selection.get() != 2 || right_tab_selection.get() != 10) {
        return example::fail("Binding Tabs navigation/isolation failed");
    }
    if (left_tab_notifications != 1 || right_tab_notifications != 0) {
        return example::fail("Binding Tabs did not notify exactly once in isolation");
    }

    if (!left_list_renderer.render(left_list) || !left_tabs_renderer.render(left_tabs)) {
        return example::fail("Binding ListView/Tabs post-input render failed");
    }
    if (left_list.layout_dirty() || left_list.paint_dirty() ||
        left_tabs.layout_dirty() || left_tabs.paint_dirty()) {
        return example::fail("Binding ListView/Tabs render did not settle invalidation state");
    }

    left_list_binding.set(std::optional<int>{1});
    if (left_list_selection.get() != std::optional<int>{1} || left_list_notifications != 2 ||
        right_list_notifications != 0) {
        return example::fail("external ListView Binding write/notification isolation failed");
    }
    if (left_list.layout_dirty() || !left_list.paint_dirty()) {
        return example::fail("ListView Binding update broadened or lost paint invalidation");
    }
    if (right_list.layout_dirty() || right_list.paint_dirty()) {
        return example::fail("ListView Binding update dirtied independent UI");
    }

    left_tab_binding.set(1);
    if (left_tab_selection.get() != 1 || left_tab_notifications != 2 || right_tab_notifications != 0) {
        return example::fail("external Tabs Binding write/notification isolation failed");
    }
    if (!left_tabs.layout_dirty() || !left_tabs.paint_dirty()) {
        return example::fail("Tabs Binding update lost selection availability invalidation");
    }
    if (right_tabs.layout_dirty() || right_tabs.paint_dirty()) {
        return example::fail("external Tabs Binding update dirtied independent UI");
    }

    ui::State<std::optional<int>> legacy_list_selection{1};
    ui::State<int> legacy_tab_selection{1};
    [[maybe_unused]] auto legacy_list = ui::ListView<int>{legacy_list_selection}
        .item(1, ui::Spacer{80.0f, 20.0f})
        .item(2, ui::Spacer{80.0f, 20.0f})
        .spec();
    [[maybe_unused]] auto legacy_tabs = ui::Tabs<int>{legacy_tab_selection}
        .tab(1, "One", ui::Spacer{80.0f, 20.0f})
        .tab(2, "Two", ui::Spacer{80.0f, 20.0f})
        .spec();

    auto stale_list = [] {
        ui::State<std::optional<int>> temporary_selection{1};
        return ui::ListView<int>{temporary_selection.binding()}
            .item(1, ui::Spacer{100.0f, 30.0f})
            .item(2, ui::Spacer{100.0f, 30.0f});
    }();
    ui::UI stale_list_tree{std::move(stale_list)};
    example::Platform stale_list_platform;
    stale_list_tree.resize({100.0f, 60.0f});
    stale_list_tree.activate(stale_list_platform);
    ui::HeadlessRenderer stale_list_renderer{{100.0f, 60.0f}, 1.0f};
    if (!stale_list_renderer.render(stale_list_tree)) {
        return example::fail("ListView retained Binding could not read after State destruction");
    }
    (void)stale_list_tree.dispatch(example::key(ui::Key::Down), stale_list_platform);

    auto stale_tabs = [] {
        ui::State<int> temporary_selection{1};
        return ui::Tabs<int>{temporary_selection.binding()}
            .tab(1, "One", ui::Spacer{100.0f, 30.0f})
            .tab(2, "Two", ui::Spacer{100.0f, 30.0f});
    }();
    ui::UI stale_tabs_tree{std::move(stale_tabs)};
    example::Platform stale_tabs_platform;
    stale_tabs_tree.resize({200.0f, 90.0f});
    stale_tabs_tree.activate(stale_tabs_platform);
    ui::HeadlessRenderer stale_tabs_renderer{{200.0f, 90.0f}, 1.0f};
    if (!stale_tabs_renderer.render(stale_tabs_tree)) {
        return example::fail("Tabs retained Binding could not read after State destruction");
    }
    (void)stale_tabs_tree.dispatch(example::key(ui::Key::Right), stale_tabs_platform);

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    ui::State<std::optional<int>> list_selection{1};
    ui::State<int> tab_selection{1};
    ui::UI tree{
        ui::Row{
            ui::ListView<int>{list_selection.binding()}
                .item(1, ui::Spacer{180.0f, 36.0f})
                .item(2, ui::Spacer{180.0f, 36.0f}),
            ui::Tabs<int>{tab_selection.binding()}
                .tab(1, "One", ui::Spacer{240.0f, 100.0f})
                .tab(2, "Two", ui::Spacer{240.0f, 100.0f})
        }.gap(16.0f)};
    return example::run_window(tree, "NativeUI T140 Binding ListView/Tabs", {520.0f, 220.0f});
}
