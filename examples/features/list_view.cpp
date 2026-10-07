#include "example_support.hpp"
#include <nativeui/list_view.hpp>

namespace {
int self_test() {
    ui::State<std::optional<int>> selection{1}; int activated=0;
    ui::UI tree{ui::ListView{selection}.item(1,ui::Spacer{160.0f,28.0f})
        .item(2,ui::Spacer{160.0f,28.0f},false).item(3,ui::Spacer{160.0f,28.0f})
        .on_activate([&](const int& key) { activated=key; })};
    example::Platform platform; tree.resize({160.0f,100.0f}); tree.activate(platform);
    tree.dispatch(example::key(ui::Key::Down),platform);
    tree.dispatch(example::key(ui::Key::Enter),platform);
    if (selection.get()!=std::optional<int>{3} || activated!=3) return example::fail("ListView did not skip disabled row and activate selection");
    tree.dispatch(example::pointer(ui::InputType::PointerDown,10.0f,12.0f),platform);
    tree.dispatch(example::pointer(ui::InputType::PointerUp,10.0f,12.0f),platform);
    if (selection.get()!=std::optional<int>{1} || activated!=1) return example::fail("ListView pointer did not activate selected row");
    ui::HeadlessRenderer renderer{{160.0f,100.0f},1.0f};
    return renderer.render(tree)?0:example::fail("headless ListView render failed");
}
}
int main(int argc,char** argv) {
    if (example::self_test_requested(argc,argv)) return self_test();
    ui::State<std::optional<int>> selection{1};
    ui::UI tree{ui::ListView{selection}
        .item(1,ui::Padding{12.0f,ui::Label{"First"}})
        .item(2,ui::Padding{12.0f,ui::Label{"Unavailable"}},false)
        .item(3,ui::Padding{12.0f,ui::Label{"Third"}})};
    return example::run_window(tree,"NativeUI ListView",{360.0f,220.0f});
}
