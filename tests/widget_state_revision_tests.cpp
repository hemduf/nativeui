#include "test_support.hpp"
#include <nativeui/state.hpp>
namespace {
void suite(){
 auto owner=std::make_unique<ui::State<int>>(0);auto value=owner->binding();NUI_CHECK(owner->revision()==0 && value.revision()==0);
 value.set(0);NUI_CHECK(value.revision()==0);
 int observed{};bool fail{};auto subscription=value.observe([&](int v){++observed;NUI_CHECK(value.revision()==static_cast<std::uint64_t>(v));if(v==1)value.set(2);if(fail){value.set(4);throw std::runtime_error("observer");}});
 value.set(1);NUI_CHECK(value.get()==2 && value.revision()==2 && observed==2);
 fail=true;bool caught{};try{value.set(3);}catch(const std::runtime_error&){caught=true;}NUI_CHECK(caught && value.get()==3 && value.revision()==3 && observed==3);
 subscription.reset();value.set(5);NUI_CHECK(value.get()==5 && value.revision()==4);value.set(5);NUI_CHECK(value.revision()==4);
 owner.reset();NUI_CHECK(!value.valid() && value.revision()==4 && value.get()==5);value.set(6);NUI_CHECK(value.revision()==4 && value.get()==5);
}
}
int main(){return test::run("widget_state_revision",&suite);}
