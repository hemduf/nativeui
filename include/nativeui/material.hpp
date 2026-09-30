#pragma once

#include <nativeui/paint_style.hpp>
#include <nativeui/scalar_source.hpp>

#include <type_traits>
#include <utility>

namespace ui {

class Material final {
public:
    Material() noexcept = default;

    explicit Material(Brush albedo)
        : albedo_(std::move(albedo)) {}

    Material(const Material&) = default;

    Material& operator=(const Material& other) {
        if (this == &other) return *this;
        Material replacement{other};
        *this = std::move(replacement);
        return *this;
    }

    Material(Material&& other) noexcept
        : albedo_(std::move(other.albedo_)),
          roughness_(std::move(other.roughness_)),
          metallic_(std::move(other.metallic_)),
          has_emissive_(other.has_emissive_),
          emissive_color_(std::move(other.emissive_color_)),
          emissive_intensity_(std::move(other.emissive_intensity_)) {
        other.reset_to_default();
    }

    Material& operator=(Material&& other) noexcept {
        if (this == &other) {
            reset_to_default();
            return *this;
        }

        albedo_ = std::move(other.albedo_);
        roughness_ = std::move(other.roughness_);
        metallic_ = std::move(other.metallic_);
        has_emissive_ = other.has_emissive_;
        emissive_color_ = std::move(other.emissive_color_);
        emissive_intensity_ = std::move(other.emissive_intensity_);
        other.reset_to_default();
        return *this;
    }

    ~Material() noexcept = default;

    Material& set_albedo(Brush albedo) {
        albedo_ = std::move(albedo);
        return *this;
    }

    Material& set_roughness(ScalarSource roughness) {
        roughness_ = std::move(roughness);
        return *this;
    }

    Material& set_roughness(float roughness) noexcept {
        roughness_ = ScalarSource{roughness};
        return *this;
    }

    Material& set_metallic(ScalarSource metallic) {
        metallic_ = std::move(metallic);
        return *this;
    }

    Material& set_metallic(float metallic) noexcept {
        metallic_ = ScalarSource{metallic};
        return *this;
    }

    Material& set_emissive(
        Brush color,
        ScalarSource intensity = ScalarSource{1.0f}) {
        // Both by-value arguments are fully prepared before entering this
        // function. Publication below consists only of no-throw moves.
        emissive_color_ = std::move(color);
        emissive_intensity_ = std::move(intensity);
        has_emissive_ = true;
        return *this;
    }

    Material& set_emissive(Brush color, float intensity) noexcept {
        emissive_color_ = std::move(color);
        emissive_intensity_ = ScalarSource{intensity};
        has_emissive_ = true;
        return *this;
    }

    Material& clear_emissive() noexcept {
        has_emissive_ = false;
        emissive_color_ = transparent_brush();
        emissive_intensity_ = ScalarSource{0.0f};
        return *this;
    }

    [[nodiscard]] const Brush& albedo() const noexcept {
        return albedo_;
    }

    [[nodiscard]] const ScalarSource& roughness() const noexcept {
        return roughness_;
    }

    [[nodiscard]] const ScalarSource& metallic() const noexcept {
        return metallic_;
    }

    [[nodiscard]] bool has_emissive() const noexcept {
        return has_emissive_;
    }

    [[nodiscard]] const Brush& emissive_color() const noexcept {
        return emissive_color_;
    }

    [[nodiscard]] const ScalarSource& emissive_intensity() const noexcept {
        return emissive_intensity_;
    }

private:
    [[nodiscard]] static Brush white_brush() noexcept {
        return Brush{Color{1.0f, 1.0f, 1.0f, 1.0f}};
    }

    [[nodiscard]] static Brush transparent_brush() noexcept {
        return Brush{Color{0.0f, 0.0f, 0.0f, 0.0f}};
    }

    void reset_to_default() noexcept {
        albedo_ = white_brush();
        roughness_ = ScalarSource{0.5f};
        metallic_ = ScalarSource{0.0f};
        has_emissive_ = false;
        emissive_color_ = transparent_brush();
        emissive_intensity_ = ScalarSource{0.0f};
    }

    Brush albedo_{white_brush()};
    ScalarSource roughness_{0.5f};
    ScalarSource metallic_{0.0f};
    bool has_emissive_{false};
    Brush emissive_color_{transparent_brush()};
    ScalarSource emissive_intensity_{0.0f};
};

static_assert(std::is_nothrow_default_constructible_v<Material>);
static_assert(std::is_nothrow_move_constructible_v<Material>);
static_assert(std::is_nothrow_move_assignable_v<Material>);
static_assert(std::is_nothrow_destructible_v<Material>);

} // namespace ui
