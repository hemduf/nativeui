#pragma once

#include <nativeui/shader.hpp>

#include <cstddef>
#include <span>

namespace ui::detail {

/// Private renderer/test access to a borrowed read-only view of logical binding
/// bytes. The view is valid only while the ShaderInstance remains alive and is
/// not mutated; snapshot users must copy it before retaining state.
///
/// This is intentionally not installed as a public API surface. T081 may reuse
/// the same access boundary while creating an owning Brush snapshot.
struct ShaderInstanceAccess {
    [[nodiscard]] static std::span<const std::byte> binding_bytes(
        const ShaderInstance& instance) noexcept {
        return instance.binding_bytes();
    }
};

} // namespace ui::detail
