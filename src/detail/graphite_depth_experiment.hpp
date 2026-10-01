#pragma once

#include "include/gpu/graphite/ContextOptions.h"

namespace ui::detail {

enum class GraphiteDepthMode {
    Default,
    AvoidDepth,
};

class GraphiteDepthExperiment final {
public:
    constexpr explicit GraphiteDepthExperiment(
        GraphiteDepthMode mode = GraphiteDepthMode::Default) noexcept
        : mode_(mode) {}

    void configure(skgpu::graphite::ContextOptions& options) const noexcept {
        options.fAvoidDepthMode = mode_ == GraphiteDepthMode::AvoidDepth;
    }

    [[nodiscard]] constexpr GraphiteDepthMode mode() const noexcept {
        return mode_;
    }

    [[nodiscard]] constexpr bool avoids_depth() const noexcept {
        return mode_ == GraphiteDepthMode::AvoidDepth;
    }

private:
    GraphiteDepthMode mode_{GraphiteDepthMode::Default};
};

} // namespace ui::detail
