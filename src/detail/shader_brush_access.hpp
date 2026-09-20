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
};

} // namespace ui::detail
