#include "example_support.hpp"

namespace {

int self_test() {
    constexpr ui::Size size{240.0f, 72.0f};
    example::Platform platform;
    ui::State<bool> value{false};
    int notifications = 0;

    auto observer = value.observe([&](const bool& current) {
        ++notifications;
        if (current) value.set(false);
    });

    ui::UI tree{ui::Toggle{"Reentrant", value}};
    tree.resize(size);
    tree.activate(platform);
    ui::HeadlessRenderer renderer{size, 1.0f};
    if (!renderer.render(tree)) return example::fail("Toggle reentrancy baseline render failed");

    tree.dispatch(example::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    tree.dispatch(example::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
    if (value.get() || notifications != 2) {
        return example::fail("first reentrant Toggle activation did not settle deterministically");
    }

    tree.dispatch(example::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    tree.dispatch(example::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
    if (value.get() || notifications != 4) {
        return example::fail("Toggle retained stale cached state after reentrant State notification");
    }

    return 0;
}

ui::UI make_demo() {
    return ui::UI{ui::Column{
        ui::Header{"T038 — Toggle Reentrancy"},
        ui::Label{"State observers may re-enter without leaving stale component state."}.size(12.0f),
    }.gap(12.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    auto tree = make_demo();
    return example::run_window(tree, "NativeUI T038 Toggle Reentrancy", {620.0f, 220.0f});
}
