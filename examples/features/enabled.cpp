#include "example_support.hpp"
#include <nativeui/enabled.hpp>

namespace {

int self_test() {
    ui::State<bool> allowed{true};
    int clicks = 0;
    ui::UI tree{ui::Enabled{allowed.binding(), ui::Button{"Action", [&] { ++clicks; }}}};
    example::Platform platform;
    tree.resize({160.0f, 40.0f});
    tree.activate(platform);
    tree.dispatch(example::pointer(ui::InputType::PointerDown, 10.0f, 10.0f), platform);
    allowed.set(false);
    allowed.set(true);
    tree.dispatch(example::pointer(ui::InputType::PointerUp, 10.0f, 10.0f), platform);
    if (clicks != 0) return example::fail("disabled armed button was activated");
    tree.dispatch(example::pointer(ui::InputType::PointerDown, 10.0f, 10.0f), platform);
    tree.dispatch(example::pointer(ui::InputType::PointerUp, 10.0f, 10.0f), platform);
    if (clicks != 1) return example::fail("re-enabled button did not recover");
    ui::HeadlessRenderer renderer{{160.0f, 40.0f}, 1.0f};
    return renderer.render(tree) ? 0 : example::fail("headless Enabled render failed");
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    ui::State<bool> allowed{true};
    ui::State<std::string> status{"Ready"};
    ui::UI tree{ui::Column{
        ui::Header{"Enabled with Binding"},
        ui::Button{"Enable / disable", [&] { allowed.set(!allowed.get()); }},
        ui::Enabled{allowed.binding(), ui::Button{"Action", [&] { status.set("Activated"); }}},
        ui::TextInput{"Status", status}}.padding(20.0f)};
    return example::run_window(tree, "NativeUI Enabled", {420.0f, 220.0f});
}
