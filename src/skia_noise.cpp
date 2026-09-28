#include <nativeui/noise.hpp>
#include <nativeui/shader.hpp>

#include "detail/noise_sksl.hpp"
#include "detail/noise_test_seams.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
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

[[nodiscard]] bool valid_fractal_base(NoiseType base) noexcept {
    return base == NoiseType::Value || base == NoiseType::Perlin ||
           base == NoiseType::Simplex;
}

[[nodiscard]] bool valid_fractal_mode(FractalNoiseMode mode) noexcept {
    return mode == FractalNoiseMode::FBm ||
           mode == FractalNoiseMode::Turbulence ||
           mode == FractalNoiseMode::Ridged;
}

[[nodiscard]] std::string sksl_float_literal(float value) {
    char buffer[64]{};
    const auto [end, error] = std::to_chars(
        buffer, buffer + sizeof(buffer), value, std::chars_format::general,
        std::numeric_limits<float>::max_digits10);
    if (error != std::errc{}) {
        throw std::logic_error{"Failed to format fractal noise scalar"};
    }
    std::string result{buffer, end};
    if (result.find_first_of(".eE") == std::string::npos) result.append(".0");
    return result;
}

void append_fractal_kernel(std::string& source,
                           NoiseType base,
                           const FractalNoiseOptions& options) {
    const char* base_function =
        base == NoiseType::Value ? "value_noise" :
        base == NoiseType::Perlin ? "perlin_noise" : "simplex_noise";
    const std::string lacunarity = sksl_float_literal(options.lacunarity());
    const std::string gain = sksl_float_literal(options.gain());

    source.append("\nfloat fractal_base(float2 p) { return ");
    source.append(base_function);
    source.append("(p); }\n");
    source.append("float fractal_noise(float2 p) {\n");
    source.append("    float frequency = 1.0;\n");
    source.append("    float amplitude = 1.0;\n");
    source.append("    float weight_sum = 0.0;\n");
    source.append("    float total = 0.0;\n");

    for (std::uint8_t octave = 0; octave < options.octaves(); ++octave) {
        const std::string suffix = std::to_string(unsigned(octave));
        source.append("    float n");
        source.append(suffix);
        source.append(" = fractal_base(p * frequency);\n");
        source.append("    float s");
        source.append(suffix);
        source.append(" = 2.0 * n");
        source.append(suffix);
        source.append(" - 1.0;\n");

        switch (options.mode()) {
            case FractalNoiseMode::FBm:
                source.append("    total = total + amplitude * s");
                source.append(suffix);
                source.append(";\n");
                break;
            case FractalNoiseMode::Turbulence:
                source.append("    total = total + amplitude * abs(s");
                source.append(suffix);
                source.append(");\n");
                break;
            case FractalNoiseMode::Ridged:
                source.append("    float ridge");
                source.append(suffix);
                source.append(" = 1.0 - abs(s");
                source.append(suffix);
                source.append(");\n");
                source.append("    total = total + amplitude * ridge");
                source.append(suffix);
                source.append(" * ridge");
                source.append(suffix);
                source.append(";\n");
                break;
        }

        source.append("    weight_sum = weight_sum + amplitude;\n");
        source.append("    frequency = frequency * ");
        source.append(lacunarity);
        source.append(";\n");
        source.append("    amplitude = amplitude * ");
        source.append(gain);
        source.append(";\n");
    }

    if (options.mode() == FractalNoiseMode::FBm) {
        source.append(
            "    return clamp(0.5 + 0.5 * (total / weight_sum), 0.0, 1.0);\n");
    } else {
        source.append("    return clamp(total / weight_sum, 0.0, 1.0);\n");
    }
    source.append("}\n");
    source.append(
        "half4 main(float2 p) {\n"
        "    float v = fractal_noise(p);\n"
        "    float4 color = mix(low_color, high_color, v);\n"
        "    return half4(color.rgb * color.a, color.a);\n"
        "}\n");
}

} // namespace

NoiseCreateResult NoiseSource::create(NoiseType type, NoiseOptions options) {
    if ((type != NoiseType::Value && type != NoiseType::Perlin &&
         type != NoiseType::Simplex && type != NoiseType::WorleyF1 &&
         type != NoiseType::WorleyF2) ||
        !std::isfinite(options.feature_size) || options.feature_size <= 0.0f) {
        return {NoiseSource{}, NoiseCreateError::InvalidArgument, {}};
    }

    std::string source{detail::kNoiseHashSkSL};
    if (type == NoiseType::WorleyF1 || type == NoiseType::WorleyF2) {
        source.append(detail::kWorleyNoiseKernelSkSL);
        source.append(type == NoiseType::WorleyF1
                          ? detail::kWorleyF1MainSkSL
                          : detail::kWorleyF2MainSkSL);
    } else if (type == NoiseType::Simplex) {
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
        const char* const fallback =
            (type == NoiseType::WorleyF1 || type == NoiseType::WorleyF2)
                ? "Built-in Worley-noise shader compilation failed"
                : type == NoiseType::Simplex
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

NoiseCreateResult NoiseSource::create_fractal(
    NoiseType base,
    NoiseOptions base_options,
    FractalNoiseOptions fractal) {
    if (!valid_fractal_base(base) ||
        !std::isfinite(base_options.feature_size) ||
        base_options.feature_size <= 0.0f ||
        fractal.octaves() < 1 || fractal.octaves() > 6 ||
        !std::isfinite(fractal.lacunarity()) ||
        fractal.lacunarity() < 1.0f || fractal.lacunarity() > 4.0f ||
        !std::isfinite(fractal.gain()) ||
        fractal.gain() < 0.0f || fractal.gain() > 1.0f ||
        !valid_fractal_mode(fractal.mode())) {
        return {NoiseSource{}, NoiseCreateError::InvalidArgument, {}};
    }

    std::string source{detail::kNoiseHashSkSL};
    if (base == NoiseType::Simplex) {
        source.append(detail::kSimplexNoiseKernelSkSL);
    } else if (base == NoiseType::Perlin) {
        source.append(detail::kPerlinNoiseKernelSkSL);
    } else {
        source.append(detail::kValueNoiseKernelSkSL);
    }
    append_fractal_kernel(source, base, fractal);

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    const auto failure = detail::creation_failure_point;
    detail::creation_failure_point = detail::NoiseCreationFailurePoint::None;
    if (failure == detail::NoiseCreationFailurePoint::Compile) {
        source = "half4 main(float2 p) { return missing_fractal_noise_builtin; }";
    }
#endif

    auto compiled = ShaderProgram::compile(source);
    if (!compiled.ok()) {
        const char* const fallback =
            "Built-in fractal-noise shader compilation failed";
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
        std::move(compiled.program), base_options);
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
