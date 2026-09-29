#include "example_support.hpp"

#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui::detail {

struct PainterLayerFaultAccess {
    static void fail_after_hard_clip(Painter& painter) noexcept {
        painter.layer_fault_point_ = Painter::LayerFaultPoint::AfterHardClip;
    }

    static void fail_after_save_layer(Painter& painter) noexcept {
        painter.layer_fault_point_ = Painter::LayerFaultPoint::AfterSaveLayer;
    }
};

} // namespace ui::detail

namespace {

using LayerResult = decltype(
    std::declval<ui::Painter&>().scoped_layer(ui::Rect{}, ui::PaintOptions{}));

static_assert(std::is_same_v<LayerResult, ui::Painter::StateGuard>);
static_assert(!std::is_copy_constructible_v<ui::Painter::StateGuard>);
static_assert(!std::is_copy_assignable_v<ui::Painter::StateGuard>);
static_assert(!std::is_move_constructible_v<ui::Painter::StateGuard>);
static_assert(!std::is_move_assignable_v<ui::Painter::StateGuard>);
static_assert(std::is_nothrow_destructible_v<ui::Painter::StateGuard>);

ui::Color byte_color(int r, int g, int b, int a = 255) {
    constexpr float scale = 1.0f / 255.0f;
    return {static_cast<float>(r) * scale,
            static_cast<float>(g) * scale,
            static_cast<float>(b) * scale,
            static_cast<float>(a) * scale};
}

bool near_channel(std::uint8_t actual, int expected, int tolerance = 2) {
    const int value = static_cast<int>(actual);
    return value >= expected - tolerance && value <= expected + tolerance;
}

bool near_rgb(ui::Rgba8 pixel, int r, int g, int b, int tolerance = 2) {
    return near_channel(pixel.r, r, tolerance) &&
           near_channel(pixel.g, g, tolerance) &&
           near_channel(pixel.b, b, tolerance) &&
           pixel.a > 250;
}

bool red(ui::Rgba8 pixel) {
    return pixel.r > 220 && pixel.g < 40 && pixel.b < 40 && pixel.a > 220;
}

bool green(ui::Rgba8 pixel) {
    return pixel.g > 220 && pixel.r < 40 && pixel.b < 40 && pixel.a > 220;
}

bool black(ui::Rgba8 pixel) {
    return pixel.r < 8 && pixel.g < 8 && pixel.b < 8 && pixel.a > 220;
}

class PainterProbeComponent final : public ui::Component {
public:
    explicit PainterProbeComponent(std::function<void(ui::Painter&)> draw)
        : draw_(std::move(draw)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {64.0f, 64.0f};
    }

    void paint(ui::PaintContext& context) const override {
        draw_(context.painter());
    }

private:
    std::function<void(ui::Painter&)> draw_;
};

class PainterProbe {
public:
    explicit PainterProbe(std::function<void(ui::Painter&)> draw)
        : draw_(std::move(draw)) {}

    ui::Spec spec() && {
        auto draw = std::move(draw_);
        return ui::Spec{
            [draw = std::move(draw)]() mutable {
                return std::make_unique<PainterProbeComponent>(std::move(draw));
            },
            {}};
    }

private:
    std::function<void(ui::Painter&)> draw_;
};

sk_sp<SkSurface> make_surface() {
    const auto info = SkImageInfo::Make(
        64, 64, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    return SkSurfaces::Raster(info);
}

const char* compositing_contract() {
    // Group opacity: the blue child is opaque inside the isolated layer and
    // therefore replaces the red child before one 50% group composite occurs.
    // Applying 50% independently to both children would leave extra red in the
    // overlap sample and fail this check.
    {
        ui::UI tree{PainterProbe{[](ui::Painter& painter) {
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, byte_color(64, 64, 64));

            ui::PaintOptions options;
            options.opacity = 0.5f;
            auto layer = painter.scoped_layer({8.0f, 8.0f, 48.0f, 48.0f}, options);
            painter.fill_rounded_rect(
                {12.0f, 12.0f, 28.0f, 28.0f}, 0.0f, byte_color(128, 64, 64));
            painter.fill_rounded_rect(
                {28.0f, 12.0f, 24.0f, 28.0f}, 0.0f, byte_color(64, 64, 128));
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        if (!renderer.render(tree)) return "group-opacity render failed";
        if (!near_rgb(renderer.pixel(16, 20), 96, 64, 64)) {
            return "group opacity was not applied once to the isolated red result";
        }
        if (!near_rgb(renderer.pixel(32, 20), 64, 64, 96)) {
            return "group opacity appears to have been applied per child";
        }
        if (!near_rgb(renderer.pixel(2, 2), 64, 64, 64)) {
            return "group opacity changed pixels outside its layer";
        }
    }

    // Multiply is likewise applied once when the completed group is restored.
    // With opaque children the overlap source is exactly the later child before
    // it is multiplied against the parent surface.
    {
        ui::UI tree{PainterProbe{[](ui::Painter& painter) {
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, byte_color(96, 96, 96));

            ui::PaintOptions options;
            options.blend = ui::BlendMode::Multiply;
            auto layer = painter.scoped_layer({8.0f, 8.0f, 48.0f, 48.0f}, options);
            painter.fill_rounded_rect(
                {12.0f, 12.0f, 28.0f, 28.0f}, 0.0f, byte_color(100, 120, 90));
            painter.fill_rounded_rect(
                {28.0f, 12.0f, 24.0f, 28.0f}, 0.0f, byte_color(120, 100, 90));
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        if (!renderer.render(tree)) return "multiply-layer render failed";
        if (!near_rgb(renderer.pixel(16, 20), 38, 45, 34, 3)) {
            return "multiply layer produced the wrong first-child group result";
        }
        if (!near_rgb(renderer.pixel(32, 20), 45, 38, 34, 3)) {
            return "multiply blend appears to have been applied per child";
        }
    }

    return nullptr;
}

const char* hard_bounds_and_transform_contract() {
    // The saveLayer bounds are not trusted as a clip: the red primitive is much
    // larger than the logical layer, but no red pixel may escape the hard bound.
    {
        ui::UI tree{PainterProbe{[](ui::Painter& painter) {
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
            auto layer = painter.scoped_layer({16.0f, 16.0f, 32.0f, 32.0f});
            painter.fill_rounded_rect(
                {-100.0f, -100.0f, 264.0f, 264.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        if (!renderer.render(tree)) return "hard-bounds render failed";
        if (!red(renderer.pixel(32, 32))) return "hard-bounded layer lost its interior";
        if (!green(renderer.pixel(8, 32)) || !green(renderer.pixel(56, 32))) {
            return "hard-bounded layer leaked outside its captured bounds";
        }
    }

    // The boundary is captured under the transform active at layer creation.
    // A later translate moves child content, not the already-established clip.
    {
        ui::UI tree{PainterProbe{[](ui::Painter& painter) {
            auto state = painter.scoped_state();
            painter.translate(16.0f, 8.0f);
            auto layer = painter.scoped_layer({0.0f, 0.0f, 16.0f, 16.0f});
            painter.translate(16.0f, 0.0f);
            painter.fill_rounded_rect(
                {-16.0f, 0.0f, 32.0f, 16.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        if (!renderer.render(tree)) return "transform-capture render failed";
        if (!red(renderer.pixel(20, 16))) return "captured layer transform lost valid content";
        if (!black(renderer.pixel(36, 16))) return "later transform moved the captured layer bound";
    }

    return nullptr;
}

const char* option_and_invalid_geometry_contract() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    // PaintOptions canonicalization is shared with ordinary draw operations.
    {
        ui::UI tree{PainterProbe{[nan](ui::Painter& painter) {
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, byte_color(64, 64, 64));

            ui::PaintOptions hidden;
            hidden.opacity = -10.0f;
            {
                auto layer = painter.scoped_layer({0.0f, 0.0f, 20.0f, 64.0f}, hidden);
                painter.fill_rounded_rect(
                    {0.0f, 0.0f, 20.0f, 64.0f}, 0.0f, byte_color(128, 64, 64));
            }

            ui::PaintOptions opaque;
            opaque.opacity = 10.0f;
            {
                auto layer = painter.scoped_layer({22.0f, 0.0f, 20.0f, 64.0f}, opaque);
                painter.fill_rounded_rect(
                    {22.0f, 0.0f, 20.0f, 64.0f}, 0.0f, byte_color(128, 64, 64));
            }

            ui::PaintOptions non_finite;
            non_finite.opacity = nan;
            {
                auto layer = painter.scoped_layer({44.0f, 0.0f, 20.0f, 64.0f}, non_finite);
                painter.fill_rounded_rect(
                    {44.0f, 0.0f, 20.0f, 64.0f}, 0.0f, byte_color(64, 64, 128));
            }
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        if (!renderer.render(tree)) return "PaintOptions canonicalization render failed";
        if (!near_rgb(renderer.pixel(10, 20), 64, 64, 64)) {
            return "negative layer opacity did not clamp to zero";
        }
        if (!near_rgb(renderer.pixel(30, 20), 128, 64, 64)) {
            return "oversized layer opacity did not clamp to one";
        }
        if (!near_rgb(renderer.pixel(52, 20), 64, 64, 128)) {
            return "non-finite layer opacity did not resolve to one";
        }
    }

    // Empty, inverted and non-finite bounds are balanced empty clip scopes.
    // Drawing inside each scope must contribute no pixels.
    {
        ui::UI tree{PainterProbe{[nan, inf](ui::Painter& painter) {
            {
                auto layer = painter.scoped_layer({8.0f, 8.0f, 0.0f, 20.0f});
                painter.fill_rounded_rect(
                    {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            }
            {
                auto layer = painter.scoped_layer({8.0f, 8.0f, -4.0f, 20.0f});
                painter.fill_rounded_rect(
                    {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            }
            {
                auto layer = painter.scoped_layer({nan, 0.0f, 20.0f, 20.0f});
                painter.fill_rounded_rect(
                    {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            }
            {
                auto layer = painter.scoped_layer({0.0f, 0.0f, inf, 20.0f});
                painter.fill_rounded_rect(
                    {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            }
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 4.0f, 4.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        if (!renderer.render(tree)) return "invalid-bounds render failed";
        if (!green(renderer.pixel(2, 2))) return "invalid layer scope poisoned later drawing";
        if (!black(renderer.pixel(32, 32))) return "invalid layer bounds became unbounded output";
    }

    return nullptr;
}

const char* stack_and_recovery_contract() {
    auto surface = make_surface();
    if (!surface) return "failed to create stack-contract surface";
    auto* canvas = surface->getCanvas();
    if (!canvas) return "stack-contract surface has no canvas";

    const int baseline_save_count = canvas->getSaveCount();
    ui::Painter painter{*canvas};

    {
        auto layer = painter.scoped_layer({4.0f, 4.0f, 24.0f, 24.0f});
        if (painter.save_depth() != 2 || canvas->getSaveCount() != baseline_save_count + 2) {
            return "valid layer did not protect both private backend frames";
        }

        painter.save();
        if (painter.save_depth() != 3) return "manual save inside layer was not accepted";
        painter.restore();
        if (painter.save_depth() != 2) return "manual restore inside layer crossed the wrong frame";

#if defined(NDEBUG)
        // In release builds restore() must be a safe no-op at the protected
        // floor. Debug builds deliberately assert on this contract violation.
        painter.restore();
        if (painter.save_depth() != 2) return "restore crossed the protected layer floor";
#endif

        {
            auto clip = painter.scoped_clip({6.0f, 6.0f, 12.0f, 12.0f});
            if (painter.save_depth() != 3) return "clip-inside-layer depth mismatch";
            {
                auto state = painter.scoped_state();
                if (painter.save_depth() != 4) return "state-inside-layer depth mismatch";
            }
            if (painter.save_depth() != 3) return "nested state did not restore inside layer";
        }
        if (painter.save_depth() != 2) return "nested clip did not restore inside layer";
    }
    if (painter.save_depth() != 0 || canvas->getSaveCount() != baseline_save_count) {
        return "layer guard did not restore exact pre-scope stack state";
    }

    {
        auto outer = painter.scoped_layer({2.0f, 2.0f, 40.0f, 40.0f});
        if (painter.save_depth() != 2) return "outer layer depth mismatch";
        {
            auto inner = painter.scoped_layer({6.0f, 6.0f, 20.0f, 20.0f});
            if (painter.save_depth() != 4) return "nested layer depth mismatch";
        }
        if (painter.save_depth() != 2) return "nested layer did not restore outer layer depth";
    }
    if (painter.save_depth() != 0) return "nested layers leaked stack depth";

    {
        auto clip = painter.scoped_clip({0.0f, 0.0f, 32.0f, 32.0f});
        if (painter.save_depth() != 1) return "outer clip depth mismatch";
        {
            auto layer = painter.scoped_layer({4.0f, 4.0f, 20.0f, 20.0f});
            if (painter.save_depth() != 3) return "layer-inside-clip depth mismatch";
        }
        if (painter.save_depth() != 1) return "layer-inside-clip did not restore clip depth";
    }
    if (painter.save_depth() != 0) return "outer clip leaked after layer nesting";

    const auto early_return = [&painter, canvas, baseline_save_count]() -> const char* {
        auto layer = painter.scoped_layer({4.0f, 4.0f, 20.0f, 20.0f});
        if (painter.save_depth() != 2 || canvas->getSaveCount() != baseline_save_count + 2) {
            return "early-return layer was not fully active";
        }
        return nullptr;
    };
    if (const char* error = early_return()) return error;
    if (painter.save_depth() != 0 || canvas->getSaveCount() != baseline_save_count) {
        return "early return did not restore layer stack";
    }

    std::string propagated;
    try {
        auto layer = painter.scoped_layer({4.0f, 4.0f, 20.0f, 20.0f});
        if (painter.save_depth() != 2) return "exception-unwind layer depth mismatch";
        throw std::runtime_error("t076 unwind");
    } catch (const std::runtime_error& error) {
        propagated = error.what();
    }
    if (propagated != "t076 unwind") return "layer exception did not propagate";
    if (painter.save_depth() != 0 || canvas->getSaveCount() != baseline_save_count) {
        return "exception unwind did not restore Painter and SkCanvas stacks";
    }

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const auto invalid_is_clip_only = [&](ui::Rect bounds) -> bool {
        {
            auto layer = painter.scoped_layer(bounds);
            if (painter.save_depth() != 1 ||
                canvas->getSaveCount() != baseline_save_count + 1) {
                return false;
            }
        }
        return painter.save_depth() == 0 && canvas->getSaveCount() == baseline_save_count;
    };
    if (!invalid_is_clip_only({0.0f, 0.0f, 0.0f, 10.0f}) ||
        !invalid_is_clip_only({0.0f, 0.0f, -1.0f, 10.0f}) ||
        !invalid_is_clip_only({nan, 0.0f, 10.0f, 10.0f}) ||
        !invalid_is_clip_only({0.0f, 0.0f, inf, 10.0f})) {
        return "invalid bounds opened a backend layer instead of clip-only empty output";
    }

    bool threw = false;
    ui::detail::PainterLayerFaultAccess::fail_after_hard_clip(painter);
    try {
        auto layer = painter.scoped_layer({4.0f, 4.0f, 20.0f, 20.0f});
        (void)layer;
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    if (!threw || painter.save_depth() != 0 || canvas->getSaveCount() != baseline_save_count) {
        return "failure after hard clip did not roll back exact stack state";
    }

    threw = false;
    ui::detail::PainterLayerFaultAccess::fail_after_save_layer(painter);
    try {
        auto layer = painter.scoped_layer({4.0f, 4.0f, 20.0f, 20.0f});
        (void)layer;
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    if (!threw || painter.save_depth() != 0 || canvas->getSaveCount() != baseline_save_count) {
        return "failure after saveLayer did not roll back both private frames";
    }

    // Recovery is proven by a later normal layer and draw on the same Painter.
    {
        auto layer = painter.scoped_layer({4.0f, 4.0f, 20.0f, 20.0f});
        painter.fill_rounded_rect(
            {4.0f, 4.0f, 10.0f, 10.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
    }
    if (painter.save_depth() != 0 || canvas->getSaveCount() != baseline_save_count) {
        return "later valid layer failed after injected construction failure";
    }

    return nullptr;
}

const char* independent_painter_contract() {
    auto surface_b = make_surface();
    if (!surface_b || !surface_b->getCanvas()) return "failed to create Painter B surface";
    ui::Painter painter_b{*surface_b->getCanvas()};

    {
        auto layer_b = painter_b.scoped_layer({0.0f, 0.0f, 32.0f, 32.0f});
        if (painter_b.save_depth() != 2) return "Painter B layer depth mismatch";

        {
            auto surface_a = make_surface();
            if (!surface_a || !surface_a->getCanvas()) return "failed to create Painter A surface";
            ui::Painter painter_a{*surface_a->getCanvas()};
            {
                auto layer_a = painter_a.scoped_layer({0.0f, 0.0f, 24.0f, 24.0f});
                if (painter_a.save_depth() != 2 || painter_b.save_depth() != 2) {
                    return "independent layer scopes shared Painter state";
                }
                {
                    auto nested_a = painter_a.scoped_layer({4.0f, 4.0f, 8.0f, 8.0f});
                    if (painter_a.save_depth() != 4 || painter_b.save_depth() != 2) {
                        return "nested Painter A layer changed Painter B state";
                    }
                }
            }
            if (painter_a.save_depth() != 0) return "Painter A leaked layer state";
        }

        if (painter_b.save_depth() != 2) {
            return "destroying Painter A changed live Painter B layer state";
        }
    }

    if (painter_b.save_depth() != 0) return "Painter B leaked layer state";
    return nullptr;
}

class LayersDemoComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {420.0f, 160.0f};
    }

    void paint(ui::PaintContext& context) const override {
        auto& painter = context.painter();
        const auto bounds = context.bounds();
        painter.fill_rounded_rect(bounds, 12.0f, ui::colors::panel);

        // Group opacity: overlapping children are first composed together, then
        // the complete result is faded once.
        const ui::Rect opacity_bounds{bounds.x + 20.0f, bounds.y + 22.0f, 105.0f, 100.0f};
        {
            ui::PaintOptions options;
            options.opacity = 0.55f;
            auto layer = painter.scoped_layer(opacity_bounds, options);
            painter.circle({opacity_bounds.x + 40.0f, opacity_bounds.y + 50.0f},
                           36.0f, {1.0f, 0.30f, 0.18f, 1.0f});
            painter.circle({opacity_bounds.x + 66.0f, opacity_bounds.y + 50.0f},
                           36.0f, {0.20f, 0.45f, 1.0f, 1.0f});
        }

        // Group blend: the whole pair is multiplied against one base surface.
        const ui::Rect blend_bounds{bounds.x + 158.0f, bounds.y + 22.0f, 105.0f, 100.0f};
        painter.fill_rounded_rect(blend_bounds, 10.0f, {0.70f, 0.55f, 0.32f, 1.0f});
        {
            ui::PaintOptions options;
            options.blend = ui::BlendMode::Multiply;
            auto layer = painter.scoped_layer(blend_bounds, options);
            painter.circle({blend_bounds.x + 40.0f, blend_bounds.y + 50.0f},
                           36.0f, {0.80f, 0.95f, 0.55f, 1.0f});
            painter.circle({blend_bounds.x + 66.0f, blend_bounds.y + 50.0f},
                           36.0f, {0.55f, 0.75f, 0.95f, 1.0f});
        }

        // Hard bound: this oversized circle cannot escape the finite layer.
        const ui::Rect hard_bounds{bounds.x + 300.0f, bounds.y + 22.0f, 96.0f, 100.0f};
        painter.fill_rounded_rect(hard_bounds, 8.0f, {0.12f, 0.12f, 0.14f, 1.0f});
        {
            auto layer = painter.scoped_layer(hard_bounds);
            painter.circle({hard_bounds.x + 48.0f, hard_bounds.y + 50.0f},
                           72.0f, ui::colors::accent);
        }

        painter.text({opacity_bounds.x + opacity_bounds.w * 0.5f, bounds.y + 146.0f},
                     "Group opacity", 11.0f, ui::colors::textMuted, ui::TextAlign::Center);
        painter.text({blend_bounds.x + blend_bounds.w * 0.5f, bounds.y + 146.0f},
                     "Multiply", 11.0f, ui::colors::textMuted, ui::TextAlign::Center);
        painter.text({hard_bounds.x + hard_bounds.w * 0.5f, bounds.y + 146.0f},
                     "Hard bounds", 11.0f, ui::colors::textMuted, ui::TextAlign::Center);
    }
};

class LayersDemo {
public:
    ui::Spec spec() && {
        return ui::Spec{
            [] { return std::make_unique<LayersDemoComponent>(); },
            {}};
    }
};

} // namespace

int main(int argc, char** argv) {
    auto make_ui = [] {
        return std::make_unique<ui::UI>(LayersDemo{});
    };

    if (example::self_test_requested(argc, argv)) {
        if (const char* error = compositing_contract()) return example::fail(error);
        if (const char* error = hard_bounds_and_transform_contract()) return example::fail(error);
        if (const char* error = option_and_invalid_geometry_contract()) return example::fail(error);
        if (const char* error = stack_and_recovery_contract()) return example::fail(error);
        if (const char* error = independent_painter_contract()) return example::fail(error);

        auto tree = make_ui();
        ui::HeadlessRenderer renderer{{420.0f, 160.0f}, 1.0f};
        if (!renderer.render(*tree)) return example::fail("demo headless render failed");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree,
                               "NativeUI T076 - Bounded Layers",
                               {460.0f, 220.0f});
}
