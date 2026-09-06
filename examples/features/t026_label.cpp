#include "example_support.hpp"

namespace {

std::unique_ptr<ui::UI> make_ui() {
    ui::TextStyle custom{};
    custom.size = 15.0f;
    custom.color = ui::colors::accent;
    custom.align = ui::TextAlign::Right;
    custom.weight = ui::FontWeight::Bold;

    return std::make_unique<ui::UI>(
        ui::Column{
            ui::Header{"T026 / LABEL + TEXT METRICS"},
            ui::Label{"Reusable Label"}.size(24.0f).bold(),
            ui::Label{"Centered muted text"}
                .size(16.0f)
                .color(ui::colors::textMuted)
                .align(ui::TextAlign::Center),
            ui::Label{"Right aligned accent style"}.style(custom),
            ui::Canvas{560.0f, 70.0f, [](ui::CanvasContext2D& g) {
                ui::TextStyle style{};
                style.size = 18.0f;
                const auto metrics = g.text_metrics("Measured through TextService", style);
                g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 10.0f, ui::colors::panel);
                g.text({12.0f, 22.0f}, "TextService metrics", 13.0f, ui::colors::text);
                g.text({12.0f, 48.0f},
                       "width=" + std::to_string(metrics.width) +
                           " height=" + std::to_string(metrics.height),
                       11.0f,
                       ui::colors::textMuted);
            }}
        }.padding(20.0f).gap(14.0f));
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) {
        ui::TextStyle style{};
        style.size = 18.0f;
        const auto metrics = ui::TextService::measure("NativeUI", style);
        ui::UI label{ui::Label{"NativeUI"}.style(style)};
        const auto measured = label.measure();
        if (!example::near(measured.preferred.w, metrics.width) ||
            !example::near(measured.preferred.h, metrics.height)) {
            return example::fail("Label measurement diverged from TextService");
        }
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T026 - Label", {640.0f, 360.0f});
}
