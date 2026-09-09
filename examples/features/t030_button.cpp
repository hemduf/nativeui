#include "example_support.hpp"

namespace {

struct DemoState {
    ui::State<bool> enabled{true};
    ui::State<bool> read_only{false};
    int activations{};
};

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T030 — Button"},
            ui::Row{
                ui::Toggle{"Enabled", state.enabled},
                ui::Toggle{"Read only", state.read_only}
            }.gap(12.0f),
            ui::Enabled{
                state.enabled,
                ui::ReadOnly{
                    state.read_only,
                    ui::Button{"Activate", [&state] { ++state.activations; }}
                }
            },
            ui::Label{
                "Pointer release-inside, Space release and Enter press activate. "
                "ReadOnly does not suppress Button actions; Enabled=false does."
            }.size(12.0f).color(ui::colors::textMuted)
        }.gap(14.0f)
    };
}

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

int self_test() {
    DemoState state;
    auto tree = make_ui(state);
    example::Platform platform;
    tree.resize({560.0f, 240.0f});
    tree.activate(platform);

    // Focus starts on the first Toggle. Move to ReadOnly, then Button.
    tree.dispatch(example::key(ui::Key::Tab), platform);
    tree.dispatch(example::key(ui::Key::Tab), platform);
    tree.dispatch(example::key(ui::Key::Space), platform);
    tree.dispatch(key_up(ui::Key::Space), platform);
    if (state.activations != 1) return example::fail("Space must activate on release");

    tree.dispatch(example::key(ui::Key::Enter), platform);
    if (state.activations != 2) return example::fail("Enter must activate on key down");
    tree.dispatch(key_up(ui::Key::Enter), platform);

    state.read_only.set(true);
    tree.dispatch(example::key(ui::Key::Enter), platform);
    if (state.activations != 3) {
        return example::fail("ReadOnly must not suppress Button activation");
    }
    tree.dispatch(key_up(ui::Key::Enter), platform);

    state.enabled.set(false);
    if (tree.dispatch(example::key(ui::Key::Enter), platform) != ui::EventResult::Ignored ||
        state.activations != 3) {
        return example::fail("Disabled Button must be excluded from activation routing");
    }

    ui::HeadlessRenderer renderer{{560.0f, 240.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("headless Button render failed");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T030 Button", {560.0f, 240.0f});
}
