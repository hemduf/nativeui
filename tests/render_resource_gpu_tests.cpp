#include "detail/platform_test_access.hpp"

#include <nativeui/nativeui.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>

namespace {

using ui::detail::PlatformReadbackPixel;
using ui::detail::PlatformTestAccess;
using ui::detail::SceneFaultStage;

int fail(const char* message) {
    std::cerr << "render resource GPU: " << message << '\n';
    return 1;
}

std::optional<PlatformReadbackPixel> read_pixel(
    ui::Application& application,
    ui::StandaloneWindow& window,
    ui::Point point) {
    if (!PlatformTestAccess::request_gpu_readback(window, point)) {
        return std::nullopt;
    }
    for (int attempt = 0; attempt < 96; ++attempt) {
        (void)application.poll(0.0);
        if (auto pixel = PlatformTestAccess::take_gpu_readback(window)) {
            return pixel;
        }
    }
    return std::nullopt;
}

bool wait_for_failure(
    ui::Application& application,
    ui::StandaloneWindow& window,
    std::uint64_t previous) {
    for (int attempt = 0; attempt < 96; ++attempt) {
        (void)application.poll(0.0);
        if (PlatformTestAccess::scene_diagnostics(window).failed_exposes > previous) {
            return true;
        }
    }
    return false;
}

bool wait_for_resource(
    ui::Application& application,
    ui::StandaloneWindow& window) {
    for (int attempt = 0; attempt < 96; ++attempt) {
        (void)application.poll(0.0);
        const auto diagnostics = PlatformTestAccess::scene_diagnostics(window);
        if (diagnostics.scene_valid && diagnostics.render_resource_entries > 0) {
            return true;
        }
    }
    return false;
}

ui::Brush make_shader_brush() {
    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform float gain;
        half4 main(float2) {
            return half4(gain, 0.0, 0.0, 1.0);
        }
    )");
    if (!compiled.ok()) throw std::runtime_error("shader compilation failed");
    ui::ShaderInstance shader{compiled.program};
    if (shader.set_float("gain", 0.75f) != ui::ShaderSetResult::Ok) {
        throw std::runtime_error("shader binding failed");
    }
    return ui::Brush{shader};
}

ui::UI make_ui(const ui::Brush& brush) {
    return ui::UI{ui::Canvas{
        32.0f, 32.0f, [brush](ui::CanvasContext2D& canvas) {
            canvas.fill_rect({0.0f, 0.0f, 32.0f, 32.0f}, brush);
        }}};
}

} // namespace

int main() {
    ui::Application application;
    if (!application.valid()) return fail("application creation failed");

    const auto brush = make_shader_brush();
    auto first_ui = make_ui(brush);
    auto second_ui = make_ui(brush);

    auto first = std::make_unique<ui::StandaloneWindow>(
        application,
        first_ui,
        ui::WindowDesc{.title = "render-resource-a", .size = {32.0f, 32.0f}});
    ui::StandaloneWindow second{
        application,
        second_ui,
        ui::WindowDesc{.title = "render-resource-b", .size = {32.0f, 32.0f}}};
    if (!first->valid() || !second.valid()) return fail("window creation failed");

    if (!read_pixel(application, *first, {8.0f, 8.0f}) ||
        !read_pixel(application, second, {8.0f, 8.0f}) ||
        !wait_for_resource(application, *first) ||
        !wait_for_resource(application, second)) {
        return fail("initial renderer cache did not populate");
    }

    const auto first_before = PlatformTestAccess::scene_diagnostics(*first);
    const auto second_before = PlatformTestAccess::scene_diagnostics(second);
    if (first_before.render_resource_entries == 0 ||
        second_before.render_resource_entries == 0) {
        return fail("resource cache was empty after warm render");
    }

    const auto first_before_recreate =
        PlatformTestAccess::scene_diagnostics(*first);
    const auto second_before_recreate =
        PlatformTestAccess::scene_diagnostics(second);
    if (!PlatformTestAccess::request_context_recreation(*first)) {
        return fail("live context recreation request rejected");
    }
    for (int attempt = 0; attempt < 96; ++attempt) {
        (void)application.poll(0.0);
        const auto diagnostics = PlatformTestAccess::scene_diagnostics(*first);
        if (diagnostics.render_resource_cache_clears >
                first_before_recreate.render_resource_cache_clears &&
            diagnostics.scene_allocations >
                first_before_recreate.scene_allocations) {
            break;
        }
    }
    if (!read_pixel(application, *first, {8.0f, 8.0f}) ||
        !wait_for_resource(application, *first)) {
        return fail("renderer cache did not repopulate after live recreation");
    }
    const auto first_after_recreate =
        PlatformTestAccess::scene_diagnostics(*first);
    const auto second_after_recreate =
        PlatformTestAccess::scene_diagnostics(second);
    if (first_after_recreate.render_resource_cache_clears <=
            first_before_recreate.render_resource_cache_clears ||
        first_after_recreate.scene_allocations <=
            first_before_recreate.scene_allocations ||
        !first_after_recreate.scene_valid ||
        first_after_recreate.render_resource_entries == 0) {
        return fail("live recreation did not clear and rebuild owning cache");
    }
    if (second_after_recreate.render_resource_cache_clears !=
            second_before_recreate.render_resource_cache_clears ||
        second_after_recreate.render_resource_entries !=
            second_before_recreate.render_resource_entries) {
        return fail("live recreation in one view mutated another view cache");
    }

    if (!PlatformTestAccess::inject_scene_fault(
            *first, SceneFaultStage::ConfirmedContextLoss) ||
        !PlatformTestAccess::request_gpu_readback(*first, {8.0f, 8.0f}) ||
        !wait_for_failure(application, *first, first_before.failed_exposes)) {
        return fail("confirmed context loss did not execute");
    }

    const auto first_after_loss = PlatformTestAccess::scene_diagnostics(*first);
    const auto second_after_loss = PlatformTestAccess::scene_diagnostics(second);
    if (first_after_loss.render_resource_cache_clears <=
            first_before.render_resource_cache_clears) {
        return fail("context loss did not clear owning renderer cache");
    }
    if (second_after_loss.render_resource_cache_clears !=
            second_before.render_resource_cache_clears ||
        second_after_loss.render_resource_entries !=
            second_before.render_resource_entries) {
        return fail("context loss in one view mutated another view cache");
    }

    if (!read_pixel(application, *first, {8.0f, 8.0f}) ||
        !wait_for_resource(application, *first)) {
        return fail("renderer cache did not repopulate after context loss");
    }
    const auto first_recovered = PlatformTestAccess::scene_diagnostics(*first);
    if (!first_recovered.scene_valid ||
        first_recovered.render_resource_entries == 0) {
        return fail("recreated renderer did not establish fresh cache state");
    }

    const auto second_pixel = read_pixel(application, second, {8.0f, 8.0f});
    const auto second_final = PlatformTestAccess::scene_diagnostics(second);
    if (!second_pixel || second_pixel->r < 120 ||
        !second_final.scene_valid ||
        second_final.render_resource_entries == 0 ||
        second_final.render_resource_cache_clears !=
            second_before.render_resource_cache_clears) {
        return fail("surviving view did not remain independent");
    }

    const auto second_before_destroy =
        PlatformTestAccess::scene_diagnostics(second);
    first.reset();

    const auto second_after_destroy_pixel =
        read_pixel(application, second, {8.0f, 8.0f});
    const auto second_after_destroy =
        PlatformTestAccess::scene_diagnostics(second);
    if (!second_after_destroy_pixel || second_after_destroy_pixel->r < 120 ||
        !second_after_destroy.scene_valid ||
        second_after_destroy.render_resource_entries == 0 ||
        second_after_destroy.render_resource_cache_clears !=
            second_before_destroy.render_resource_cache_clears) {
        return fail("destroying one view mutated the surviving view cache");
    }

    return 0;
}
