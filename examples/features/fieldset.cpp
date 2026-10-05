#include "example_support.hpp"
#include <nativeui/fieldset.hpp>
#include <nativeui/field.hpp>

namespace {
int self_test() {
    ui::State<bool> allowed{false},value{true};
    ui::UI tree{ui::Column{ui::Button{"Outside",[] {}},
        ui::Fieldset{"Preferences",ui::Field{"Option",ui::Checkbox{value,""}}}.enabled(allowed)}.padding(0.0f).gap(0.0f)};
    example::Platform platform; tree.resize({300.0f,180.0f}); tree.activate(platform);
    // Tab skips the unavailable group and keeps the exterior control active.
    tree.dispatch(example::key(ui::Key::Tab),platform);
    tree.dispatch(example::key(ui::Key::Space),platform);
    auto up=example::key(ui::Key::Space); up.type=ui::InputType::KeyUp; tree.dispatch(up,platform);
    if (!value.get()) return example::fail("disabled Fieldset changed a control value");
    allowed.set(true); tree.refresh_focus(platform);
    // Enabling a group makes its controls reachable without stealing focus.
    tree.dispatch(example::key(ui::Key::Space),platform); tree.dispatch(up,platform);
    if (!value.get()) return example::fail("enabled Fieldset unexpectedly stole focus");
    tree.dispatch(example::key(ui::Key::Tab),platform);
    tree.dispatch(example::key(ui::Key::Space),platform); tree.dispatch(up,platform);
    if (value.get()) return example::fail("enabled Fieldset did not restore control interaction");
    ui::HeadlessRenderer renderer{{300.0f,180.0f},1.0f};
    return renderer.render(tree)?0:example::fail("headless Fieldset render failed");
}
}
int main(int argc,char** argv) {
    if (example::self_test_requested(argc,argv)) return self_test();
    ui::State<bool> allowed{true},checked{true}; ui::State<std::string> city{"Paris"};
    ui::UI tree{ui::Column{ui::Checkbox{allowed,"Enable preferences"},
        ui::Fieldset{"Shipping",ui::Field{"City",ui::TextInput{"",city}},ui::Field{"Notifications",ui::Checkbox{checked,""}}}
            .description("Values are preserved while the group is disabled.").enabled(allowed)}.padding(20.0f)};
    return example::run_window(tree,"NativeUI Fieldset",{480.0f,300.0f});
}
