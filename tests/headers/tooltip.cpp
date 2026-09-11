#include <nativeui/tooltip.hpp>

#include <chrono>
#include <memory>
#include <vector>

namespace {

class HeaderProbeComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {};
    }

    void paint(ui::PaintContext&) const override {}
};

class HeaderProbe {
public:
    ui::Spec spec() && {
        return ui::Spec{[] { return std::make_unique<HeaderProbeComponent>(); }, {}};
    }
};

} // namespace

void nativeui_header_compile_tooltip() {
    auto default_delay = ui::make_spec(ui::Tooltip{"Reset to default", HeaderProbe{}});
    auto fluent_delay = ui::make_spec(
        ui::Tooltip{"Reset to default", HeaderProbe{}}
            .delay(std::chrono::milliseconds{250}));
    static_assert(ui::Tooltip::kDefaultDelay == std::chrono::milliseconds{500});
    static_assert(ui::Tooltip::kDefaultMaxWidth == 320.0f);
    (void)default_delay;
    (void)fluent_delay;
}
