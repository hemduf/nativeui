#include "src/detail/platform_test_access.hpp"

#include <nativeui/nativeui.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {

std::optional<ui::detail::PlatformReadbackPixel> read_pixel(
    ui::Application& application,
    ui::StandaloneWindow& window,
    ui::Point point) {
    if (!ui::detail::PlatformTestAccess::request_gpu_readback(window, point)) {
        return std::nullopt;
    }
    for (int attempt = 0; attempt < 64; ++attempt) {
        (void)application.poll(0.0);
        if (auto pixel = ui::detail::PlatformTestAccess::take_gpu_readback(window)) {
            return pixel;
        }
    }
    return std::nullopt;
}

std::optional<ui::detail::PlatformReadbackPixel> read_pixel(
    ui::Application& application,
    ui::EmbeddedView& view,
    ui::Point point) {
    if (!ui::detail::PlatformTestAccess::request_gpu_readback(view, point)) {
        return std::nullopt;
    }
    for (int attempt = 0; attempt < 64; ++attempt) {
        (void)application.poll(0.0);
        (void)view.poll();
        if (auto pixel = ui::detail::PlatformTestAccess::take_gpu_readback(view)) {
            return pixel;
        }
    }
    return std::nullopt;
}

int fail(const char* message) {
    std::cerr << "T095 scene GPU: " << message << '\n';
    return 1;
}

bool wait_for_failure(ui::Application& application,
                      ui::StandaloneWindow& window,
                      std::uint64_t previous_failures) {
    for (int attempt = 0; attempt < 64; ++attempt) {
        (void)application.poll(0.0);
        if (ui::detail::PlatformTestAccess::scene_diagnostics(window).failed_exposes >
            previous_failures) {
            return true;
        }
    }
    return false;
}

bool wait_for_failure(ui::Application& application,
                      ui::EmbeddedView& view,
                      std::uint64_t previous_failures) {
    for (int attempt = 0; attempt < 64; ++attempt) {
        (void)application.poll(0.0);
        (void)view.poll();
        if (ui::detail::PlatformTestAccess::scene_diagnostics(view).failed_exposes >
            previous_failures) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    ui::Application application;
    if (!application.valid()) return fail("application creation failed");

    int paints = 0;
    std::uint8_t red = 255;
    bool draw_overlay = true;
    bool throw_paint = false;
    bool invalidate_during_paint = false;
    ui::UI* retained_owner = nullptr;
    ui::UI retained{ui::Canvas{48.0f, 32.0f, [&](ui::CanvasContext2D& canvas) {
        ++paints;
        if (invalidate_during_paint) {
            invalidate_during_paint = false;
            retained_owner->invalidate();
        }
        canvas.fill_rect({0.0f, 0.0f, 24.0f, 32.0f},
                         {static_cast<float>(red) / 255.0f, 0.0f, 0.0f, 1.0f});
        canvas.fill_rect({1.0f, 1.0f, 3.0f, 4.0f}, {0.0f, 1.0f, 0.0f, 1.0f});
        canvas.fill_rect({43.0f, 26.0f, 4.0f, 5.0f}, {0.0f, 0.0f, 1.0f, 1.0f});
        if (draw_overlay) {
            canvas.fill_rect({30.0f, 10.0f, 8.0f, 8.0f},
                             {1.0f, 1.0f, 0.0f, 0.5f});
        }
        if (throw_paint) {
            throw_paint = false;
            canvas.save();
            canvas.push_clip({0.0f, 0.0f, 4.0f, 4.0f});
            canvas.scale(0.5f);
            canvas.fill_rect({0.0f, 0.0f, 4.0f, 4.0f},
                             {1.0f, 0.0f, 1.0f, 1.0f});
            throw std::runtime_error("injected T095 retained paint fault");
        }
    }}};
    retained_owner = &retained;
    ui::StandaloneWindow window{application, retained,
                                ui::WindowDesc{.title = "T095 scene GPU",
                                               .size = {48.0f, 32.0f},
                                               .resizable = true}};
    if (!window.valid()) return fail("window creation failed");

    // Let native configure and initial focus/layout notifications settle before
    // asserting that later system exposes do not traverse the retained tree.
    for (int attempt = 0; attempt < 32; ++attempt) {
        (void)application.poll(0.0);
    }

    const auto first = read_pixel(application, window, {16.0f, 12.0f});
    if (!first || first->r < 240 || first->g > 10 || paints == 0) {
        return fail("initial retained scene mismatch");
    }
    const int first_paints = paints;
    const auto first_scene = ui::detail::PlatformTestAccess::scene_diagnostics(window);

    const auto clean = read_pixel(application, window, {16.0f, 12.0f});
    if (!clean || clean->r < 240 || clean->g > 10) {
        return fail("clean expose lost the scene");
    }
    if (paints != first_paints) {
        return fail("clean expose repainted the retained tree");
    }
    const auto top_left = read_pixel(application, window, {2.0f, 2.0f});
    const auto bottom_right = read_pixel(application, window, {45.0f, 29.0f});
    const auto translucent = read_pixel(application, window, {32.0f, 12.0f});
    if (!top_left || top_left->g < 240 || top_left->r > 10 ||
        !bottom_right || bottom_right->b < 240 || bottom_right->r > 10 ||
        !translucent || translucent->r < 110 || translucent->g < 110 ||
        translucent->a < 240 || paints != first_paints) {
        return fail("scene orientation, alpha or clean presentation mismatch");
    }
    const auto warm_scene = ui::detail::PlatformTestAccess::scene_diagnostics(window);
    if (warm_scene.scene_allocations != first_scene.scene_allocations ||
        warm_scene.scene_builds != first_scene.scene_builds ||
        warm_scene.presentations <= first_scene.presentations) {
        return fail("warm clean exposes reallocated or rebuilt scene storage");
    }

    red = 0;
    retained.invalidate();
    const auto changed = read_pixel(application, window, {16.0f, 12.0f});
    if (!changed || changed->r > 10 || paints <= first_paints) {
        return fail("invalidation did not rebuild the scene");
    }

    using ui::detail::PlatformTestAccess;
    using ui::detail::SceneFaultStage;

    for (const auto stage : {SceneFaultStage::AfterSceneReset,
                             SceneFaultStage::AfterRetainedPaint,
                             SceneFaultStage::SceneSubmission}) {
        const auto before = PlatformTestAccess::scene_diagnostics(window);
        if (!PlatformTestAccess::inject_scene_fault(window, stage)) {
            return fail("fault injection rejected");
        }
        retained.invalidate();
        if (!wait_for_failure(application, window, before.failed_exposes)) {
            return fail("full-build fault did not reach native expose");
        }
        const auto failed = PlatformTestAccess::scene_diagnostics(window);
        if (!window.valid() || window.should_close() || failed.scene_valid ||
            !failed.full_repaint_required || !failed.present_pending) {
            return fail("failed full build was accepted or closed the view");
        }
        if (!read_pixel(application, window, {16.0f, 12.0f})) {
            return fail("full-build failure did not recover on later expose");
        }
        const auto recovered = PlatformTestAccess::scene_diagnostics(window);
        if (!recovered.scene_valid || recovered.full_repaint_required ||
            recovered.scene_builds <= before.scene_builds) {
            return fail("full-build recovery did not commit a fresh scene");
        }
    }

    const auto before_throw = PlatformTestAccess::scene_diagnostics(window);
    throw_paint = true;
    retained.invalidate();
    if (!wait_for_failure(application, window, before_throw.failed_exposes) ||
        !window.valid() || window.should_close()) {
        return fail("throwing retained paint crossed the native boundary");
    }
    const auto recovered_after_throw = read_pixel(application, window, {45.0f, 29.0f});
    if (!recovered_after_throw || recovered_after_throw->b < 240 ||
        PlatformTestAccess::scene_diagnostics(window).scene_builds <=
            before_throw.scene_builds) {
        return fail("throwing retained paint left the canvas clipped or invalid");
    }

    const auto before_reentrant = PlatformTestAccess::scene_diagnostics(window);
    invalidate_during_paint = true;
    retained.invalidate();
    for (int attempt = 0; attempt < 64; ++attempt) {
        (void)application.poll(0.0);
        if (PlatformTestAccess::scene_diagnostics(window).scene_builds >=
            before_reentrant.scene_builds + 2) break;
    }
    if (PlatformTestAccess::scene_diagnostics(window).scene_builds <
        before_reentrant.scene_builds + 2) {
        return fail("invalidation during paint was lost");
    }

    const auto before_rejection = PlatformTestAccess::scene_diagnostics(window);
    if (!PlatformTestAccess::reject_next_deferred_redraw(window)) {
        return fail("deferred redraw rejection seam refused the window");
    }
    invalidate_during_paint = true;
    retained.invalidate();
    for (int attempt = 0; attempt < 64; ++attempt) {
        (void)application.poll(0.0);
        if (PlatformTestAccess::scene_diagnostics(window).scene_builds >
            before_rejection.scene_builds) break;
    }
    for (int attempt = 0; attempt < 32; ++attempt) {
        (void)application.poll(0.0);
    }
    const auto rejected = PlatformTestAccess::scene_diagnostics(window);
    if (rejected.scene_builds != before_rejection.scene_builds + 1 ||
        !rejected.full_repaint_required) {
        return fail("rejected deferred redraw lost work or spun at idle");
    }
    if (!read_pixel(application, window, {16.0f, 12.0f}) ||
        PlatformTestAccess::scene_diagnostics(window).scene_builds <=
            rejected.scene_builds) {
        return fail("external expose did not recover rejected redraw");
    }

    for (const auto stage : {SceneFaultStage::Snapshot,
                             SceneFaultStage::PresentationCopy,
                             SceneFaultStage::PresentationSubmission}) {
        const auto before = PlatformTestAccess::scene_diagnostics(window);
        const int before_paints = paints;
        if (!PlatformTestAccess::inject_scene_fault(window, stage) ||
            !PlatformTestAccess::request_gpu_readback(window, {16.0f, 12.0f}) ||
            !wait_for_failure(application, window, before.failed_exposes)) {
            return fail("presentation fault did not reach native expose");
        }
        const auto failed = PlatformTestAccess::scene_diagnostics(window);
        if (!window.valid() || window.should_close() || !failed.scene_valid ||
            failed.full_repaint_required || !failed.present_pending) {
            return fail("presentation-only fault lost the valid scene");
        }
        if (!read_pixel(application, window, {16.0f, 12.0f})) {
            return fail("presentation-only fault did not recover");
        }
        const auto recovered = PlatformTestAccess::scene_diagnostics(window);
        if (paints != before_paints ||
            recovered.scene_builds != before.scene_builds ||
            recovered.present_pending ||
            recovered.presentations <= before.presentations) {
            return fail("presentation retry repainted or remained pending");
        }
    }

    const auto before_new_work = PlatformTestAccess::scene_diagnostics(window);
    if (!PlatformTestAccess::inject_scene_fault(window, SceneFaultStage::PresentationCopy) ||
        !PlatformTestAccess::request_gpu_readback(window, {16.0f, 12.0f}) ||
        !wait_for_failure(application, window, before_new_work.failed_exposes)) {
        return fail("presentation fault before new work did not execute");
    }
    red = 255;
    retained.invalidate();
    const auto latest = read_pixel(application, window, {16.0f, 12.0f});
    if (!latest || latest->r < 240 ||
        PlatformTestAccess::scene_diagnostics(window).scene_builds <=
            before_new_work.scene_builds) {
        return fail("new work after failed present was not rebuilt");
    }

    draw_overlay = false;
    retained.invalidate();
    const auto erased = read_pixel(application, window, {32.0f, 12.0f});
    if (!erased || erased->r > 10 || erased->g > 10 || erased->b > 10 ||
        erased->a < 240) {
        return fail("canonical reset did not erase translucent removed content");
    }

    const auto before_recreate = PlatformTestAccess::scene_diagnostics(window);
    if (!PlatformTestAccess::request_context_recreation(window)) {
        return fail("context recreation request rejected");
    }
    for (int attempt = 0; attempt < 64; ++attempt) {
        (void)application.poll(0.0);
        if (PlatformTestAccess::scene_diagnostics(window).scene_allocations >
            before_recreate.scene_allocations) break;
    }
    const auto recreated = PlatformTestAccess::scene_diagnostics(window);
    if (recreated.scene_allocations <= before_recreate.scene_allocations ||
        recreated.scene_builds <= before_recreate.scene_builds ||
        !recreated.scene_valid) {
        return fail("same-size renderer recreation reused obsolete scene");
    }

    const auto before_scale = PlatformTestAccess::scene_diagnostics(window);
    if (!PlatformTestAccess::override_scene_scale(
            window, window.scale_factor() * 0.75f)) {
        return fail("scale-only renderer change rejected");
    }
    for (int attempt = 0; attempt < 64; ++attempt) {
        (void)application.poll(0.0);
        if (PlatformTestAccess::scene_diagnostics(window).scene_builds >
            before_scale.scene_builds) break;
    }
    const auto scaled = PlatformTestAccess::scene_diagnostics(window);
    if (scaled.scene_builds <= before_scale.scene_builds ||
        scaled.scene_allocations != before_scale.scene_allocations) {
        return fail("scale-only change did not repaint with compatible storage");
    }
    if (!PlatformTestAccess::override_scene_scale(window, std::nullopt) ||
        !read_pixel(application, window, {16.0f, 12.0f})) {
        return fail("restoring native scale failed");
    }

    const auto before_creation_fault = PlatformTestAccess::scene_diagnostics(window);
    if (!PlatformTestAccess::inject_scene_fault(
            window, SceneFaultStage::BeforeSceneAllocation) ||
        !PlatformTestAccess::request_context_recreation(window) ||
        !wait_for_failure(application, window,
                          before_creation_fault.failed_exposes)) {
        return fail("scene acquisition fault did not execute");
    }
    const auto acquisition_failed = PlatformTestAccess::scene_diagnostics(window);
    if (acquisition_failed.scene_valid ||
        acquisition_failed.scene_allocations != before_creation_fault.scene_allocations ||
        window.should_close()) {
        return fail("scene acquisition failure installed partial storage");
    }
    if (!read_pixel(application, window, {16.0f, 12.0f}) ||
        PlatformTestAccess::scene_diagnostics(window).scene_allocations <=
            before_creation_fault.scene_allocations) {
        return fail("scene acquisition failure did not recover");
    }

    const auto before_loss = PlatformTestAccess::scene_diagnostics(window);
    if (!PlatformTestAccess::inject_scene_fault(
            window, SceneFaultStage::ConfirmedContextLoss) ||
        !PlatformTestAccess::request_gpu_readback(window, {16.0f, 12.0f}) ||
        !wait_for_failure(application, window, before_loss.failed_exposes)) {
        return fail("confirmed context loss did not reach native expose");
    }
    if (!window.valid() ||
        PlatformTestAccess::scene_diagnostics(window).scene_valid ||
        PlatformTestAccess::scene_diagnostics(window).scene_allocations !=
            before_loss.scene_allocations) {
        return fail("lost context retained usable scene identity");
    }
    if (!read_pixel(application, window, {16.0f, 12.0f}) ||
        PlatformTestAccess::scene_diagnostics(window).scene_allocations <=
            before_loss.scene_allocations) {
        return fail("renderer did not recreate after confirmed context loss");
    }

    const auto before_resize = PlatformTestAccess::scene_diagnostics(window);
    if (!window.set_size({64.0f, 40.0f})) return fail("native resize rejected");
    for (int attempt = 0; attempt < 64; ++attempt) {
        (void)application.poll(0.0);
        if (PlatformTestAccess::scene_diagnostics(window).scene_allocations >
            before_resize.scene_allocations) break;
    }
    if (PlatformTestAccess::scene_diagnostics(window).scene_allocations <=
            before_resize.scene_allocations ||
        !read_pixel(application, window, {16.0f, 12.0f})) {
        return fail("physical resize did not replace and rebuild scene storage");
    }

    int child_paints = 0;
    ui::UI child_ui{ui::Canvas{48.0f, 32.0f, [&](ui::CanvasContext2D& canvas) {
        ++child_paints;
        canvas.fill_rect({0.0f, 0.0f, 48.0f, 32.0f},
                         {0.0f, 1.0f, 0.0f, 1.0f});
    }}};
    {
        ui::EmbeddedView child{child_ui, window.native_handle(), {48.0f, 32.0f}};
        if (!child.native_handle()) return fail("embedded child creation failed");
        for (int attempt = 0; attempt < 32; ++attempt) {
            (void)application.poll(0.0);
            (void)child.poll();
        }
        const auto child_pixel = read_pixel(application, child, {16.0f, 12.0f});
        if (!child_pixel || child_pixel->g < 240 || child_paints == 0) {
            return fail("embedded scene initial paint failed");
        }
        const auto child_before = PlatformTestAccess::scene_diagnostics(child);
        if (!PlatformTestAccess::inject_scene_fault(child,
                SceneFaultStage::AfterRetainedPaint)) {
            return fail("embedded fault injection rejected");
        }
        child_ui.invalidate();
        if (!wait_for_failure(application, child, child_before.failed_exposes) ||
            child.should_close()) {
            return fail("embedded scene fault closed or skipped the view");
        }
        const auto parent_survived = read_pixel(application, window, {16.0f, 12.0f});
        if (!parent_survived || parent_survived->r < 240) {
            return fail("embedded scene fault damaged parent view");
        }
        const auto child_recovered = read_pixel(application, child, {16.0f, 12.0f});
        if (!child_recovered || child_recovered->g < 240 ||
            !PlatformTestAccess::scene_diagnostics(child).scene_valid) {
            return fail("embedded scene did not recover");
        }

        const auto parent_before = PlatformTestAccess::scene_diagnostics(window);
        if (!PlatformTestAccess::inject_scene_fault(window,
                SceneFaultStage::PresentationCopy) ||
            !PlatformTestAccess::request_gpu_readback(window, {16.0f, 12.0f}) ||
            !wait_for_failure(application, window, parent_before.failed_exposes)) {
            return fail("parent fault with embedded sibling did not execute");
        }
        const auto child_survived = read_pixel(application, child, {16.0f, 12.0f});
        if (!child_survived || child_survived->g < 240) {
            return fail("parent scene fault damaged embedded sibling");
        }
        if (!read_pixel(application, window, {16.0f, 12.0f})) {
            return fail("parent scene failed to recover with embedded sibling");
        }
    }
    if (!read_pixel(application, window, {16.0f, 12.0f})) {
        return fail("parent scene failed after embedded sibling destruction");
    }
    for (int attempt = 0; attempt < 32; ++attempt) {
        (void)application.poll(0.0);
    }
    const auto settled = PlatformTestAccess::scene_diagnostics(window);
    for (int attempt = 0; attempt < 32; ++attempt) {
        (void)application.poll(0.0);
    }
    const auto idle = PlatformTestAccess::scene_diagnostics(window);
    if (idle.scene_builds != settled.scene_builds ||
        idle.presentations != settled.presentations ||
        idle.failed_exposes != settled.failed_exposes) {
        return fail("settled scene spun or retried at idle");
    }

    if (argc > 1 && std::string_view{argv[1]} == "--benchmark") {
        constexpr int kFrames = 32;
        const auto run_phase = [&](bool rebuild) -> std::optional<long long> {
            const auto start = std::chrono::steady_clock::now();
            for (int frame = 0; frame < kFrames; ++frame) {
                const auto before = PlatformTestAccess::scene_diagnostics(window);
                if (rebuild) {
                    retained.invalidate();
                } else if (!PlatformTestAccess::request_expose(window)) {
                    return std::nullopt;
                }
                bool finished = false;
                for (int attempt = 0; attempt < 64; ++attempt) {
                    (void)application.poll(0.0);
                    const auto after = PlatformTestAccess::scene_diagnostics(window);
                    if (after.presentations > before.presentations) {
                        if (rebuild && after.scene_builds <= before.scene_builds) {
                            return std::nullopt;
                        }
                        finished = true;
                        break;
                    }
                }
                if (!finished) return std::nullopt;
            }
            return std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::steady_clock::now() - start).count();
        };
        const auto before = PlatformTestAccess::scene_diagnostics(window);
        const auto present_time = run_phase(false);
        const auto rebuild_time = run_phase(true);
        const auto after = PlatformTestAccess::scene_diagnostics(window);
        if (!present_time || !rebuild_time ||
            after.scene_allocations != before.scene_allocations) {
            return fail("warm benchmark did not retain scene storage");
        }
        std::cout << "T095 " << kFrames << " frames: present-only "
                  << *present_time << " us, full rebuild " << *rebuild_time
                  << " us; scene allocations " << after.scene_allocations << '\n';
    }
    return 0;
}
