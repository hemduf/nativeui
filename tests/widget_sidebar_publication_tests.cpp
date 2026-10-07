#include "test_support.hpp"
#include <nativeui/sidebar.hpp>
#include <nativeui/enabled.hpp>
#include <nativeui/read_only.hpp>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
namespace {
enum class Policy { ReadOnly, Enabled };
void settle(ui::UI& tree, ui::HeadlessRenderer& renderer) {
  for (int pass = 0; pass < 4; ++pass) NUI_CHECK(renderer.render(tree));
}
template <class Value>
void publication_boundary(std::string_view family, Policy policy, ui::State<Value>& value,
                          ui::Spec control, const ui::InputEvent& publication,
                          const Value& accepted) {
  const Value initial = value.snapshot();
  int notifications{};
  auto subscription = value.observe([&](const Value&) { ++notifications; });
  ui::State<bool> enabled{true};
  ui::State<bool> read_only{false};
  ui::UI tree{ui::Enabled{enabled, ui::ReadOnly{read_only, std::move(control)}}};
  test::MockPlatform platform;
  const ui::Size size{100.0f, 120.0f};
  tree.resize(size);
  tree.activate(platform);
  ui::HeadlessRenderer renderer{size, 1.0f};
  settle(tree, renderer);
  NUI_CHECK(value.get() == initial && notifications == 0);

  bool armed{};
  bool before_publication{};
  int boundaries{};
  tree.set_invalidation_callback([&](ui::Rect) {
    if (!std::exchange(armed, false)) return;
    ++boundaries;
    before_publication = value.get() == initial && notifications == 0;
    if (policy == Policy::ReadOnly) read_only.set(true);
    else enabled.set(false);
  });
  std::cout << "ORACLE " << family << ' '
            << (policy == Policy::ReadOnly ? "read_only" : "enabled") << '\n';
  armed = true;
  tree.dispatch(publication, platform);
  NUI_CHECK(boundaries == 1);
  NUI_CHECK(before_publication);
  NUI_CHECK(value.get() == initial);
  NUI_CHECK(notifications == 0);

  tree.clear_invalidation_callback();
  // Recover normal permission without reconstructing the UI or source model.
  enabled.set(true);
  read_only.set(false);
  settle(tree, renderer);
  tree.dispatch(publication, platform);
  NUI_CHECK(value.get() == accepted);
  NUI_CHECK(notifications == 1);
  tree.deactivate(platform);
  NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}

void sidebar(Policy policy) {
  ui::State<std::optional<int>> value{1};
  auto control = ui::make_spec(ui::Sidebar<int>{value}.item(1,"one").item(2,"two"));
  publication_boundary("sidebar_selection",policy,value,std::move(control),
                       test::key(ui::Key::Down),std::optional<int>{2});
}
void read_only() { sidebar(Policy::ReadOnly); }
void enabled() { sidebar(Policy::Enabled); }
void external_selection() {
  ui::State<std::optional<int>> value{1};
  int notifications{};
  auto subscription=value.observe([&](const auto&){++notifications;});
  ui::UI tree{ui::Sidebar<int>{value}.item(1,"one").item(2,"two").item(9,"nine")};
  test::MockPlatform platform;tree.resize({150,150});tree.activate(platform);
  ui::HeadlessRenderer renderer{{150,150},1};settle(tree,renderer);
  bool armed{};int boundaries{};
  tree.set_invalidation_callback([&](ui::Rect){
    if(!std::exchange(armed,false))return;
    ++boundaries;NUI_CHECK(value.get()==std::optional<int>{1});
    value.set(9);
  });
  armed=true;tree.dispatch(test::key(ui::Key::Down),platform);
  NUI_CHECK(boundaries==1 && value.get()==std::optional<int>{9} && notifications==1);
  tree.clear_invalidation_callback();settle(tree,renderer);
  tree.dispatch(test::key(ui::Key::Home),platform);
  NUI_CHECK(value.get()==std::optional<int>{1} && notifications==2);
  tree.deactivate(platform);
}
void suite(){read_only();enabled();external_selection();}
}
int main(int argc,char** argv){
 const std::string_view mode=argc>1?argv[1]:"all";
 if(mode=="read_only")return test::run("sidebar_read_only",&read_only);
 if(mode=="enabled")return test::run("sidebar_enabled",&enabled);
 if(mode=="external")return test::run("sidebar_external",&external_selection);
 return test::run("sidebar_publication",&suite);
}
