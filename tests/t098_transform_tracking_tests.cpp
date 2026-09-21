#include "test_support.hpp"

#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <cmath>
#include <limits>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace ui::detail {

struct PainterTransformHistoryFaultAccess {
    static int inline_depth() noexcept { return Painter::kInlineTransformSaveDepth; }

    static void fail_next_overflow_snapshot(Painter& painter) noexcept {
        painter.fail_transform_history_overflow_ = true;
    }
};

} // namespace ui::detail

namespace {

[[nodiscard]] bool near(float a, float b, float epsilon = 1.0e-5f) noexcept {
    return std::abs(a - b) <= epsilon;
}

[[nodiscard]] bool same(const ui::Transform2D& a,
                        const ui::Transform2D& b,
                        float epsilon = 1.0e-5f) noexcept {
    return near(a.m00, b.m00, epsilon) &&
           near(a.m01, b.m01, epsilon) &&
           near(a.m02, b.m02, epsilon) &&
           near(a.m10, b.m10, epsilon) &&
           near(a.m11, b.m11, epsilon) &&
           near(a.m12, b.m12, epsilon);
}

[[nodiscard]] bool same_backend_affine(const SkMatrix& a,
                                       const SkMatrix& b) noexcept {
    return a.getScaleX() == b.getScaleX() &&
           a.getSkewX() == b.getSkewX() &&
           a.getTranslateX() == b.getTranslateX() &&
           a.getSkewY() == b.getSkewY() &&
           a.getScaleY() == b.getScaleY() &&
           a.getTranslateY() == b.getTranslateY();
}

[[nodiscard]] bool backend_matches(const SkMatrix& matrix,
                                   const ui::Transform2D& transform,
                                   float epsilon = 2.0e-5f) noexcept {
    return near(matrix.getScaleX(), transform.m00, epsilon) &&
           near(matrix.getSkewX(), transform.m01, epsilon) &&
           near(matrix.getTranslateX(), transform.m02, epsilon) &&
           near(matrix.getSkewY(), transform.m10, epsilon) &&
           near(matrix.getScaleY(), transform.m11, epsilon) &&
           near(matrix.getTranslateY(), transform.m12, epsilon);
}

[[nodiscard]] sk_sp<SkSurface> make_surface() {
    const auto info = SkImageInfo::Make(
        96, 96, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    return SkSurfaces::Raster(info);
}

static_assert(noexcept(ui::Transform2D::rotation(0.0f)));
static_assert(noexcept(std::declval<const ui::Transform2D&>().map_point(ui::Point{})));
static_assert(noexcept(std::declval<const ui::Transform2D&>().inverse()));
static_assert(noexcept(std::declval<const ui::Transform2D&>() *
                       std::declval<const ui::Transform2D&>()));
static_assert(noexcept(std::declval<const ui::Painter&>().current_transform()));

void transform_value_contract() {
    const auto identity = ui::Transform2D::identity();
    NUI_CHECK(same(ui::Transform2D::rotation(
                       std::numeric_limits<float>::quiet_NaN()),
                   identity));
    NUI_CHECK(same(ui::Transform2D::rotation(
                       std::numeric_limits<float>::infinity()),
                   identity));
    NUI_CHECK(same(ui::Transform2D::rotation(
                       -std::numeric_limits<float>::infinity()),
                   identity));

    const auto quarter_turn = ui::Transform2D::rotation(ui::kPi * 0.5f);
    const auto p = quarter_turn.map_point({2.0f, 0.0f});
    NUI_CHECK(near(p.x, 0.0f, 2.0e-5f));
    NUI_CHECK(near(p.y, 2.0f, 2.0e-5f));

    const auto translate = ui::Transform2D::translation(10.0f, -4.0f);
    const auto scale = ui::Transform2D::scaling(2.0f, 3.0f);
    const auto combined = translate * scale;
    const auto mapped = combined.map_point({1.0f, 2.0f});
    NUI_CHECK(near(mapped.x, 12.0f));
    NUI_CHECK(near(mapped.y, 2.0f));

    // a * b means b first, then a.
    const auto non_commuting_a =
        ui::Transform2D::translation(5.0f, 7.0f) *
        ui::Transform2D::rotation(0.31f);
    const auto non_commuting_b =
        ui::Transform2D::rotation(0.31f) *
        ui::Transform2D::translation(5.0f, 7.0f);
    const auto pa = non_commuting_a.map_point({3.0f, -2.0f});
    const auto pb = non_commuting_b.map_point({3.0f, -2.0f});
    NUI_CHECK(!near(pa.x, pb.x) || !near(pa.y, pb.y));

    const ui::Transform2D shear_and_scale{
        2.0f, 0.5f, 3.0f,
        -0.25f, 4.0f, -2.0f};
    const auto shear_mapped = shear_and_scale.map_point({2.0f, -3.0f});
    NUI_CHECK(near(shear_mapped.x, 5.5f));
    NUI_CHECK(near(shear_mapped.y, -14.5f));
}

void inverse_contract() {
    const auto value =
        ui::Transform2D::translation(11.0f, -3.0f) *
        ui::Transform2D{2.0f, 0.25f, 0.0f, -0.5f, 3.0f, 0.0f};
    const auto inverse = value.inverse();
    NUI_CHECK(inverse.has_value());

    const ui::Point samples[]{{0.0f, 0.0f}, {2.0f, -1.0f}, {-9.0f, 4.0f}};
    for (const auto sample : samples) {
        const auto roundtrip = inverse->map_point(value.map_point(sample));
        NUI_CHECK(near(roundtrip.x, sample.x, 2.0e-4f));
        NUI_CHECK(near(roundtrip.y, sample.y, 2.0e-4f));
    }

    const auto non_finite = ui::Transform2D{
        std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f};
    NUI_CHECK(!non_finite.inverse().has_value());

    const ui::Transform2D zero_linear{
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f};
    NUI_CHECK(!zero_linear.inverse().has_value());

    // Normalized determinant threshold: immediately below/equal/above on both signs.
    const auto below_threshold =
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, 0.9e-8f, 0.0f};
    NUI_CHECK(!below_threshold.inverse().has_value());

    const auto at_threshold =
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, 1.0e-8f, 0.0f};
    NUI_CHECK(!at_threshold.inverse().has_value());

    const auto above_threshold =
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, 1.1e-8f, 0.0f};
    NUI_CHECK(above_threshold.inverse().has_value());

    const auto below_negative_threshold =
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, -0.9e-8f, 0.0f};
    NUI_CHECK(!below_negative_threshold.inverse().has_value());

    const auto at_negative_threshold =
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, -1.0e-8f, 0.0f};
    NUI_CHECK(!at_negative_threshold.inverse().has_value());

    const auto above_negative_threshold =
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, -1.1e-8f, 0.0f};
    NUI_CHECK(above_negative_threshold.inverse().has_value());

    // Uniform magnitude changes must not change the singularity decision.
    const auto tiny =
        ui::Transform2D{1.0e-20f, 0.0f, 0.0f, 0.0f, 2.0e-20f, 0.0f};
    NUI_CHECK(tiny.inverse().has_value());

    const auto huge =
        ui::Transform2D{1.0e20f, 0.0f, 0.0f, 0.0f, 2.0e20f, 0.0f};
    NUI_CHECK(huge.inverse().has_value());

    // A mathematically invertible matrix is still rejected when its public
    // binary32 inverse translation cannot be represented finitely.
    const auto unrepresentable_inverse =
        ui::Transform2D{1.0e-20f, 0.0f, 1.0e20f,
                        0.0f, 1.0e-20f, -1.0e20f};
    NUI_CHECK(!unrepresentable_inverse.inverse().has_value());
}

void painter_tracking_contract() {
    auto surface = make_surface();
    NUI_CHECK(surface != nullptr);
    auto* canvas = surface->getCanvas();
    NUI_CHECK(canvas != nullptr);

    // A backend/device transform may already exist before Painter begins. It
    // must never leak into the public logical local-to-scene transform.
    canvas->scale(2.0f, 2.0f);
    ui::Painter painter{*canvas};
    NUI_CHECK(same(painter.current_transform(), ui::Transform2D::identity(), 0.0f));

    painter.translate(10.0f, -4.0f);
    painter.rotate(0.25f);
    painter.scale(2.0f, 3.0f);
    const auto expected =
        ui::Transform2D::translation(10.0f, -4.0f) *
        ui::Transform2D::rotation(0.25f) *
        ui::Transform2D::scaling(2.0f, 3.0f);
    NUI_CHECK(same(painter.current_transform(), expected, 2.0e-5f));

    const auto before_manual = painter.current_transform();
    painter.save();
    painter.translate(7.0f, 9.0f);
    NUI_CHECK(!same(painter.current_transform(), before_manual));
    painter.restore();
    NUI_CHECK(same(painter.current_transform(), before_manual, 0.0f));

    {
        auto state = painter.scoped_state();
        painter.concat(ui::Transform2D{
            1.0f, 0.2f, 3.0f,
            -0.3f, 1.0f, 5.0f});
        NUI_CHECK(!same(painter.current_transform(), before_manual));
        {
            auto clip = painter.scoped_clip({0.0f, 0.0f, 24.0f, 24.0f});
            const auto before_layer = painter.current_transform();
            {
                auto layer = painter.scoped_layer({0.0f, 0.0f, 24.0f, 24.0f});
                painter.translate(2.0f, 1.0f);
            }
            NUI_CHECK(same(painter.current_transform(), before_layer, 0.0f));

            // Filtered layers temporarily reset/set the backend matrix in
            // device space. That private renderer work must not affect the
            // NativeUI logical transform tracker.
            {
                auto effect_layer = painter.scoped_layer(
                    {0.0f, 0.0f, 24.0f, 24.0f},
                    ui::Effect::gaussian_blur(1.5f, 1.5f));
                NUI_CHECK(same(painter.current_transform(), before_layer, 0.0f));
                painter.scale(0.75f, 1.25f);
            }
            NUI_CHECK(same(painter.current_transform(), before_layer, 0.0f));
        }
    }
    NUI_CHECK(same(painter.current_transform(), before_manual, 0.0f));
    NUI_CHECK(painter.save_depth() == 0);

    // Exercise the deep overflow history path as well as the inline common
    // path. Restoring every frame must recover the exact original value.
    constexpr int deep_save_count = 40;
    for (int i = 0; i < deep_save_count; ++i) {
        painter.save();
        painter.translate(0.25f, -0.125f);
    }
    for (int i = 0; i < deep_save_count; ++i) painter.restore();
    NUI_CHECK(painter.save_depth() == 0);
    NUI_CHECK(same(painter.current_transform(), before_manual, 0.0f));
}

void painter_concat_order_contract() {
    auto surface = make_surface();
    NUI_CHECK(surface != nullptr);
    auto* canvas = surface->getCanvas();
    NUI_CHECK(canvas != nullptr);
    ui::Painter painter{*canvas};

    const auto translation = ui::Transform2D::translation(5.0f, 7.0f);
    const auto local = ui::Transform2D{
        0.75f, 0.4f, 3.0f,
        -0.2f, 1.1f, -2.0f};
    painter.translate(5.0f, 7.0f);
    painter.concat(local);

    const auto expected = translation * local;
    const auto opposite = local * translation;
    NUI_CHECK(same(painter.current_transform(), expected, 0.0f));
    NUI_CHECK(backend_matches(canvas->getLocalToDeviceAs3x3(), expected));

    const ui::Point sample{4.0f, -3.0f};
    const auto expected_point = expected.map_point(sample);
    const auto opposite_point = opposite.map_point(sample);
    NUI_CHECK(!near(expected_point.x, opposite_point.x) ||
              !near(expected_point.y, opposite_point.y));
}

void device_scale_exclusion_contract() {
    auto surface_1x = make_surface();
    auto surface_2x = make_surface();
    NUI_CHECK(surface_1x != nullptr);
    NUI_CHECK(surface_2x != nullptr);
    auto* canvas_1x = surface_1x->getCanvas();
    auto* canvas_2x = surface_2x->getCanvas();
    NUI_CHECK(canvas_1x != nullptr);
    NUI_CHECK(canvas_2x != nullptr);

    canvas_2x->scale(2.0f, 2.0f);
    ui::Painter painter_1x{*canvas_1x};
    ui::Painter painter_2x{*canvas_2x};

    const auto apply = [](ui::Painter& painter) {
        painter.translate(13.0f, -6.0f);
        painter.rotate(0.37f);
        painter.concat(ui::Transform2D{
            1.25f, 0.15f, 2.0f,
            -0.1f, 0.8f, 4.0f});
    };
    apply(painter_1x);
    apply(painter_2x);

    NUI_CHECK(same(painter_1x.current_transform(),
                   painter_2x.current_transform(), 0.0f));

    const auto map_affine = [](const SkMatrix& matrix, SkPoint point) noexcept {
        return SkPoint{
            matrix.getScaleX() * point.x() +
                matrix.getSkewX() * point.y() +
                matrix.getTranslateX(),
            matrix.getSkewY() * point.x() +
                matrix.getScaleY() * point.y() +
                matrix.getTranslateY()};
    };
    const SkPoint sample{3.0f, -2.0f};
    const SkPoint physical_1x =
        map_affine(canvas_1x->getLocalToDeviceAs3x3(), sample);
    const SkPoint physical_2x =
        map_affine(canvas_2x->getLocalToDeviceAs3x3(), sample);
    NUI_CHECK(near(physical_2x.x(), physical_1x.x() * 2.0f, 1.0e-4f));
    NUI_CHECK(near(physical_2x.y(), physical_1x.y() * 2.0f, 1.0e-4f));
}

void mixed_scope_restore_contract() {
    auto surface = make_surface();
    NUI_CHECK(surface != nullptr);
    auto* canvas = surface->getCanvas();
    NUI_CHECK(canvas != nullptr);
    ui::Painter painter{*canvas};

    painter.translate(4.0f, 6.0f);
    const auto entry = painter.current_transform();

    painter.save();
    painter.rotate(0.2f);
    const auto manual = painter.current_transform();
    {
        auto state = painter.scoped_state();
        painter.scale(1.25f, 0.75f);
        const auto state_transform = painter.current_transform();
        {
            auto clip = painter.scoped_clip({0.0f, 0.0f, 40.0f, 40.0f});
            painter.translate(3.0f, -2.0f);
            const auto clip_transform = painter.current_transform();

            painter.push_clip({1.0f, 1.0f, 20.0f, 20.0f});
            painter.rotate(-0.15f);
            const auto pushed_transform = painter.current_transform();
            {
                auto layer = painter.scoped_layer({0.0f, 0.0f, 16.0f, 16.0f});
                painter.translate(2.0f, 5.0f);
            }
            NUI_CHECK(same(painter.current_transform(), pushed_transform, 0.0f));
            painter.pop_clip();
            NUI_CHECK(same(painter.current_transform(), clip_transform, 0.0f));
        }
        NUI_CHECK(same(painter.current_transform(), state_transform, 0.0f));
    }
    NUI_CHECK(same(painter.current_transform(), manual, 0.0f));
    painter.restore();
    NUI_CHECK(same(painter.current_transform(), entry, 0.0f));
    NUI_CHECK(painter.save_depth() == 0);
}

void transform_history_failure_is_transactional() {
    auto surface = make_surface();
    NUI_CHECK(surface != nullptr);
    auto* canvas = surface->getCanvas();
    NUI_CHECK(canvas != nullptr);
    ui::Painter painter{*canvas};

    const int inline_depth = ui::detail::PainterTransformHistoryFaultAccess::inline_depth();
    for (int i = 0; i < inline_depth; ++i) {
        painter.save();
        painter.translate(0.5f, -0.25f);
    }

    const auto logical_before = painter.current_transform();
    const int painter_depth_before = painter.save_depth();
    const int backend_depth_before = canvas->getSaveCount();
    ui::detail::PainterTransformHistoryFaultAccess::fail_next_overflow_snapshot(painter);

    bool threw = false;
    try {
        painter.save();
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(painter.save_depth() == painter_depth_before);
    NUI_CHECK(canvas->getSaveCount() == backend_depth_before);
    NUI_CHECK(same(painter.current_transform(), logical_before, 0.0f));

    // Recovery is part of the contract: the one-shot seam is cleared before
    // throwing, so the next normal overflow save can publish and restore.
    painter.save();
    painter.translate(9.0f, 3.0f);
    painter.restore();
    NUI_CHECK(same(painter.current_transform(), logical_before, 0.0f));

    for (int i = 0; i < inline_depth; ++i) painter.restore();
    NUI_CHECK(painter.save_depth() == 0);
    NUI_CHECK(same(painter.current_transform(), ui::Transform2D::identity(), 0.0f));
}

void invalid_painter_mutations_are_atomic() {
    auto surface = make_surface();
    NUI_CHECK(surface != nullptr);
    auto* canvas = surface->getCanvas();
    NUI_CHECK(canvas != nullptr);
    ui::Painter painter{*canvas};

    painter.translate(8.0f, 6.0f);
    painter.rotate(0.2f);

    const auto check_noop = [&](auto&& mutation) {
        const auto logical_before = painter.current_transform();
        const auto backend_before = canvas->getLocalToDeviceAs3x3();
        mutation();
        NUI_CHECK(same(painter.current_transform(), logical_before, 0.0f));
        NUI_CHECK(same_backend_affine(
            canvas->getLocalToDeviceAs3x3(), backend_before));
    };

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    check_noop([&] { painter.translate(nan, 0.0f); });
    check_noop([&] { painter.translate(0.0f, inf); });
    check_noop([&] { painter.scale(inf, 1.0f); });
    check_noop([&] { painter.rotate(nan); });
    check_noop([&] {
        painter.concat(ui::Transform2D{
            1.0f, nan, 0.0f,
            0.0f, 1.0f, 0.0f});
    });

    // Every finite angle is valid. Use the affine matrix path so an otherwise
    // valid huge angle cannot become a no-op only because degrees overflow float.
    const float huge_radians = (std::numeric_limits<float>::max)();
    const auto before_huge_rotation = painter.current_transform();
    const auto huge_operation = ui::Transform2D::rotation(huge_radians);
    painter.rotate(huge_radians);
    const auto after_huge_rotation = before_huge_rotation * huge_operation;
    NUI_CHECK(same(painter.current_transform(), after_huge_rotation, 2.0e-5f));
    NUI_CHECK(backend_matches(canvas->getLocalToDeviceAs3x3(), after_huge_rotation, 4.0e-5f));

    // A finite operation whose NativeUI composition overflows is also an exact
    // no-op on both logical and backend state.
    painter.save();
    painter.scale(1.0e30f, 1.0f);
    check_noop([&] { painter.scale(1.0e20f, 1.0f); });
    painter.restore();

    // Finite near-singular transforms remain drawable/tracked; only inverse()
    // rejects them according to the shared T098 numerical contract.
    painter.save();
    painter.concat(ui::Transform2D{
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0e-9f, 0.0f});
    const auto near_singular = painter.current_transform();
    NUI_CHECK(!near_singular.inverse().has_value());
    NUI_CHECK(backend_matches(
        canvas->getLocalToDeviceAs3x3(), near_singular, 4.0e-5f));
    painter.fill_rounded_rect(
        {0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, {1.0f, 1.0f, 1.0f, 1.0f});
    NUI_CHECK(backend_matches(
        canvas->getLocalToDeviceAs3x3(), near_singular, 4.0e-5f));
    painter.restore();
}

void suite() {
    transform_value_contract();
    inverse_contract();
    painter_tracking_contract();
    painter_concat_order_contract();
    device_scale_exclusion_contract();
    mixed_scope_restore_contract();
    transform_history_failure_is_transactional();
    invalid_painter_mutations_are_atomic();
}

} // namespace

int main() { return test::run("t098_transform_tracking", suite); }
