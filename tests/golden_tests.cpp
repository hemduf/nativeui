#include "test_support.hpp"
#include "golden/golden.hpp"

#include <filesystem>
#include <sstream>
#include <string_view>

#ifndef NATIVEUI_GOLDEN_BASELINE_DIR
#define NATIVEUI_GOLDEN_BASELINE_DIR "tests/golden/baselines"
#endif
#ifndef NATIVEUI_GOLDEN_ARTIFACT_DIR
#define NATIVEUI_GOLDEN_ARTIFACT_DIR "golden-artifacts"
#endif

namespace {

using test::golden::CompareOptions;
using test::golden::Region;

bool update_requested(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view{argv[i]} == "--update-goldens") return true;
    }
    return false;
}

void comparator_self_check() {
    test::golden::Image a{2, 1, {10, 20, 30, 40, 50, 60}};
    auto b = a;
    const CompareOptions exact{};
    NUI_CHECK(test::golden::compare(a, b, exact).matched);

    b.rgb[0] = 100;
    const auto mismatch = test::golden::compare(a, b, exact);
    NUI_CHECK(!mismatch.matched);
    NUI_CHECK(mismatch.mismatched_pixels == 1);
    NUI_CHECK(mismatch.first_mismatch_x == 0);
    NUI_CHECK(mismatch.first_mismatch_y == 0);

    CompareOptions tolerant;
    tolerant.channel_tolerance = 100;
    NUI_CHECK(test::golden::compare(a, b, tolerant).matched);
}

void failure_artifact_self_check() {
    const auto root = std::filesystem::temp_directory_path() / "nativeui-golden-self-check";
    const auto baselines = root / "baselines";
    const auto artifacts = root / "artifacts";
    std::filesystem::remove_all(root);

    test::golden::Image expected{2, 1, {10, 20, 30, 40, 50, 60}};
    auto actual = expected;
    actual.rgb[0] = 200;
    test::golden::write_ppm(baselines / "failure.ppm", expected);

    std::ostringstream captured;
    auto* previous = std::cerr.rdbuf(captured.rdbuf());
    const bool matched = test::golden::verify(
        "failure", actual, baselines, artifacts, CompareOptions{}, false);
    std::cerr.rdbuf(previous);

    NUI_CHECK(!matched);
    NUI_CHECK(captured.str().find("GOLDEN MISMATCH failure") != std::string::npos);
    NUI_CHECK(std::filesystem::exists(artifacts / "failure.actual.ppm"));
    NUI_CHECK(std::filesystem::exists(artifacts / "failure.diff.ppm"));
    std::filesystem::remove_all(root);
}

bool verify_canvas(bool update) {
    ui::UI tree{
        ui::Canvas{64.0f, 64.0f, [](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 64.0f, 64.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
        }}
    };
    ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
    if (!renderer.render(tree)) return false;

    CompareOptions options;
    options.channel_tolerance = 1;
    // Exclude the framework footer at the bottom; the upper area is pure
    // geometry and should be bit-stable across platforms.
    options.compare_regions = {Region{4, 4, 56, 24}};
    return test::golden::verify(
        "canvas_solid", test::golden::from_renderer(renderer),
        NATIVEUI_GOLDEN_BASELINE_DIR, NATIVEUI_GOLDEN_ARTIFACT_DIR, options, update);
}

bool verify_layout(bool update) {
    ui::UI tree{
        ui::Row{
            ui::Canvas{32.0f, 32.0f, [](ui::CanvasContext2D& g) {
                g.fill_rect({0.0f, 0.0f, 32.0f, 32.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
            }},
            ui::Canvas{32.0f, 32.0f, [](ui::CanvasContext2D& g) {
                g.fill_rect({0.0f, 0.0f, 32.0f, 32.0f}, {0.0f, 1.0f, 0.0f, 1.0f});
            }}
        }.gap(0.0f)
    };
    ui::HeadlessRenderer renderer{{96.0f, 64.0f}, 1.0f};
    if (!renderer.render(tree)) return false;

    CompareOptions options;
    options.channel_tolerance = 1;
    options.compare_regions = {
        Region{4, 4, 24, 24},
        Region{36, 4, 24, 24},
    };
    return test::golden::verify(
        "layout_row", test::golden::from_renderer(renderer),
        NATIVEUI_GOLDEN_BASELINE_DIR, NATIVEUI_GOLDEN_ARTIFACT_DIR, options, update);
}

bool verify_toggle(bool update) {
    ui::State<bool> enabled{true};
    ui::UI tree{ui::Toggle{"Enabled", enabled}};
    ui::HeadlessRenderer renderer{{210.0f, 90.0f}, 1.0f};
    if (!renderer.render(tree)) return false;

    CompareOptions options;
    // Only compare geometry interiors, away from anti-aliased rounded edges and
    // away from text. This is the cross-platform tolerance policy for widgets
    // that contain platform-shaped glyphs.
    options.channel_tolerance = 3;
    options.compare_regions = {
        Region{154, 22, 10, 10}, // accent track interior
        Region{176, 23, 7, 8},   // white knob interior
    };
    return test::golden::verify(
        "toggle_on", test::golden::from_renderer(renderer),
        NATIVEUI_GOLDEN_BASELINE_DIR, NATIVEUI_GOLDEN_ARTIFACT_DIR, options, update);
}


bool verify_label(bool update) {
    ui::UI tree{
        ui::Stack{
            ui::Canvas{220.0f, 80.0f, [](ui::CanvasContext2D& g) {
                g.fill_rect({0.0f, 34.0f, g.width(), 6.0f}, ui::colors::accent);
            }},
            ui::Label{"Golden Label"}
                .size(18.0f)
                .align(ui::TextAlign::Center)
                .bold()
        }
    };
    ui::HeadlessRenderer renderer{{220.0f, 80.0f}, 1.0f};
    if (!renderer.render(tree)) return false;

    CompareOptions options;
    options.channel_tolerance = 2;
    // Glyph rasterization varies with CoreText/DirectWrite/Fontconfig. The
    // deterministic stripe and background are compared here while label
    // measurement/paint consistency is covered by nativeui_label_tests.
    options.compare_regions = {
        Region{8, 4, 24, 12},
        Region{8, 35, 204, 4},
    };
    return test::golden::verify(
        "label_scene", test::golden::from_renderer(renderer),
        NATIVEUI_GOLDEN_BASELINE_DIR, NATIVEUI_GOLDEN_ARTIFACT_DIR, options, update);
}

int run_suite(bool update) {
    comparator_self_check();
    failure_artifact_self_check();
    NUI_CHECK(verify_canvas(update));
    NUI_CHECK(verify_layout(update));
    NUI_CHECK(verify_toggle(update));
    NUI_CHECK(verify_label(update));
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const bool update = update_requested(argc, argv);
    try {
        return run_suite(update);
    } catch (const std::exception& error) {
        std::cerr << "FAIL goldens: " << error.what() << '\n';
        return 1;
    }
}
