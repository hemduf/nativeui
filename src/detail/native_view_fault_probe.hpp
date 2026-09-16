#pragma once

#include <nativeui/window.hpp>

#include <cstdint>

namespace ui::detail {

enum class NativeViewConstructionFaultStage : std::uint8_t {
    AfterViewCreation,
    AfterRealize,
    AfterImeCreate,
    AfterSizeConstraints,
    AfterInitialResize,
    AfterShow,
    AfterInvalidationCallback,
};

struct NativeViewConstructionFaultResult {
    bool injected_failure{};
    bool unexpected_failure{};
    bool unexpected_success{};
    std::uint32_t world_acquired{};
    std::uint32_t world_released{};
    std::uint32_t view_acquired{};
    std::uint32_t view_released{};
    std::uint32_t realize_succeeded{};
    std::uint32_t unrealize_released{};
    std::uint32_t ime_acquired{};
    std::uint32_t ime_released{};
    std::uint32_t size_constraints_applied{};
    std::uint32_t initial_resize_completed{};
    std::uint32_t show_completed{};
    std::uint32_t invalidation_attached{};
    std::uint32_t invalidation_cleared{};
};

NativeViewConstructionFaultResult exercise_native_view_construction_fault(
    UI& ui,
    PlatformServices& services,
    NativeParentHandle parent,
    Size size,
    NativeViewConstructionFaultStage stage) noexcept;

} // namespace ui::detail
