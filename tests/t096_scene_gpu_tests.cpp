#include "src/detail/platform_test_access.hpp"

#include <nativeui/nativeui.hpp>

#include <cstdint>
#include <array>
#include <iostream>
#include <optional>
#include <vector>

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

int fail(const char* message) {
    std::cerr << "T096 scene GPU: " << message << '\n';
    return 1;
}

} // namespace

int main() {
    ui::Application application;
    if (!application.valid()) return fail("application creation failed");

    bool left_changed = false;
    bool invalidate_during_paint = false;
    int left_paints = 0;
    int right_paints = 0;
    ui::UI* retained_owner = nullptr;
    ui::UI retained{ui::Row{
        ui::Canvas{24.0f, 32.0f, [&](ui::CanvasContext2D& canvas) {
            ++left_paints;
            if (invalidate_during_paint) {
                invalidate_during_paint = false;
                retained_owner->invalidate({6.0f, 6.0f, 8.0f, 8.0f});
            }
            canvas.fill_rect({0.0f, 0.0f, 24.0f, 32.0f},
                             {1.0f, 0.0f, 0.0f, 1.0f});
            if (left_changed) {
                canvas.fill_rect({6.0f, 6.0f, 8.0f, 8.0f},
                                 {0.0f, 0.0f, 0.0f, 1.0f});
            }
        }},
        ui::Canvas{24.0f, 32.0f, [&](ui::CanvasContext2D& canvas) {
            ++right_paints;
            canvas.fill_rect({0.0f, 0.0f, 24.0f, 32.0f},
                             {0.0f, 1.0f, 0.0f, 1.0f});
        }},
    }.gap(0.0f)};
    retained_owner = &retained;
    ui::StandaloneWindow window{application, retained,
                                ui::WindowDesc{.title = "T096 partial scene",
                                               .size = {48.0f, 32.0f},
                                               .resizable = true}};
    if (!window.valid()) return fail("window creation failed");

    for (int attempt = 0; attempt < 32; ++attempt) (void)application.poll(0.0);

    const auto left_before = read_pixel(application, window, {8.0f, 12.0f});
    const auto right_before = read_pixel(application, window, {34.0f, 12.0f});
    if (!left_before || left_before->r < 240 || left_before->g > 10 ||
        !right_before || right_before->g < 240 || right_before->r > 10) {
        return fail("initial retained scene mismatch");
    }

    constexpr std::array<ui::Point, 20> sample_points{{
        {2.0f, 2.0f}, {8.0f, 2.0f}, {18.0f, 2.0f}, {30.0f, 2.0f},
        {44.0f, 2.0f}, {2.0f, 8.0f}, {8.0f, 8.0f}, {18.0f, 8.0f},
        {30.0f, 8.0f}, {44.0f, 8.0f}, {2.0f, 16.0f}, {8.0f, 16.0f},
        {18.0f, 16.0f}, {30.0f, 16.0f}, {44.0f, 16.0f}, {2.0f, 28.0f},
        {8.0f, 28.0f}, {18.0f, 28.0f}, {30.0f, 28.0f}, {44.0f, 28.0f},
    }};
    const auto capture_grid = [&]() -> std::optional<std::vector<
        ui::detail::PlatformReadbackPixel>> {
        std::vector<ui::detail::PlatformReadbackPixel> pixels;
        pixels.reserve(sample_points.size());
        for (const auto point : sample_points) {
            const auto pixel = read_pixel(application, window, point);
            if (!pixel) return std::nullopt;
            pixels.push_back(*pixel);
        }
        return pixels;
    };
    const auto initial_grid = capture_grid();
    if (!initial_grid) return fail("initial reference grid readback failed");

    const int previous_left_paints = left_paints;
    const int previous_right_paints = right_paints;
    const auto previous_scene =
        ui::detail::PlatformTestAccess::scene_diagnostics(window);
    left_changed = true;
    retained.invalidate({6.0f, 6.0f, 8.0f, 8.0f});

    const auto changed = read_pixel(application, window, {8.0f, 12.0f});
    const auto preserved = read_pixel(application, window, {18.0f, 12.0f});
    const auto untouched = read_pixel(application, window, {34.0f, 12.0f});
    const auto updated_scene =
        ui::detail::PlatformTestAccess::scene_diagnostics(window);
    if (!changed || changed->r > 10 || changed->g > 10 || changed->b > 10 ||
        !preserved || preserved->r < 240 || preserved->g > 10 ||
        !untouched || untouched->g < 240 || untouched->r > 10) {
        return fail("partial update changed pixels outside its damage region");
    }
    if (left_paints <= previous_left_paints || right_paints != previous_right_paints) {
        return fail("partial traversal repainted an unchanged sibling");
    }
    if (updated_scene.scene_allocations != previous_scene.scene_allocations ||
        updated_scene.scene_builds != previous_scene.scene_builds + 1 ||
        updated_scene.partial_scene_updates !=
            previous_scene.partial_scene_updates + 1 ||
        (updated_scene.last_update_width >=
             static_cast<int>(window.size().w * window.scale_factor()) &&
         updated_scene.last_update_height >=
             static_cast<int>(window.size().h * window.scale_factor())) ||
        !updated_scene.scene_valid || updated_scene.full_repaint_required) {
        return fail("partial scene update did not commit into persistent storage");
    }

    const auto partial_grid = capture_grid();
    if (!partial_grid) return fail("partial scene grid readback failed");
    const float scale = window.scale_factor();
    for (std::size_t index = 0; index < sample_points.size(); ++index) {
        const int x = static_cast<int>(sample_points[index].x * scale);
        const int y = static_cast<int>(sample_points[index].y * scale);
        const bool outside_update =
            x < updated_scene.last_update_x ||
            x >= updated_scene.last_update_x + updated_scene.last_update_width ||
            y < updated_scene.last_update_y ||
            y >= updated_scene.last_update_y + updated_scene.last_update_height;
        const auto& before = (*initial_grid)[index];
        const auto& after = (*partial_grid)[index];
        if (outside_update && (before.r != after.r || before.g != after.g ||
                               before.b != after.b || before.a != after.a)) {
            return fail("persistent pixels outside the actual device update changed");
        }
    }

    const auto before_reentrant =
        ui::detail::PlatformTestAccess::scene_diagnostics(window);
    invalidate_during_paint = true;
    retained.invalidate({6.0f, 6.0f, 8.0f, 8.0f});
    for (int attempt = 0; attempt < 64; ++attempt) {
        (void)application.poll(0.0);
        const auto current =
            ui::detail::PlatformTestAccess::scene_diagnostics(window);
        if (current.scene_builds >= before_reentrant.scene_builds + 2) break;
    }
    const auto after_reentrant =
        ui::detail::PlatformTestAccess::scene_diagnostics(window);
    if (after_reentrant.scene_builds < before_reentrant.scene_builds + 2 ||
        !after_reentrant.scene_valid || after_reentrant.full_repaint_required) {
        return fail("overlapping invalidation raised during paint was lost");
    }

    using ui::detail::PlatformTestAccess;
    using ui::detail::SceneFaultStage;
    for (const auto stage : {SceneFaultStage::AfterSceneReset,
                             SceneFaultStage::AfterRetainedPaint,
                             SceneFaultStage::SceneSubmission}) {
        const auto before_failure = PlatformTestAccess::scene_diagnostics(window);
        if (!PlatformTestAccess::inject_scene_fault(window, stage)) {
            return fail("partial scene fault injection was rejected");
        }
        retained.invalidate({6.0f, 6.0f, 8.0f, 8.0f});
        bool failed_expose = false;
        for (int attempt = 0; attempt < 64; ++attempt) {
            (void)application.poll(0.0);
            if (PlatformTestAccess::scene_diagnostics(window).failed_exposes >
                before_failure.failed_exposes) {
                failed_expose = true;
                break;
            }
        }
        const auto failed = PlatformTestAccess::scene_diagnostics(window);
        if (!failed_expose || failed.scene_valid ||
            !failed.full_repaint_required || !failed.present_pending) {
            return fail("failed partial update was accepted as a complete scene");
        }
        if (!read_pixel(application, window, {8.0f, 12.0f})) {
            return fail("failed partial update did not recover on a later expose");
        }
        const auto recovered = PlatformTestAccess::scene_diagnostics(window);
        if (!recovered.scene_valid || recovered.full_repaint_required ||
            recovered.scene_builds <= before_failure.scene_builds) {
            return fail("partial failure recovery did not commit a full scene");
        }
    }

    retained.invalidate();
    const auto full_reference = capture_grid();
    if (!full_reference || full_reference->size() != partial_grid->size()) {
        return fail("independent full-render reference failed");
    }
    for (std::size_t index = 0; index < partial_grid->size(); ++index) {
        const auto& partial = (*partial_grid)[index];
        const auto& full = (*full_reference)[index];
        if (partial.r != full.r || partial.g != full.g ||
            partial.b != full.b || partial.a != full.a) {
            return fail("partial scene differs from independent full-render reference");
        }
    }

    return 0;
}
