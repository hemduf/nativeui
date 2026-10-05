#include "example_support.hpp"
#include <nativeui/checkbox_group.hpp>

namespace {
int self_test() {
    ui::State<bool> first{false}, second{true};
    ui::UI tree{ui::CheckboxGroup{"Export", {
        {"audio", "Audio", first.binding()}, {"notes", "Notes", second.binding()}}}};
    example::Platform platform;
    tree.resize({300.0f, 160.0f}); tree.activate(platform);
    tree.dispatch(example::key(ui::Key::Space), platform);
    auto up = example::key(ui::Key::Space); up.type = ui::InputType::KeyUp;
    tree.dispatch(up, platform);
    ui::HeadlessRenderer renderer{{300.0f, 160.0f}, 1.0f};
    return first.get() && second.get() && renderer.render(tree)
        ? 0 : example::fail("group activation did not select both mutable children");
}
}
int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    ui::State<bool> audio{true}, notes{false}, markers{true};
    ui::UI tree{ui::Padding{20.0f, ui::CheckboxGroup{"Exporter", {
        {"audio", "Audio", audio.binding()}, {"notes", "Notes", notes.binding()},
        {"markers", "Marqueurs", markers.binding()}}}}};
    return example::run_window(tree, "NativeUI CheckboxGroup", {360.0f, 220.0f});
}
