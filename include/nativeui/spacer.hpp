#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

/// Non-painting retained element with a fixed reported logical extent.
class SpacerComponent final : public Component {
public:
    explicit SpacerComponent(Size size);

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>&) const override;

    void paint(PaintContext&) const override;

private:
    Size size_{};
};

/// Detached fixed-size layout value with no painted output.
class Spacer {
public:
/// Construct a vertical-only logical spacer.
    explicit Spacer(float height);
/// Construct a requested logical width and height.
    Spacer(float width, float height);
/// Construct from an owned logical Size.
    explicit Spacer(Size size);

/// Consume the requested size into a retained Spacer.
    Spec spec() &&;

private:
    Size size_{};
};

} // namespace ui
