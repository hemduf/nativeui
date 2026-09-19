#include "test_support.hpp"
#include "golden/golden.hpp"

#include <string_view>

#ifndef NATIVEUI_GOLDEN_BASELINE_DIR
#define NATIVEUI_GOLDEN_BASELINE_DIR "tests/golden/baselines"
#endif
#ifndef NATIVEUI_GOLDEN_ARTIFACT_DIR
#define NATIVEUI_GOLDEN_ARTIFACT_DIR "golden-artifacts"
#endif

namespace {

bool update_requested(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view{argv[i]} == "--update-goldens") return true;
    }
    return false;
}

ui::LinearGradient plateau_gradient() {
    constexpr ui::Color left{
        64.0f / 255.0f, 80.0f / 255.0f, 96.0f / 255.0f, 1.0f};
    constexpr ui::Color right{
        96.0f / 255.0f, 80.0f / 255.0f, 64.0f / 255.0f, 1.0f};
    return ui::LinearGradient{
        {0.0f, 0.0f},
        {8.0f, 0.0f},
        {
            ui::GradientStop{0.0f, left},
            ui::GradientStop{0.375f, left},
            ui::GradientStop{0.625f, right},
            ui::GradientStop{1.0f, right},
        },
    };
}

test::golden::CompareOptions plateau_options() {
    test::golden::CompareOptions options;
    options.channel_tolerance = 1;
    options.compare_regions = {
        test::golden::Region{0, 0, 3, 2},
        test::golden::Region{5, 0, 3, 2},
    };
    return options;
}

bool verify_gradient(bool update) {
    const auto gradient = plateau_gradient();
    ui::UI tree{
        ui::Canvas{8.0f, 2.0f, [gradient](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 8.0f, 2.0f}, gradient);
        }}
    };
    ui::HeadlessRenderer renderer{{8.0f, 2.0f}, 1.0f};
    if (!renderer.render(tree)) return false;

    return test::golden::verify(
        "gradient_scene",
        test::golden::from_renderer(renderer),
        NATIVEUI_GOLDEN_BASELINE_DIR,
        NATIVEUI_GOLDEN_ARTIFACT_DIR,
        plateau_options(),
        update);
}

bool verify_brush_linear_path(bool update) {
    const ui::Brush brush{plateau_gradient()};
    ui::Path path;
    path.move_to({0.0f, 0.0f})
        .line_to({8.0f, 0.0f})
        .line_to({8.0f, 2.0f})
        .line_to({0.0f, 2.0f})
        .close();

    ui::UI tree{
        ui::Canvas{8.0f, 2.0f, [brush, path](ui::CanvasContext2D& g) {
            g.fill_path(path, brush);
        }}
    };
    ui::HeadlessRenderer renderer{{8.0f, 2.0f}, 1.0f};
    if (!renderer.render(tree)) return false;

    return test::golden::verify(
        "brush_linear_path",
        test::golden::from_renderer(renderer),
        NATIVEUI_GOLDEN_BASELINE_DIR,
        NATIVEUI_GOLDEN_ARTIFACT_DIR,
        plateau_options(),
        update);
}

bool verify_brush_linear_path_stroke(bool update) {
    const ui::Brush brush{plateau_gradient()};
    ui::Path path;
    path.move_to({-4.0f, 1.0f}).line_to({12.0f, 1.0f});
    const ui::StrokeStyle style{4.0f, ui::StrokeCap::Butt, ui::StrokeJoin::Miter, 4.0f};

    ui::UI tree{
        ui::Canvas{8.0f, 2.0f, [brush, path, style](ui::CanvasContext2D& g) {
            g.stroke_path(path, brush, style);
        }}
    };
    ui::HeadlessRenderer renderer{{8.0f, 2.0f}, 1.0f};
    if (!renderer.render(tree)) return false;

    // The oversized horizontal stroke fully covers the 8x2 target, so its
    // initial baseline is byte-identical to the fill baseline. Keep a distinct
    // golden ID so --update-goldens cannot let one scene overwrite the other.
    return test::golden::verify(
        "brush_linear_path_stroke",
        test::golden::from_renderer(renderer),
        NATIVEUI_GOLDEN_BASELINE_DIR,
        NATIVEUI_GOLDEN_ARTIFACT_DIR,
        plateau_options(),
        update);
}

bool verify_shader_brush_primitives(bool update) {
    const auto compiled = ui::ShaderProgram::compile(R"(
        half4 main(float2 p) {
            return half4(0.25, 0.5, 0.75, 1.0);
        }
    )");
    if (!compiled.ok()) return false;

    ui::ShaderInstance shader{compiled.program};
    const ui::Brush brush{shader};

    ui::Path triangle;
    triangle.move_to({8.0f, 1.0f})
        .line_to({11.0f, 1.0f})
        .line_to({9.5f, 6.0f})
        .close();

    ui::Path line;
    line.move_to({12.0f, 4.0f}).line_to({15.5f, 4.0f});

    ui::UI tree{
        ui::Canvas{16.0f, 8.0f, [brush, triangle, line](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 16.0f, 8.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
            g.fill_rounded_rect({0.0f, 1.0f, 3.0f, 6.0f}, 1.0f, brush);
            g.circle({5.0f, 4.0f}, 2.5f, brush);
            g.fill_path(triangle, brush);
            g.stroke_path(
                line,
                brush,
                ui::StrokeStyle{3.0f, ui::StrokeCap::Butt, ui::StrokeJoin::Miter, 4.0f});
        }}
    };

    ui::HeadlessRenderer renderer{{16.0f, 8.0f}, 1.0f};
    if (!renderer.render(tree)) return false;

    test::golden::CompareOptions options;
    options.channel_tolerance = 1;
    options.compare_regions = {
        test::golden::Region{1, 4, 1, 1},
        test::golden::Region{5, 4, 1, 1},
        test::golden::Region{9, 3, 1, 1},
        test::golden::Region{14, 4, 1, 1},
    };

    return test::golden::verify(
        "shader_brush_primitives",
        test::golden::from_renderer(renderer),
        NATIVEUI_GOLDEN_BASELINE_DIR,
        NATIVEUI_GOLDEN_ARTIFACT_DIR,
        options,
        update);
}

bool verify_brush_radial_circle(bool update) {
    constexpr ui::Color color{
        64.0f / 255.0f, 80.0f / 255.0f, 96.0f / 255.0f, 1.0f};
    const ui::Brush brush{ui::RadialGradient{
        {4.0f, 4.0f},
        3.0f,
        {
            ui::GradientStop{0.0f, color},
            ui::GradientStop{1.0f, color},
        }}};

    ui::UI tree{
        ui::Canvas{8.0f, 8.0f, [brush](ui::CanvasContext2D& g) {
            g.circle({4.0f, 4.0f}, 3.0f, brush);
        }}
    };
    ui::HeadlessRenderer renderer{{8.0f, 8.0f}, 1.0f};
    if (!renderer.render(tree)) return false;

    test::golden::CompareOptions options;
    options.channel_tolerance = 1;
    options.compare_regions = {
        test::golden::Region{3, 3, 2, 2},
        test::golden::Region{0, 0, 1, 1},
        test::golden::Region{7, 0, 1, 1},
        test::golden::Region{0, 7, 1, 1},
        test::golden::Region{7, 7, 1, 1},
    };

    return test::golden::verify(
        "brush_radial_circle",
        test::golden::from_renderer(renderer),
        NATIVEUI_GOLDEN_BASELINE_DIR,
        NATIVEUI_GOLDEN_ARTIFACT_DIR,
        options,
        update);
}

} // namespace

int main(int argc, char** argv) {
    try {
        const bool update = update_requested(argc, argv);
        NUI_CHECK(verify_gradient(update));
        NUI_CHECK(verify_brush_linear_path(update));
        NUI_CHECK(verify_brush_linear_path_stroke(update));
        NUI_CHECK(verify_shader_brush_primitives(update));
        NUI_CHECK(verify_brush_radial_circle(update));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL gradient golden: " << error.what() << '\n';
        return 1;
    }
}
