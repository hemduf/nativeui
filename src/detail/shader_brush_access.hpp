#pragma once

#include <nativeui/paint_style.hpp>
#include <nativeui/shader.hpp>

#include <cstddef>
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
};

} // namespace ui::detail
