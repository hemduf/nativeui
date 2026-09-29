#include "example_support.hpp"

int main(int argc,char** argv){
    if(example::self_test_requested(argc,argv)) {
        // The native lifecycle is exercised by embedded_visibility_tests;
        // this public example's self-test requires no display server.
        static_assert(ui::EmbeddedViewOptions{}.initially_visible);
        static_assert(!ui::EmbeddedViewOptions{.initially_visible=false}.initially_visible);
        ui::State<float> value{0.5f};
        ui::UI content{ui::Knob{"Embedded content",value}};
        ui::HeadlessRenderer renderer{{280,200},1};
        return renderer.render(content) ? 0 : example::fail("content render failed");
    }
    ui::Application application;
    std::unique_ptr<ui::EmbeddedView> child;
    ui::UI parent_ui{ui::Column{
        ui::Spacer{220},
        ui::Button{"Show child",[&]{if(child) child->show();}},
        ui::Button{"Hide child",[&]{if(child) child->hide();}}
    }.gap(12).padding(16)};
    ui::StandaloneWindow parent{application,parent_ui,
        ui::WindowDesc{.title="Embedded show / hide",.size={460,380}}};
    ui::State<float> value{0.5f};
    ui::UI content{ui::Knob{"Embedded content",value}};
    child=std::make_unique<ui::EmbeddedView>(content,parent.native_handle(),ui::Size{280,200},
        std::shared_ptr<ui::DesktopServicesBackend>{},ui::EmbeddedViewOptions{.initially_visible=false});
    while(!parent.should_close()) {
        application.poll(0.01); child->poll();
    }
    child.reset(); // borrowed content remains alive until the view is destroyed
    return 0;
}
