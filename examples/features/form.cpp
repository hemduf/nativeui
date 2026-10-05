#include "example_support.hpp"
#include <nativeui/form.hpp>
#include <nativeui/field.hpp>
#include <nativeui/fieldset.hpp>

namespace {
int self_test() {
    auto first=std::make_shared<example::BoxObservation>(),second=std::make_shared<example::BoxObservation>();
    ui::UI tree{ui::Form{
        ui::Field{"Libellé extérieur assez long",example::Box{"A",{120.0f,28.0f},ui::colors::accent,{50.0f,20.0f},first}},
        ui::Fieldset{"Groupe",ui::Field{"Court",example::Box{"B",{120.0f,28.0f},ui::colors::accent,{50.0f,20.0f},second}}}}
        .layout(ui::FormLayout::Responsive)};
    ui::HeadlessRenderer wide{{600.0f,240.0f},1.0f};
    if (!wide.render(tree) || !example::near(first->bounds.x,second->bounds.x)) return example::fail("Fieldset did not share the Form label column");
    const auto aligned_x=first->bounds.x;
    ui::HeadlessRenderer narrow{{280.0f,320.0f},1.0f};
    if (!narrow.render(tree) || !(first->bounds.x<aligned_x) || !example::near(first->bounds.x,second->bounds.x)) return example::fail("responsive Form did not stack fields");
    int submits=0;
    ui::UI action{ui::Form{ui::Button{"Control",[] {}}}.on_submit([&] { ++submits; })};
    example::Platform platform; action.resize({320.0f,160.0f}); action.activate(platform);
    action.dispatch(example::key(ui::Key::Enter),platform);
    if (submits!=0) return example::fail("Form submitted from ordinary Enter");
    ui::InputEvent command; command.type=ui::InputType::Command; command.command=ui::Command::Submit;
    action.dispatch(command,platform);
    return submits==1?0:example::fail("explicit Submit did not invoke Form callback once");
}
}
int main(int argc,char** argv) {
    if (example::self_test_requested(argc,argv)) return self_test();
    ui::State<std::string> name{"Camille"},notes{""}; ui::State<bool> newsletter{true};
    ui::UI tree{ui::Padding{20.0f,ui::Form{
        ui::Field{"Nom",ui::TextInput{"",name}}.required(),
        ui::Fieldset{"Préférences",ui::Field{"Lettre d’information",ui::Checkbox{newsletter,""}}},
        ui::Field{"Notes",ui::TextArea{"",notes}}.description("Entrée ajoute une ligne dans ce champ.")}
        .layout(ui::FormLayout::Responsive)}};
    return example::run_window(tree,"NativeUI Form",{640.0f,360.0f});
}
