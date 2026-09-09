#include "test_support.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/dispatcher.hpp>

#include <chrono>
#include <limits>
#include <memory>
#include <vector>

namespace {

using namespace std::chrono_literals;

void suite() {
    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        const auto dispatcher = owner.dispatcher();

        NUI_CHECK(!dispatcher.post({}));
        NUI_CHECK(!dispatcher.schedule_after(1ms, {}).valid());
        NUI_CHECK(!dispatcher.schedule_every(1ms, {}).valid());
        NUI_CHECK(!dispatcher.schedule_after(
                       ui::DispatcherDuration{std::numeric_limits<double>::quiet_NaN()}, [] {})
                       .valid());
        NUI_CHECK(!dispatcher.schedule_after(
                       ui::DispatcherDuration{std::numeric_limits<double>::infinity()}, [] {})
                       .valid());
        NUI_CHECK(!dispatcher.schedule_every(
                       ui::DispatcherDuration{std::numeric_limits<double>::infinity()}, [] {})
                       .valid());
        NUI_CHECK(!dispatcher.schedule_after(
                       ui::DispatcherDuration{std::numeric_limits<double>::max()}, [] {})
                       .valid());
    }

    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        const auto dispatcher = owner.dispatcher();
        std::vector<int> observed;

        auto repeating = dispatcher.schedule_every(
            1ms,
            [local_count = 0, &observed]() mutable { observed.push_back(++local_count); });
        NUI_CHECK(repeating.valid());

        clock->advance(1ms);
        NUI_CHECK(owner.checkpoint() == 1);
        clock->advance(1ms);
        NUI_CHECK(owner.checkpoint() == 1);
        clock->advance(1ms);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK((observed == std::vector<int>{1, 2, 3}));
        NUI_CHECK(dispatcher.cancel(repeating));
    }

    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        const auto dispatcher = owner.dispatcher();
        bool cancel_result = true;
        ui::TimerHandle one_shot;
        one_shot = dispatcher.schedule_after(0s, [&] {
            // One-shot is inactive before the queued callback begins.
            cancel_result = dispatcher.cancel(one_shot);
        });
        NUI_CHECK(one_shot.valid());
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(!cancel_result);
        NUI_CHECK(owner.active_timer_count() == 0);
    }
}

} // namespace

int main() {
    return test::run("dispatcher_edge", suite);
}
