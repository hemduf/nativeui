#include "example_support.hpp"
#include <nativeui/field.hpp>

namespace {
int self_test() {
    ui::State<bool> value{false},locked{false}; ui::State<std::string> error{"An error that wraps across several lines in a narrow field."};
    ui::UI tree{ui::ReadOnly{locked,ui::Field{"Enable",ui::Checkbox{value,""}}.error(error).required()}};
    example::Platform platform; tree.resize({220.0f,220.0f}); tree.activate(platform);
    tree.dispatch(example::pointer(ui::InputType::PointerDown,4.0f,4.0f),platform);
    tree.dispatch(example::pointer(ui::InputType::PointerUp,4.0f,4.0f),platform);
    if (!value.get()) return example::fail("Field label did not activate its checkbox");
    locked.set(true);
    tree.dispatch(example::pointer(ui::InputType::PointerDown,4.0f,4.0f),platform);
    tree.dispatch(example::pointer(ui::InputType::PointerUp,4.0f,4.0f),platform);
    if (!value.get()) return example::fail("read-only Field label changed checkbox");
    const auto with_error=tree.measure(ui::Constraints{{},{220.0f,ui::kUnboundedExtent}}).preferred.h;
    error.set("");
    const auto without_error=tree.measure(ui::Constraints{{},{220.0f,ui::kUnboundedExtent}}).preferred.h;
    ui::HeadlessRenderer renderer{{220.0f,220.0f},1.0f};
    return without_error<with_error && renderer.render(tree)?0:example::fail("Field error removal did not reflow");
}
}
int main(int argc,char** argv) {
    if (example::self_test_requested(argc,argv)) return self_test();
    ui::State<std::string> email{""},error{"The address is required."};
    ui::UI tree{ui::Column{ui::Header{"Field"},
        ui::Field{"Address",ui::TextInput{"Email address",email}}.description("The label targets the field control.").error(error).required(),
        ui::Button{"Clear the error",[&] { error.set(""); }}}.padding(20.0f)};
    return example::run_window(tree,"NativeUI Field",{480.0f,280.0f});
}
