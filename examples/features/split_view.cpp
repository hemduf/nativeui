#include "example_support.hpp"
#include <nativeui/split_view.hpp>

namespace {
int self_test() {
    ui::State<double> extent{100.0};
    int changes = 0;
    int commits = 0;
    ui::UI tree{ui::SplitView{extent.binding(),ui::Spacer{80.0f,80.0f},ui::Spacer{80.0f,80.0f}}
        .on_change([&](double) { ++changes; }).on_commit([&](double) { ++commits; })};
    example::Platform platform;
    tree.resize({301.0f,100.0f});
    tree.activate(platform);
    tree.dispatch(example::key(ui::Key::Right),platform);
    if (extent.get() != 110.0 || changes != 1 || commits != 1)
        return example::fail("split keyboard edit did not publish and commit once");
    tree.dispatch(example::pointer(ui::InputType::PointerDown,110.5f,40.0f),platform);
    tree.dispatch(example::pointer(ui::InputType::PointerMove,130.5f,40.0f),platform);
    tree.dispatch(example::pointer(ui::InputType::PointerUp,130.5f,40.0f),platform);
    if (extent.get() != 130.0 || changes != 2 || commits != 2)
        return example::fail("split drag did not publish and commit once");
    tree.dispatch(example::pointer(ui::InputType::PointerDown,130.5f,40.0f),platform);
    tree.dispatch(example::pointer(ui::InputType::PointerMove,150.5f,40.0f),platform);
    tree.dispatch(example::key(ui::Key::Escape),platform);
    if (extent.get() != 130.0 || commits != 2)
        return example::fail("split Escape did not restore the gesture origin");
    tree.resize({61.0f,100.0f});
    if (extent.get() != 130.0) return example::fail("split resize changed the application model");
    ui::HeadlessRenderer renderer{{301.0f,100.0f},1.0f};
    return renderer.render(tree) ? 0 : example::fail("headless SplitView render failed");
}
} // namespace

int main(int argc,char** argv) {
    if (example::self_test_requested(argc,argv)) return self_test();
    ui::State<double> extent{220.0};
    ui::State<std::string> document{"Adjust the separator or use its arrow keys"};
    ui::State<std::string> status{"First pane: 220 DIP"};
    ui::UI tree{ui::Column{
        ui::Header{"SplitView"},
        ui::SplitView{extent,
            ui::Column{ui::Label{"Sources"},ui::Button{"Document",[&] { document.set("A document"); }}}
                .padding(12.0f),
            ui::Column{ui::Label{"Editor"},ui::TextInput{"Document",document}}.padding(12.0f)}
            .minimum_panes(100.0,160.0)
            .on_change([&](double value) { status.set("First pane: " + std::to_string(value) + " DIP"); }),
        ui::TextInput{"Status",status}}.padding(16.0f)};
    return example::run_window(tree,"NativeUI SplitView",{760.0f,300.0f});
}
