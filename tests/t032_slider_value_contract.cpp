#include <nativeui/detail/slider_value.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

int failures = 0;

void check(bool condition) {
    if (!condition) ++failures;
}

bool near(float actual, float expected, float tolerance = 1.0e-6f) {
    return std::abs(actual - expected) <= tolerance;
}

template <class Fn>
void check_invalid(Fn&& fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected);
}

} // namespace

int main() {
    using ui::detail::SliderDomain;

    check_invalid([] { (void)SliderDomain{1.0f, 1.0f, 0.0f}; });
    check_invalid([] { (void)SliderDomain{2.0f, 1.0f, 0.0f}; });
    check_invalid([] {
        (void)SliderDomain{std::numeric_limits<float>::quiet_NaN(), 1.0f, 0.0f};
    });
    check_invalid([] {
        (void)SliderDomain{0.0f, std::numeric_limits<float>::infinity(), 0.0f};
    });
    check_invalid([] { (void)SliderDomain{0.0f, 1.0f, -0.1f}; });
    check_invalid([] {
        (void)SliderDomain{0.0f, 1.0f, std::numeric_limits<float>::quiet_NaN()};
    });
    check_invalid([] {
        (void)SliderDomain{0.0f, 1.0f, std::numeric_limits<float>::infinity()};
    });

    const SliderDomain continuous{-2.0f, 2.0f, 0.0f};
    check(near(continuous.normalize(-3.0f), -2.0f));
    check(near(continuous.normalize(3.0f), 2.0f));
    check(near(continuous.normalize(0.25f), 0.25f));
    check(near(
        continuous.normalize(std::numeric_limits<float>::quiet_NaN()), -2.0f));
    check(near(
        continuous.normalize(std::numeric_limits<float>::infinity()), -2.0f));
    check(near(continuous.effective_external(-3.0f), -2.0f));
    check(near(continuous.effective_external(3.0f), 2.0f));
    check(near(
        continuous.effective_external(std::numeric_limits<float>::quiet_NaN()), -2.0f));
    check(near(
        continuous.effective_external(std::numeric_limits<float>::infinity()), -2.0f));
    check(near(continuous.fraction(-2.0f), 0.0f));
    check(near(continuous.fraction(0.0f), 0.5f));
    check(near(continuous.fraction(2.0f), 1.0f));
    check(near(continuous.value_from_fraction(0.75f), 1.0f));
    check(near(continuous.value_from_fraction(-1.0f), -2.0f));
    check(near(continuous.value_from_fraction(2.0f), 2.0f));
    check(near(
        continuous.value_from_fraction(std::numeric_limits<float>::quiet_NaN()), -2.0f));
    check(near(continuous.keyboard_increment(false), 0.04f));
    check(near(continuous.keyboard_increment(true), 0.004f));

    const SliderDomain stepped{0.0f, 1.0f, 0.25f};
    check(near(stepped.normalize(0.11f), 0.0f));
    check(near(stepped.normalize(0.14f), 0.25f));
    check(near(stepped.normalize(0.62f), 0.5f));
    check(near(stepped.normalize(0.64f), 0.75f));
    check(near(stepped.keyboard_increment(false), 0.25f));
    check(near(stepped.keyboard_increment(true), 0.25f));

    // External finite state is rendered from its clamped effective value, not
    // silently snapped to the user's step grid. Quantization is only applied to
    // values written by user interaction.
    check(near(stepped.effective_external(0.14f), 0.14f));
    check(near(stepped.fraction(0.14f), 0.14f));

    // The ticket fixes the order as quantize first, then clamp. With a step that
    // does not divide the range, the maximum can therefore quantize to the last
    // in-range grid value rather than being silently special-cased.
    const SliderDomain non_divisible{0.0f, 1.0f, 0.3f};
    check(near(non_divisible.normalize(1.0f), 0.9f));
    check(near(non_divisible.normalize(100.0f), 1.0f));

    // Mapping and normalization share one domain model and remain inverse for
    // representative continuous values.
    for (float fraction : {0.0f, 0.1f, 0.5f, 0.9f, 1.0f}) {
        const float value = continuous.value_from_fraction(fraction);
        check(near(continuous.fraction(value), fraction, 2.0e-6f));
    }

    // Finite float endpoints are valid even when their span is wider than a
    // representable float. Intermediate arithmetic must stay finite and keep
    // the center/keyboard mapping deterministic rather than overflowing.
    const float fmax = std::numeric_limits<float>::max();
    const SliderDomain wide{-fmax, fmax, 0.0f};
    check(std::isfinite(wide.fraction(0.0f)));
    check(near(wide.fraction(0.0f), 0.5f));
    check(std::isfinite(wide.raw_value_from_fraction(0.5f)));
    check(near(wide.raw_value_from_fraction(0.5f), 0.0f));
    check(std::isfinite(wide.keyboard_increment(false)));
    check(wide.keyboard_increment(false) > 0.0f);
    check(std::isfinite(wide.value_from_fraction(0.75f)));
    check(wide.value_from_fraction(0.75f) > 0.0f);

    return failures == 0 ? 0 : 1;
}
