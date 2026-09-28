#pragma once

#include <nativeui/paint_style.hpp>
#include <nativeui/shader.hpp>

#include <cstddef>
#include <memory>
#include <span>

namespace ui::detail {

struct ShaderBrushAccess {
    [[nodiscard]] static bool is_shader(const Brush& brush) noexcept;
    [[nodiscard]] static bool is_transparent_solid(const Brush& brush) noexcept;
    [[nodiscard]] static const ShaderProgram* program(const Brush& brush) noexcept;
    [[nodiscard]] static std::span<const std::byte> binding_bytes(
        const Brush& brush) noexcept;
    [[nodiscard]] static std::size_t depth(const Brush& brush) noexcept;
    [[nodiscard]] static std::size_t child_count(const Brush& brush) noexcept;
    [[nodiscard]] static const Brush* child(
        const Brush& brush,
        std::size_t index) noexcept;

    [[nodiscard]] static const std::shared_ptr<const ShaderBrushSnapshot>*
    snapshot(const Brush& brush) noexcept;

    [[nodiscard]] static std::size_t semantic_hash(
        const std::shared_ptr<const ShaderBrushSnapshot>& snapshot) noexcept;

    [[nodiscard]] static bool semantic_equal(
        const std::shared_ptr<const ShaderBrushSnapshot>& a,
        const std::shared_ptr<const ShaderBrushSnapshot>& b) noexcept;

    [[nodiscard]] static std::size_t semantic_hash(
        const Brush& brush) noexcept;

    [[nodiscard]] static bool semantic_equal(
        const Brush& a,
        const Brush& b) noexcept;
};

} // namespace ui::detail
