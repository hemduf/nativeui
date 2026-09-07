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

bool verify_gradient(bool update) {
    constexpr ui::Color left{
        64.0f / 255.0f, 80.0f / 255.0f, 96.0f / 255.0f, 1.0f};
    constexpr ui::Color right{
        96.0f / 255.0f, 80.0f / 255.0f, 64.0f / 255.0f, 1.0f};

    const ui::LinearGradient gradient{
        {0.0f, 0.0f},
        {8.0f, 0.0f},
        {
            ui::GradientStop{0.0f, left},
            ui::GradientStop{0.375f, left},
            ui::GradientStop{0.625f, right},
            ui::GradientStop{1.0f, right},
        },
    };

    ui::UI tree{
        ui::Canvas{8.0f, 2.0f, [gradient](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 8.0f, 2.0f}, gradient);
        }}
    };
    ui::HeadlessRenderer renderer{{8.0f, 2.0f}, 1.0f};
    if (!renderer.render(tree)) return false;

    test::golden::CompareOptions options;
    options.channel_tolerance = 1;
    // Compare the two constant plateaus away from the interpolation band. This
    // keeps the snapshot deterministic while still proving that a gradient
    // shader, not a single solid fallback, reaches the headless renderer.
    options.compare_regions = {
        test::golden::Region{0, 0, 3, 2},
        test::golden::Region{5, 0, 3, 2},
    };

    return test::golden::verify(
        "gradient_scene",
        test::golden::from_renderer(renderer),
        NATIVEUI_GOLDEN_BASELINE_DIR,
        NATIVEUI_GOLDEN_ARTIFACT_DIR,
        options,
        update);
}

} // namespace

int main(int argc, char** argv) {
    try {
        NUI_CHECK(verify_gradient(update_requested(argc, argv)));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL gradient golden: " << error.what() << '\n';
        return 1;
    }
}
