#include "test_support.hpp"
namespace {
struct Record {
  std::function<void()> request;
  ui::NodeId target{};
  int actions{}, inputs{}, focuses{};
  bool throw_action{};
  std::function<void()> on_focus;
  std::function<void()> on_blur;
};
class Control final : public ui::Component {
public:
  Control(std::shared_ptr<Record> record,std::string name) : record_(std::move(record)),name_(std::move(name)) {}
  bool focusable() const noexcept override { return true; }
  void mount(ui::MountContext& context) override { record_->target = context.node_id(); }
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {40,25}; }
  void paint(ui::PaintContext&) const override {}
  ui::SemanticInfo semantics() const override {
    ui::SemanticInfo info; info.role = ui::SemanticRole::Toggle; info.name = name_;
    info.description = "Own"; info.focusable = true;
    info.enabled = effective_enabled(); info.read_only = effective_read_only();
    if (info.enabled) { info.actions.push_back(ui::SemanticAction::Focus); if (!info.read_only) info.actions.push_back(ui::SemanticAction::Toggle); }
    return info;
  }
  ui::EventResult input(const ui::InputEvent&,ui::InputContext&) override { ++record_->inputs; return ui::EventResult::Ignored; }
  void focus_changed(bool focused,ui::FocusContext&) override {
    auto record = record_; if (!focused) { auto callback = record->on_blur; if (callback) callback(); return; } ++record->focuses; auto callback = record->on_focus; if (callback) callback();
  }
  ui::EventResult semantic_action(ui::SemanticAction action,ui::InputContext&) override {
    NUI_CHECK(action == ui::SemanticAction::Toggle); auto record = record_; ++record->actions;
    if (std::exchange(record->throw_action,false)) throw std::runtime_error("action begun");
    return ui::EventResult::Handled;
  }
private: std::shared_ptr<Record> record_; std::string name_;
};
class Scope final : public ui::Component {
public:
  Scope(std::shared_ptr<Record> record,std::string key) : record_(std::move(record)),key_(std::move(key)) {}
  void mount(ui::MountContext& context) override { record_->request = context.descendant_action_requester(key_); }
  ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override { return children.empty() ? ui::Size{} : children.front().preferred; }
  void layout_children(ui::Rect bounds,const std::vector<ui::ChildMetrics>&,std::vector<ui::ChildPlacement>& placements) const override { for (auto& placement : placements) placement.bounds = bounds; }
  void paint(ui::PaintContext&) const override {}
private:
  std::optional<ui::detail::DescendantSemanticDecoration> descendant_semantic_decoration() const override {
    return ui::detail::DescendantSemanticDecoration{key_,"Field label","Help\nError"};
  }
  std::shared_ptr<Record> record_; std::string key_;
};
ui::Spec control(std::shared_ptr<Record> record,std::string name = {}) { return {[record,name] { return std::make_unique<Control>(record,name); },{}}; }
ui::Spec scope(std::shared_ptr<Record> record,std::vector<ui::Spec> children,std::string key = {}) { return {[record,key] { return std::make_unique<Scope>(record,key); },std::move(children)}; }
void label_projection_owned_actions_and_weak_lifetime() {
  auto record = std::make_shared<Record>(); std::function<void()> stale;
  {
    ui::UI tree{scope(record,{control(record)})}; test::MockPlatform platform; tree.resize({200,80}); tree.activate(platform);
    const auto info = tree.component_semantics(record->target);
    NUI_CHECK(info && info->name == "Field label" && info->description == "Own\nHelp\nError" && info->role == ui::SemanticRole::Toggle);
    stale = record->request; stale(); NUI_CHECK(record->actions == 0); tree.resize({200,80});
    NUI_CHECK(record->actions == 1 && record->inputs == 0);
    record->throw_action = true; record->request(); bool caught{};
    try { tree.resize({200,80}); } catch (const std::runtime_error&) { caught = true; }
    NUI_CHECK(caught && record->actions == 2); tree.resize({200,80}); NUI_CHECK(record->actions == 2);
  }
  stale(); NUI_CHECK(record->actions == 2);
}
void explicit_names_read_only_and_ambiguous_targets() {
  { auto record = std::make_shared<Record>(); ui::State<bool> read_only{true};
    ui::UI tree{scope(record,{ui::make_spec(ui::ReadOnly{read_only,control(record,"Explicit")})})};
    test::MockPlatform platform; tree.resize({200,80}); tree.activate(platform);
    const auto info = tree.component_semantics(record->target); NUI_CHECK(info && info->name == "Explicit" && info->description == "Own\nHelp\nError");
    record->request(); tree.resize({200,80}); NUI_CHECK(record->actions == 0 && record->inputs == 0);
  }
  for (const auto& key : {std::string{"missing"},std::string{"duplicate"}}) {
    auto owner = std::make_shared<Record>(),a = std::make_shared<Record>(),b = std::make_shared<Record>();
    ui::UI tree{scope(owner,{ui::keyed("duplicate",control(a)),ui::keyed("duplicate",control(b))},key)};
    test::MockPlatform platform; tree.resize({200,80}); tree.activate(platform); owner->request(); tree.resize({200,80});
    NUI_CHECK(a->actions == 0 && b->actions == 0);
  }
}
void focus_callback_cannot_activate_a_replacement_with_the_same_key() {
  auto owner = std::make_shared<Record>(),first = std::make_shared<Record>(),replacement = std::make_shared<Record>();
  ui::State<int> branch{1};
  auto dynamic = ui::Switch{branch}.when(1,ui::keyed("target",control(first))).when(2,ui::keyed("target",control(replacement)));
  ui::UI tree{scope(owner,{ui::make_spec(ui::Row{ui::Button{"First",[]{}},std::move(dynamic)})},"target")};
  test::MockPlatform platform; tree.resize({300,80}); tree.activate(platform);
  first->on_focus = [&] { branch.set(2); tree.resize({301,80}); };
  owner->request(); tree.resize({301,80});
  NUI_CHECK(first->actions == 0 && replacement->actions == 0);
  tree.resize({301,80}); NUI_CHECK(replacement->actions == 0);
}
void deactivation_during_structural_blur_does_not_consume_an_empty_queue() {
  auto owner=std::make_shared<Record>(), first=std::make_shared<Record>(), next=std::make_shared<Record>();
  ui::State<int> branch{1};
  auto dynamic=ui::Switch{branch}.when(1,ui::keyed("target",control(first))).when(2,ui::keyed("target",control(next)));
  ui::UI tree{scope(owner,{ui::make_spec(std::move(dynamic))},"target")};
  test::MockPlatform platform; tree.resize({200,80}); tree.activate(platform);
  bool fired{}; first->on_blur=[&]{ if (!std::exchange(fired,true)) tree.deactivate(platform); };
  owner->request(); branch.set(2); tree.resize({201,80});
  NUI_CHECK(fired && first->actions==0 && next->actions==0);
  tree.activate(platform); tree.resize({201,80}); owner->request(); tree.resize({201,80});
  NUI_CHECK(next->actions==1);
}
void budgeted_actions_retain_a_following_frame() {
  auto owner=std::make_shared<Record>(), target=std::make_shared<Record>();
  ui::UI tree{scope(owner,{control(target)})}; test::MockPlatform platform;
  tree.resize({200,80}); tree.activate(platform);
  for (int i=0; i<1000; ++i) owner->request();
  ui::HeadlessRenderer renderer{{200,80},1}; NUI_CHECK(renderer.render(tree));
  NUI_CHECK(target->actions==1000 || tree.dirty());
  for (int frame=0; frame<100 && tree.dirty(); ++frame) NUI_CHECK(renderer.render(tree));
  NUI_CHECK(target->actions==1000); NUI_CHECK(renderer.render(tree)); NUI_CHECK(target->actions==1000);
}
void suite() { label_projection_owned_actions_and_weak_lifetime(); explicit_names_read_only_and_ambiguous_targets(); focus_callback_cannot_activate_a_replacement_with_the_same_key(); deactivation_during_structural_blur_does_not_consume_an_empty_queue(); budgeted_actions_retain_a_following_frame(); }
}
int main(int argc,char** argv) { if(argc>1 && std::string_view{argv[1]}=="deactivate") return test::run("deactivate",&deactivation_during_structural_blur_does_not_consume_an_empty_queue); if(argc>1 && std::string_view{argv[1]}=="budget") return test::run("budget",&budgeted_actions_retain_a_following_frame); return test::run("widget_descendant_actions",&suite); }
