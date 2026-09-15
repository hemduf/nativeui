#include "example_support.hpp"

#include <nativeui/animation.hpp>
#include <nativeui/widgets.hpp>

#include <chrono>
#include <cmath>
#include <memory>
#include <string>

namespace {

using namespace std::chrono_literals;

int self_test() {
    if (ui::kInvalidAnimationHandle.valid()) {
        return example::fail("default animation handle must be invalid");
    }

    ui::AnimationInvalidationTarget invalid_target;
    if (invalid_target.valid()) {
        return example::fail("default invalidation target must be invalid");
    }

    int paint_invalidations = 0;
    int layout_invalidations = 0;
    ui::AnimationInvalidationTarget target{
        [&] { ++paint_invalidations; },
        [&] { ++layout_invalidations; }};
    if (!target.valid()) {
        return example::fail("two public invalidation routes must form a valid target");
    }

    const ui::SpringOptions spring{};
    if (spring.stiffness != 170.0f || spring.damping != 26.0f ||
        spring.initial_velocity != 0.0f || spring.distance_epsilon != 0.001f ||
        spring.velocity_epsilon != 0.001f || spring.max_dt != ui::DispatcherDuration{1.0 / 30.0}) {
        return example::fail("public spring defaults changed");
    }

    // A default Dispatcher has no owner. Public animation APIs must fail closed
    // without invoking user callbacks or retained-tree invalidation routes.
    ui::AnimationContext animations{ui::Dispatcher{}};
    int writes = 0;
    int completions = 0;
    const auto tween = animations.start_tween(
        0.0f, 1.0f, 100ms, ui::Easing::EaseInOut, ui::AnimationInvalidation::Paint,
        target, [&](float) { ++writes; }, [&] { ++completions; });
    if (tween.valid() || animations.active_count() != 0 || writes != 0 || completions != 0 ||
        paint_invalidations != 0 || layout_invalidations != 0) {
        return example::fail("animation accepted an unowned dispatcher");
    }

    const auto spring_handle = animations.start_spring(
        0.0f, 1.0f, {}, ui::AnimationInvalidation::Layout, target,
        [&](float) { ++writes; }, [&] { ++completions; });
    if (spring_handle.valid() || animations.active_count() != 0 || writes != 0 ||
        completions != 0 || paint_invalidations != 0 || layout_invalidations != 0) {
        return example::fail("spring accepted an unowned dispatcher");
    }

    animations.set_reduced_motion(true);
    if (!animations.reduced_motion()) {
        return example::fail("reduced-motion state did not enable");
    }
    animations.set_reduced_motion(false);
    if (animations.reduced_motion()) {
        return example::fail("reduced-motion state did not disable");
    }
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
            ui::Label{"One Dispatcher-backed wake per animation context. Paint animations use a bounded retained invalidation target; layout animations explicitly request layout + paint."}.size(12.0f).color(ui::colors::textMuted),
            ui::ProgressBar{state.value}.formatter([](float value) {
                return std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%";
            }),
            ui::Row{
                ui::Button{"Tween", [&state] {
                    if (!state.animations) return;
                    const float target_value = state.value.get() < 0.5f ? 1.0f : 0.0f;
                    (void)state.animations->start_tween(
                        state.value.get(), target_value, 700ms, ui::Easing::EaseInOut,
                        ui::AnimationInvalidation::Paint, demo_target(state),
                        [&state](float next) { state.value.set(next); });
                }},
                ui::Button{"Spring", [&state] {
                    if (!state.animations) return;
                    const float target_value = state.value.get() < 0.5f ? 1.0f : 0.0f;
                    (void)state.animations->start_spring(
                        state.value.get(), target_value, {}, ui::AnimationInvalidation::Paint,
                        demo_target(state), [&state](float next) { state.value.set(next); });
                }},
                ui::Button{"Reduced motion", [&state] {
                    if (!state.animations) return;
                    state.reduced = !state.reduced;
                    state.animations->set_reduced_motion(state.reduced);
                }}
            }.gap(8.0f),
            ui::Label{"The public --self-test covers stable animation defaults, invalid handles/targets, fail-closed dispatcher ownership and reduced-motion state without depending on implementation-only headers."}.size(11.0f).color(ui::colors::textMuted)
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
