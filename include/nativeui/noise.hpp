#pragma once

#include <nativeui/paint_style.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace ui {

enum class NoiseType { Value, Perlin, Simplex, WorleyF1, WorleyF2 };

/// feature_size is the number of logical Painter pixels per noise cell.
/// It must be finite and positive; seed is an exact, repeatable 32-bit value.
struct NoiseOptions {
    float feature_size{64.0f};
    std::uint32_t seed{0};
};

enum class FractalNoiseMode {
    FBm,
    Turbulence,
    Ridged,
};

class FractalNoiseOptions final {
public:
    FractalNoiseOptions& set_octaves(std::uint8_t value) noexcept {
        octaves_ = value;
        return *this;
    }

    FractalNoiseOptions& set_lacunarity(float value) noexcept {
        lacunarity_ = value;
        return *this;
    }

    FractalNoiseOptions& set_gain(float value) noexcept {
        gain_ = value;
        return *this;
    }

    FractalNoiseOptions& set_mode(FractalNoiseMode value) noexcept {
        mode_ = value;
        return *this;
    }

    [[nodiscard]] std::uint8_t octaves() const noexcept { return octaves_; }
    [[nodiscard]] float lacunarity() const noexcept { return lacunarity_; }
    [[nodiscard]] float gain() const noexcept { return gain_; }
    [[nodiscard]] FractalNoiseMode mode() const noexcept { return mode_; }

private:
    std::uint8_t octaves_{4};
    float lacunarity_{2.0f};
    float gain_{0.5f};
    FractalNoiseMode mode_{FractalNoiseMode::FBm};
};

enum class NoiseCreateError {
    None,
    InvalidArgument,
    BackendCompileFailed,
};

struct NoiseCreateResult;

namespace detail {
struct NoiseSourceData;
}

/// Immutable built-in procedural source. Creation compiles its Skia effect;
/// brushes only bind values and never compile source during paint.
/// A default or failed source produces a transparent solid Brush.
class NoiseSource final {
public:
    NoiseSource() noexcept = default;

    [[nodiscard]] static NoiseCreateResult create(NoiseType type,
                                                  NoiseOptions options = {});
    [[nodiscard]] static NoiseCreateResult create_fractal(
        NoiseType base,
        NoiseOptions base_options = {},
        FractalNoiseOptions fractal = {});
    /// Color channels are clamped to [0,1], with non-finite channels set to 0.
    /// Interpolation occurs before premultiplication in the renderer space.
    [[nodiscard]] Brush as_brush(Color low = {0, 0, 0, 1},
                                 Color high = {1, 1, 1, 1}) const;

private:
    friend struct NoiseCreateResult;
    std::shared_ptr<const detail::NoiseSourceData> data_;
};

struct NoiseCreateResult {
    NoiseSource noise;
    NoiseCreateError error{NoiseCreateError::None};
    std::string diagnostic;

    [[nodiscard]] bool ok() const noexcept {
        return error == NoiseCreateError::None && noise.data_ != nullptr;
    }
};

} // namespace ui
