#include "test_support.hpp"
#include <nativeui/tooltip.hpp>
namespace {
class Anchor final:public ui::Component {
public:
 bool focusable()const noexcept override{return true;}
 ui::Size measure(const std::vector<ui::ChildMetrics>&)const override{return {100,30};}
 ui::SemanticInfo semantics()const override{ui::SemanticInfo info;info.role=ui::SemanticRole::Button;info.name="Anchor";info.description=description;return info;}
 void paint(ui::PaintContext&)const override{}
 std::string description;
};
void suite(){
 ui::TooltipStyle narrow;narrow.max_width=100.0f;narrow.padding=5.0f;narrow.background=ui::Color{.9f,.1f,.1f,1};
 ui::TooltipStyle broad=narrow;broad.max_width=300.0f;
 ui::detail::TooltipSurfaceComponent a{"A long message which must wrap into several lines",narrow},b{"A long message which must wrap into several lines",broad};
 const auto small=a.measure({}),large=b.measure({});NUI_CHECK(small.w<=100 && small.h>large.h);
 auto anchor=ui::Spec{[]{return std::make_unique<Anchor>();},{}};
 ui::UI tree{ui::Tooltip{"Help",anchor}.style(narrow)};test::MockPlatform platform;tree.resize({200,100});tree.activate(platform);
 auto info=tree.component_semantics(3);NUI_CHECK(info && info->name=="Anchor" && info->description=="Help");
 auto explicit_anchor=ui::Spec{[]{auto value=std::make_unique<Anchor>();value->description="Explicit";return value;},{}};
 ui::UI explicit_tree{ui::Tooltip{"Help",std::move(explicit_anchor)}};explicit_tree.resize({200,100});explicit_tree.activate(platform);
 auto preserved=explicit_tree.component_semantics(3);NUI_CHECK(preserved && preserved->description=="Explicit");
 ui::HeadlessRenderer renderer{{200,100},1};NUI_CHECK(renderer.render(tree));
 auto builder=ui::Tooltip{"Legacy",ui::Button{"B",[]{}}};builder.delay(std::chrono::milliseconds{-1});NUI_CHECK(builder.delay().count()==0);
}
}
int main(){return test::run("widget_tooltip_extensions",&suite);}
