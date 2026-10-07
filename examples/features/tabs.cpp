#include "example_support.hpp"
#include <nativeui/tabs.hpp>

namespace {
int self_test() {
    ui::State<int> selection{1};
    ui::UI tree{ui::Tabs{selection}.tab(1,"General",ui::Label{"Retained first panel"})
        .tab(2,"Unavailable",ui::Spacer{100.0f,40.0f},false)
        .tab(3,"Advanced",ui::Label{"Retained third panel"})};
    example::Platform platform;
    tree.resize({360.0f,180.0f}); tree.activate(platform);
    tree.dispatch(example::key(ui::Key::Right),platform);
    if (selection.get() != 3) return example::fail("Tabs Right did not skip disabled tab");
    tree.dispatch(example::key(ui::Key::Right),platform);
    if (selection.get() != 1) return example::fail("Tabs Right did not wrap");
    tree.dispatch(example::key(ui::Key::End),platform);
    if (selection.get() != 3) return example::fail("Tabs End did not choose last enabled tab");
    selection.set(99);
    ui::HeadlessRenderer renderer{{360.0f,180.0f},1.0f};
    if (!renderer.render(tree) || selection.get() != 99) return example::fail("Tabs unknown key changed model");
    return 0;
}
}
int main(int argc,char** argv) {
    if (example::self_test_requested(argc,argv)) return self_test();
    ui::State<int> page{1}; ui::State<std::string> title{"Untitled"};
    ui::UI tree{ui::Tabs{page}
        .tab(1,"General",ui::Column{ui::Header{"General"},ui::TextInput{"Name",title}}.padding(16.0f))
        .tab(2,"Advanced",ui::Column{ui::Header{"Advanced"},ui::Label{"Panel state survives selection changes"}}.padding(16.0f))};
    return example::run_window(tree,"NativeUI Tabs",{560.0f,300.0f});
}
