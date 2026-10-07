#include "test_support.hpp"
namespace {
struct Record { std::vector<ui::FocusChangeReason> reasons; std::vector<bool> commits; bool fail{}; };
class Probe final : public ui::Component {
public:
 explicit Probe(std::shared_ptr<Record> r):r_(std::move(r)){}
 bool focusable() const noexcept override {return true;}
 ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {return {40,20};}
 void paint(ui::PaintContext&) const override{}
 void focus_changed(bool focused,ui::FocusContext& context) override {
  if(focused)return;
  r_->reasons.push_back(context.reason());r_->commits.push_back(context.allows_edit_commit());
  if(std::exchange(r_->fail,false))throw std::runtime_error("blur once");
 }
private:std::shared_ptr<Record> r_;
};
ui::Spec spec(std::shared_ptr<Record> r){return {[r]{return std::make_unique<Probe>(r);},{}};}
void suite(){
 test::MockPlatform platform;
 {auto r=std::make_shared<Record>();ui::UI tree{ui::Row{spec(r),ui::Button{"Next",[]{}}}};tree.resize({100,40});tree.activate(platform);tree.dispatch(test::key(ui::Key::Tab),platform);NUI_CHECK(r->reasons==std::vector{ui::FocusChangeReason::Ordinary});NUI_CHECK(r->commits==std::vector{true});tree.deactivate(platform);}
 {auto r=std::make_shared<Record>();ui::State<bool> present{true};ui::UI tree{ui::If{present,spec(r)}};tree.resize({100,40});tree.activate(platform);r->fail=true;present.set(false);bool threw{};try{tree.resize({100,40});}catch(const std::runtime_error&){threw=true;}NUI_CHECK(threw);tree.resize({100,40});NUI_CHECK(r->reasons==std::vector{ui::FocusChangeReason::Removed});NUI_CHECK(r->commits==std::vector{false});present.set(true);tree.resize({100,40});tree.deactivate(platform);}
 {auto r=std::make_shared<Record>();ui::State<bool> enabled{true};ui::UI tree{ui::Enabled{enabled,spec(r)}};tree.resize({100,40});tree.activate(platform);enabled.set(false);NUI_CHECK(r->reasons==std::vector{ui::FocusChangeReason::Unavailable});NUI_CHECK(r->commits==std::vector{false});tree.deactivate(platform);}
 {auto r=std::make_shared<Record>();ui::UI tree{spec(r)};tree.resize({100,40});tree.activate(platform);tree.deactivate(platform);NUI_CHECK(r->reasons==std::vector{ui::FocusChangeReason::Teardown});NUI_CHECK(r->commits==std::vector{false});}
 ui::FocusContext old{{},platform,[]{},[]{}};NUI_CHECK(old.reason()==ui::FocusChangeReason::Ordinary && old.allows_edit_commit());
}
}
int main(){return test::run("widget_focus_reason",&suite);}
