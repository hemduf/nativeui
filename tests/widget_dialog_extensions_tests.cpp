#include "test_support.hpp"
#include <nativeui/dialog.hpp>
namespace {
void suite() {
  ui::UI tree{ui::Button{"Document", [] {}}}; test::MockPlatform platform;
  tree.resize({300,210}); tree.activate(platform); ui::Dialog dialog{tree};
  int completions{}; ui::DialogResult result;
  ui::AlertDialogSpec alert; alert.title="Confirm";alert.message="The message is owned";
  alert.actions={{"cancel","Close",true,ui::DialogActionRole::Cancel},{"ok","Confirm",true,ui::DialogActionRole::Default}};
  NUI_CHECK(dialog.show_alert(std::move(alert),[&](ui::DialogResult r){++completions;result=std::move(r);})==ui::DialogShowResult::Shown);
  ui::HeadlessRenderer renderer{{300,210},1};NUI_CHECK(renderer.render(tree));
  tree.dispatch(test::key(ui::Key::Escape),platform);NUI_CHECK(!dialog.active() && completions==1 && result.action_id=="cancel");
  ui::DialogSpec request;request.body=ui::Label{"Body"}.spec();request.style=ui::DialogStyle{};request.style->width=240.0f;request.style->padding=12.0f;request.description="Owned description";
  request.actions={{"a","First very long action",true},{"b","Second very long action",true},{"c","Third very long action",true}};
  NUI_CHECK(dialog.show(std::move(request),[&](ui::DialogResult){++completions;})==ui::DialogShowResult::Shown);NUI_CHECK(renderer.render(tree));
  // The shared panel layout must wrap and bound every action independently.
  ui::detail::DialogPanelLayout layout{0,std::nullopt,{1,2,3}};
  ui::detail::DialogPanelComponent panel{layout,std::make_shared<ui::ScrollState>(),[]{}};
  std::vector<ui::ChildMetrics> metrics{ui::ChildMetrics{{100,40}},ui::ChildMetrics{{140,30}},ui::ChildMetrics{{140,30}},ui::ChildMetrics{{140,30}}};
  std::vector<ui::ChildPlacement> places(4); panel.layout_children({0,0,230,220},metrics,places);
  for(std::size_t i=1;i<places.size();++i){NUI_CHECK(places[i].bounds.x>=20 && places[i].bounds.x+places[i].bounds.w<=210);NUI_CHECK(places[i].bounds.h>=0);}
  NUI_CHECK(places[2].bounds.y>places[1].bounds.y && places[3].bounds.y>places[2].bounds.y);
  NUI_CHECK(dialog.close() && completions==2);
  ui::AlertDialogSpec empty;NUI_CHECK(dialog.show_alert(std::move(empty),[](ui::DialogResult){})==ui::DialogShowResult::Shown);NUI_CHECK(dialog.close());
}
}
int main(){return test::run("widget_dialog_extensions",&suite);}
