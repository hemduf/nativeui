#include "test_support.hpp"

#include <algorithm>

namespace {

void painted_alignment() {
    constexpr int width = 320;
    constexpr int height = 120;
    ui::TextStyle style{};
    style.size = 18.0f;
    style.color = {1.0f, 0.0f, 0.0f, 1.0f};
    style.weight = ui::FontWeight::Bold;
    const auto metrics = ui::TextService::measure("NativeUI", style);
    NUI_CHECK(metrics.width > 0.0f && metrics.width < width);

    ui::HeadlessRenderer renderer{{width, height}, 1.0f};
    ui::UI empty{ui::Label{""}.style(style)};
    NUI_CHECK(renderer.render(empty));
    const auto background = renderer.rgba_pixels();

    int left_ink_x = 0;
    for (const auto align : {ui::TextAlign::Left, ui::TextAlign::Center, ui::TextAlign::Right}) {
        ui::UI label{ui::Label{"NativeUI"}.style(style).align(align)};
        NUI_CHECK(renderer.render(label));
        const auto& pixels = renderer.rgba_pixels();
        int first_ink_x = width;
        bool visible_red_ink = false;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const auto i = (y * width + x) * 4;
                if (pixels[i] == background[i] && pixels[i + 1] == background[i + 1] &&
                    pixels[i + 2] == background[i + 2]) continue;
                first_ink_x = std::min(first_ink_x, x);
                NUI_CHECK(pixels[i] >= background[i]);
                NUI_CHECK(pixels[i + 1] <= background[i + 1]);
                NUI_CHECK(pixels[i + 2] <= background[i + 2]);
                visible_red_ink = visible_red_ink || pixels[i] > 100;
            }
        }
        // Same-platform ink bounds test actual painting, color and alignment
        // without requiring CoreText/DirectWrite/Fontconfig to share glyphs.
        NUI_CHECK(visible_red_ink);
        if (align == ui::TextAlign::Left) left_ink_x = first_ink_x;
        const float shift = align == ui::TextAlign::Left ? 0.0f :
                            align == ui::TextAlign::Center ? (width - metrics.width) * 0.5f :
                            width - metrics.width;
        NUI_CHECK_NEAR(first_ink_x - left_ink_x, shift, 1.0f);
    }
}

void suite() {
    painted_alignment();
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
