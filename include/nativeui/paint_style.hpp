#pragma once

#include <nativeui/geometry.hpp>

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
struct ShaderBrushAccess;
struct ShaderBrushMaterializer;
struct ShaderBrushSnapshot;
}

struct VisualOutset {
    float left{};
    float top{};
    float right{};
    float bottom{};

    [[nodiscard]] static VisualOutset uniform(float value) noexcept {
        if (!std::isfinite(value) || value <= 0.0f) value = 0.0f;
        return {value, value, value, value};
    }
};

class Effect {
public:
    [[nodiscard]] static Effect gaussian_blur(float sigma_x, float sigma_y) noexcept {
        return Effect{Kind::GaussianBlur,
                      canonical_sigma(sigma_x),
                      canonical_sigma(sigma_y)};
    }

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

    Effect(const Effect&) noexcept = default;
    Effect& operator=(const Effect&) noexcept = default;
    Effect(Effect&&) noexcept = default;
    Effect& operator=(Effect&&) noexcept = default;
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

struct GradientStop {
    float offset{};
    Color color{};
};

enum class BlendMode {
    SourceOver,
    Multiply,
    Screen,
    Plus,
};

struct PaintOptions {
    float opacity{1.0f};
    BlendMode blend{BlendMode::SourceOver};
};

class LinearGradient {
public:
    LinearGradient(Point start, Point end, Color start_color, Color end_color)
        : start_(start), end_(end), stops_{{0.0f, start_color}, {1.0f, end_color}} {}

    LinearGradient(Point start, Point end, std::initializer_list<GradientStop> stops)
        : start_(start), end_(end), stops_(stops) {}

    [[nodiscard]] Point start() const noexcept { return start_; }
    [[nodiscard]] Point end() const noexcept { return end_; }
    [[nodiscard]] const std::vector<GradientStop>& stops() const noexcept { return stops_; }

    // Preserve the original two-stop convenience surface while generalized
    // gradients expose their complete immutable stop list through stops().
    [[nodiscard]] Color start_color() const noexcept {
        return stops_.empty() ? Color{} : stops_.front().color;
    }
    [[nodiscard]] Color end_color() const noexcept {
        return stops_.empty() ? Color{} : stops_.back().color;
    }

private:
    Point start_{};
    Point end_{};
    std::vector<GradientStop> stops_;
};

class RadialGradient {
public:
    RadialGradient(Point center, float radius, Color inner_color, Color outer_color)
        : center_(center), radius_(radius), stops_{{0.0f, inner_color}, {1.0f, outer_color}} {}

    RadialGradient(Point center, float radius, std::initializer_list<GradientStop> stops)
        : center_(center), radius_(radius), stops_(stops) {}

    [[nodiscard]] Point center() const noexcept { return center_; }
    [[nodiscard]] float radius() const noexcept { return radius_; }
    [[nodiscard]] const std::vector<GradientStop>& stops() const noexcept { return stops_; }

private:
    Point center_{};
    float radius_{};
    std::vector<GradientStop> stops_;
};

class Painter;

class Brush {
public:
    Brush(Color color) noexcept : value_(color) {}
    Brush(LinearGradient gradient) : value_(std::move(gradient)) {}
    Brush(RadialGradient gradient) : value_(std::move(gradient)) {}
    explicit Brush(const ShaderInstance& shader);

    Brush(const Brush&) = default;

    Brush& operator=(const Brush& other) {
        if (this == &other) return *this;
        Brush replacement{other};
        *this = std::move(replacement);
        return *this;
    }

    Brush(Brush&& other) noexcept : value_(std::move(other.value_)) {
        other.reset_to_transparent();
    }

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
