#include "example_support.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/shader.hpp>

#include <iostream>
#include <string_view>

namespace {

constexpr std::string_view kShader = R"(
    uniform float gain;
    half4 main(float2 p) {
        return half4(gain, p.x / 320.0, 0.25, 1.0);
    }
)";

int self_test() {
    const auto compiled = ui::ShaderProgram::compile(kShader);
    if (!compiled.ok()) return example::fail("runtime shader did not compile");

    ui::ShaderInstance shader{compiled.program};
    if (shader.set_float("gain", 0.25f) != ui::ShaderSetResult::Ok) {
        return example::fail("initial shader binding failed");
    }
    const ui::Brush first{shader};

    if (shader.set_float("gain", 0.75f) != ui::ShaderSetResult::Ok) {
        return example::fail("updated shader binding failed");
    }
    const ui::Brush second{shader};

    ui::UI tree{
        ui::Canvas{64.0f, 16.0f, [first, second](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 32.0f, 16.0f}, first);
            g.fill_rect({32.0f, 0.0f, 32.0f, 16.0f}, second);
        }}
    };

    ui::HeadlessRenderer renderer{{64.0f, 16.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("headless shader render failed");

    const auto left = renderer.pixel(8, 8);
    const auto right = renderer.pixel(48, 8);
    if (!(left.r < 100 && right.r > 150 && right.r > left.r + 80)) {
        return example::fail("Brush snapshot rendering is not independent");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) {
        return self_test();
    }

    const auto compiled = ui::ShaderProgram::compile(kShader);
    if (!compiled.ok()) {
        for (const auto& diagnostic : compiled.diagnostics) {
            std::cerr << "SkSL error: " << diagnostic.message << '\n';
        }
        return 1;
    }

    ui::ShaderInstance shader{compiled.program};
    shader.set_float("gain", 0.65f);
    const ui::Brush brush{shader};

    ui::UI tree{
        ui::Canvas{320.0f, 180.0f, [brush](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 320.0f, 180.0f}, brush);
            g.stroke_rounded_rect(
                {32.0f, 32.0f, 256.0f, 116.0f},
                18.0f,
                5.0f,
                brush);
        }}
    };
    return example::run_window(tree, "NativeUI T081 shader Brush", {320.0f, 180.0f});
}
