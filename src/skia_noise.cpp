#include <nativeui/noise.hpp>
#include <nativeui/shader.hpp>

#include "detail/noise_sksl.hpp"
#include "detail/noise_test_seams.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace ui::detail {

struct NoiseSourceData final {
    NoiseSourceData(std::shared_ptr<const ShaderProgram> compiled,
                    NoiseOptions configuration) noexcept
        : program(std::move(compiled)), options(configuration) {}

    std::shared_ptr<const ShaderProgram> program;
    NoiseOptions options;
};

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
namespace {
NoiseCreationFailurePoint creation_failure_point = NoiseCreationFailurePoint::None;
}

void set_noise_creation_failure_for_test(NoiseCreationFailurePoint point) noexcept {
    creation_failure_point = point;
}
#endif

} // namespace ui::detail

namespace ui {

namespace {

[[nodiscard]] constexpr std::array<float, 4> seed_bytes(std::uint32_t seed) noexcept {
    return {static_cast<float>(seed & 255u),
            static_cast<float>((seed >> 8) & 255u),
            static_cast<float>((seed >> 16) & 255u),
            static_cast<float>((seed >> 24) & 255u)};
}

[[nodiscard]] float canonical_channel(float channel) noexcept {
    if (!std::isfinite(channel)) return 0.0f;
    return std::clamp(channel, 0.0f, 1.0f);
}

[[nodiscard]] Color canonical_color(Color color) noexcept {
    return {canonical_channel(color.r),
            canonical_channel(color.g),
            canonical_channel(color.b),
            canonical_channel(color.a)};
}

} // namespace

NoiseCreateResult NoiseSource::create(NoiseType type, NoiseOptions options) {
    if ((type != NoiseType::Value && type != NoiseType::Perlin &&
         type != NoiseType::Simplex) ||
        !std::isfinite(options.feature_size) || options.feature_size <= 0.0f) {
        return {NoiseSource{}, NoiseCreateError::InvalidArgument, {}};
    }

    std::string source{detail::kNoiseHashSkSL};
    if (type == NoiseType::Simplex) {
        source.append(detail::kSimplexNoiseKernelSkSL);
        source.append(detail::kSimplexNoiseMainSkSL);
    } else if (type == NoiseType::Perlin) {
        source.append(detail::kPerlinNoiseKernelSkSL);
        source.append(detail::kPerlinNoiseMainSkSL);
    } else {
        source.append(detail::kValueNoiseKernelSkSL);
        source.append(detail::kValueNoiseMainSkSL);
    }
#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    const auto failure = detail::creation_failure_point;
    detail::creation_failure_point = detail::NoiseCreationFailurePoint::None;
    if (failure == detail::NoiseCreationFailurePoint::Compile) {
        source = "half4 main(float2 p) { return missing_noise_builtin; }";
    }
#endif

    auto compiled = ShaderProgram::compile(source);
    if (!compiled.ok()) {
        const char* const fallback = type == NoiseType::Simplex
            ? "Built-in simplex-noise shader compilation failed"
            : type == NoiseType::Perlin
                ? "Built-in perlin-noise shader compilation failed"
                : "Built-in value-noise shader compilation failed";
        std::string diagnostic = compiled.diagnostics.empty()
            ? fallback
            : compiled.diagnostics.front().message;
        if (diagnostic.empty()) diagnostic = fallback;
        return {NoiseSource{}, NoiseCreateError::BackendCompileFailed,
                std::move(diagnostic)};
    }

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    if (failure == detail::NoiseCreationFailurePoint::AfterCompileAllocation) {
        throw std::bad_alloc{};
    }
#endif

    NoiseSource noise;
    noise.data_ = std::make_shared<const detail::NoiseSourceData>(
        std::move(compiled.program), options);
    return {std::move(noise), NoiseCreateError::None, {}};
}

Brush NoiseSource::as_brush(Color low, Color high) const {
    if (!data_) return Brush{Color{0.0f, 0.0f, 0.0f, 0.0f}};

    ShaderInstance shader{data_->program};
    const bool bound =
        shader.set_float("feature_size", data_->options.feature_size) ==
            ShaderSetResult::Ok &&
        shader.set_float4("seed_bytes", seed_bytes(data_->options.seed)) ==
            ShaderSetResult::Ok &&
        shader.set_color("low_color", canonical_color(low)) ==
            ShaderSetResult::Ok &&
        shader.set_color("high_color", canonical_color(high)) ==
            ShaderSetResult::Ok;
    if (!bound) throw std::logic_error{"Built-in noise shader interface mismatch"};
    return Brush{shader};
}

} // namespace ui
