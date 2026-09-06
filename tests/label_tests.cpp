#include "test_support.hpp"

namespace {

void suite() {
    ui::TextStyle style{};
    style.size = 18.0f;
    style.color = ui::colors::text;

    const auto metrics = ui::TextService::measure("NativeUI", style);
    NUI_CHECK(metrics.width >= 0.0f);
    NUI_CHECK(metrics.height >= 0.0f);
    NUI_CHECK(metrics.descent >= metrics.ascent);

    ui::UI label{ui::Label{"NativeUI"}.style(style)};
    const auto measured = label.measure();
    NUI_CHECK_NEAR(measured.preferred.w, metrics.width, 0.01f);
    NUI_CHECK_NEAR(measured.preferred.h, metrics.height, 0.01f);

    const auto empty = ui::TextService::measure("", style);
    NUI_CHECK_NEAR(empty.width, 0.0f, 0.001f);
    NUI_CHECK(empty.height >= 0.0f);

    ui::TextStyle bold = style;
    bold.weight = ui::FontWeight::Bold;
    const auto bold_metrics = ui::TextService::measure("NativeUI", bold);
    NUI_CHECK(bold_metrics.width >= 0.0f);
    NUI_CHECK(bold_metrics.height >= 0.0f);

    // Exercise the public fluent API and headless paint path. Pixel-perfect
    // glyph comparison lives in the golden suite with platform-variable glyph
    // regions excluded.
    ui::UI scene{
        ui::Stack{
            ui::Canvas{220.0f, 80.0f, [](ui::CanvasContext2D& g) {
                g.fill_rect({0.0f, 34.0f, g.width(), 6.0f}, ui::colors::accent);
            }},
            ui::Label{"Centered Label"}
                .size(18.0f)
                .color(ui::colors::text)
                .align(ui::TextAlign::Center)
                .bold()
        }
    };
    ui::HeadlessRenderer renderer{{220.0f, 80.0f}, 1.0f};
    NUI_CHECK(renderer.render(scene));
    // Probe the deterministic Canvas stripe away from centered glyphs.  The
    // previous x=110 sample landed under the label text on real CoreText and
    // therefore measured the glyph color rather than the stripe.
    const auto stripe = renderer.pixel(10, 36);
    NUI_CHECK(stripe.r > 200 && stripe.g > 100 && stripe.b < 100);
}

} // namespace

int main() { return test::run("label", &suite); }
