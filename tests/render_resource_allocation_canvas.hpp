#pragma once

#include "include/core/SkCanvas.h"

#include <memory>

namespace test {

// Construct outside the allocation probe; forward every draw to the real target.
[[nodiscard]] std::unique_ptr<SkCanvas> make_allocation_observing_canvas(
    SkCanvas& target);

} // namespace test
