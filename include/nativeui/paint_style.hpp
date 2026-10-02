/// \file
/// Backend-neutral paint values: brushes, gradients, effects and composition options.
#pragma once

#include <nativeui/geometry.hpp>
#include <nativeui/image.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace ui {

class ShaderInstance;

namespace detail {
struct EffectTestAccess;
struct EffectCacheAccess;
struct ShaderBrushAccess;
struct ImageTextureBrushAccess;
struct ShaderBrushMaterializer;
struct ShaderBrushSnapshot;
}

/// Conservative non-negative visual expansion in logical units.
struct VisualOutset {
    /// Extra logical extent required to the left of nominal paint bounds.
    float left{};
    /// Extra logical extent required above nominal paint bounds.
    float top{};
    /// Extra logical extent required to the right of nominal paint bounds.
    float right{};
    /// Extra logical extent required below nominal paint bounds.
    float bottom{};

    /// Equal expansion on every edge; non-finite/non-positive becomes zero.
    [[nodiscard]] static VisualOutset uniform(float value) noexcept {
        if (!std::isfinite(value) || value <= 0.0f) value = 0.0f;
        return {value, value, value, value};
    }
};

/// Immutable bounded layer-effect value.
///
/// Effect owns no renderer resource and is noexcept-copyable/movable. Factory
/// methods canonicalize numeric input before Painter materializes backend filters.
class Effect {
public:
    /// Gaussian blur sigma in logical/local units.
    ///
    /// Each axis canonicalizes to [0, 64]; non-finite/non-positive becomes zero.
    [[nodiscard]] static Effect gaussian_blur(float sigma_x, float sigma_y) noexcept {
        return Effect{Kind::GaussianBlur,
                      canonical_sigma(sigma_x),
                      canonical_sigma(sigma_y)};
    }

    /// Source plus drop shadow.
    ///
    /// Offset axes clamp to [-256, 256] logical units (non-finite -> 0), sigma
    /// canonicalizes to [0, 64], and color channels clamp to [0, 1]
    /// (non-finite -> 0).
    [[nodiscard]] static Effect drop_shadow(Point offset,
                                            float sigma,
                                            Color color) noexcept {
        sigma = canonical_sigma(sigma);
        return Effect{Kind::DropShadow,
                      sigma,
                      sigma,
                      canonical_offset(offset),
                      canonical_color(color)};
    }

    /// Shadow-only effect with the same input canonicalization as drop_shadow().
    [[nodiscard]] static Effect drop_shadow_only(Point offset,
                                                 float sigma,
                                                 Color color) noexcept {
        sigma = canonical_sigma(sigma);
        return Effect{Kind::DropShadowOnly,
                      sigma,
                      sigma,
                      canonical_offset(offset),
                      canonical_color(color)};
    }

    /// Conservative logical expansion for retained damage accounting.
    ///
    /// Blur uses three sigma per edge; shadows additionally include direction
    /// from the signed offset. Fully transparent shadow color yields zero outset.
    [[nodiscard]] VisualOutset visual_outset() const noexcept {
        if (kind_ == Kind::GaussianBlur) {
            return {3.0f * sigma_x_,
                    3.0f * sigma_y_,
                    3.0f * sigma_x_,
                    3.0f * sigma_y_};
        }

        if (color_.a <= 0.0f) return {};

        const float support = 3.0f * sigma_x_;
        return {
            std::max(0.0f, support - offset_.x),
            std::max(0.0f, support - offset_.y),
            std::max(0.0f, support + offset_.x),
            std::max(0.0f, support + offset_.y),
        };
    }

    /// Copy the complete backend-neutral value; no renderer resource is shared.
    Effect(const Effect&) noexcept = default;
    /// Replace this value without allocation or renderer interaction.
    Effect& operator=(const Effect&) noexcept = default;
    /// Move the complete value; no backend handle transfer is involved.
    Effect(Effect&&) noexcept = default;
    /// Move-assign without allocation or renderer interaction.
    Effect& operator=(Effect&&) noexcept = default;
    /// Destroy the value; no callback or renderer lifetime is involved.
    ~Effect() noexcept = default;

private:
    enum class Kind : unsigned char {
        GaussianBlur,
        DropShadow,
        DropShadowOnly,
    };

    Effect(Kind kind,
           float sigma_x,
           float sigma_y,
           Point offset = {},
           Color color = {}) noexcept
        : kind_(kind),
          sigma_x_(sigma_x),
          sigma_y_(sigma_y),
          offset_(offset),
          color_(color) {}

    [[nodiscard]] static float canonical_sigma(float sigma) noexcept {
        if (!std::isfinite(sigma) || sigma <= 0.0f) return 0.0f;
        return std::min(sigma, 64.0f);
    }

    [[nodiscard]] static float canonical_offset_axis(float value) noexcept {
        if (!std::isfinite(value)) return 0.0f;
        return std::clamp(value, -256.0f, 256.0f);
    }

    [[nodiscard]] static Point canonical_offset(Point value) noexcept {
        return {canonical_offset_axis(value.x), canonical_offset_axis(value.y)};
    }

    [[nodiscard]] static float canonical_color_channel(float value) noexcept {
        if (!std::isfinite(value)) return 0.0f;
        return std::clamp(value, 0.0f, 1.0f);
    }

    [[nodiscard]] static Color canonical_color(Color value) noexcept {
        return {canonical_color_channel(value.r),
                canonical_color_channel(value.g),
                canonical_color_channel(value.b),
                canonical_color_channel(value.a)};
    }

    friend class Painter;
    friend struct detail::EffectTestAccess;
    friend struct detail::EffectCacheAccess;

    Kind kind_{Kind::GaussianBlur};
    float sigma_x_{};
    float sigma_y_{};
    Point offset_{};
    Color color_{};
};

static_assert(std::is_nothrow_copy_constructible_v<Effect>);
static_assert(std::is_nothrow_copy_assignable_v<Effect>);
static_assert(std::is_nothrow_move_constructible_v<Effect>);
static_assert(std::is_nothrow_move_assignable_v<Effect>);
static_assert(std::is_nothrow_destructible_v<Effect>);

/// One gradient stop; `offset` is normalized to [0, 1].
///
/// Painter requires at least two finite strictly increasing offsets. Invalid
/// non-empty sequences fall back to the first stop color at materialization.
struct GradientStop {
    /// Normalized position. Materialization requires a finite value in [0, 1]
    /// that is strictly greater than the previous stop.
    float offset{};
    /// Color sampled at offset; copied and owned as part of the gradient value.
    Color color{};
};

/// Blend equation used by PaintOptions and scoped layers.
enum class BlendMode {
    /// Standard source-over alpha compositing.
    SourceOver,
    /// Multiply source and destination color components.
    Multiply,
    /// Screen source and destination color components.
    Screen,
    /// Add source and destination contributions.
    Plus,
};

/// Per-draw/layer composition options.
///
/// Painter clamps finite opacity to [0, 1]; non-finite opacity falls back to 1.
/// Blend defaults to SourceOver.
struct PaintOptions {
    /// Additional source opacity. Finite values clamp to [0, 1] at
    /// materialization; non-finite values fall back to 1.
    float opacity{1.0f};
    /// Compositing equation. Unknown enum payloads fall back to SourceOver.
    BlendMode blend{BlendMode::SourceOver};
};

/// Owned linear-gradient description in logical coordinates.
///
/// Initializer-list stops are copied into owned storage and may allocate.
/// Validation is deferred until Painter materialization.
class LinearGradient {
public:
    /// Convenience two-stop gradient from normalized offset 0 to 1.
    ///
    /// start/end use logical Painter coordinates. Colors are copied by value;
    /// no caller storage is retained. Constructing owned stops may allocate.
    LinearGradient(Point start, Point end, Color start_color, Color end_color)
        : start_(start), end_(end), stops_{{0.0f, start_color}, {1.0f, end_color}} {}

    /// Gradient with caller-supplied stops copied into owned storage.
    ///
    /// The initializer-list is borrowed only for construction. Stop ordering and
    /// range are validated later by Painter, not by this constructor.
    LinearGradient(Point start, Point end, std::initializer_list<GradientStop> stops)
        : start_(start), end_(end), stops_(stops) {}

    /// Start point in logical coordinates.
    [[nodiscard]] Point start() const noexcept { return start_; }
    /// End point in logical coordinates.
    [[nodiscard]] Point end() const noexcept { return end_; }
    /// Borrow the owned stop vector; assignment/destruction invalidates the reference.
    [[nodiscard]] const std::vector<GradientStop>& stops() const noexcept { return stops_; }

    // Preserve the original two-stop convenience surface while generalized
    // gradients expose their complete immutable stop list through stops().
    /// First stored stop color, or default Color{} (opaque black) when empty.
    [[nodiscard]] Color start_color() const noexcept {
        return stops_.empty() ? Color{} : stops_.front().color;
    }
    /// Last stored stop color, or default Color{} (opaque black) when empty.
    [[nodiscard]] Color end_color() const noexcept {
        return stops_.empty() ? Color{} : stops_.back().color;
    }

private:
    Point start_{};
    Point end_{};
    std::vector<GradientStop> stops_;
};

/// Owned radial-gradient description in logical coordinates.
///
/// Radius is a logical length. A non-positive/non-finite radius cannot produce
/// a radial shader and painting falls back to the first stop color.
class RadialGradient {
public:
    /// Convenience two-stop radial gradient from normalized offset 0 to 1.
    ///
    /// center/radius use logical Painter units. Radius is stored verbatim and is
    /// validated only when Painter materializes the gradient.
    RadialGradient(Point center, float radius, Color inner_color, Color outer_color)
        : center_(center), radius_(radius), stops_{{0.0f, inner_color}, {1.0f, outer_color}} {}

    /// Radial gradient with caller-supplied stops copied into owned storage.
    ///
    /// The initializer-list is borrowed only for construction and copied before
    /// return. Radius and stop validation are deferred to Painter.
    RadialGradient(Point center, float radius, std::initializer_list<GradientStop> stops)
        : center_(center), radius_(radius), stops_(stops) {}

    /// Center in logical coordinates.
    [[nodiscard]] Point center() const noexcept { return center_; }
    /// Radius in logical units.
    [[nodiscard]] float radius() const noexcept { return radius_; }
    /// Borrow the owned stop vector; assignment/destruction invalidates the reference.
    [[nodiscard]] const std::vector<GradientStop>& stops() const noexcept { return stops_; }

private:
    Point center_{};
    float radius_{};
    std::vector<GradientStop> stops_;
};

class Painter;

/// Owned/snapshotted fill source accepted by Painter.
///
/// Color/gradients/ImageTexture are stored by value; ShaderInstance construction
/// captures an immutable shader snapshot. Invalid ImageTexture becomes
/// transparent. Copying may allocate for owned gradient vectors. Moving is
/// noexcept and leaves the source valid and transparent.
class Brush {
public:
    /// Construct a solid-color brush.
    Brush(Color color) noexcept : value_(color) {}
    /// Take ownership of a linear-gradient value.
    Brush(LinearGradient gradient) : value_(std::move(gradient)) {}
    /// Take ownership of a radial-gradient value.
    Brush(RadialGradient gradient) : value_(std::move(gradient)) {}
    /// Snapshot current ShaderInstance bindings into an immutable brush source.
    ///
    /// shader is borrowed only during construction. Later ShaderInstance mutation
    /// does not affect this Brush; snapshot allocation errors propagate.
    explicit Brush(const ShaderInstance& shader);
    /// Take an ImageTexture value; invalid textures become transparent.
    explicit Brush(ImageTexture texture) noexcept
        : value_(texture.valid()
            ? Storage{std::move(texture)}
            : Storage{transparent()}) {}

    /// Copy the owned source; may allocate for gradient stop vectors.
    Brush(const Brush&) = default;

    /// Copy-assign through a replacement value; allocation may occur before commit.
    Brush& operator=(const Brush& other) {
        if (this == &other) return *this;
        Brush replacement{other};
        *this = std::move(replacement);
        return *this;
    }

    /// Move the source and leave `other` transparent.
    Brush(Brush&& other) noexcept : value_(transparent()) {
        // Construct the variant in a known active alternative before moving the
        // payload. Besides preserving the zero-allocation move contract, this
        // avoids GCC's false-positive maybe-uninitialized diagnostic when a
        // Brush is moved into a closure at -O3/-Werror.
        value_ = std::move(other.value_);
        other.reset_to_transparent();
    }

    /// Move-assign; source becomes transparent and self-move also becomes transparent.
    Brush& operator=(Brush&& other) noexcept {
        if (this == &other) {
            reset_to_transparent();
            return *this;
        }
        value_ = std::move(other.value_);
        other.reset_to_transparent();
        return *this;
    }

    ~Brush() noexcept = default;

private:
    using Storage = std::variant<
        Color,
        LinearGradient,
        RadialGradient,
        ImageTexture,
        std::shared_ptr<const detail::ShaderBrushSnapshot>>;

    static_assert(std::is_nothrow_constructible_v<Storage, Color>);
    static_assert(std::is_nothrow_move_constructible_v<Storage>);
    static_assert(std::is_nothrow_move_assignable_v<Storage>);
    static_assert(std::is_nothrow_destructible_v<Storage>);

    [[nodiscard]] static constexpr Color transparent() noexcept {
        return Color{0.0f, 0.0f, 0.0f, 0.0f};
    }

    void reset_to_transparent() noexcept {
        Storage replacement{transparent()};
        value_ = std::move(replacement);
    }

    template <class Visitor>
    decltype(auto) visit(Visitor&& visitor) const {
        return std::visit(std::forward<Visitor>(visitor), value_);
    }

    friend class Painter;
    friend struct detail::ShaderBrushAccess;
    friend struct detail::ImageTextureBrushAccess;
    friend struct detail::ShaderBrushMaterializer;
    Storage value_;
};

static_assert(std::is_nothrow_move_constructible_v<LinearGradient>);
static_assert(std::is_nothrow_move_assignable_v<LinearGradient>);
static_assert(std::is_nothrow_move_constructible_v<RadialGradient>);
static_assert(std::is_nothrow_move_assignable_v<RadialGradient>);
static_assert(std::is_nothrow_constructible_v<Brush, Color>);
static_assert(std::is_nothrow_move_constructible_v<Brush>);
static_assert(std::is_nothrow_move_assignable_v<Brush>);
static_assert(std::is_nothrow_destructible_v<Brush>);

} // namespace ui
