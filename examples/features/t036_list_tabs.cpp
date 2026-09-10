#include "example_support.hpp"

#include <optional>
#include <string>
#include <utility>

namespace {

struct DemoState {
    ui::State<std::optional<int>> list_selection{2};
    ui::State<int> tab_selection{1};
    int last_activated{};
};

ui::Canvas list_item(std::string label, bool enabled = true) {
    return ui::Canvas{
        {248.0f, 42.0f},
        [label = std::move(label), enabled](ui::CanvasContext2D& context) {
            context.text(
                {18.0f, context.height() * 0.5f},
                label,
                13.0f,
                enabled ? ui::colors::text : ui::Color{0.39f, 0.42f, 0.47f, 1.0f},
                ui::TextAlign::Left);
        }};
}

ui::Canvas panel_content(std::string title, std::string description) {
    return ui::Canvas{
        {334.0f, 174.0f},
        [title = std::move(title), description = std::move(description)](
            ui::CanvasContext2D& context) {
            context.text({20.0f, 30.0f}, title, 17.0f, ui::colors::text, ui::TextAlign::Left);
            context.text(
                {20.0f, 56.0f}, description, 12.0f, ui::colors::textMuted,
                ui::TextAlign::Left);
            context.line(
                {20.0f, 78.0f}, {context.width() - 20.0f, 78.0f}, 1.0f,
                ui::Color{0.20f, 0.22f, 0.25f, 0.85f});
            context.fill_rounded_rect(
                {20.0f, 98.0f, 112.0f, 34.0f}, 7.0f, ui::colors::input);
            context.text(
                {76.0f, 115.0f}, "Selected state", 11.0f, ui::colors::textMuted,
                ui::TextAlign::Center);
            context.fill_rounded_rect(
                {142.0f, 98.0f, 86.0f, 34.0f}, 7.0f, ui::colors::input);
            context.text(
                {185.0f, 115.0f}, "Retained", 11.0f, ui::colors::textMuted,
                ui::TextAlign::Center);
        }};
}

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Label{"T036 — ListView and Tabs"}.size(22.0f).bold(),
            ui::Label{
                "Keyboard and pointer interaction with stable, application-owned selection state."
            }.size(12.0f).color(ui::colors::textMuted),
            ui::Row{
                ui::Column{
                    ui::Label{"LIST VIEW"}.size(11.0f).bold().color(ui::colors::textMuted),
                    ui::ListView<int>{state.list_selection}
                        .item(1, list_item("Oscillator"))
                        .item(2, list_item("Filter"))
                        .item(3, list_item("Disabled row", false), false)
                        .item(4, list_item("Envelope"))
                        .item(5, list_item("Effects"))
                        .on_activate([&state](const int& key) { state.last_activated = key; }),
                    ui::Label{"↑ ↓ navigate  ·  Home / End  ·  Enter activates"}
                        .size(11.0f)
                        .color(ui::colors::textMuted)
                }.gap(9.0f),
                ui::Column{
                    ui::Label{"TABS"}.size(11.0f).bold().color(ui::colors::textMuted),
                    ui::Tabs<int>{state.tab_selection}
                        .tab(
                            1, "Overview",
                            panel_content(
                                "Overview",
                                "Automatic activation keeps selection and visible panel in sync."))
                        .tab(
                            2, "Disabled",
                            panel_content("Disabled", "Unavailable tabs are skipped by keyboard navigation."),
                            false)
                        .tab(
                            3, "Details",
                            panel_content(
                                "Details",
                                "Inactive panels collapse completely instead of remaining in layout.")),
                    ui::Label{"← → switches tab  ·  Home / End  ·  click a segment"}
                        .size(11.0f)
                        .color(ui::colors::textMuted)
                }.gap(9.0f)
            }.gap(24.0f)
        }.gap(14.0f).padding(20.0f)
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
    return example::run_window(tree, "NativeUI T036 ListView and Tabs", {760.0f, 430.0f});
}
