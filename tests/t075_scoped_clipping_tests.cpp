#include "test_support.hpp"

#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {

using RectClipResult = decltype(std::declval<ui::Painter&>().scoped_clip(ui::Rect{}));
using RoundedClipResult = decltype(std::declval<ui::Painter&>().scoped_clip(ui::Rect{}, 4.0f));
using PathClipResult = decltype(std::declval<ui::Painter&>().scoped_clip(std::declval<const ui::Path&>()));

static_assert(std::is_same_v<RectClipResult, ui::Painter::StateGuard>);
static_assert(std::is_same_v<RoundedClipResult, ui::Painter::StateGuard>);
static_assert(std::is_same_v<PathClipResult, ui::Painter::StateGuard>);
static_assert(!std::is_copy_constructible_v<ui::Painter::StateGuard>);
static_assert(!std::is_copy_assignable_v<ui::Painter::StateGuard>);
static_assert(!std::is_move_constructible_v<ui::Painter::StateGuard>);
static_assert(!std::is_move_assignable_v<ui::Painter::StateGuard>);
static_assert(std::is_nothrow_destructible_v<ui::Painter::StateGuard>);

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

bool red(ui::Rgba8 pixel) {
    return pixel.r > 220 && pixel.g < 40 && pixel.b < 40 && pixel.a > 220;
}

bool green(ui::Rgba8 pixel) {
    return pixel.g > 220 && pixel.r < 40 && pixel.b < 40 && pixel.a > 220;
}

bool black(ui::Rgba8 pixel) {
    return pixel.r < 8 && pixel.g < 8 && pixel.b < 8 && pixel.a > 220;
}

void scoped_clip_pixels() {
    // Nested Rect + rounded-Rect probes. The rounded corner sample proves this
    // is not accidentally implemented as a rectangular inner clip.
    {
        ui::UI tree{PainterProbe{[](ui::Painter& painter) {
            {
                auto outer = painter.scoped_clip({8.0f, 8.0f, 48.0f, 48.0f});
                painter.fill_rounded_rect(
                    {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
                {
                    auto inner = painter.scoped_clip({16.0f, 16.0f, 32.0f, 32.0f}, 8.0f);
                    painter.fill_rounded_rect(
                        {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
                }
            }
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 4.0f, 4.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(green(renderer.pixel(2, 2)));
        NUI_CHECK(red(renderer.pixel(10, 10)));
        NUI_CHECK(red(renderer.pixel(17, 17)));
        NUI_CHECK(green(renderer.pixel(24, 24)));
        NUI_CHECK(red(renderer.pixel(52, 52)));
        NUI_CHECK(black(renderer.pixel(60, 60)));
    }

    // Path clipping nests through the same Painter stack and intersects the
    // inherited rectangular clip rather than escaping it.
    {
        ui::Path triangle;
        triangle.move_to({32.0f, 8.0f})
                .line_to({56.0f, 56.0f})
                .line_to({8.0f, 56.0f})
                .close();
        ui::UI tree{PainterProbe{[triangle](ui::Painter& painter) {
            auto outer = painter.scoped_clip({12.0f, 12.0f, 40.0f, 40.0f});
            auto inner = painter.scoped_clip(triangle);
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(32, 32)));
        NUI_CHECK(black(renderer.pixel(10, 50)));
        NUI_CHECK(black(renderer.pixel(54, 50)));
        NUI_CHECK(black(renderer.pixel(4, 4)));
    }

    // Translation is captured at scope creation and restored by scoped_state().
    {
        ui::UI tree{PainterProbe{[](ui::Painter& painter) {
            {
                auto state = painter.scoped_state();
                painter.translate(12.0f, 4.0f);
                auto clip = painter.scoped_clip({0.0f, 0.0f, 12.0f, 12.0f});
                painter.fill_rounded_rect(
                    {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            }
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 4.0f, 4.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(green(renderer.pixel(2, 2)));
        NUI_CHECK(red(renderer.pixel(16, 8)));
        NUI_CHECK(black(renderer.pixel(6, 8)));
        NUI_CHECK(black(renderer.pixel(28, 8)));
    }
}

void transformed_clip_semantics() {
    // Scale applies to clip geometry at scope creation.
    {
        ui::UI tree{PainterProbe{[](ui::Painter& painter) {
            auto state = painter.scoped_state();
            painter.scale(2.0f);
            auto clip = painter.scoped_clip({4.0f, 4.0f, 8.0f, 8.0f});
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 32.0f, 32.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(12, 12)));
        NUI_CHECK(black(renderer.pixel(4, 12)));
        NUI_CHECK(black(renderer.pixel(28, 12)));
    }

    // Rotation produces a rotated thin clip, not an axis-aligned fallback.
    {
        ui::UI tree{PainterProbe{[](ui::Painter& painter) {
            auto state = painter.scoped_state();
            painter.translate(32.0f, 32.0f);
            painter.rotate(ui::kPi * 0.25f);
            auto clip = painter.scoped_clip({-12.0f, -4.0f, 24.0f, 8.0f});
            painter.fill_rounded_rect(
                {-32.0f, -32.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(32, 32)));
        NUI_CHECK(red(renderer.pixel(39, 39)));
        NUI_CHECK(black(renderer.pixel(42, 32)));
    }

    // concat() uses the same current-transform semantics as translate().
    {
        ui::UI tree{PainterProbe{[](ui::Painter& painter) {
            auto state = painter.scoped_state();
            painter.concat(ui::Transform2D::translation(20.0f, 8.0f));
            auto clip = painter.scoped_clip({0.0f, 0.0f, 12.0f, 12.0f});
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 32.0f, 32.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(24, 12)));
        NUI_CHECK(black(renderer.pixel(8, 12)));
        NUI_CHECK(black(renderer.pixel(36, 12)));
    }
}

void rounded_radius_canonicalization() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    const auto check_rectangular_fallback = [](float radius) {
        ui::UI tree{PainterProbe{[radius](ui::Painter& painter) {
            auto clip = painter.scoped_clip({8.0f, 8.0f, 16.0f, 16.0f}, radius);
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(9, 9)));
        NUI_CHECK(red(renderer.pixel(20, 20)));
        NUI_CHECK(black(renderer.pixel(4, 4)));
    };

    check_rectangular_fallback(-4.0f);
    check_rectangular_fallback(nan);
    check_rectangular_fallback(inf);

    // For a 16x8 rect the maximum canonical radius is exactly 4. An oversized
    // radius must therefore retain the center and round away the extreme corner.
    {
        ui::UI tree{PainterProbe{[](ui::Painter& painter) {
            auto clip = painter.scoped_clip({8.0f, 8.0f, 16.0f, 8.0f}, 1000.0f);
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(16, 12)));
        NUI_CHECK(!red(renderer.pixel(8, 8)));
        NUI_CHECK(black(renderer.pixel(4, 4)));
    }
}

void invalid_geometry_is_empty() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    // Invalid/inverted/non-finite rectangles create a balanced empty clip.
    {
        ui::UI tree{PainterProbe{[nan, inf](ui::Painter& painter) {
            {
                auto clip = painter.scoped_clip({nan, 0.0f, 20.0f, 20.0f});
                painter.fill_rounded_rect(
                    {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            }
            {
                auto clip = painter.scoped_clip({8.0f, 8.0f, -10.0f, 20.0f}, 4.0f);
                painter.fill_rounded_rect(
                    {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            }
            {
                auto clip = painter.scoped_clip({0.0f, 0.0f, inf, 20.0f});
                painter.fill_rounded_rect(
                    {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            }
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 4.0f, 4.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(green(renderer.pixel(2, 2)));
        NUI_CHECK(black(renderer.pixel(32, 32)));
    }

    // Empty and non-finite Paths both create empty effective clips.
    {
        ui::Path empty;
        ui::Path invalid;
        invalid.move_to({nan, 0.0f}).line_to({20.0f, 20.0f});
        ui::UI tree{PainterProbe{[empty, invalid](ui::Painter& painter) {
            {
                auto clip = painter.scoped_clip(empty);
                painter.fill_rounded_rect(
                    {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            }
            {
                auto clip = painter.scoped_clip(invalid);
                painter.fill_rounded_rect(
                    {0.0f, 0.0f, 64.0f, 64.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            }
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 4.0f, 4.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
        }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(green(renderer.pixel(2, 2)));
        NUI_CHECK(black(renderer.pixel(32, 32)));
    }
}

sk_sp<SkSurface> make_surface() {
    const auto info = SkImageInfo::Make(
        64, 64, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    return SkSurfaces::Raster(info);
}

void stack_lifetime_contract() {
    auto surface = make_surface();
    NUI_CHECK(surface != nullptr);
    auto* canvas = surface->getCanvas();
    NUI_CHECK(canvas != nullptr);
    ui::Painter painter{*canvas};

    int depth_during_throw = -1;
    std::string propagated;
    try {
        auto clip = painter.scoped_clip({4.0f, 4.0f, 20.0f, 20.0f});
        depth_during_throw = painter.save_depth();
        throw std::runtime_error("t075 unwind");
    } catch (const std::runtime_error& error) {
        propagated = error.what();
    }
    NUI_CHECK(propagated == "t075 unwind");
    NUI_CHECK(depth_during_throw == 1);
    NUI_CHECK(painter.save_depth() == 0);

    // Early return follows the same lexical teardown path.
    const auto early_return = [&painter] {
        auto clip = painter.scoped_clip({4.0f, 4.0f, 20.0f, 20.0f});
        NUI_CHECK(painter.save_depth() == 1);
        return;
    };
    early_return();
    NUI_CHECK(painter.save_depth() == 0);

    // Manual state outside a scoped clip, with a nested balanced manual save.
    painter.save();
    NUI_CHECK(painter.save_depth() == 1);
    {
        auto clip = painter.scoped_clip({4.0f, 4.0f, 20.0f, 20.0f});
        NUI_CHECK(painter.save_depth() == 2);
        painter.save();
        NUI_CHECK(painter.save_depth() == 3);
        painter.restore();
        NUI_CHECK(painter.save_depth() == 2);
    }
    NUI_CHECK(painter.save_depth() == 1);
    painter.restore();
    NUI_CHECK(painter.save_depth() == 0);

    // Scoped state outside the legacy manual clip API.
    {
        auto state = painter.scoped_state();
        NUI_CHECK(painter.save_depth() == 1);
        painter.push_clip({0.0f, 0.0f, 8.0f, 8.0f});
        NUI_CHECK(painter.save_depth() == 2);
        painter.pop_clip();
        NUI_CHECK(painter.save_depth() == 1);
    }
    NUI_CHECK(painter.save_depth() == 0);

    // Legacy manual clip outside a new scoped clip.
    painter.push_clip({0.0f, 0.0f, 16.0f, 16.0f});
    NUI_CHECK(painter.save_depth() == 1);
    {
        auto clip = painter.scoped_clip({2.0f, 2.0f, 8.0f, 8.0f});
        NUI_CHECK(painter.save_depth() == 2);
    }
    NUI_CHECK(painter.save_depth() == 1);
    painter.pop_clip();
    NUI_CHECK(painter.save_depth() == 0);

    // New scoped clip outside the legacy manual clip API.
    {
        auto clip = painter.scoped_clip({0.0f, 0.0f, 16.0f, 16.0f});
        NUI_CHECK(painter.save_depth() == 1);
        painter.push_clip({2.0f, 2.0f, 8.0f, 8.0f});
        NUI_CHECK(painter.save_depth() == 2);
        painter.pop_clip();
        NUI_CHECK(painter.save_depth() == 1);
    }
    NUI_CHECK(painter.save_depth() == 0);

    // Legacy manual clip outside a scoped state token.
    painter.push_clip({0.0f, 0.0f, 8.0f, 8.0f});
    NUI_CHECK(painter.save_depth() == 1);
    {
        auto state = painter.scoped_state();
        NUI_CHECK(painter.save_depth() == 2);
    }
    NUI_CHECK(painter.save_depth() == 1);
    painter.pop_clip();
    NUI_CHECK(painter.save_depth() == 0);
}

void independent_painters_are_isolated() {
    auto surface_b = make_surface();
    NUI_CHECK(surface_b != nullptr);
    auto* canvas_b = surface_b->getCanvas();
    NUI_CHECK(canvas_b != nullptr);
    ui::Painter painter_b{*canvas_b};

    {
        auto guard_b = painter_b.scoped_clip({0.0f, 0.0f, 32.0f, 32.0f});
        NUI_CHECK(painter_b.save_depth() == 1);

        {
            auto surface_a = make_surface();
            NUI_CHECK(surface_a != nullptr);
            auto* canvas_a = surface_a->getCanvas();
            NUI_CHECK(canvas_a != nullptr);
            ui::Painter painter_a{*canvas_a};
            auto guard_a = painter_a.scoped_clip({0.0f, 0.0f, 24.0f, 24.0f});
            NUI_CHECK(painter_a.save_depth() == 1);
            NUI_CHECK(painter_b.save_depth() == 1);
            {
                auto nested_a = painter_a.scoped_clip({4.0f, 4.0f, 8.0f, 8.0f});
                NUI_CHECK(painter_a.save_depth() == 2);
                NUI_CHECK(painter_b.save_depth() == 1);
            }
            NUI_CHECK(painter_a.save_depth() == 1);
        }

        // Painter A and all of its scopes have been destroyed. Painter B's live
        // frame is unaffected and remains valid until its own lexical exit.
        NUI_CHECK(painter_b.save_depth() == 1);
    }
    NUI_CHECK(painter_b.save_depth() == 0);
}

void suite() {
    scoped_clip_pixels();
    transformed_clip_semantics();
    rounded_radius_canonicalization();
    invalid_geometry_is_empty();
    stack_lifetime_contract();
    independent_painters_are_isolated();
}

} // namespace

int main() { return test::run("t075 scoped clipping", &suite); }
