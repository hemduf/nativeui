#include "test_support.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/dispatcher.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

struct ReentrantLifetimeProbe final {
    ui::Dispatcher dispatcher;
    std::atomic<int>* destructions{};

    ~ReentrantLifetimeProbe() {
        // A capture destructor is user lifetime code. Re-entering Dispatcher
        // here must never deadlock because a queue/timer mutex is still held.
        (void)dispatcher.valid();
        destructions->fetch_add(1, std::memory_order_release);
    }
};

template <class Operation>
bool completes_without_deadlock(Operation&& operation, std::string_view stage) {
    std::atomic<bool> completed{false};
    std::atomic<bool> result{false};
    std::thread worker{[&] {
        result.store(operation(), std::memory_order_release);
        completed.store(true, std::memory_order_release);
    }};

    const auto deadline = std::chrono::steady_clock::now() + 500ms;
    while (!completed.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }

    if (!completed.load(std::memory_order_acquire)) {
        std::cerr << "[dispatcher_edge] callback capture destruction deadlocked during "
                  << stage << '\n';
        worker.detach();
        std::_Exit(1);
    }

    worker.join();
    return result.load(std::memory_order_acquire);
}

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

    // CODE_REVIEW.md lifetime regression: rejected task capture destruction is
    // allowed to re-enter the same Dispatcher and must happen after unlock.
    {
        ui::detail::DispatcherOwner owner;
        const auto dispatcher = owner.dispatcher();
        for (std::size_t i = 0; i < ui::kDispatcherMaxPendingTasks; ++i) {
            NUI_CHECK(dispatcher.post([] {}));
        }

        std::atomic<int> destructions{0};
        auto probe = std::make_shared<ReentrantLifetimeProbe>(
            ReentrantLifetimeProbe{dispatcher, &destructions});
        auto rejected = [probe] {};
        probe.reset();

        NUI_CHECK(completes_without_deadlock(
            [&] { return !dispatcher.post(std::move(rejected)); }, "post rejection"));
        NUI_CHECK(destructions.load(std::memory_order_acquire) == 1);
    }

    // Cancelling a timer must detach the user callable under lock but destroy
    // its captures only after the lock has been released.
    {
        ui::detail::DispatcherOwner owner;
        const auto dispatcher = owner.dispatcher();
        std::atomic<int> destructions{0};
        auto probe = std::make_shared<ReentrantLifetimeProbe>(
            ReentrantLifetimeProbe{dispatcher, &destructions});
        const auto timer = dispatcher.schedule_after(1s, [probe] {});
        probe.reset();
        NUI_CHECK(timer.valid());

        NUI_CHECK(completes_without_deadlock(
            [&] { return dispatcher.cancel(timer); }, "timer cancellation"));
        NUI_CHECK(destructions.load(std::memory_order_acquire) == 1);
    }

    // Owner shutdown has the same rule for every discarded queued/timer
    // callable. The probe deliberately re-enters a now-closing Dispatcher.
    {
        ui::detail::DispatcherOwner owner;
        const auto dispatcher = owner.dispatcher();
        std::atomic<int> destructions{0};
        auto probe = std::make_shared<ReentrantLifetimeProbe>(
            ReentrantLifetimeProbe{dispatcher, &destructions});
        const auto timer = dispatcher.schedule_after(1s, [probe] {});
        probe.reset();
        NUI_CHECK(timer.valid());

        NUI_CHECK(completes_without_deadlock(
            [&] {
                owner.shutdown();
                return true;
            },
            "owner shutdown"));
        NUI_CHECK(destructions.load(std::memory_order_acquire) == 1);
        NUI_CHECK(!dispatcher.valid());
    }
}

} // namespace

int main() {
    return test::run("dispatcher_edge", suite);
}
