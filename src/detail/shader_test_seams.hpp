#pragma once

#include <cstddef>

namespace ui::detail {

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
[[nodiscard]] std::size_t shader_compile_call_count_for_test() noexcept;
#endif

} // namespace ui::detail
