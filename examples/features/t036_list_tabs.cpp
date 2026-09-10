#include "example_support.hpp"

#include <optional>

namespace {

struct DemoState {
    ui::State<std::optional<int>> list_selection{2};
    ui::State<int> tab_selection{1};
    int last_activated{};
};

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T036 — ListView and Tabs"},
            ui::Label{
                "ListView is a single composite focus stop with key-based selection and automatic "
                "scroll reveal. Tabs use automatic activation and collapse inactive panels."
            }.size(12.0f).color(ui::colors::textMuted),
            ui::Row{
                ui::Column{
                    ui::Header{"ListView"},
                    ui::Label{
                        "Use Up/Down/Home/End, Enter/Space, or the pointer. The disabled row is skipped."
                    }.size(11.0f).color(ui::colors::textMuted),
                    ui::ListView<int>{state.list_selection}
                        .item(1, example::Box{"Oscillator", {240.0f, 42.0f}})
                        .item(2, example::Box{"Filter", {240.0f, 42.0f}, ui::colors::input})
                        .item(3, example::Box{"Disabled row", {240.0f, 42.0f}}, false)
                        .item(4, example::Box{"Envelope", {240.0f, 42.0f}, ui::colors::input})
                        .item(5, example::Box{"Effects", {240.0f, 42.0f}})
                        .on_activate([&state](const int& key) { state.last_activated = key; })
                }.gap(8.0f),
                ui::Column{
                    ui::Header{"Tabs"},
                    ui::Label{
                        "Use Left/Right/Home/End or click a header. The disabled tab is skipped."
                    }.size(11.0f).color(ui::colors::textMuted),
                    ui::Tabs<int>{state.tab_selection}
                        .tab(1, "Overview", example::Box{"Overview panel", {320.0f, 174.0f}})
                        .tab(2, "Disabled", example::Box{"Disabled panel", {320.0f, 174.0f}}, false)
                        .tab(3, "Details", example::Box{"Details panel", {320.0f, 174.0f}, ui::colors::input})
                }.gap(8.0f)
            }.gap(20.0f)
        }.gap(14.0f).padding(16.0f)
    };
}

int self_test() {
    example::Platform platform;

    ui::State<std::optional<int>> list_selection{1};
    int activations = 0;
    int activated_key = 0;
    ui::UI list{
        ui::ListView<int>{list_selection}
            .item(1, ui::Spacer{120.0f, 30.0f})
            .item(2, ui::Spacer{120.0f, 30.0f}, false)
            .item(3, ui::Spacer{120.0f, 30.0f})
            .item(4, ui::Spacer{120.0f, 30.0f})
            .on_activate([&](const int& key) {
                ++activations;
                activated_key = key;
            })};
    list.resize({120.0f, 60.0f});
    list.activate(platform);

    if (list.dispatch(example::key(ui::Key::Down), platform) != ui::EventResult::Handled ||
        !list_selection.get() || *list_selection.get() != 3) {
        return example::fail("ListView Down navigation did not skip the disabled row");
    }
    if (list.dispatch(example::key(ui::Key::Enter), platform) != ui::EventResult::Handled ||
        activations != 1 || activated_key != 3) {
        return example::fail("ListView activation callback was not invoked exactly once");
    }
    if (list.dispatch(example::key(ui::Key::End), platform) != ui::EventResult::Handled ||
        !list_selection.get() || *list_selection.get() != 4) {
        return example::fail("ListView End navigation failed");
    }

    ui::HeadlessRenderer list_renderer{{120.0f, 60.0f}, 1.0f};
    if (!list_renderer.render(list)) {
        return example::fail("headless ListView render failed");
    }

    ui::State<int> tab_selection{1};
    ui::UI tabs{
        ui::Tabs<int>{tab_selection}
            .tab(1, "One", ui::Spacer{300.0f, 84.0f})
            .tab(2, "Disabled", ui::Spacer{300.0f, 84.0f}, false)
            .tab(3, "Three", ui::Spacer{300.0f, 84.0f})};
    tabs.resize({300.0f, 120.0f});
    tabs.activate(platform);

    if (tabs.dispatch(example::key(ui::Key::Right), platform) != ui::EventResult::Handled ||
        tab_selection.get() != 3) {
        return example::fail("Tabs Right navigation did not skip the disabled tab");
    }
    if (tabs.dispatch(example::key(ui::Key::Home), platform) != ui::EventResult::Handled ||
        tab_selection.get() != 1) {
        return example::fail("Tabs Home navigation failed");
    }
    if (tabs.dispatch(
            example::pointer(ui::InputType::PointerDown, 250.0f, 18.0f), platform) !=
            ui::EventResult::Handled ||
        tabs.dispatch(
            example::pointer(ui::InputType::PointerUp, 250.0f, 18.0f), platform) !=
            ui::EventResult::Handled ||
        tab_selection.get() != 3) {
        return example::fail("Tabs pointer activation failed");
    }

    ui::HeadlessRenderer tabs_renderer{{300.0f, 120.0f}, 1.0f};
    if (!tabs_renderer.render(tabs)) {
        return example::fail("headless Tabs render failed");
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T036 ListView and Tabs", {720.0f, 420.0f});
}
