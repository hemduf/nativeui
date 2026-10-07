#include "example_support.hpp"
#include <nativeui/visibility.hpp>

namespace {

int self_test() {
    ui::State<ui::VisibilityMode> mode{ui::VisibilityMode::Visible};
    ui::UI tree{ui::Column{
        ui::Visibility{mode.binding(), ui::Spacer{40.0f, 30.0f}},
        ui::Spacer{40.0f, 20.0f}}.gap(10.0f).padding(0.0f)};
    if (!example::near(tree.measure().preferred.h, 60.0f))
        return example::fail("visible subtree has wrong intrinsic height");
    mode.set(ui::VisibilityMode::Hidden);
    if (!example::near(tree.measure().preferred.h, 60.0f))
        return example::fail("Hidden removed layout space");
    mode.set(ui::VisibilityMode::Collapsed);
    if (!example::near(tree.measure().preferred.h, 20.0f))
        return example::fail("Collapsed retained a ghost gap");
    mode.set(ui::VisibilityMode::Visible);
    tree.resize({140.0f, 80.0f});
    ui::HeadlessRenderer renderer{{140.0f, 80.0f}, 1.0f};
    return renderer.render(tree) ? 0 : example::fail("headless Visibility render failed");
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    ui::State<ui::VisibilityMode> mode{ui::VisibilityMode::Visible};
    ui::UI tree{ui::Column{
        ui::Header{"Visibility with Binding"},
        ui::Row{
            ui::Button{"Visible", [&] { mode.set(ui::VisibilityMode::Visible); }},
            ui::Button{"Hidden", [&] { mode.set(ui::VisibilityMode::Hidden); }},
            ui::Button{"Collapsed", [&] { mode.set(ui::VisibilityMode::Collapsed); }}},
        ui::Visibility{mode.binding(), ui::Label{"This retained content keeps its identity."}},
        ui::Label{"Hidden preserves space; Collapsed removes it."}}.padding(20.0f)};
    return example::run_window(tree, "NativeUI Visibility", {520.0f, 240.0f});
}
