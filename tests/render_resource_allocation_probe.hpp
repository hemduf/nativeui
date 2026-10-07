#pragma once

#include <cstddef>

namespace test::render_resource_allocations {

void begin() noexcept;
void end() noexcept;
[[nodiscard]] bool pause_counting() noexcept;
void resume_counting(bool previously_enabled) noexcept;
[[nodiscard]] std::size_t count() noexcept;
void print_first_allocation_trace() noexcept;

} // namespace test::render_resource_allocations
