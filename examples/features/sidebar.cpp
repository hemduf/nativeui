#include "example_support.hpp"
#include <nativeui/sidebar.hpp>
namespace {
int self_test() {
    ui::State<std::optional<std::string>> chosen{std::string{"inbox"}};
    ui::State<bool> open{true};
    int calls = 0;
    ui::SidebarStyle style;
    style.padding = 0;
    style.gap = 0;
    ui::UI tree{ui::Sidebar<std::string>{chosen}
                    .section("mail", "Mail", open)
                    .item("inbox", "Inbox")
                    .item("sent", "Sent")
                    .style(style)
                    .on_navigate([&](const auto &) { ++calls; })};
    example::Platform platform;
    tree.resize({220, 180});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{220, 180}, 1};
    if (!renderer.render(tree))
        return example::fail("sidebar render failed");
    tree.dispatch(example::key(ui::Key::Down), platform);
    tree.dispatch(example::key(ui::Key::Enter), platform);
    if (chosen.get() != std::optional<std::string>{"sent"} || calls != 2)
        return example::fail("sidebar selection/navigation failed");
    tree.dispatch(example::pointer(ui::InputType::PointerDown, 25, 12), platform);
    tree.dispatch(example::pointer(ui::InputType::PointerUp, 25, 12), platform);
    if (open.get() || chosen.get() != std::optional<std::string>{"sent"} || calls != 2)
        return example::fail("sidebar section closure rewrote selection");
    tree.dispatch(example::key(ui::Key::Right), platform);
    if (!open.get() || calls != 2 || !renderer.render(tree))
        return example::fail("sidebar recovery failed");
    return 0;
}
} // namespace
int main(int argc, char **argv) {
    if (example::self_test_requested(argc, argv))
        return self_test();
    ui::State<std::optional<std::string>> selected{std::string{"inbox"}};
    ui::State<bool> mail_open{true}, projects_open{true};
    ui::UI tree{
        ui::Sidebar<std::string>{selected}
            .item("overview", "Overview")
            .section("mail", "Mail", mail_open)
            .item("inbox", "Inbox", ui::Label{"12"})
            .item("sent", "Sent")
            .item("archive", "Archive")
            .section("projects", "Projects", projects_open)
            .item("nativeui", "NativeUI")
            .item("mygo", "MyGo")
            .item("unavailable", "Unavailable", false)
            .on_navigate([](const auto &key) { std::cout << "Navigate: " << key << '\n'; })};
    return example::run_window(tree, "NativeUI Sidebar", {300, 480});
}
