#include "test_support.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/dispatcher.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

class CountingWakeBackend final : public ui::detail::DispatcherWakeBackend {
public:
    void request_wake() noexcept override { ++wake_count; }

    std::atomic<std::size_t> wake_count{};
};

void suite() {
    {
        auto backend = std::make_shared<CountingWakeBackend>();
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{backend, clock};
        const auto dispatcher = owner.dispatcher();

        std::vector<int> order;
        const auto ui_thread = std::this_thread::get_id();
        std::thread::id callback_thread{};
        std::thread worker{[dispatcher, &order, &callback_thread] {
            NUI_CHECK(dispatcher.post([&] {
                callback_thread = std::this_thread::get_id();
                order.push_back(1);
            }));
            NUI_CHECK(dispatcher.post([&] { order.push_back(2); }));
            NUI_CHECK(dispatcher.post([&] { order.push_back(3); }));
        }};
        worker.join();

        NUI_CHECK(backend->wake_count.load() == 1);
        NUI_CHECK(owner.checkpoint() == 3);
        NUI_CHECK((order == std::vector<int>{1, 2, 3}));
        NUI_CHECK(callback_thread == ui_thread);
        NUI_CHECK(owner.pending_task_count() == 0);
    }

    {
        ui::detail::DispatcherOwner owner;
        const auto dispatcher = owner.dispatcher();
        for (std::size_t i = 0; i < ui::kDispatcherMaxPendingTasks; ++i) {
            NUI_CHECK(dispatcher.post([] {}));
        }
        NUI_CHECK(owner.pending_task_count() == ui::kDispatcherMaxPendingTasks);
        NUI_CHECK(!dispatcher.post([] {}));
        NUI_CHECK(owner.checkpoint() == ui::kDispatcherMaxTasksPerCheckpoint);
        NUI_CHECK(owner.pending_task_count() ==
                  ui::kDispatcherMaxPendingTasks - ui::kDispatcherMaxTasksPerCheckpoint);
        NUI_CHECK(dispatcher.post([] {}));
    }

    {
        auto backend = std::make_shared<CountingWakeBackend>();
        ui::detail::DispatcherOwner owner{backend};
        const auto dispatcher = owner.dispatcher();
        int count = 0;
        std::shared_ptr<ui::Dispatcher::Callback> self =
            std::make_shared<ui::Dispatcher::Callback>();
        *self = [dispatcher, self, &count] {
            ++count;
            if (count < 3) NUI_CHECK(dispatcher.post(*self));
        };
        NUI_CHECK(dispatcher.post(*self));
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(count == 1);
        NUI_CHECK(owner.pending_task_count() == 1);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(count == 2);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(count == 3);
    }

    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        const auto dispatcher = owner.dispatcher();
        std::vector<ui::TimerHandle> handles;
        handles.reserve(ui::kDispatcherMaxActiveTimers);
        for (std::size_t i = 0; i < ui::kDispatcherMaxActiveTimers; ++i) {
            const auto handle = dispatcher.schedule_after(1s, [] {});
            NUI_CHECK(handle.valid());
            handles.push_back(handle);
        }
        NUI_CHECK(owner.active_timer_count() == ui::kDispatcherMaxActiveTimers);
        NUI_CHECK(!dispatcher.schedule_after(1s, [] {}).valid());
        NUI_CHECK(dispatcher.cancel(handles.back()));
        NUI_CHECK(owner.active_timer_count() == ui::kDispatcherMaxActiveTimers - 1);
        NUI_CHECK(dispatcher.schedule_after(1s, [] {}).valid());
    }

    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        const auto dispatcher = owner.dispatcher();
        int fired = 0;

        const auto zero = dispatcher.schedule_after(0s, [&] { ++fired; });
        NUI_CHECK(zero.valid());
        NUI_CHECK(fired == 0);
        NUI_CHECK(owner.active_timer_count() == 1);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(fired == 1);
        NUI_CHECK(owner.active_timer_count() == 0);
        NUI_CHECK(!dispatcher.cancel(zero));

        const auto cancelled = dispatcher.schedule_after(10ms, [&] { ++fired; });
        NUI_CHECK(cancelled.valid());
        NUI_CHECK(dispatcher.cancel(cancelled));
        clock->advance(10ms);
        NUI_CHECK(owner.checkpoint() == 0);
        NUI_CHECK(fired == 1);

        NUI_CHECK(!dispatcher.schedule_after(-1ms, [] {}).valid());
        NUI_CHECK(!dispatcher.schedule_every(0ms, [] {}).valid());
        NUI_CHECK(!dispatcher.schedule_every(-1ms, [] {}).valid());
    }

    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        const auto dispatcher = owner.dispatcher();
        int repeat_count = 0;
        ui::TimerHandle repeating;
        repeating = dispatcher.schedule_every(10ms, [&] {
            ++repeat_count;
            if (repeat_count == 2) NUI_CHECK(dispatcher.cancel(repeating));
        });
        NUI_CHECK(repeating.valid());

        clock->advance(100ms);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(repeat_count == 1);
        NUI_CHECK(owner.active_timer_count() == 1);

        // Fixed-delay: the 90 ms of missed periods are never replayed.
        NUI_CHECK(owner.checkpoint() == 0);
        NUI_CHECK(repeat_count == 1);
        clock->advance(10ms);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(repeat_count == 2);
        NUI_CHECK(owner.active_timer_count() == 0);
    }

    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        const auto dispatcher = owner.dispatcher();
        std::vector<int> order;

        NUI_CHECK(dispatcher.post([&] { order.push_back(0); }));
        NUI_CHECK(dispatcher.schedule_after(20ms, [&] { order.push_back(2); }).valid());
        NUI_CHECK(dispatcher.schedule_after(10ms, [&] { order.push_back(1); }).valid());
        NUI_CHECK(dispatcher.schedule_after(10ms, [&] { order.push_back(3); }).valid());
        clock->advance(20ms);
        NUI_CHECK(owner.checkpoint() == 4);
        // Already queued tasks stay ahead; equal-due timers use creation order.
        NUI_CHECK((order == std::vector<int>{0, 1, 3, 2}));
    }

    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        const auto dispatcher = owner.dispatcher();
        int timer_fired = 0;
        for (std::size_t i = 0; i < ui::kDispatcherMaxPendingTasks; ++i) {
            NUI_CHECK(dispatcher.post([] {}));
        }
        NUI_CHECK(dispatcher.schedule_after(0s, [&] { ++timer_fired; }).valid());
        NUI_CHECK(owner.checkpoint() == ui::kDispatcherMaxTasksPerCheckpoint);
        NUI_CHECK(owner.active_timer_count() == 1);
        NUI_CHECK(timer_fired == 0);

        while (owner.pending_task_count() > ui::kDispatcherMaxTasksPerCheckpoint) {
            owner.checkpoint();
        }
        owner.checkpoint();
        NUI_CHECK(timer_fired == 1);
        NUI_CHECK(owner.active_timer_count() == 0);
    }

    {
        auto backend = std::make_shared<CountingWakeBackend>();
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        auto owner_a = std::make_unique<ui::detail::DispatcherOwner>(backend, clock);
        ui::detail::DispatcherOwner owner_b{backend, clock};
        const auto a = owner_a->dispatcher();
        const auto b = owner_b.dispatcher();
        int a_count = 0;
        int b_count = 0;

        const auto a_timer = a.schedule_after(1ms, [&] { ++a_count; });
        const auto b_timer = b.schedule_after(1ms, [&] { ++b_count; });
        NUI_CHECK(a_timer.valid() && b_timer.valid());
        NUI_CHECK(!a.cancel(b_timer));
        NUI_CHECK(!b.cancel(a_timer));

        owner_a.reset();
        NUI_CHECK(!a.post([&] { ++a_count; }));
        clock->advance(1ms);
        NUI_CHECK(owner_b.checkpoint() == 1);
        NUI_CHECK(a_count == 0);
        NUI_CHECK(b_count == 1);
        NUI_CHECK(b.post([&] { ++b_count; }));
        NUI_CHECK(owner_b.checkpoint() == 1);
        NUI_CHECK(b_count == 2);
    }

    {
        auto backend = std::make_shared<CountingWakeBackend>();
        ui::detail::DispatcherOwner owner{backend};
        const auto dispatcher = owner.dispatcher();
        backend.reset();
        int count = 0;
        NUI_CHECK(dispatcher.post([&] { ++count; }));
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(count == 1);
    }

    {
        auto owner = std::make_unique<ui::detail::DispatcherOwner>();
        const auto dispatcher = owner->dispatcher();
        int first = 0;
        int second = 0;
        NUI_CHECK(dispatcher.post([&] {
            ++first;
            owner.reset();
        }));
        NUI_CHECK(dispatcher.post([&] { ++second; }));

        // checkpoint() must survive its logical owner being destroyed by the
        // first callback and must not begin the remaining snapshot callback.
        auto* raw = owner.get();
        NUI_CHECK(raw->checkpoint() == 1);
        NUI_CHECK(first == 1);
        NUI_CHECK(second == 0);
        NUI_CHECK(!dispatcher.post([] {}));
    }

    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        const auto dispatcher = owner.dispatcher();
        NUI_CHECK(!owner.next_delay().has_value());
        NUI_CHECK(dispatcher.schedule_after(25ms, [] {}).valid());
        NUI_CHECK_NEAR(owner.next_delay()->count(), 0.025, 0.000001);
        clock->advance(20ms);
        NUI_CHECK_NEAR(owner.next_delay()->count(), 0.005, 0.000001);
        NUI_CHECK(dispatcher.post([] {}));
        NUI_CHECK_NEAR(owner.next_delay()->count(), 0.0, 0.000001);
    }
}

} // namespace

int main() {
    return test::run("dispatcher", suite);
}
