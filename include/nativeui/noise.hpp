#pragma once

#include <nativeui/paint_style.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace ui {

enum class NoiseType { Value, Perlin, Simplex };

/// feature_size is the number of logical Painter pixels per noise cell.
/// It must be finite and positive; seed is an exact, repeatable 32-bit value.
struct NoiseOptions {
    float feature_size{64.0f};
    std::uint32_t seed{0};
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
