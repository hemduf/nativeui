#include "example_support.hpp"
#include <nativeui/read_only.hpp>

namespace {

int self_test() {
    ui::State<bool> locked{true};
    ui::State<std::string> value{"Original"};
    ui::UI tree{ui::ReadOnly{locked.binding(), ui::TextInput{"Value", value}}};
    example::Platform platform;
    tree.resize({220.0f, 40.0f});
    tree.activate(platform);
    ui::InputEvent edit{};
    edit.type = ui::InputType::TextInput;
    edit.text = "x";
    tree.dispatch(edit, platform);
    if (value.get() != "Original") return example::fail("read-only input accepted editing");
    value.set("External");
    tree.dispatch(edit, platform);
    if (value.get() != "External") return example::fail("external update was changed");
    locked.set(false);
    tree.dispatch(example::key(ui::Key::End), platform);
    tree.dispatch(edit, platform);
    if (value.get() != "Externalx") return example::fail("unlocked input did not recover");
    ui::HeadlessRenderer renderer{{220.0f, 40.0f}, 1.0f};
    return renderer.render(tree) ? 0 : example::fail("headless ReadOnly render failed");
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    ui::State<bool> locked{true};
    ui::State<std::string> value{"An externally managed value"};
    ui::UI tree{ui::Column{
        ui::Header{"ReadOnly with Binding"},
        ui::Button{"Lock / unlock", [&] { locked.set(!locked.get()); }},
        ui::Button{"Replace externally", [&] { value.set("External update"); }},
        ui::ReadOnly{locked.binding(), ui::TextInput{"Value", value}}}.padding(20.0f)};
    return example::run_window(tree, "NativeUI ReadOnly", {440.0f, 240.0f});
}
