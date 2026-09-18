#pragma once

#include <nativeui/shader.hpp>

#include <cstddef>
#include <span>

namespace ui::detail {

/// Private renderer/test access to the immutable logical binding bytes.
///
/// This is intentionally not installed as a public API surface. T081 may reuse
/// the same access boundary when snapshotting ShaderInstance into Brush.
struct ShaderInstanceAccess {
    [[nodiscard]] static std::span<const std::byte> binding_bytes(
        const ShaderInstance& instance) noexcept {
        return instance.binding_bytes();
    }
};

} // namespace ui::detail
