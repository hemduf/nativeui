#pragma once

namespace ui::detail {

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
enum class NoiseCreationFailurePoint {
    None,
    Compile,
    AfterCompileAllocation,
};

void set_noise_creation_failure_for_test(NoiseCreationFailurePoint point) noexcept;
#endif

} // namespace ui::detail
