#include "example_support.hpp"

namespace {
struct Demo {
    ui::State<float> value{0.5f};
    ui::State<bool> enabled{true};
    std::string trace;
    template<class T> ui::EditCallbacks<T> callbacks() {
        return {
            [this](ui::EditSource){trace+='B';},
            [this](const T&,ui::EditSource){trace+='C';},
            [this](ui::EditSource){trace+='E';std::cout<<trace<<'\n';trace.clear();},
            [this](ui::EditSource){trace+='X';std::cout<<trace<<'\n';trace.clear();}};
    }
    ui::UI make_ui() {
        return ui::UI{ui::Column{
            ui::Label{"Drag, use arrow keys, or scroll. Escape cancels a drag."},
            ui::Knob{"Value",value}.on_edit(callbacks<float>()).wheel_enabled(),
            ui::Slider{value}.on_edit(callbacks<float>()).wheel_enabled(),
            ui::Toggle{"Enabled",enabled}.on_edit(callbacks<bool>()),
            ui::Label{"Begin/change/end/cancel events are printed to stdout."}
        }.padding(16).gap(12)};
    }
};
int self_test(){
    ui::State<double> value{0.0}; std::string trace;
    ui::EditSession<double> custom_control{value.binding(),{
        [&](ui::EditSource){trace+='B';},
        [&](const double&,ui::EditSource){trace+='C';},
        [&](ui::EditSource){trace+='E';},
        [&](ui::EditSource){trace+='X';}}};
    custom_control.set(0.5,ui::EditSource::Keyboard);
    custom_control.begin(ui::EditSource::Pointer);
    custom_control.update(0.75);custom_control.cancel();
    value.set(0.25); // model synchronization emits no editing callbacks
    if(trace!="BCEBCX") return example::fail("incorrect edit lifetime");
    Demo demo; auto tree=demo.make_ui();
    ui::HeadlessRenderer renderer{{640,420},1};
    return renderer.render(tree) ? 0 : example::fail("render failed");
}
}
int main(int argc,char** argv){
    if(example::self_test_requested(argc,argv)) return self_test();
    Demo demo;auto tree=demo.make_ui();
    return example::run_window(tree,"Value edit sessions",{640,420});
}
