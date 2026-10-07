#include "example_support.hpp"
#include <nativeui/accordion.hpp>

namespace {
int self_test() {
    ui::State<std::vector<std::string>> keys{{"unknown","second","first","first"}};
    int changes = 0;
    ui::UI tree{ui::Accordion{keys}.mode(ui::AccordionMode::Single)
        .style(ui::AccordionStyle{.section=ui::CollapsibleStyle{.reduced_motion=true}})
        .section("first","First",ui::Label{"First panel"})
        .section("disabled","Disabled",ui::Label{"Unused"},false)
        .section("second","Second",ui::Label{"Second panel"})
        .on_change([&](const auto&) { ++changes; })};
    example::Platform platform;
    tree.resize({320.0f,200.0f}); tree.activate(platform);
    if (keys.get().size() != 4 || changes != 0) return example::fail("accordion rewrote external keys");
    tree.dispatch(example::key(ui::Key::Down),platform);
    tree.dispatch(example::key(ui::Key::Right),platform);
    if (keys.get() != std::vector<std::string>{"second"} || changes != 1)
        return example::fail("accordion roving or exclusive opening failed");
    tree.dispatch(example::key(ui::Key::Home),platform);
    tree.dispatch(example::key(ui::Key::Right),platform);
    if (keys.get() != std::vector<std::string>{"first"} || changes != 2)
        return example::fail("accordion Home did not return to first header");
    ui::HeadlessRenderer renderer{{320.0f,200.0f},1.0f};
    return renderer.render(tree) ? 0 : example::fail("headless Accordion render failed");
}
}
int main(int argc,char** argv) {
    if (example::self_test_requested(argc,argv)) return self_test();
    ui::State<std::vector<std::string>> keys{{"general"}};
    ui::State<std::string> value{"Document"};
    ui::UI tree{ui::Column{ui::Header{"Accordion"},
        ui::Accordion{keys}.mode(ui::AccordionMode::Single)
            .section("general","General",ui::TextInput{"Name",value})
            .section("advanced","Advanced",ui::Button{"Apply settings",[] {}})
            .section("unavailable","Unavailable",ui::Label{"Disabled section"},false)}.padding(16.0f)};
    return example::run_window(tree,"NativeUI Accordion",{480.0f,320.0f});
}
