#include <nativeui/spacer.hpp>
#include <algorithm>
#include <cmath>

namespace ui {

SpacerComponent::SpacerComponent(Size size)
    : size_{std::isfinite(size.w) ? std::max(0.0f, size.w) : 0.0f,
            std::isfinite(size.h) ? std::max(0.0f, size.h) : 0.0f} {}

Size SpacerComponent::measure(const std::vector<ChildMetrics>&) const {
    return size_;
}

Size SpacerComponent::minimum_size(const std::vector<ChildMetrics>&) const {
    return size_;
}

void SpacerComponent::paint(PaintContext&) const {}

Spacer::Spacer(float height) : size_{0.0f, height} {}

Spacer::Spacer(float width, float height) : size_{width, height} {}

Spacer::Spacer(Size size) : size_(size) {}

Spec Spacer::spec() && {
    const auto size = size_;
    return Spec{[size] { return std::make_unique<SpacerComponent>(size); }, {}};
}

} // namespace ui
