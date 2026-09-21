#include "test_support.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

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

static_assert(noexcept(ui::Transform2D::rotation(0.0f)));
static_assert(noexcept(std::declval<const ui::Transform2D&>().map_point(ui::Point{})));
static_assert(noexcept(std::declval<const ui::Transform2D&>().inverse()));
static_assert(noexcept(std::declval<const ui::Transform2D&>() *
                       std::declval<const ui::Transform2D&>()));

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

    NUI_CHECK(!ui::Transform2D{0.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 0.0f}
                   .inverse()
                   .has_value());

    // Normalized determinant threshold: exactly 1e-8 is rejected.
    const auto at_threshold =
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, 1.0e-8f, 0.0f};
    NUI_CHECK(!at_threshold.inverse().has_value());

    const auto above_threshold =
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, 1.1e-8f, 0.0f};
    NUI_CHECK(above_threshold.inverse().has_value());

    const auto below_negative_threshold =
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, -0.9e-8f, 0.0f};
    NUI_CHECK(!below_negative_threshold.inverse().has_value());

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
}

void suite() {
    transform_value_contract();
    inverse_contract();
}

} // namespace

int main() { return test::run("t098_transform_tracking", suite); }
