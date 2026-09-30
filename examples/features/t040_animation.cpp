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

ui::AnimationInvalidationTarget noop_target() {
    return ui::AnimationInvalidationTarget{[] {}, [] {}};
}

float expected_easing(ui::Easing easing, float t) {
    switch (easing) {
    case ui::Easing::Linear: return t;
    case ui::Easing::EaseIn: return t * t * t;
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
        ui::Easing::Linear, ui::Easing::EaseIn,
        ui::Easing::EaseOut, ui::Easing::EaseInOut};

    for (const auto easing : easings) {
        for (const float t : samples) {
            auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
            ui::detail::DispatcherOwner owner{{}, clock};
            ui::AnimationContext animations{owner.dispatcher()};
            float value = 0.0f;
            int completion_count = 0;
            const auto handle = animations.start_tween(
                0.0f, 1.0f, 1s, easing, ui::AnimationInvalidation::Paint,
                noop_target(), [&](float next) { value = next; },
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

int verify_defaults_and_validation() {
    const ui::SpringOptions defaults;
    if (defaults.stiffness != 170.0f || defaults.damping != 26.0f ||
        defaults.initial_velocity != 0.0f || defaults.distance_epsilon != 0.001f ||
        defaults.velocity_epsilon != 0.001f ||
        !near(static_cast<float>(defaults.max_dt.count()), 1.0f / 30.0f, 0.000001f)) {
        return example::fail("SpringOptions defaults differ from the v1 contract");
    }

    auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
    ui::detail::DispatcherOwner owner{{}, clock};
    ui::AnimationContext animations{owner.dispatcher()};

    float value = 0.0f;
    int invalidations = 0;
    int completions = 0;
    auto zero = animations.start_tween(
        0.0f, 0.75f, 0s, ui::Easing::EaseInOut,
        ui::AnimationInvalidation::Layout,
        ui::AnimationInvalidationTarget{
            [&] { invalidations += 10; }, [&] { ++invalidations; }},
        [&](float next) { value = next; }, [&] { ++completions; });
    if (zero.valid() || value != 0.75f || invalidations != 1 || completions != 1 ||
        owner.active_timer_count() != 0) {
        return example::fail("zero-duration tween contract failed");
    }

    if (animations.start_tween(
            0.0f, 1.0f, 1s, ui::Easing::Linear,
            ui::AnimationInvalidation::Paint, {}, [](float) {}).valid()) {
        return example::fail("animation without retained invalidation target was accepted");
    }

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const double dnan = std::numeric_limits<double>::quiet_NaN();
    const double dinf = std::numeric_limits<double>::infinity();
    int invalid_writes = 0;
    int invalid_completions = 0;

    auto invalid_tween = [&](float from, float to, ui::DispatcherDuration duration,
                             ui::Easing easing = ui::Easing::Linear) {
        return animations.start_tween(
            from, to, duration, easing, ui::AnimationInvalidation::Paint,
            noop_target(), [&](float) { ++invalid_writes; },
            [&] { ++invalid_completions; });
    };

    if (invalid_tween(nan, 1.0f, 1s).valid() ||
        invalid_tween(0.0f, nan, 1s).valid() ||
        invalid_tween(0.0f, inf, 1s).valid() ||
        invalid_tween(0.0f, 1.0f, -1ms).valid() ||
        invalid_tween(0.0f, 1.0f, ui::DispatcherDuration{dnan}).valid() ||
        invalid_tween(0.0f, 1.0f, ui::DispatcherDuration{dinf}).valid() ||
        invalid_tween(0.0f, 1.0f, 1s, static_cast<ui::Easing>(99)).valid()) {
        return example::fail("invalid tween finite/duration/easing configuration was accepted");
    }

    auto invalid_spring = [&](float from, float to, ui::SpringOptions options) {
        return animations.start_spring(
            from, to, options, ui::AnimationInvalidation::Paint,
            noop_target(), [&](float) { ++invalid_writes; },
            [&] { ++invalid_completions; });
    };

    std::vector<ui::SpringOptions> invalid_options;
    {
        auto o = defaults;
        o.stiffness = -1.0f; invalid_options.push_back(o);
        o = defaults; o.stiffness = nan; invalid_options.push_back(o);
        o = defaults; o.stiffness = inf; invalid_options.push_back(o);
        o = defaults; o.damping = -1.0f; invalid_options.push_back(o);
        o = defaults; o.damping = nan; invalid_options.push_back(o);
        o = defaults; o.initial_velocity = nan; invalid_options.push_back(o);
        o = defaults; o.initial_velocity = inf; invalid_options.push_back(o);
        o = defaults; o.distance_epsilon = -1.0f; invalid_options.push_back(o);
        o = defaults; o.distance_epsilon = nan; invalid_options.push_back(o);
        o = defaults; o.velocity_epsilon = -1.0f; invalid_options.push_back(o);
        o = defaults; o.velocity_epsilon = nan; invalid_options.push_back(o);
        o = defaults; o.max_dt = 0s; invalid_options.push_back(o);
        o = defaults; o.max_dt = ui::DispatcherDuration{-0.1}; invalid_options.push_back(o);
        o = defaults; o.max_dt = ui::DispatcherDuration{dnan}; invalid_options.push_back(o);
        o = defaults; o.max_dt = ui::DispatcherDuration{dinf}; invalid_options.push_back(o);
    }

    if (invalid_spring(nan, 1.0f, defaults).valid() ||
        invalid_spring(0.0f, nan, defaults).valid()) {
        return example::fail("invalid spring value/target was accepted");
    }
    for (const auto& options : invalid_options) {
        if (invalid_spring(0.0f, 1.0f, options).valid()) {
            return example::fail("invalid SpringOptions configuration was accepted");
        }
    }

    if (invalid_writes != 0 || invalid_completions != 0 ||
        animations.active_count() != 0 || owner.active_timer_count() != 0) {
        return example::fail("rejected animation configuration mutated scheduler state");
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
            noop_target(), [&](float next) { value = next; });
        if (!handle.valid()) return example::fail("spring did not start");
        clock->advance(16ms);
        if (owner.checkpoint() != 1) return example::fail("spring wake did not execute");
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
            noop_target(), [&](float next) { value = next; });
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
            noop_target(), [&](float next) { value = next; },
            [&] { ++completions; });
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
            noop_target(), [&](float next) { value = next; });
        if (!handle.valid()) return example::fail("zero-stiffness spring did not start");
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
        noop_target(), [&](float next) { first = next; },
        [&] { ++first_completion; });
    auto h2 = a.start_tween(
        0.0f, 2.0f, 1s, ui::Easing::Linear, ui::AnimationInvalidation::Paint,
        noop_target(), [&](float next) { second = next; });
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
        noop_target(),
        [&](float next) { first = next; (void)a.cancel(self_cancel); },
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
            noop_target(), [](float) {});
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

int verify_retained_invalidation() {
    constexpr ui::Size size{220.0f, 100.0f};
    constexpr ui::Rect paint_region{12.0f, 16.0f, 48.0f, 24.0f};
    ui::UI tree{ui::Label{"Animation invalidation target"}};
    tree.resize(size);
    ui::HeadlessRenderer renderer{size, 1.0f};
    if (!renderer.render(tree)) return example::fail("animation invalidation baseline render failed");
    if (tree.layout_dirty() || tree.paint_dirty()) {
        return example::fail("animation invalidation baseline was not clean");
    }

    ui::AnimationInvalidationTarget target{
        [&] { tree.invalidate(paint_region); },
        [&] { tree.invalidate_layout(); }};

    auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
    ui::detail::DispatcherOwner owner{{}, clock};
    ui::AnimationContext animations{owner.dispatcher()};
    float value = 0.0f;

    auto paint = animations.start_tween(
        0.0f, 1.0f, 1s, ui::Easing::Linear, ui::AnimationInvalidation::Paint,
        target, [&](float next) { value = next; });
    if (!paint.valid()) return example::fail("paint animation did not start");
    clock->advance(16ms);
    if (owner.checkpoint() != 1 || tree.layout_dirty() || !tree.paint_dirty()) {
        return example::fail("Paint animation did not produce paint-only retained dirtiness");
    }
    const auto& regions = tree.dirty_regions();
    if (regions.size() != 1 || regions.front().x != paint_region.x ||
        regions.front().y != paint_region.y || regions.front().w != paint_region.w ||
        regions.front().h != paint_region.h) {
        return example::fail("Paint animation invalidation was not bounded to the retained region");
    }
    if (!animations.cancel(paint)) return example::fail("paint invalidation tween cancel failed");
    if (!renderer.render(tree)) return example::fail("paint invalidation cleanup render failed");

    auto layout = animations.start_tween(
        value, 1.0f, 1s, ui::Easing::Linear, ui::AnimationInvalidation::Layout,
        target, [&](float next) { value = next; });
    if (!layout.valid()) return example::fail("layout animation did not start");
    clock->advance(16ms);
    if (owner.checkpoint() != 1 || !tree.layout_dirty() || !tree.paint_dirty()) {
        return example::fail("Layout animation did not produce layout + paint retained dirtiness");
    }
    (void)animations.cancel(layout);
    return 0;
}

int verify_reduced_motion_and_reentrancy() {
    auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
    ui::detail::DispatcherOwner owner{{}, clock};
    auto animations = std::make_unique<ui::AnimationContext>(owner.dispatcher());

    animations->set_reduced_motion(true);
    float value = 0.0f;
    int paint_invalidations = 0;
    int layout_invalidations = 0;
    int completions = 0;
    const ui::AnimationInvalidationTarget target{
        [&] { ++paint_invalidations; }, [&] { ++layout_invalidations; }};

    auto immediate_tween = animations->start_tween(
        0.0f, 1.0f, 1s, ui::Easing::EaseInOut,
        ui::AnimationInvalidation::Paint, target,
        [&](float next) { value = next; }, [&] { ++completions; });
    if (immediate_tween.valid() || value != 1.0f || paint_invalidations != 1 ||
        layout_invalidations != 0 || completions != 1 || owner.active_timer_count() != 0) {
        return example::fail("reduced-motion tween start was not immediate/exact");
    }

    value = 0.0f;
    auto immediate_spring = animations->start_spring(
        0.0f, 2.0f, {}, ui::AnimationInvalidation::Layout, target,
        [&](float next) { value = next; }, [&] { ++completions; });
    if (immediate_spring.valid() || value != 2.0f || paint_invalidations != 1 ||
        layout_invalidations != 1 || completions != 2 || owner.active_timer_count() != 0) {
        return example::fail("reduced-motion spring start was not immediate/exact");
    }

    animations->set_reduced_motion(false);
    value = 0.0f;
    const auto active_tween = animations->start_tween(
        0.0f, 3.0f, 1s, ui::Easing::Linear, ui::AnimationInvalidation::Paint,
        target, [&](float next) { value = next; }, [&] { ++completions; });
    if (!active_tween.valid() || owner.active_timer_count() != 1) {
        return example::fail("normal tween did not arm after reduced motion disabled");
    }
    animations->set_reduced_motion(true);
    if (value != 3.0f || completions != 3 || animations->active_count() != 0 ||
        owner.active_timer_count() != 0) {
        return example::fail("mid-flight reduced motion did not finish tween exactly once");
    }

    animations->set_reduced_motion(false);
    value = 0.0f;
    const auto active_spring = animations->start_spring(
        0.0f, 4.0f, {}, ui::AnimationInvalidation::Layout, target,
        [&](float next) { value = next; }, [&] { ++completions; });
    if (!active_spring.valid() || owner.active_timer_count() != 1) {
        return example::fail("normal spring did not arm after reduced motion disabled");
    }
    animations->set_reduced_motion(true);
    if (value != 4.0f || completions != 4 || animations->active_count() != 0 ||
        owner.active_timer_count() != 0) {
        return example::fail("mid-flight reduced motion did not finish spring exactly once");
    }

    animations->set_reduced_motion(false);
    clock->advance(1s);
    if (owner.checkpoint() != 0 || completions != 4) {
        return example::fail("disabled reduced motion resurrected completed animations");
    }

    auto reentrant = std::make_unique<ui::AnimationContext>(owner.dispatcher());
    int reentrant_completion = 0;
    const auto handle = reentrant->start_tween(
        0.0f, 1.0f, 16ms, ui::Easing::Linear, ui::AnimationInvalidation::Paint,
        noop_target(), [](float) {}, [&] {
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
    if (const int result = verify_defaults_and_validation(); result != 0) return result;
    if (const int result = verify_spring_solver(); result != 0) return result;
    if (const int result = verify_scheduler_cancel_and_isolation(); result != 0) return result;
    if (const int result = verify_retained_invalidation(); result != 0) return result;
    if (const int result = verify_reduced_motion_and_reentrancy(); result != 0) return result;
    return 0;
}

struct DemoState {
    ui::State<float> value{0.0f};
    std::shared_ptr<ui::AnimationContext> animations;
    ui::UI* tree{};
    bool reduced{};
};

ui::AnimationInvalidationTarget demo_target(DemoState& state) {
    return ui::AnimationInvalidationTarget{
        [&state] {
            if (state.tree) state.tree->invalidate(ui::Rect{16.0f, 72.0f, 648.0f, 72.0f});
        },
        [&state] {
            if (state.tree) state.tree->invalidate_layout();
        }};
}

ui::UI make_demo(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T040 — Animation clock, tweens and springs"},
            ui::Label{"One T065-backed 16 ms wake per animation context. Paint animations use a bounded retained invalidation target; layout animations explicitly request layout + paint."}.size(12.0f).color(ui::colors::textMuted),
            ui::ProgressBar{state.value}.formatter([](float value) {
                return std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%";
            }),
            ui::Row{
                ui::Button{"Tween", [&state] {
                    if (!state.animations) return;
                    const float target = state.value.get() < 0.5f ? 1.0f : 0.0f;
                    (void)state.animations->start_tween(
                        state.value.get(), target, 700ms, ui::Easing::EaseInOut,
                        ui::AnimationInvalidation::Paint, demo_target(state),
                        [&state](float next) { state.value.set(next); });
                }},
                ui::Button{"Spring", [&state] {
                    if (!state.animations) return;
                    const float target = state.value.get() < 0.5f ? 1.0f : 0.0f;
                    (void)state.animations->start_spring(
                        state.value.get(), target, {}, ui::AnimationInvalidation::Paint,
                        demo_target(state), [&state](float next) { state.value.set(next); });
                }},
                ui::Button{"Reduced motion", [&state] {
                    if (!state.animations) return;
                    state.reduced = !state.reduced;
                    state.animations->set_reduced_motion(state.reduced);
                }}
            }.gap(8.0f),
            ui::Label{"The deterministic --self-test verifies exact easing/defaults, finite-value rejection, spring stepping/rest/max_dt, retained Paint-vs-Layout invalidation, reduced-motion tween+spring, timer coalescing, isolation and reentrant teardown."}.size(11.0f).color(ui::colors::textMuted)
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
        application, tree,
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
