#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

class SpacerComponent final : public Component {
public:
    explicit SpacerComponent(Size size);

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>&) const override;

    void paint(PaintContext&) const override;

private:
    Size size_{};
};

class Spacer {
public:
    explicit Spacer(float height);
    Spacer(float width, float height);
    explicit Spacer(Size size);

    Spec spec() &&;

private:
    Size size_{};
};

} // namespace ui
