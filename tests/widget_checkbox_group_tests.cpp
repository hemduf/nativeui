#include "test_support.hpp"
#include <nativeui/checkbox_group.hpp>
namespace {
void suite() {
  ui::State<bool> a{false},b{true},fixed{false},locked{false};
  int notifications{}; bool fail{};
  auto observation=a.observe([&](bool){ ++notifications; if(fail) throw std::runtime_error("observer"); });
  ui::UI tree{ui::ReadOnly{locked,ui::CheckboxGroup{"Permissions",{
    {"a","A",a.binding()},{"b","B",b.binding()},{"fixed","Fixed",fixed.binding(),false}}}}};
  test::MockPlatform platform; tree.resize({300,180}); tree.activate(platform);
  auto root=tree.component_semantics(4); NUI_CHECK(root && root->role==ui::SemanticRole::Checkbox && root->checked==ui::SemanticCheckedState::Mixed);
  tree.dispatch(test::key(ui::Key::Space),platform); auto up=test::key(ui::Key::Space);up.type=ui::InputType::KeyUp;tree.dispatch(up,platform);
  NUI_CHECK(a.get() && b.get() && !fixed.get() && notifications==1);
  locked.set(true); tree.dispatch(test::key(ui::Key::Space),platform);tree.dispatch(up,platform);NUI_CHECK(notifications==1);
  locked.set(false); a.set(false); b.set(false);fail=true;
  tree.dispatch(test::key(ui::Key::Space),platform);bool caught{};try{tree.dispatch(up,platform);}catch(const std::runtime_error&){caught=true;}
  NUI_CHECK(caught && a.get() && !b.get());fail=false;tree.resize({300,180});
  tree.dispatch(up,platform);NUI_CHECK(!b.get());
  tree.dispatch(test::key(ui::Key::Space),platform);tree.dispatch(up,platform);NUI_CHECK(a.get() && b.get());
  bool invalid{};try{(void)ui::CheckboxGroup{"Duplicate",{{"x","A",a.binding()},{"x","B",b.binding()}}};}catch(const std::invalid_argument&){invalid=true;}NUI_CHECK(invalid);
  ui::HeadlessRenderer renderer{{300,180},1};NUI_CHECK(renderer.render(tree));
}
}
int main(){return test::run("widget_checkbox_group",&suite);}
