#include "example_support.hpp"

#include <stdexcept>

namespace {

enum class Mode { Clean, Drive, Wide };

struct DemoState {
    ui::State<bool> enabled{true};
    ui::State<bool> read_only{false};
    ui::State<bool> armed{false};
    ui::State<Mode> mode{Mode::Drive};
    ui::RadioGroup<Mode> mode_group{mode};
};

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T031 — Checkbox and RadioButton"},
            ui::Row{
                ui::Toggle{"Enabled", state.enabled},
                ui::Toggle{"Read only", state.read_only}
            }.gap(12.0f),
            ui::Enabled{
                state.enabled,
                ui::ReadOnly{
                    state.read_only,
                    ui::Column{
                        ui::Checkbox{state.armed, "Arm processing"},
                        ui::RadioButton{state.mode_group, Mode::Clean, "Clean"},
                        ui::RadioButton{state.mode_group, Mode::Drive, "Drive"},
                        ui::RadioButton{state.mode_group, Mode::Wide, "Wide"}
                    }.gap(8.0f)
                }
            },
            ui::Label{
                "Checkbox toggles with pointer/Space. Radio arrows wrap within the group; "
                "Tab treats the group as one stop. ReadOnly preserves focus but blocks value changes."
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
    tree.resize({620.0f, 360.0f});
    tree.activate(platform);

    // Focus starts on Enabled, then ReadOnly, then enters Checkbox.
    tree.dispatch(example::key(ui::Key::Tab), platform);
    tree.dispatch(example::key(ui::Key::Tab), platform);
    tree.dispatch(example::key(ui::Key::Space), platform);
    tree.dispatch(key_up(ui::Key::Space), platform);
    if (!state.armed.get()) return example::fail("Checkbox Space activation failed");

    // Tab enters the radio group at the selected Drive option. Right selects
    // and focuses Wide, then wraps to Clean.
    tree.dispatch(example::key(ui::Key::Tab), platform);
    tree.dispatch(example::key(ui::Key::Right), platform);
    if (state.mode.get() != Mode::Wide) return example::fail("Radio right navigation failed");
    tree.dispatch(example::key(ui::Key::Right), platform);
    if (state.mode.get() != Mode::Clean) return example::fail("Radio wrap failed");

    state.read_only.set(true);
    tree.dispatch(example::key(ui::Key::Right), platform);
    if (state.mode.get() != Mode::Clean) {
        return example::fail("ReadOnly must block RadioButton selection mutation");
    }

    // A RadioGroup is value-backed, so duplicate simultaneously-live option
    // values would otherwise both render selected for the same State<T> value.
    // Reject that invalid configuration deterministically while still allowing
    // the same values to be reused after earlier components are destroyed.
    bool duplicate_rejected = false;
    try {
        ui::State<int> duplicate_selected{1};
        ui::RadioGroup<int> duplicate_group{duplicate_selected};
        ui::UI duplicate_tree{ui::Column{
            ui::RadioButton{duplicate_group, 1, "First"},
            ui::RadioButton{duplicate_group, 1, "Duplicate"}
        }};
        (void)duplicate_tree;
    } catch (const std::invalid_argument&) {
        duplicate_rejected = true;
    }
    if (!duplicate_rejected) {
        return example::fail("RadioGroup must reject duplicate live option values");
    }

    ui::HeadlessRenderer renderer{{620.0f, 360.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("headless Checkbox/Radio render failed");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(
        tree, "NativeUI T031 Checkbox and RadioButton", {620.0f, 360.0f});
}
