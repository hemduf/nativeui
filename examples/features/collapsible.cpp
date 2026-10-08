#include "example_support.hpp"
#include <nativeui/collapsible.hpp>

namespace {
int self_test() {
    ui::State<bool> open{false};
    int changes = 0;
    ui::UI tree{ui::Collapsible{"Options",open,ui::Button{"Content",[] {}}}
        .style(ui::CollapsibleStyle{.reduced_motion=true})
        .on_change([&](bool) { ++changes; })};
    example::Platform platform;
    tree.resize({320.0f,160.0f});
    tree.activate(platform);
    tree.dispatch(example::key(ui::Key::Enter),platform);
    if (open.get() || changes != 0) return example::fail("disclosure toggled before Enter release");
    auto up = example::key(ui::Key::Enter); up.type = ui::InputType::KeyUp;
    tree.dispatch(up,platform);
    if (!open.get() || changes != 1) return example::fail("disclosure release did not toggle once");
    tree.dispatch(up,platform);
    if (changes != 1) return example::fail("disclosure repeated a completed press");
    tree.dispatch(example::key(ui::Key::Left),platform);
    if (open.get() || changes != 2) return example::fail("disclosure Left did not close");
    open.set(true);
    if (changes != 2) return example::fail("external disclosure state invoked user callback");
    ui::HeadlessRenderer renderer{{320.0f,160.0f},1.0f};
    return renderer.render(tree) ? 0 : example::fail("headless Collapsible render failed");
}
}
int main(int argc,char** argv) {
    if (example::self_test_requested(argc,argv)) return self_test();
    ui::State<bool> open{true};
    ui::State<std::string> value{"Retained editor content"};
    ui::UI tree{ui::Column{ui::Header{"Collapsible"},
        ui::Collapsible{"Advanced options",open,
            ui::Column{ui::TextInput{"Description",value},ui::Button{"Apply",[] {}}}.padding(8.0f)}}.padding(16.0f)};
    return example::run_window(tree,"NativeUI Collapsible",{480.0f,240.0f});
}
