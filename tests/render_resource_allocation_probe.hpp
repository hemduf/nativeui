#pragma once

#include <cstddef>

namespace test::render_resource_allocations {

void begin() noexcept;
void end() noexcept;
[[nodiscard]] std::size_t count() noexcept;

} // namespace test::render_resource_allocations
