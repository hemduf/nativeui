#include "test_support.hpp"
#include <nativeui/scroll.hpp>

#include <limits>
#include <memory>

namespace {
void metrics_use_the_existing_clamp_and_notification_transaction() {
    ui::ScrollState state{ui::ScrollAxis::Both};
    state.set_offset({400.0f,300.0f});
    int notifications=0;
    const auto subscription=state.observe([&](ui::Point) { ++notifications; });
    NUI_CHECK(ui::detail::ScrollMetricsAccess::publish(&state,state.lifetime_token(),{100.0f,80.0f},{250.0f,180.0f}));
    NUI_CHECK(state.offset().x==150.0f && state.offset().y==100.0f && notifications==1);
    NUI_CHECK(ui::detail::ScrollMetricsAccess::publish(&state,state.lifetime_token(),{100.0f,80.0f},{250.0f,180.0f}));
    NUI_CHECK(notifications==1);
    const auto invalid=std::numeric_limits<float>::infinity();
    NUI_CHECK(ui::detail::ScrollMetricsAccess::publish(&state,state.lifetime_token(),{invalid,-2.0f},{-1.0f,invalid}));
    NUI_CHECK(state.offset().x==0.0f && state.offset().y==0.0f && notifications==2);
    NUI_CHECK(state.viewport_size().w==0.0f && state.content_size().h==0.0f);
}
void stale_and_retired_owner_tokens_prevent_borrowed_access() {
    auto owner=std::make_unique<ui::ScrollState>();
    auto* borrowed=owner.get(); auto lifetime=owner->lifetime_token();
    owner.reset();
    NUI_CHECK(!ui::detail::ScrollMetricsAccess::publish(borrowed,lifetime,{10.0f,10.0f},{20.0f,20.0f}));
    NUI_CHECK(!ui::detail::ScrollMetricsAccess::publish(nullptr,{}, {},{}));
    owner=std::make_unique<ui::ScrollState>(); borrowed=owner.get(); lifetime=owner->lifetime_token();
    owner->set_offset({0.0f,100.0f});
    int notifications=0;
    const auto subscription=owner->observe([&](ui::Point) { ++notifications; owner.reset(); });
    NUI_CHECK(!ui::detail::ScrollMetricsAccess::publish(borrowed,lifetime,{10.0f,10.0f},{20.0f,20.0f}));
    NUI_CHECK(!lifetime.active() && notifications==1);
}
void suite() { metrics_use_the_existing_clamp_and_notification_transaction(); stale_and_retired_owner_tokens_prevent_borrowed_access(); }
}
int main() { return test::run("scroll_metrics_access",&suite); }
