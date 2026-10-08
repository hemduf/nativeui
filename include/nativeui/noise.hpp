#pragma once

#include <nativeui/paint_style.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace ui {

/// Built-in deterministic procedural noise algorithms.
enum class NoiseType {
    /// Interpolated lattice values.
    Value,
    /// Gradient Perlin noise.
    Perlin,
    /// Simplex gradient noise.
    Simplex,
    /// Cellular/Worley first-nearest feature distance.
    WorleyF1,
    /// Cellular/Worley second-nearest feature distance.
    WorleyF2
};

/// Base options shared by all built-in noise sources.
struct NoiseOptions {
    /// Number of logical Painter pixels per base noise cell. Creation rejects
    /// non-finite and non-positive values.
    float feature_size{64.0f};
    /// Exact deterministic 32-bit seed. Reusing the same seed/options produces
    /// the same procedural field for a given noise algorithm.
    std::uint32_t seed{0};
};

/// How octave samples are combined by `NoiseSource::create_fractal`.
enum class FractalNoiseMode {
    /// Weighted signed octave sum, normalized and remapped to [0,1].
    FBm,
    /// Weighted absolute signed octave values, normalized to [0,1].
    Turbulence,
    /// Weighted squared ridges `(1 - abs(signed_noise))^2`, normalized to [0,1].
    Ridged,
};

/// Value-only fractal configuration.
///
/// Setters deliberately store values without validation or canonicalization.
/// `NoiseSource::create_fractal` validates the complete option set atomically:
/// octaves 1..6, lacunarity 1..4, gain 0..1, all floating values finite.
class FractalNoiseOptions final {
public:
    /// Sets octave count. Valid creation range is 1..6.
    FractalNoiseOptions& set_octaves(std::uint8_t value) noexcept {
        octaves_ = value;
        return *this;
    }

    /// Sets the per-octave frequency multiplier. Valid creation range is
    /// finite [1,4].
    FractalNoiseOptions& set_lacunarity(float value) noexcept {
        lacunarity_ = value;
        return *this;
    }

    /// Sets the per-octave amplitude multiplier. Valid creation range is
    /// finite [0,1].
    FractalNoiseOptions& set_gain(float value) noexcept {
        gain_ = value;
        return *this;
    }

    /// Sets the octave-combination mode. Invalid enum values are rejected by
    /// `create_fractal`.
    FractalNoiseOptions& set_mode(FractalNoiseMode value) noexcept {
        mode_ = value;
        return *this;
    }

    /// Returns the stored octave count without validation.
    [[nodiscard]] std::uint8_t octaves() const noexcept { return octaves_; }
    /// Returns the stored lacunarity exactly.
    [[nodiscard]] float lacunarity() const noexcept { return lacunarity_; }
    /// Returns the stored gain exactly.
    [[nodiscard]] float gain() const noexcept { return gain_; }
    /// Returns the stored combination mode.
    [[nodiscard]] FractalNoiseMode mode() const noexcept { return mode_; }

private:
    std::uint8_t octaves_{4};
    float lacunarity_{2.0f};
    float gain_{0.5f};
    FractalNoiseMode mode_{FractalNoiseMode::FBm};
};

/// Result category for procedural-source creation.
enum class NoiseCreateError {
    /// Source creation succeeded.
    None,
    /// The requested algorithm/options violate the public creation contract.
    InvalidArgument,
    /// NativeUI's internal shader backend failed to compile the built-in source.
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

    /// Creates one immutable single-octave built-in source.
    ///
    /// All declared `NoiseType` values are supported. `feature_size` must be
    /// finite and positive. Creation performs backend shader preparation and
    /// may allocate; it is not an audio-real-time operation.
    [[nodiscard]] static NoiseCreateResult create(NoiseType type,
                                                  NoiseOptions options = {});
    /// Creates a compiled multi-octave source from Value, Perlin, or Simplex
    /// base noise. Worley bases are intentionally rejected.
    ///
    /// Validation is atomic: feature size must be finite/positive, octaves
    /// 1..6, lacunarity finite [1,4], gain finite [0,1], and mode valid.
    /// Creation compiles one combined built-in shader independent of octave
    /// count and may allocate; it is not an audio-real-time operation.
    [[nodiscard]] static NoiseCreateResult create_fractal(
        NoiseType base,
        NoiseOptions base_options = {},
        FractalNoiseOptions fractal = {});
    /// Creates a brush mapping noise value 0 to `low` and 1 to `high`.
    ///
    /// Color channels are clamped to [0,1], with non-finite channels set to 0.
    /// Interpolation occurs before premultiplication in renderer space. A
    /// default/failed `NoiseSource` returns a transparent solid brush.
    [[nodiscard]] Brush as_brush(Color low = {0, 0, 0, 1},
                                 Color high = {1, 1, 1, 1}) const;

private:
    friend struct NoiseCreateResult;
    std::shared_ptr<const detail::NoiseSourceData> data_;
};

/// Complete creation result. Check `ok()` before using `noise` as a prepared
/// source; `diagnostic` is populated for backend compile failures when
/// available.
struct NoiseCreateResult {
    /// Prepared immutable source on success; default/empty on failure.
    NoiseSource noise;
    /// Stable error category for callers.
    NoiseCreateError error{NoiseCreateError::None};
    /// Human-readable backend diagnostic when one exists.
    std::string diagnostic;

    /// Returns true only when the error category is `None` and the immutable
    /// source payload exists.
    [[nodiscard]] bool ok() const noexcept {
        return error == NoiseCreateError::None && noise.data_ != nullptr;
    }
};

} // namespace ui
