#include "test_support.hpp"

#include <nativeui/animation.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/dispatcher.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;

struct ReentrantLifetimeProbe final {
    ReentrantLifetimeProbe(ui::Dispatcher target, std::atomic<int>* count)
        : dispatcher(std::move(target)), destructions(count) {}

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

ui::AnimationInvalidationTarget noop_animation_target() {
    return ui::AnimationInvalidationTarget{[] {}, [] {}};
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

    // T128: a callback that begins and throws is consumed, but the unstarted
    // snapshot suffix remains accepted work. Reentrant posts stay behind that
    // suffix in the original FIFO/sequence order.
    {
        ui::detail::DispatcherOwner owner;
        const auto dispatcher = owner.dispatcher();
        std::vector<int> observed;
        bool posted_d = false;

        NUI_CHECK(dispatcher.post([&] {
            observed.push_back(1);
            posted_d = dispatcher.post([&] { observed.push_back(4); });
            throw std::runtime_error{"expected dispatcher callback failure"};
        }));
        NUI_CHECK(dispatcher.post([&] { observed.push_back(2); }));
        NUI_CHECK(dispatcher.post([&] { observed.push_back(3); }));

        bool threw = false;
        try {
            (void)owner.checkpoint();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(posted_d);
        NUI_CHECK((observed == std::vector<int>{1}));
        NUI_CHECK(owner.pending_task_count() == 3);
        NUI_CHECK(owner.checkpoint() == 3);
        NUI_CHECK((observed == std::vector<int>{1, 2, 3, 4}));
        NUI_CHECK(owner.pending_task_count() == 0);
    }

    // T128: only the suffix after the throwing callback is restored. Callbacks
    // that completed before the throw and the callback that threw are not retried.
    {
        ui::detail::DispatcherOwner owner;
        const auto dispatcher = owner.dispatcher();
        std::vector<int> observed;

        NUI_CHECK(dispatcher.post([&] { observed.push_back(1); }));
        NUI_CHECK(dispatcher.post([&] {
            observed.push_back(2);
            throw std::runtime_error{"middle dispatcher callback failure"};
        }));
        NUI_CHECK(dispatcher.post([&] { observed.push_back(3); }));

        bool threw = false;
        try {
            (void)owner.checkpoint();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK((observed == std::vector<int>{1, 2}));
        NUI_CHECK(owner.pending_task_count() == 1);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK((observed == std::vector<int>{1, 2, 3}));
    }

    // T128: shutdown remains terminal even when initiated by a throwing callback.
    // The unstarted suffix must be discarded instead of resurrected into closing work.
    {
        ui::detail::DispatcherOwner owner;
        const auto dispatcher = owner.dispatcher();
        std::vector<int> observed;

        NUI_CHECK(dispatcher.post([&] {
            observed.push_back(1);
            owner.shutdown();
            throw std::runtime_error{"shutdown during dispatcher callback"};
        }));
        NUI_CHECK(dispatcher.post([&] { observed.push_back(2); }));

        bool threw = false;
        try {
            (void)owner.checkpoint();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK((observed == std::vector<int>{1}));
        NUI_CHECK(!dispatcher.valid());
        NUI_CHECK(owner.pending_task_count() == 0);
        NUI_CHECK(owner.checkpoint() == 0);
    }

    // T128: due one-shot work is a normal queued task after timer extraction and
    // therefore survives an earlier throwing task exactly once.
    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        const auto dispatcher = owner.dispatcher();
        int timer_calls = 0;

        NUI_CHECK(dispatcher.post([] {
            throw std::runtime_error{"preceding dispatcher task failure"};
        }));
        const auto timer = dispatcher.schedule_after(0s, [&] { ++timer_calls; });
        NUI_CHECK(timer.valid());

        bool threw = false;
        try {
            (void)owner.checkpoint();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(timer_calls == 0);
        NUI_CHECK(owner.active_timer_count() == 0);
        NUI_CHECK(owner.pending_task_count() == 1);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(timer_calls == 1);
        NUI_CHECK(owner.checkpoint() == 0);
    }

    // T128: repeating timers remain fixed-delay and do not duplicate/catch up
    // when one already-queued firing is restored after a neighboring throw.
    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        const auto dispatcher = owner.dispatcher();
        int timer_calls = 0;

        NUI_CHECK(dispatcher.post([] {
            throw std::runtime_error{"preceding repeating timer failure"};
        }));
        const auto timer = dispatcher.schedule_every(1ms, [&] { ++timer_calls; });
        NUI_CHECK(timer.valid());
        clock->advance(1ms);

        bool threw = false;
        try {
            (void)owner.checkpoint();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(timer_calls == 0);
        NUI_CHECK(owner.pending_task_count() == 1);
        NUI_CHECK(owner.active_timer_count() == 1);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(timer_calls == 1);
        NUI_CHECK(owner.checkpoint() == 0);
        clock->advance(1ms);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(timer_calls == 2);
        NUI_CHECK(dispatcher.cancel(timer));
    }

    // T128: a timed tween write failure terminalizes only the failing animation,
    // rearms the shared scheduler for siblings, and never retries the begun write.
    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        ui::AnimationContext animations{owner.dispatcher()};
        int failing_writes = 0;
        float sibling_value = 0.0f;

        const auto failing = animations.start_tween(
            0.0f, 1.0f, 1s, ui::Easing::Linear,
            ui::AnimationInvalidation::Paint, noop_animation_target(),
            [&](float) {
                ++failing_writes;
                throw std::runtime_error{"animation write failure"};
            });
        const auto sibling = animations.start_tween(
            0.0f, 1.0f, 1s, ui::Easing::Linear,
            ui::AnimationInvalidation::Paint, noop_animation_target(),
            [&](float value) { sibling_value = value; });
        NUI_CHECK(failing.valid());
        NUI_CHECK(sibling.valid());
        NUI_CHECK(owner.active_timer_count() == 1);

        clock->advance(16ms);
        bool threw = false;
        try {
            (void)owner.checkpoint();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(failing_writes == 1);
        NUI_CHECK(animations.active_count() == 1);
        NUI_CHECK(owner.active_timer_count() == 1);

        clock->advance(16ms);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(failing_writes == 1);
        NUI_CHECK(sibling_value > 0.0f);
        NUI_CHECK(animations.cancel(sibling));
    }

    // T128: the spring path has the same rule when retained invalidation throws.
    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        ui::AnimationContext animations{owner.dispatcher()};
        int invalidations = 0;
        float sibling_value = 0.0f;
        const ui::AnimationInvalidationTarget throwing_target{
            [&] {
                ++invalidations;
                throw std::runtime_error{"animation invalidation failure"};
            },
            [] {}};

        const auto failing = animations.start_spring(
            0.0f, 1.0f, {}, ui::AnimationInvalidation::Paint,
            throwing_target, [](float) {});
        const auto sibling = animations.start_tween(
            0.0f, 1.0f, 1s, ui::Easing::Linear,
            ui::AnimationInvalidation::Paint, noop_animation_target(),
            [&](float value) { sibling_value = value; });
        NUI_CHECK(failing.valid());
        NUI_CHECK(sibling.valid());

        clock->advance(16ms);
        bool threw = false;
        try {
            (void)owner.checkpoint();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(invalidations == 1);
        NUI_CHECK(animations.active_count() == 1);
        NUI_CHECK(owner.active_timer_count() == 1);

        clock->advance(16ms);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(invalidations == 1);
        NUI_CHECK(sibling_value > 0.0f);
        NUI_CHECK(animations.cancel(sibling));
    }

    // T128: completion is committed (entry erased) before application code and
    // remains at-most-once even when that completion throws.
    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        ui::AnimationContext animations{owner.dispatcher()};
        int completions = 0;
        float sibling_value = 0.0f;

        const auto completing = animations.start_tween(
            0.0f, 1.0f, 16ms, ui::Easing::Linear,
            ui::AnimationInvalidation::Paint, noop_animation_target(), [](float) {},
            [&] {
                ++completions;
                throw std::runtime_error{"animation completion failure"};
            });
        const auto sibling = animations.start_tween(
            0.0f, 1.0f, 1s, ui::Easing::Linear,
            ui::AnimationInvalidation::Paint, noop_animation_target(),
            [&](float value) { sibling_value = value; });
        NUI_CHECK(completing.valid());
        NUI_CHECK(sibling.valid());

        clock->advance(16ms);
        bool threw = false;
        try {
            (void)owner.checkpoint();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(completions == 1);
        NUI_CHECK(animations.active_count() == 1);
        NUI_CHECK(owner.active_timer_count() == 1);

        clock->advance(16ms);
        NUI_CHECK(owner.checkpoint() == 1);
        NUI_CHECK(completions == 1);
        NUI_CHECK(sibling_value > 0.0f);
        NUI_CHECK(animations.cancel(sibling));
    }

    // T128: reduced-motion bulk completion is transactional on failure. The
    // deterministic terminal policy is to cancel all remaining entries and wake.
    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        ui::AnimationContext animations{owner.dispatcher()};
        int failing_writes = 0;
        int sibling_writes = 0;

        const auto failing = animations.start_tween(
            0.0f, 1.0f, 1s, ui::Easing::Linear,
            ui::AnimationInvalidation::Paint, noop_animation_target(),
            [&](float) {
                ++failing_writes;
                throw std::runtime_error{"reduced-motion write failure"};
            });
        const auto sibling = animations.start_spring(
            0.0f, 1.0f, {}, ui::AnimationInvalidation::Paint,
            noop_animation_target(), [&](float) { ++sibling_writes; });
        NUI_CHECK(failing.valid());
        NUI_CHECK(sibling.valid());
        NUI_CHECK(owner.active_timer_count() == 1);

        bool threw = false;
        try {
            animations.set_reduced_motion(true);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(animations.reduced_motion());
        NUI_CHECK(failing_writes == 1);
        NUI_CHECK(sibling_writes == 0);
        NUI_CHECK(animations.active_count() == 0);
        NUI_CHECK(owner.active_timer_count() == 0);
        clock->advance(1s);
        NUI_CHECK(owner.checkpoint() == 0);
    }

    // T128: immediate paths have no hidden retry state. A zero-duration write
    // failure and reduced-motion invalidation failure both leave the context idle.
    {
        ui::detail::DispatcherOwner owner;
        ui::AnimationContext animations{owner.dispatcher()};
        int zero_writes = 0;
        bool zero_threw = false;
        try {
            (void)animations.start_tween(
                0.0f, 1.0f, 0s, ui::Easing::Linear,
                ui::AnimationInvalidation::Paint, noop_animation_target(),
                [&](float) {
                    ++zero_writes;
                    throw std::runtime_error{"zero-duration animation failure"};
                });
        } catch (const std::runtime_error&) {
            zero_threw = true;
        }
        NUI_CHECK(zero_threw);
        NUI_CHECK(zero_writes == 1);
        NUI_CHECK(animations.active_count() == 0);
        NUI_CHECK(owner.active_timer_count() == 0);

        animations.set_reduced_motion(true);
        int reduced_writes = 0;
        int reduced_invalidations = 0;
        int reduced_completions = 0;
        const ui::AnimationInvalidationTarget throwing_target{
            [&] {
                ++reduced_invalidations;
                throw std::runtime_error{"reduced-motion invalidation failure"};
            },
            [] {}};
        bool reduced_threw = false;
        try {
            (void)animations.start_spring(
                0.0f, 1.0f, {}, ui::AnimationInvalidation::Paint,
                throwing_target, [&](float) { ++reduced_writes; },
                [&] { ++reduced_completions; });
        } catch (const std::runtime_error&) {
            reduced_threw = true;
        }
        NUI_CHECK(reduced_threw);
        NUI_CHECK(reduced_writes == 1);
        NUI_CHECK(reduced_invalidations == 1);
        NUI_CHECK(reduced_completions == 0);
        NUI_CHECK(animations.active_count() == 0);
        NUI_CHECK(owner.active_timer_count() == 0);
    }

    // CODE_REVIEW.md multi-instance rule: an exception in one context cannot
    // poison the independent scheduler owned by another instance.
    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner_a{{}, clock};
        ui::detail::DispatcherOwner owner_b{{}, clock};
        ui::AnimationContext a{owner_a.dispatcher()};
        ui::AnimationContext b{owner_b.dispatcher()};
        float b_value = 0.0f;

        const auto failing = a.start_tween(
            0.0f, 1.0f, 1s, ui::Easing::Linear,
            ui::AnimationInvalidation::Paint, noop_animation_target(),
            [](float) { throw std::runtime_error{"instance A failure"}; });
        const auto survivor = b.start_tween(
            0.0f, 1.0f, 1s, ui::Easing::Linear,
            ui::AnimationInvalidation::Paint, noop_animation_target(),
            [&](float value) { b_value = value; });
        NUI_CHECK(failing.valid());
        NUI_CHECK(survivor.valid());

        clock->advance(16ms);
        bool threw = false;
        try {
            (void)owner_a.checkpoint();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(a.active_count() == 0);
        NUI_CHECK(owner_a.active_timer_count() == 0);
        NUI_CHECK(owner_b.checkpoint() == 1);
        NUI_CHECK(b_value > 0.0f);
        NUI_CHECK(b.cancel(survivor));
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
        auto probe = std::make_shared<ReentrantLifetimeProbe>(dispatcher, &destructions);
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
        auto probe = std::make_shared<ReentrantLifetimeProbe>(dispatcher, &destructions);
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
        auto probe = std::make_shared<ReentrantLifetimeProbe>(dispatcher, &destructions);
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
