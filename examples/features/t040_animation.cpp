#include "example_support.hpp"

#include <nativeui/animation.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace std::chrono_literals;

bool near(float left, float right, float epsilon = 0.0001f) {
    return std::fabs(left - right) <= epsilon;
}

float expected_easing(ui::Easing easing, float t) {
    switch (easing) {
    case ui::Easing::Linear:
        return t;
    case ui::Easing::EaseIn:
        return t * t * t;
    case ui::Easing::EaseOut: {
        const float u = 1.0f - t;
        return 1.0f - u * u * u;
    }
    case ui::Easing::EaseInOut:
        if (t < 0.5f) return 4.0f * t * t * t;
        {
            const float u = -2.0f * t + 2.0f;
            return 1.0f - (u * u * u) * 0.5f;
        }
    }
    return t;
}

int verify_easing_vectors() {
    constexpr float samples[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    constexpr ui::Easing easings[] = {
        ui::Easing::Linear,
        ui::Easing::EaseIn,
        ui::Easing::EaseOut,
        ui::Easing::EaseInOut,
    };

    for (const auto easing : easings) {
        for (const float t : samples) {
            auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
            ui::detail::DispatcherOwner owner{{}, clock};
            ui::AnimationContext animations{owner.dispatcher()};
            float value = 0.0f;
            int completion_count = 0;
            const auto handle = animations.start_tween(
                0.0f,
                1.0f,
                1s,
                easing,
                ui::AnimationInvalidation::Paint,
                [&](float next) { value = next; },
                {},
                [&] { ++completion_count; });
            if (!handle.valid()) return example::fail("non-zero tween did not become active");

            if (t > 0.0f) {
                clock->advance(ui::DispatcherDuration{t});
                if (owner.checkpoint() != 1) {
                    return example::fail("tween checkpoint did not fire exactly once");
                }
            }
            if (!near(value, expected_easing(easing, t))) {
                return example::fail("easing sample differs from exact cubic formula");
            }
            if ((t == 1.0f) != (completion_count == 1)) {
                return example::fail("tween completion count is not exact");
            }
            if (t == 1.0f && value != 1.0f) {
                return example::fail("completed tween did not write exact target");
            }
        }
    }
    return 0;
}

int verify_validation_and_zero_duration() {
    auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
    ui::detail::DispatcherOwner owner{{}, clock};
    ui::AnimationContext animations{owner.dispatcher()};

    float value = 0.0f;
    int invalidations = 0;
    int completions = 0;
    auto zero = animations.start_tween(
        0.0f,
        0.75f,
        0s,
        ui::Easing::EaseInOut,
        ui::AnimationInvalidation::Layout,
        [&](float next) { value = next; },
        [&](ui::AnimationInvalidation kind) {
            if (kind == ui::AnimationInvalidation::Layout) ++invalidations;
        },
        [&] { ++completions; });
    if (zero.valid() || value != 0.75f || invalidations != 1 || completions != 1 ||
        owner.active_timer_count() != 0) {
        return example::fail("zero-duration tween contract failed");
    }

    const auto nan = std::numeric_limits<float>::quiet_NaN();
    if (animations.start_tween(
            nan, 1.0f, 1s, ui::Easing::Linear, ui::AnimationInvalidation::Paint,
            [](float) {}).valid() ||
        animations.start_tween(
            0.0f, 1.0f, -1ms, ui::Easing::Linear, ui::AnimationInvalidation::Paint,
            [](float) {}).valid() ||
        animations.start_tween(
            0.0f, 1.0f, 1s, static_cast<ui::Easing>(99),
            ui::AnimationInvalidation::Paint, [](float) {}).valid()) {
        return example::fail("invalid tween configuration was accepted");
    }

    ui::SpringOptions invalid_spring;
    invalid_spring.damping = -1.0f;
    if (animations.start_spring(
            0.0f, 1.0f, invalid_spring, ui::AnimationInvalidation::Paint,
            [](float) {}).valid()) {
        return example::fail("invalid spring configuration was accepted");
    }
    invalid_spring = {};
    invalid_spring.max_dt = 0s;
    if (animations.start_spring(
            0.0f, 1.0f, invalid_spring, ui::AnimationInvalidation::Paint,
            [](float) {}).valid()) {
        return example::fail("zero spring max_dt was accepted");
    }
    return 0;
}

int verify_spring_solver() {
    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        ui::AnimationContext animations{owner.dispatcher()};
        ui::SpringOptions options;
        options.stiffness = 100.0f;
        options.damping = 10.0f;
        options.initial_velocity = 0.0f;
        options.max_dt = 100ms;
        float value = 0.0f;
        const auto handle = animations.start_spring(
            value, 1.0f, options, ui::AnimationInvalidation::Paint,
            [&](float next) { value = next; });
        if (!handle.valid()) return example::fail("spring did not start");
        clock->advance(16ms);
        if (owner.checkpoint() != 1) return example::fail("spring wake did not execute");

        // Exact semi-implicit order: a=100, v=1.6, x=0.0256.
        if (!near(value, 0.0256f, 0.00001f)) {
            return example::fail("spring semi-implicit update order differs");
        }
    }

    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        ui::AnimationContext animations{owner.dispatcher()};
        ui::SpringOptions options;
        options.stiffness = 10.0f;
        options.damping = 0.0f;
        options.max_dt = 30ms;
        float value = 0.0f;
        const auto handle = animations.start_spring(
            value, 1.0f, options, ui::AnimationInvalidation::Paint,
            [&](float next) { value = next; });
        if (!handle.valid()) return example::fail("long-stall spring did not start");
        clock->advance(1s);
        if (owner.checkpoint() != 1) return example::fail("long-stall spring wake missing");
        if (!near(value, 0.009f, 0.00001f)) {
            return example::fail("spring performed catch-up instead of one max_dt-clamped step");
        }
    }

    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        ui::AnimationContext animations{owner.dispatcher()};
        ui::SpringOptions options;
        options.stiffness = 0.0f;
        options.damping = 0.0f;
        options.distance_epsilon = 0.01f;
        options.velocity_epsilon = 0.01f;
        float value = 0.995f;
        int completions = 0;
        const auto handle = animations.start_spring(
            value, 1.0f, options, ui::AnimationInvalidation::Paint,
            [&](float next) { value = next; }, {}, [&] { ++completions; });
        if (!handle.valid()) return example::fail("resting spring did not start");
        clock->advance(16ms);
        if (owner.checkpoint() != 1 || value != 1.0f || completions != 1 ||
            animations.active_count() != 0) {
            return example::fail("spring rest snap was not exact");
        }
    }

    {
        auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
        ui::detail::DispatcherOwner owner{{}, clock};
        ui::AnimationContext animations{owner.dispatcher()};
        ui::SpringOptions options;
        options.stiffness = 0.0f;
        options.damping = 0.0f;
        float value = 0.0f;
        const auto handle = animations.start_spring(
            value, 1.0f, options, ui::AnimationInvalidation::Paint,
            [&](float next) { value = next; });
        clock->advance(16ms);
        if (owner.checkpoint() != 1 || value != 0.0f || animations.active_count() != 1) {
            return example::fail("zero-stiffness spring snapped without satisfying rest condition");
        }
        if (!animations.cancel(handle)) return example::fail("zero-stiffness spring cancel failed");
    }
    return 0;
}

int verify_scheduler_cancel_and_isolation() {
    auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
    ui::detail::DispatcherOwner owner_a{{}, clock};
    ui::detail::DispatcherOwner owner_b{{}, clock};
    ui::AnimationContext a{owner_a.dispatcher()};
    ui::AnimationContext b{owner_b.dispatcher()};
    float first = 0.0f;
    float second = 0.0f;
    int first_completion = 0;

    auto h1 = a.start_tween(
        0.0f, 1.0f, 1s, ui::Easing::Linear, ui::AnimationInvalidation::Paint,
        [&](float next) { first = next; }, {}, [&] { ++first_completion; });
    auto h2 = a.start_tween(
        0.0f, 2.0f, 1s, ui::Easing::Linear, ui::AnimationInvalidation::Paint,
        [&](float next) { second = next; });
    if (!h1.valid() || !h2.valid() || owner_a.active_timer_count() != 1) {
        return example::fail("multiple animations did not coalesce to one timer");
    }
    if (b.cancel(h1)) return example::fail("cross-context animation cancel succeeded");
    if (!a.cancel(h1) || first_completion != 0 || owner_a.active_timer_count() != 1) {
        return example::fail("animation cancellation contract failed");
    }
    if (!a.cancel(h2) || owner_a.active_timer_count() != 0 || a.active_count() != 0) {
        return example::fail("last animation did not return scheduler to idle");
    }

    ui::AnimationHandle self_cancel;
    int self_completion = 0;
    self_cancel = a.start_tween(
        0.0f, 1.0f, 1s, ui::Easing::Linear, ui::AnimationInvalidation::Paint,
        [&](float next) {
            first = next;
            (void)a.cancel(self_cancel);
        },
        {},
        [&] { ++self_completion; });
    clock->advance(16ms);
    if (owner_a.checkpoint() != 1 || self_completion != 0 || a.active_count() != 0 ||
        owner_a.active_timer_count() != 0) {
        return example::fail("reentrant cancel did not suppress completion/future wake");
    }

    {
        auto owned = std::make_unique<ui::AnimationContext>(owner_b.dispatcher());
        const auto handle = owned->start_tween(
            0.0f, 1.0f, 1s, ui::Easing::Linear, ui::AnimationInvalidation::Paint,
            [](float) {});
        if (!handle.valid() || owner_b.active_timer_count() != 1) {
            return example::fail("owned animation did not arm");
        }
        owned.reset();
        if (owner_b.active_timer_count() != 0) {
            return example::fail("AnimationContext destruction left a timer armed");
        }
    }

    (void)first;
    (void)second;
    return 0;
}

int verify_reduced_motion_and_reentrancy() {
    auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
    ui::detail::DispatcherOwner owner{{}, clock};
    auto animations = std::make_unique<ui::AnimationContext>(owner.dispatcher());
    animations->set_reduced_motion(true);
    float value = 0.0f;
    int invalidations = 0;
    int completions = 0;
    auto immediate = animations->start_tween(
        0.0f, 1.0f, 1s, ui::Easing::EaseInOut, ui::AnimationInvalidation::Paint,
        [&](float next) { value = next; },
        [&](ui::AnimationInvalidation kind) {
            if (kind == ui::AnimationInvalidation::Paint) ++invalidations;
        },
        [&] { ++completions; });
    if (immediate.valid() || value != 1.0f || invalidations != 1 || completions != 1 ||
        owner.active_timer_count() != 0) {
        return example::fail("reduced-motion start was not immediate/exact");
    }

    animations->set_reduced_motion(false);
    value = 0.0f;
    const auto active = animations->start_tween(
        0.0f, 2.0f, 1s, ui::Easing::Linear, ui::AnimationInvalidation::Paint,
        [&](float next) { value = next; }, {}, [&] { ++completions; });
    if (!active.valid() || owner.active_timer_count() != 1) {
        return example::fail("normal animation did not arm after reduced motion disabled");
    }
    animations->set_reduced_motion(true);
    if (value != 2.0f || completions != 2 || animations->active_count() != 0 ||
        owner.active_timer_count() != 0) {
        return example::fail("mid-flight reduced motion did not finish exactly once");
    }
    animations->set_reduced_motion(false);
    clock->advance(1s);
    if (owner.checkpoint() != 0 || completions != 2) {
        return example::fail("disabled reduced motion resurrected a completed animation");
    }

    auto reentrant = std::make_unique<ui::AnimationContext>(owner.dispatcher());
    int reentrant_completion = 0;
    const auto handle = reentrant->start_tween(
        0.0f, 1.0f, 16ms, ui::Easing::Linear, ui::AnimationInvalidation::Paint,
        [](float) {}, {}, [&] {
            ++reentrant_completion;
            reentrant.reset();
        });
    if (!handle.valid()) return example::fail("reentrant lifetime tween did not start");
    clock->advance(16ms);
    if (owner.checkpoint() != 1 || reentrant_completion != 1 || reentrant) {
        return example::fail("completion callback could not destroy animation owner safely");
    }
    return 0;
}

int self_test() {
    if (const int result = verify_easing_vectors(); result != 0) return result;
    if (const int result = verify_validation_and_zero_duration(); result != 0) return result;
    if (const int result = verify_spring_solver(); result != 0) return result;
    if (const int result = verify_scheduler_cancel_and_isolation(); result != 0) return result;
    if (const int result = verify_reduced_motion_and_reentrancy(); result != 0) return result;
    return 0;
}

struct DemoState {
    ui::State<float> value{0.0f};
    std::shared_ptr<ui::AnimationContext> animations;
    ui::UI* tree{};
    bool reduced{};
};

ui::UI make_demo(DemoState& state) {
    const auto invalidate = [&state](ui::AnimationInvalidation kind) {
        if (!state.tree) return;
        if (kind == ui::AnimationInvalidation::Layout) state.tree->invalidate_layout();
        // Paint is already bounded by ProgressBar's State observer; do not add a
        // whole-window repaint merely because an animation tick occurred.
    };

    return ui::UI{
        ui::Column{
            ui::Header{"T040 — Animation clock, tweens and springs"},
            ui::Label{
                "One T065-backed 16 ms wake per animation context. Tween time uses actual monotonic elapsed time; springs take one max_dt-clamped semi-implicit step per wake."
            }.size(12.0f).color(ui::colors::textMuted),
            ui::ProgressBar{state.value}.formatter([](float value) {
                return std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%";
            }),
            ui::Row{
                ui::Button{"Tween", [&state, invalidate] {
                    if (!state.animations) return;
                    const float target = state.value.get() < 0.5f ? 1.0f : 0.0f;
                    (void)state.animations->start_tween(
                        state.value.get(), target, 700ms, ui::Easing::EaseInOut,
                        ui::AnimationInvalidation::Paint,
                        [&state](float next) { state.value.set(next); }, invalidate);
                }},
                ui::Button{"Spring", [&state, invalidate] {
                    if (!state.animations) return;
                    const float target = state.value.get() < 0.5f ? 1.0f : 0.0f;
                    (void)state.animations->start_spring(
                        state.value.get(), target, {}, ui::AnimationInvalidation::Paint,
                        [&state](float next) { state.value.set(next); }, invalidate);
                }},
                ui::Button{"Reduced motion", [&state] {
                    if (!state.animations) return;
                    state.reduced = !state.reduced;
                    state.animations->set_reduced_motion(state.reduced);
                }}
            }.gap(8.0f),
            ui::Label{
                "The deterministic --self-test uses ManualDispatcherClock and verifies exact easing, spring stepping, max_dt, cancellation, reduced motion, timer coalescing, isolation and reentrant teardown."
            }.size(11.0f).color(ui::colors::textMuted)
        }.gap(14.0f)
    };
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

#ifdef NATIVEUI_EXAMPLE_SELF_TEST_ONLY
    return example::fail("window mode is disabled in self-test-only validation builds");
#else
    DemoState state;
    auto tree = make_demo(state);
    state.tree = &tree;
    ui::Application application;
    ui::StandaloneWindow window{
        application,
        tree,
        ui::WindowDesc{.title = "NativeUI T040 Animation",
                       .size = {680.0f, 300.0f},
                       .resizable = true}};
    state.animations = std::make_shared<ui::AnimationContext>(window.dispatcher());
    const int result = application.run();
    state.animations.reset();
    state.tree = nullptr;
    return result;
#endif
}
