#include "example_support.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/shader.hpp>

#include <iostream>
#include <string_view>

namespace {

constexpr std::string_view kLeafSource = R"(
    uniform float gain;
    half4 main(float2 p) {
        return half4(gain, p.x / 320.0, 0.2, 1.0);
    }
)";

constexpr std::string_view kParentSource = R"(
    uniform shader base;
    uniform shader detail;
    half4 main(float2 p) {
        return mix(base.eval(p), detail.eval(p * 0.75), 0.35);
    }
)";

bool compile_or_report(std::string_view source,
                       std::shared_ptr<const ui::ShaderProgram>& program) {
    const auto compiled = ui::ShaderProgram::compile(source);
    if (!compiled.ok()) {
        for (const auto& diagnostic : compiled.diagnostics) {
            std::cerr << "SkSL error: " << diagnostic.message << '\n';
        }
        return false;
    }
    program = compiled.program;
    return true;
}

int self_test() {
    std::shared_ptr<const ui::ShaderProgram> leaf_program;
    std::shared_ptr<const ui::ShaderProgram> parent_program;
    if (!compile_or_report(kLeafSource, leaf_program) ||
        !compile_or_report(kParentSource, parent_program)) {
        return example::fail("shader program compilation failed");
    }

    ui::ShaderInstance leaf{leaf_program};
    if (leaf.set_float("gain", 0.8f) != ui::ShaderSetResult::Ok) {
        return example::fail("leaf uniform binding failed");
    }
    const ui::Brush leaf_brush{leaf};

    const ui::Brush detail{ui::LinearGradient{
        {0.0f, 0.0f},
        {64.0f, 0.0f},
        ui::Color{0.0f, 0.2f, 1.0f, 1.0f},
        ui::Color{1.0f, 0.2f, 0.0f, 1.0f}}};

    ui::ShaderInstance parent{parent_program};
    if (parent.set_child("base", leaf_brush) != ui::ShaderSetResult::Ok ||
        parent.set_child("detail", detail) != ui::ShaderSetResult::Ok) {
        return example::fail("child binding failed");
    }
    const ui::Brush composed{parent};

    ui::UI tree{
        ui::Canvas{64.0f, 16.0f, [composed](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 64.0f, 16.0f}, composed);
        }}
    };

    ui::HeadlessRenderer renderer{{64.0f, 16.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("nested shader render failed");

    const auto left = renderer.pixel(8, 8);
    const auto right = renderer.pixel(56, 8);
    if (!(left.a > 240 && right.a > 240 &&
          (left.r != right.r || left.b != right.b))) {
        return example::fail("nested child sampling did not vary across coordinates");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) {
        return self_test();
    }

    std::shared_ptr<const ui::ShaderProgram> leaf_program;
    std::shared_ptr<const ui::ShaderProgram> parent_program;
    if (!compile_or_report(kLeafSource, leaf_program) ||
        !compile_or_report(kParentSource, parent_program)) {
        return 1;
    }

    ui::ShaderInstance leaf{leaf_program};
    if (leaf.set_float("gain", 0.72f) != ui::ShaderSetResult::Ok) return 1;
    const ui::Brush leaf_brush{leaf};

    const ui::Brush detail{ui::RadialGradient{
        {160.0f, 90.0f},
        150.0f,
        ui::Color{0.1f, 0.3f, 1.0f, 1.0f},
        ui::Color{1.0f, 0.15f, 0.05f, 1.0f}}};

    ui::ShaderInstance parent{parent_program};
    if (parent.set_child("base", leaf_brush) != ui::ShaderSetResult::Ok ||
        parent.set_child("detail", detail) != ui::ShaderSetResult::Ok) {
        return 1;
    }
    const ui::Brush composed{parent};

    ui::UI tree{
        ui::Canvas{320.0f, 180.0f, [composed](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 320.0f, 180.0f}, composed);
            g.stroke_rounded_rect(
                {28.0f, 28.0f, 264.0f, 124.0f},
                18.0f,
                5.0f,
                composed);
        }}
    };
    return example::run_window(
        tree,
        "NativeUI T082 shader children",
        {320.0f, 180.0f});
}
