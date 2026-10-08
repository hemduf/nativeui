#include "example_support.hpp"
#include <nativeui/popover.hpp>
namespace {
int self_test() {
    ui::State<bool> open{false};
    int closed = 0;
    ui::UI tree{ui::Column{ui::Popover{open, ui::Button{"Options", [&] { open.set(!open.get()); }},
                                       ui::Label{"Advanced settings"}}
                               .on_close([&] { ++closed; }),
                           ui::Spacer{0.0f, 100.0f}}
                    .padding(0.0f)
                    .gap(0.0f)};
    example::Platform platform;
    tree.resize({240.0f, 180.0f});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{240.0f, 180.0f}, 1.0f};
    open.set(true);
    if (!renderer.render(tree) || tree.overlay_entries().size() != 1)
        return example::fail("popover did not open on its retained anchor");
    tree.dispatch(example::key(ui::Key::Escape), platform);
    if (open.get() || closed != 1 || !tree.overlay_entries().empty())
        return example::fail("popover Escape did not close its binding exactly once");
    open.set(true);
    if (!renderer.render(tree) || tree.overlay_entries().size() != 1)
        return example::fail("popover did not recover after close");
    open.set(false);
    if (!renderer.render(tree) || closed != 1 || !tree.overlay_entries().empty())
        return example::fail("external close incorrectly emitted a user callback");
    return 0;
}
} // namespace
int main(int argc, char **argv) {
    if (example::self_test_requested(argc, argv))
        return self_test();
    ui::State<bool> open{false};
    ui::UI tree{ui::Padding{
        24.0f, ui::Column{ui::Label{"Anchored composition panel"},
                          ui::Popover{open, ui::Button{"Options", [&] { open.set(!open.get()); }},
                                      ui::Column{ui::Label{"Advanced settings"},
                                                 ui::Button{"Close", [&] { open.set(false); }}}
                                          .padding(0.0f)
                                          .gap(8.0f)}
                              .focus_on_open(),
                          ui::Label{"Escape or an outside click closes the panel."}}}};
    return example::run_window(tree, "NativeUI Popover", {440.0f, 320.0f});
}
