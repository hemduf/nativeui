#include "test_support.hpp"

#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkImageFilters.h"

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace allocation_probe {

std::size_t allocation_count{};

void* allocate(std::size_t size) {
    ++allocation_count;
    if (void* pointer = std::malloc(size == 0 ? 1 : size)) return pointer;
    throw std::bad_alloc{};
}

} // namespace allocation_probe

void* operator new(std::size_t size) { return allocation_probe::allocate(size); }
void* operator new[](std::size_t size) { return allocation_probe::allocate(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

namespace ui::detail {

struct EffectTestAccess {
    static float sigma_x(const Effect& effect) noexcept { return effect.sigma_x_; }
    static float sigma_y(const Effect& effect) noexcept { return effect.sigma_y_; }
};

struct PainterEffectFaultAccess {
    static void fail_before_materialization(Painter& painter) noexcept {
        painter.layer_fault_point_ = Painter::LayerFaultPoint::BeforeEffectMaterialization;
    }

    static void fail_after_output_clip(Painter& painter) noexcept {
        painter.layer_fault_point_ = Painter::LayerFaultPoint::AfterEffectOutputClip;
    }

    static void fail_after_save_layer(Painter& painter) noexcept {
        painter.layer_fault_point_ = Painter::LayerFaultPoint::AfterEffectSaveLayer;
    }

    static void fail_after_source_clip(Painter& painter) noexcept {
        painter.layer_fault_point_ = Painter::LayerFaultPoint::AfterEffectSourceClip;
    }

    static void clear(Painter& painter) noexcept {
        painter.layer_fault_point_ = Painter::LayerFaultPoint::None;
    }

    static bool device_output_bounds(Painter& painter,
                                     Rect source,
                                     const Effect& effect,
                                     SkIRect& output) {
        if (!Painter::valid_clip_rect(source)) return false;
        const SkRect source_bounds = Painter::to_sk_rect(source);
        SkIRect device_source;
        if (!painter.effect_source_device_bounds(source_bounds, device_source)) return false;

        auto filter = SkImageFilters::Blur(
            EffectTestAccess::sigma_x(effect),
            EffectTestAccess::sigma_y(effect),
            SkTileMode::kDecal,
            nullptr);
        return filter &&
               painter.effect_filter_output_bounds(*filter, device_source, output);
    }
};

} // namespace ui::detail

namespace {

using EffectLayerResult = decltype(
    std::declval<ui::Painter&>().scoped_layer(
        ui::Rect{}, ui::Effect::gaussian_blur(1.0f, 1.0f), ui::PaintOptions{}));

static_assert(std::is_same_v<EffectLayerResult, ui::Painter::StateGuard>);
static_assert(noexcept(ui::Effect::gaussian_blur(1.0f, 1.0f)));
static_assert(std::is_nothrow_copy_constructible_v<ui::Effect>);
static_assert(std::is_nothrow_copy_assignable_v<ui::Effect>);
static_assert(std::is_nothrow_move_constructible_v<ui::Effect>);
static_assert(std::is_nothrow_move_assignable_v<ui::Effect>);
static_assert(std::is_nothrow_destructible_v<ui::Effect>);

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

std::vector<std::uint8_t> render(std::function<void(ui::Painter&)> draw) {
    ui::UI tree{PainterProbe{std::move(draw)}};
    ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    return renderer.rgba_pixels();
}

ui::Rgba8 pixel(const std::vector<std::uint8_t>& pixels, int x, int y) {
    const auto offset = static_cast<std::size_t>((y * 64 + x) * 4);
    return {pixels[offset],
            pixels[offset + 1],
            pixels[offset + 2],
            pixels[offset + 3]};
}

sk_sp<SkSurface> make_surface() {
    const auto info = SkImageInfo::Make(
        64, 64, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    return SkSurfaces::Raster(info);
}

void effect_value_contract() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    const auto check = [](float sigma_x,
                          float sigma_y,
                          float expected_x,
                          float expected_y) {
        const auto effect = ui::Effect::gaussian_blur(sigma_x, sigma_y);
        NUI_CHECK(ui::detail::EffectTestAccess::sigma_x(effect) == expected_x);
        NUI_CHECK(ui::detail::EffectTestAccess::sigma_y(effect) == expected_y);
    };

    // Canonicalization is independent and exact on both axes.
    check(-2.0f, 1.25f, 0.0f, 1.25f);
    check(1.25f, -2.0f, 1.25f, 0.0f);
    check(nan, 1.25f, 0.0f, 1.25f);
    check(1.25f, nan, 1.25f, 0.0f);
    check(inf, 1.25f, 0.0f, 1.25f);
    check(1.25f, inf, 1.25f, 0.0f);
    check(65.0f, 1.25f, 64.0f, 1.25f);
    check(1.25f, 65.0f, 1.25f, 64.0f);
    check(0.0f, 64.0f, 0.0f, 64.0f);
    check(64.0f, 0.0f, 64.0f, 0.0f);
    check(1.25f, 2.5f, 1.25f, 2.5f);

    // Include destruction in the zero-allocation proof by checking only after
    // every temporary Effect has left scope.
    const auto before = allocation_probe::allocation_count;
    {
        const auto a = ui::Effect::gaussian_blur(3.0f, 4.0f);
        const auto b = a;
        auto c = b;
        c = a;
        auto d = std::move(c);
        auto e = ui::Effect::gaussian_blur(1.0f, 1.0f);
        e = std::move(d);
        (void)e;
    }
    NUI_CHECK(allocation_probe::allocation_count == before);
}

void no_op_and_invalid_do_not_materialize() {
    auto surface = make_surface();
    NUI_CHECK(surface && surface->getCanvas());
    auto* canvas = surface->getCanvas();
    ui::Painter painter{*canvas};
    const int baseline = canvas->getSaveCount();
    const auto blur = ui::Effect::gaussian_blur(2.0f, 2.0f);

    ui::detail::PainterEffectFaultAccess::fail_before_materialization(painter);
    {
        auto layer = painter.scoped_layer(
            {8.0f, 8.0f, 24.0f, 24.0f}, ui::Effect::gaussian_blur(0.0f, 0.0f));
        NUI_CHECK(painter.save_depth() == 2);
    }
    NUI_CHECK(painter.save_depth() == 0);
    NUI_CHECK(canvas->getSaveCount() == baseline);

    // The seam is still armed, proving the no-op path did not reach materialization.
    bool threw = false;
    try {
        auto layer = painter.scoped_layer({8.0f, 8.0f, 24.0f, 24.0f}, blur);
        (void)layer;
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(painter.save_depth() == 0);
    NUI_CHECK(canvas->getSaveCount() == baseline);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const ui::Rect invalid_bounds[] = {
        {8.0f, 8.0f, 0.0f, 24.0f},
        {8.0f, 8.0f, -1.0f, 24.0f},
        {nan, 8.0f, 24.0f, 24.0f},
        {8.0f, 8.0f, inf, 24.0f},
    };

    for (const auto bounds : invalid_bounds) {
        ui::detail::PainterEffectFaultAccess::fail_before_materialization(painter);
        {
            auto empty = painter.scoped_layer(bounds, blur);
            NUI_CHECK(painter.save_depth() == 1);
        }
        NUI_CHECK(painter.save_depth() == 0);
        NUI_CHECK(canvas->getSaveCount() == baseline);

        bool seam_still_armed = false;
        try {
            auto layer = painter.scoped_layer({8.0f, 8.0f, 24.0f, 24.0f}, blur);
            (void)layer;
        } catch (const std::bad_alloc&) {
            seam_still_armed = true;
        }
        NUI_CHECK(seam_still_armed);
        NUI_CHECK(painter.save_depth() == 0);
        NUI_CHECK(canvas->getSaveCount() == baseline);

        {
            auto recovered = painter.scoped_layer({8.0f, 8.0f, 24.0f, 24.0f}, blur);
            painter.fill_rounded_rect(
                {12.0f, 12.0f, 4.0f, 4.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
        }
        NUI_CHECK(painter.save_depth() == 0);
        NUI_CHECK(canvas->getSaveCount() == baseline);
    }
}

void zero_blur_is_pixel_equivalent() {
    const auto plain = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.1f, 0.1f, 0.12f, 1.0f});
        auto layer = painter.scoped_layer({16.0f, 16.0f, 32.0f, 32.0f});
        painter.fill_rounded_rect({20.0f, 20.0f, 24.0f, 24.0f}, 0.0f,
                                  {0.8f, 0.2f, 0.1f, 1.0f});
    });

    const auto filtered = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.1f, 0.1f, 0.12f, 1.0f});
        auto layer = painter.scoped_layer(
            {16.0f, 16.0f, 32.0f, 32.0f}, ui::Effect::gaussian_blur(0.0f, 0.0f));
        painter.fill_rounded_rect({20.0f, 20.0f, 24.0f, 24.0f}, 0.0f,
                                  {0.8f, 0.2f, 0.1f, 1.0f});
    });

    NUI_CHECK(plain == filtered);
}

void halo_source_clip_and_transparent_edges() {
    const auto clean = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.0f, 0.0f, 0.0f, 1.0f});
        auto layer = painter.scoped_layer(
            {24.0f, 24.0f, 16.0f, 16.0f}, ui::Effect::gaussian_blur(3.0f, 3.0f));
        painter.fill_rounded_rect({24.0f, 24.0f, 16.0f, 16.0f}, 0.0f,
                                  {1.0f, 1.0f, 1.0f, 1.0f});
    });

    const auto polluted = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.0f, 0.0f, 0.0f, 1.0f});
        auto layer = painter.scoped_layer(
            {24.0f, 24.0f, 16.0f, 16.0f}, ui::Effect::gaussian_blur(3.0f, 3.0f));
        painter.fill_rounded_rect({24.0f, 24.0f, 16.0f, 16.0f}, 0.0f,
                                  {1.0f, 1.0f, 1.0f, 1.0f});
        painter.fill_rounded_rect({12.0f, 24.0f, 10.0f, 16.0f}, 0.0f,
                                  {1.0f, 0.0f, 0.0f, 1.0f});
    });

    NUI_CHECK(clean == polluted);

    const auto halo = pixel(clean, 22, 32);
    const auto center = pixel(clean, 32, 32);
    const auto outside_support = pixel(clean, 14, 32);
    NUI_CHECK(halo.r > 4 && halo.g > 4 && halo.b > 4);
    NUI_CHECK(center.r > halo.r + 40);
    NUI_CHECK(outside_support.r < 4 && outside_support.g < 4 && outside_support.b < 4);
}

void asymmetric_blur_contract() {
    const auto pixels = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.0f, 0.0f, 0.0f, 1.0f});
        auto layer = painter.scoped_layer(
            {28.0f, 24.0f, 8.0f, 16.0f}, ui::Effect::gaussian_blur(0.0f, 3.0f));
        painter.fill_rounded_rect({28.0f, 28.0f, 8.0f, 8.0f}, 0.0f,
                                  {1.0f, 1.0f, 1.0f, 1.0f});
    });

    const auto vertical_halo = pixel(pixels, 32, 24);
    const auto horizontal_outside = pixel(pixels, 27, 32);
    NUI_CHECK(vertical_halo.r > 4);
    NUI_CHECK(horizontal_outside.r < 4);

    const auto x_only = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.0f, 0.0f, 0.0f, 1.0f});
        auto layer = painter.scoped_layer(
            {28.0f, 28.0f, 8.0f, 8.0f}, ui::Effect::gaussian_blur(3.0f, 0.0f));
        painter.fill_rounded_rect({28.0f, 28.0f, 8.0f, 8.0f}, 0.0f,
                                  {1.0f, 1.0f, 1.0f, 1.0f});
    });
    NUI_CHECK(pixel(x_only, 26, 32).r > 4);
    NUI_CHECK(pixel(x_only, 32, 27).r < 4);
}

void parent_clip_remains_authoritative() {
    const auto pixels = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.0f, 0.0f, 0.0f, 1.0f});
        auto parent = painter.scoped_clip({0.0f, 0.0f, 32.0f, 64.0f});
        auto layer = painter.scoped_layer(
            {28.0f, 24.0f, 8.0f, 16.0f}, ui::Effect::gaussian_blur(4.0f, 4.0f));
        painter.fill_rounded_rect({28.0f, 28.0f, 8.0f, 8.0f}, 0.0f,
                                  {1.0f, 1.0f, 1.0f, 1.0f});
    });

    NUI_CHECK(pixel(pixels, 30, 32).r > 4);
    NUI_CHECK(pixel(pixels, 34, 32).r < 4);
}

void opacity_applies_once_to_filtered_group() {
    const auto full = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.0f, 0.0f, 0.0f, 1.0f});
        auto layer = painter.scoped_layer(
            {24.0f, 24.0f, 16.0f, 16.0f}, ui::Effect::gaussian_blur(3.0f, 3.0f));
        painter.fill_rounded_rect({24.0f, 24.0f, 16.0f, 16.0f}, 0.0f,
                                  {1.0f, 1.0f, 1.0f, 1.0f});
    });

    const auto half = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.0f, 0.0f, 0.0f, 1.0f});
        ui::PaintOptions options;
        options.opacity = 0.5f;
        auto layer = painter.scoped_layer(
            {24.0f, 24.0f, 16.0f, 16.0f}, ui::Effect::gaussian_blur(3.0f, 3.0f), options);
        painter.fill_rounded_rect({24.0f, 24.0f, 16.0f, 16.0f}, 0.0f,
                                  {1.0f, 1.0f, 1.0f, 1.0f});
    });

    const int full_halo = static_cast<int>(pixel(full, 22, 32).r);
    const int half_halo = static_cast<int>(pixel(half, 22, 32).r);
    NUI_CHECK(full_halo > 6);
    NUI_CHECK(half_halo > 1);
    NUI_CHECK(std::abs(full_halo - 2 * half_halo) <= 4);
}


void blend_applies_to_filtered_group() {
    const auto source_over = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.5f, 0.5f, 0.5f, 1.0f});
        auto layer = painter.scoped_layer(
            {24.0f, 24.0f, 16.0f, 16.0f}, ui::Effect::gaussian_blur(3.0f, 3.0f));
        painter.fill_rounded_rect({24.0f, 24.0f, 16.0f, 16.0f}, 0.0f,
                                  {1.0f, 0.0f, 0.0f, 1.0f});
    });

    const auto multiply = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.5f, 0.5f, 0.5f, 1.0f});
        ui::PaintOptions options;
        options.blend = ui::BlendMode::Multiply;
        auto layer = painter.scoped_layer(
            {24.0f, 24.0f, 16.0f, 16.0f},
            ui::Effect::gaussian_blur(3.0f, 3.0f),
            options);
        painter.fill_rounded_rect({24.0f, 24.0f, 16.0f, 16.0f}, 0.0f,
                                  {1.0f, 0.0f, 0.0f, 1.0f});
    });

    const auto over_halo = pixel(source_over, 22, 32);
    const auto multiply_halo = pixel(multiply, 22, 32);
    NUI_CHECK(over_halo.r > multiply_halo.r + 4);
    NUI_CHECK(std::abs(static_cast<int>(over_halo.g) -
                       static_cast<int>(multiply_halo.g)) <= 3);
}

void inner_transform_does_not_redefine_effect_space() {
    const auto direct = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.0f, 0.0f, 0.0f, 1.0f});
        auto layer = painter.scoped_layer(
            {12.0f, 12.0f, 40.0f, 40.0f}, ui::Effect::gaussian_blur(3.0f, 3.0f));
        painter.fill_rounded_rect({24.0f, 24.0f, 8.0f, 8.0f}, 0.0f,
                                  {1.0f, 1.0f, 1.0f, 1.0f});
    });

    const auto translated_inside = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.0f, 0.0f, 0.0f, 1.0f});
        auto layer = painter.scoped_layer(
            {12.0f, 12.0f, 40.0f, 40.0f}, ui::Effect::gaussian_blur(3.0f, 3.0f));
        painter.translate(8.0f, 0.0f);
        painter.fill_rounded_rect({16.0f, 24.0f, 8.0f, 8.0f}, 0.0f,
                                  {1.0f, 1.0f, 1.0f, 1.0f});
    });

    NUI_CHECK(direct == translated_inside);
}

void device_support_rounding_contract() {
    {
        auto precision_surface = make_surface();
        NUI_CHECK(precision_surface && precision_surface->getCanvas());
        ui::Painter precision{*precision_surface->getCanvas()};
        precision.translate(0.25f, 0.0f);
        SkIRect precision_output;
        NUI_CHECK(ui::detail::PainterEffectFaultAccess::device_output_bounds(
            precision,
            {16777210.0f, 0.0f, 1.0f, 1.0f},
            ui::Effect::gaussian_blur(0.5f, 0.5f),
            precision_output));
        // A float mapRect would round the true right edge 16777211.25 down
        // before roundOut(). The hard bound must preserve the missing pixel.
        NUI_CHECK(precision_output.left() == 16777208);
        NUI_CHECK(precision_output.right() == 16777214);
    }

    auto surface = make_surface();
    NUI_CHECK(surface && surface->getCanvas());
    ui::Painter painter{*surface->getCanvas()};
    painter.scale(1.5f, 2.0f);

    SkIRect device_output;
    const bool valid = ui::detail::PainterEffectFaultAccess::device_output_bounds(
        painter,
        {10.25f, 20.25f, 4.0f, 4.0f},
        ui::Effect::gaussian_blur(0.5f, 0.5f),
        device_output);
    NUI_CHECK(valid);
    NUI_CHECK(device_output.left() == 12);
    NUI_CHECK(device_output.top() == 37);
    NUI_CHECK(device_output.right() == 25);
    NUI_CHECK(device_output.bottom() == 52);

    auto max_surface = make_surface();
    NUI_CHECK(max_surface && max_surface->getCanvas());
    ui::Painter max_sigma{*max_surface->getCanvas()};
    SkIRect max_output;
    NUI_CHECK(ui::detail::PainterEffectFaultAccess::device_output_bounds(
        max_sigma,
        {10.0f, 20.0f, 4.0f, 4.0f},
        ui::Effect::gaussian_blur(64.0f, 64.0f),
        max_output));
    NUI_CHECK(max_output.left() == -182);
    NUI_CHECK(max_output.top() == -172);
    NUI_CHECK(max_output.right() == 206);
    NUI_CHECK(max_output.bottom() == 216);

    auto rotated_surface = make_surface();
    NUI_CHECK(rotated_surface && rotated_surface->getCanvas());
    ui::Painter rotated{*rotated_surface->getCanvas()};
    rotated.rotate(0.78539816339f);
    SkIRect rotated_output;
    NUI_CHECK(ui::detail::PainterEffectFaultAccess::device_output_bounds(
        rotated,
        {0.0f, 0.0f, 8.0f, 8.0f},
        ui::Effect::gaussian_blur(0.5f, 0.5f),
        rotated_output));
    NUI_CHECK(rotated_output.left() == -8);
    NUI_CHECK(rotated_output.top() == -2);
    NUI_CHECK(rotated_output.right() == 8);
    NUI_CHECK(rotated_output.bottom() == 14);
}

std::vector<std::uint8_t> raw_skia_affine_blur_reference() {
    auto surface = make_surface();
    NUI_CHECK(surface && surface->getCanvas());
    auto* canvas = surface->getCanvas();
    canvas->clear(SK_ColorBLACK);

    canvas->save();
    canvas->translate(32.0f, 20.0f);
    canvas->rotate(45.0f);

    SkPaint layer_paint;
    layer_paint.setImageFilter(
        SkImageFilters::Blur(0.5f, 0.5f, SkTileMode::kDecal, nullptr));
    canvas->saveLayer(nullptr, &layer_paint);
    canvas->clipRect(SkRect::MakeXYWH(0.0f, 0.0f, 8.0f, 8.0f),
                     SkClipOp::kIntersect,
                     false);

    SkPaint red_paint;
    red_paint.setAntiAlias(true);
    red_paint.setColor(SK_ColorRED);
    canvas->drawRect(SkRect::MakeXYWH(0.0f, 0.0f, 6.0f, 8.0f), red_paint);

    SkPaint blue_paint;
    blue_paint.setAntiAlias(true);
    blue_paint.setColor(SK_ColorBLUE);
    canvas->drawRect(SkRect::MakeXYWH(2.0f, 0.0f, 6.0f, 8.0f), blue_paint);
    canvas->restore();
    canvas->restore();

    SkPixmap pixmap;
    NUI_CHECK(surface->peekPixels(&pixmap) && pixmap.addr());
    std::vector<std::uint8_t> pixels(64U * 64U * 4U);
    constexpr std::size_t row_bytes = 64U * 4U;
    const auto* source = static_cast<const std::uint8_t*>(pixmap.addr());
    for (int y = 0; y < 64; ++y) {
        std::memcpy(pixels.data() + static_cast<std::size_t>(y) * row_bytes,
                    source + static_cast<std::size_t>(y) * pixmap.rowBytes(),
                    row_bytes);
    }
    return pixels;
}

std::vector<std::uint8_t> raw_skia_fractional_scale_reference() {
    auto surface = make_surface();
    NUI_CHECK(surface && surface->getCanvas());
    auto* canvas = surface->getCanvas();
    canvas->clear(SK_ColorBLACK);
    canvas->save();
    canvas->scale(1.5f, 1.5f);

    SkPaint layer_paint;
    layer_paint.setImageFilter(
        SkImageFilters::Blur(0.5f, 0.5f, SkTileMode::kDecal, nullptr));
    canvas->saveLayer(nullptr, &layer_paint);
    canvas->clipRect(SkRect::MakeXYWH(10.25f, 10.25f, 4.0f, 4.0f),
                     SkClipOp::kIntersect,
                     false);

    SkPaint source_paint;
    source_paint.setAntiAlias(true);
    source_paint.setColor(SK_ColorWHITE);
    canvas->drawRect(SkRect::MakeXYWH(10.25f, 10.25f, 4.0f, 4.0f),
                     source_paint);
    canvas->restore();
    canvas->restore();

    SkPixmap pixmap;
    NUI_CHECK(surface->peekPixels(&pixmap) && pixmap.addr());
    std::vector<std::uint8_t> pixels(64U * 64U * 4U);
    constexpr std::size_t row_bytes = 64U * 4U;
    const auto* source = static_cast<const std::uint8_t*>(pixmap.addr());
    for (int y = 0; y < 64; ++y) {
        std::memcpy(pixels.data() + static_cast<std::size_t>(y) * row_bytes,
                    source + static_cast<std::size_t>(y) * pixmap.rowBytes(),
                    row_bytes);
    }
    return pixels;
}

void fractional_scale_blur_matches_unhinted_skia_oracle() {
    const auto nativeui_pixels = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f},
                                  0.0f,
                                  {0.0f, 0.0f, 0.0f, 1.0f});
        auto state = painter.scoped_state();
        painter.scale(1.5f, 1.5f);
        auto layer = painter.scoped_layer(
            {10.25f, 10.25f, 4.0f, 4.0f},
            ui::Effect::gaussian_blur(0.5f, 0.5f));
        painter.fill_rounded_rect({10.25f, 10.25f, 4.0f, 4.0f},
                                  0.0f,
                                  {1.0f, 1.0f, 1.0f, 1.0f});
    });
    const auto reference = raw_skia_fractional_scale_reference();

    for (int y = 10; y < 30; ++y) {
        for (int x = 10; x < 30; ++x) {
            const auto offset = static_cast<std::size_t>((y * 64 + x) * 4);
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const int delta = std::abs(
                    static_cast<int>(nativeui_pixels[offset + channel]) -
                    static_cast<int>(reference[offset + channel]));
                NUI_CHECK(delta <= 1);
            }
        }
    }
}

void affine_blur_matches_unclipped_skia_oracle() {
    const auto nativeui_pixels = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f},
                                  0.0f,
                                  {0.0f, 0.0f, 0.0f, 1.0f});
        auto state = painter.scoped_state();
        painter.translate(32.0f, 20.0f);
        painter.rotate(0.78539816339f);
        auto layer = painter.scoped_layer(
            {0.0f, 0.0f, 8.0f, 8.0f},
            ui::Effect::gaussian_blur(0.5f, 0.5f));
        painter.fill_rounded_rect({0.0f, 0.0f, 6.0f, 8.0f},
                                  0.0f,
                                  {1.0f, 0.0f, 0.0f, 1.0f});
        painter.fill_rounded_rect({2.0f, 0.0f, 6.0f, 8.0f},
                                  0.0f,
                                  {0.0f, 0.0f, 1.0f, 1.0f});
    });
    const auto reference = raw_skia_affine_blur_reference();

    for (int y = 12; y < 40; ++y) {
        for (int x = 16; x < 48; ++x) {
            const auto offset = static_cast<std::size_t>((y * 64 + x) * 4);
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const int delta = std::abs(
                    static_cast<int>(nativeui_pixels[offset + channel]) -
                    static_cast<int>(reference[offset + channel]));
                NUI_CHECK(delta <= 1);
            }
        }
    }
}

void hidpi_blur_scales_in_logical_space() {
    ui::UI tree{PainterProbe{[](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 64.0f, 64.0f}, 0.0f,
                                  {0.0f, 0.0f, 0.0f, 1.0f});
        auto layer = painter.scoped_layer(
            {24.0f, 24.0f, 16.0f, 16.0f}, ui::Effect::gaussian_blur(3.0f, 3.0f));
        painter.fill_rounded_rect({24.0f, 24.0f, 16.0f, 16.0f}, 0.0f,
                                  {1.0f, 1.0f, 1.0f, 1.0f});
    }}};

    ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 2.0f};
    NUI_CHECK(renderer.render(tree));
    const auto halo = renderer.pixel(44, 64);
    const auto outside_support = renderer.pixel(28, 64);
    NUI_CHECK(halo.r > 4);
    NUI_CHECK(outside_support.r < 4);
}

void nested_filtered_scopes_preserve_lifo() {
    auto surface = make_surface();
    NUI_CHECK(surface && surface->getCanvas());
    auto* canvas = surface->getCanvas();
    const int baseline = canvas->getSaveCount();
    ui::Painter painter{*canvas};
    const auto blur = ui::Effect::gaussian_blur(2.0f, 2.0f);

    {
        auto outer = painter.scoped_layer({4.0f, 4.0f, 56.0f, 56.0f}, blur);
        NUI_CHECK(painter.save_depth() == 2);
        {
            auto clip = painter.scoped_clip({8.0f, 8.0f, 48.0f, 48.0f});
            NUI_CHECK(painter.save_depth() == 3);
            {
                auto plain = painter.scoped_layer({10.0f, 10.0f, 44.0f, 44.0f});
                NUI_CHECK(painter.save_depth() == 5);
                {
                    auto inner =
                        painter.scoped_layer({12.0f, 12.0f, 40.0f, 40.0f}, blur);
                    NUI_CHECK(painter.save_depth() == 7);
                    painter.save();
                    NUI_CHECK(painter.save_depth() == 8);
                    painter.restore();
                    NUI_CHECK(painter.save_depth() == 7);
                }
                NUI_CHECK(painter.save_depth() == 5);
            }
            NUI_CHECK(painter.save_depth() == 3);
        }
        NUI_CHECK(painter.save_depth() == 2);
    }

    NUI_CHECK(painter.save_depth() == 0);
    NUI_CHECK(canvas->getSaveCount() == baseline);

    bool caught = false;
    try {
        auto outer = painter.scoped_layer({4.0f, 4.0f, 56.0f, 56.0f}, blur);
        auto inner = painter.scoped_layer({12.0f, 12.0f, 40.0f, 40.0f}, blur);
        throw 7;
    } catch (int) {
        caught = true;
    }
    NUI_CHECK(caught);
    NUI_CHECK(painter.save_depth() == 0);
    NUI_CHECK(canvas->getSaveCount() == baseline);
}

void fault_and_stack_recovery_contract() {
    auto surface = make_surface();
    NUI_CHECK(surface && surface->getCanvas());
    auto* canvas = surface->getCanvas();
    const int baseline = canvas->getSaveCount();
    ui::Painter painter{*canvas};
    const auto blur = ui::Effect::gaussian_blur(2.0f, 2.0f);

    const auto expect_failure = [&](auto arm) {
        arm(painter);
        bool threw = false;
        try {
            auto layer = painter.scoped_layer({12.0f, 12.0f, 24.0f, 24.0f}, blur);
            (void)layer;
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(painter.save_depth() == 0);
        NUI_CHECK(canvas->getSaveCount() == baseline);

        {
            auto recovered = painter.scoped_layer(
                {12.0f, 12.0f, 24.0f, 24.0f}, blur);
            painter.fill_rounded_rect(
                {16.0f, 16.0f, 4.0f, 4.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
        }
        NUI_CHECK(painter.save_depth() == 0);
        NUI_CHECK(canvas->getSaveCount() == baseline);
    };

    expect_failure(ui::detail::PainterEffectFaultAccess::fail_before_materialization);
    expect_failure(ui::detail::PainterEffectFaultAccess::fail_after_output_clip);
    expect_failure(ui::detail::PainterEffectFaultAccess::fail_after_save_layer);
    expect_failure(ui::detail::PainterEffectFaultAccess::fail_after_source_clip);

    const auto early_return = [&]() -> bool {
        auto layer = painter.scoped_layer({12.0f, 12.0f, 24.0f, 24.0f}, blur);
        return painter.save_depth() == 2 &&
               canvas->getSaveCount() == baseline + 2;
    };
    NUI_CHECK(early_return());
    NUI_CHECK(painter.save_depth() == 0);
    NUI_CHECK(canvas->getSaveCount() == baseline);

    {
        auto layer = painter.scoped_layer({12.0f, 12.0f, 24.0f, 24.0f}, blur);
        NUI_CHECK(painter.save_depth() == 2);
        NUI_CHECK(canvas->getSaveCount() == baseline + 2);
        painter.save();
        painter.restore();
        NUI_CHECK(painter.save_depth() == 2);
        painter.fill_rounded_rect({16.0f, 16.0f, 8.0f, 8.0f}, 0.0f,
                                  {0.0f, 1.0f, 0.0f, 1.0f});
    }

    NUI_CHECK(painter.save_depth() == 0);
    NUI_CHECK(canvas->getSaveCount() == baseline);
}

void affine_transform_and_overflow_contract() {
    auto surface = make_surface();
    NUI_CHECK(surface && surface->getCanvas());
    auto* canvas = surface->getCanvas();
    const int baseline = canvas->getSaveCount();
    ui::Painter painter{*canvas};

    painter.translate(24.0f, 20.0f);
    painter.rotate(0.35f);
    painter.scale(1.5f, 0.75f);

    ui::detail::PainterEffectFaultAccess::fail_after_output_clip(painter);
    bool threw = false;
    try {
        auto layer = painter.scoped_layer(
            {-8.0f, -8.0f, 16.0f, 16.0f}, ui::Effect::gaussian_blur(2.0f, 4.0f));
        (void)layer;
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(painter.save_depth() == 0);
    NUI_CHECK(canvas->getSaveCount() == baseline);

    {
        auto layer = painter.scoped_layer(
            {-8.0f, -8.0f, 16.0f, 16.0f}, ui::Effect::gaussian_blur(2.0f, 4.0f));
        painter.fill_rounded_rect(
            {-6.0f, -6.0f, 12.0f, 12.0f}, 0.0f, {1.0f, 1.0f, 1.0f, 1.0f});
    }
    NUI_CHECK(painter.save_depth() == 0);
    NUI_CHECK(canvas->getSaveCount() == baseline);

    auto overflow_surface = make_surface();
    NUI_CHECK(overflow_surface && overflow_surface->getCanvas());
    ui::Painter overflow{*overflow_surface->getCanvas()};
    overflow.scale(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    ui::detail::PainterEffectFaultAccess::fail_before_materialization(overflow);
    {
        auto empty = overflow.scoped_layer(
            {1.0f, 1.0f, 8.0f, 8.0f}, ui::Effect::gaussian_blur(2.0f, 2.0f));
        NUI_CHECK(overflow.save_depth() == 1);
    }
    NUI_CHECK(overflow.save_depth() == 0);
    ui::detail::PainterEffectFaultAccess::clear(overflow);

    auto huge_surface = make_surface();
    NUI_CHECK(huge_surface && huge_surface->getCanvas());
    ui::Painter huge{*huge_surface->getCanvas()};
    huge.translate(20000000.0f, 0.0f);
    ui::detail::PainterEffectFaultAccess::fail_before_materialization(huge);
    {
        auto empty = huge.scoped_layer(
            {0.0f, 0.0f, 8.0f, 8.0f}, ui::Effect::gaussian_blur(2.0f, 2.0f));
        NUI_CHECK(huge.save_depth() == 1);
    }
    NUI_CHECK(huge.save_depth() == 0);
    ui::detail::PainterEffectFaultAccess::clear(huge);

    auto nonfinite_surface = make_surface();
    NUI_CHECK(nonfinite_surface && nonfinite_surface->getCanvas());
    auto* nonfinite_canvas = nonfinite_surface->getCanvas();
    const int nonfinite_baseline = nonfinite_canvas->getSaveCount();
    ui::Painter nonfinite{*nonfinite_canvas};
    {
        auto state = nonfinite.scoped_state();
        // T098 makes non-finite Painter transform mutations atomic no-ops. Keep
        // this T077 fail-closed backend-matrix fixture by poisoning SkCanvas
        // directly inside the protected state scope, then prove restore recovers it.
        nonfinite_canvas->scale(std::numeric_limits<float>::infinity(), 1.0f);
        ui::detail::PainterEffectFaultAccess::fail_before_materialization(nonfinite);
        {
            auto empty = nonfinite.scoped_layer(
                {0.0f, 0.0f, 8.0f, 8.0f}, ui::Effect::gaussian_blur(2.0f, 2.0f));
            NUI_CHECK(nonfinite.save_depth() == 2);
        }
        NUI_CHECK(nonfinite.save_depth() == 1);
    }
    NUI_CHECK(nonfinite.save_depth() == 0);
    NUI_CHECK(nonfinite_canvas->getSaveCount() == nonfinite_baseline);

    // The seam must still be armed: the invalid backend transform failed
    // closed before backend filter materialization.
    bool seam_still_armed = false;
    try {
        auto should_fail = nonfinite.scoped_layer(
            {4.0f, 4.0f, 8.0f, 8.0f}, ui::Effect::gaussian_blur(2.0f, 2.0f));
        (void)should_fail;
    } catch (const std::bad_alloc&) {
        seam_still_armed = true;
    }
    NUI_CHECK(seam_still_armed);
    NUI_CHECK(nonfinite.save_depth() == 0);
    NUI_CHECK(nonfinite_canvas->getSaveCount() == nonfinite_baseline);

    {
        auto recovered = nonfinite.scoped_layer(
            {4.0f, 4.0f, 8.0f, 8.0f}, ui::Effect::gaussian_blur(2.0f, 2.0f));
        nonfinite.fill_rounded_rect(
            {6.0f, 6.0f, 2.0f, 2.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
    }
    NUI_CHECK(nonfinite.save_depth() == 0);
    NUI_CHECK(nonfinite_canvas->getSaveCount() == nonfinite_baseline);
}

void independent_painters_do_not_share_effect_state() {
    auto surface_b = make_surface();
    NUI_CHECK(surface_b && surface_b->getCanvas());
    auto* canvas_b = surface_b->getCanvas();
    const int baseline_b = canvas_b->getSaveCount();
    ui::Painter painter_b{*canvas_b};
    const auto blur_b = ui::Effect::gaussian_blur(1.0f, 3.0f);

    {
        auto layer_b = painter_b.scoped_layer(
            {8.0f, 8.0f, 32.0f, 32.0f}, blur_b);
        NUI_CHECK(painter_b.save_depth() == 2);

        {
            auto surface_a = make_surface();
            NUI_CHECK(surface_a && surface_a->getCanvas());
            ui::Painter painter_a{*surface_a->getCanvas()};
            const auto blur_a = ui::Effect::gaussian_blur(4.0f, 1.0f);

            {
                auto layer_a = painter_a.scoped_layer(
                    {4.0f, 4.0f, 24.0f, 24.0f}, blur_a);
                painter_a.fill_rounded_rect(
                    {8.0f, 8.0f, 8.0f, 8.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            }
            NUI_CHECK(painter_a.save_depth() == 0);

            ui::detail::PainterEffectFaultAccess::fail_before_materialization(painter_a);
            bool a_threw = false;
            try {
                auto failed = painter_a.scoped_layer(
                    {4.0f, 4.0f, 24.0f, 24.0f}, blur_a);
                (void)failed;
            } catch (const std::bad_alloc&) {
                a_threw = true;
            }
            NUI_CHECK(a_threw);
            NUI_CHECK(painter_a.save_depth() == 0);
        } // Painter A and its surface are destroyed while B's blur layer is active.

        NUI_CHECK(painter_b.save_depth() == 2);
        NUI_CHECK(canvas_b->getSaveCount() == baseline_b + 2);
        painter_b.fill_rounded_rect(
            {12.0f, 12.0f, 8.0f, 8.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
    }

    NUI_CHECK(painter_b.save_depth() == 0);
    NUI_CHECK(canvas_b->getSaveCount() == baseline_b);

    {
        auto later_b = painter_b.scoped_layer(
            {8.0f, 8.0f, 24.0f, 24.0f}, ui::Effect::gaussian_blur(2.0f, 2.0f));
        painter_b.fill_rounded_rect(
            {12.0f, 12.0f, 4.0f, 4.0f}, 0.0f, {0.0f, 0.0f, 1.0f, 1.0f});
    }
    NUI_CHECK(painter_b.save_depth() == 0);
    NUI_CHECK(canvas_b->getSaveCount() == baseline_b);
}

void suite() {
    effect_value_contract();
    no_op_and_invalid_do_not_materialize();
    zero_blur_is_pixel_equivalent();
    halo_source_clip_and_transparent_edges();
    asymmetric_blur_contract();
    parent_clip_remains_authoritative();
    opacity_applies_once_to_filtered_group();
    blend_applies_to_filtered_group();
    inner_transform_does_not_redefine_effect_space();
    device_support_rounding_contract();
    fractional_scale_blur_matches_unhinted_skia_oracle();
    affine_blur_matches_unclipped_skia_oracle();
    hidpi_blur_scales_in_logical_space();
    nested_filtered_scopes_preserve_lifo();
    fault_and_stack_recovery_contract();
    affine_transform_and_overflow_contract();
    independent_painters_do_not_share_effect_state();
}

} // namespace

int main() { return test::run("T077 Gaussian blur", suite); }
