#include "test_support.hpp"
#include "src/detail/shader_brush_access.hpp"
#include "src/detail/shader_instance_access.hpp"

#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace {

class PainterProbeComponent final : public ui::Component {
public:
    explicit PainterProbeComponent(std::function<void(ui::Painter&)> draw)
        : draw_(std::move(draw)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {20.0f, 20.0f};
    }

    void paint(ui::PaintContext& context) const override {
        draw_(context.painter());
    }

private:
    std::function<void(ui::Painter&)> draw_;
};

class PainterProbe {
public:
    explicit PainterProbe(std::function<void(ui::Painter&)> draw)
        : draw_(std::move(draw)) {}

    ui::Spec spec() && {
        auto draw = std::move(draw_);
        return ui::Spec{
            [draw = std::move(draw)]() mutable {
                return std::make_unique<PainterProbeComponent>(std::move(draw));
            },
            {}};
    }

private:
    std::function<void(ui::Painter&)> draw_;
};

std::shared_ptr<const ui::ShaderProgram> compile(std::string_view source) {
    const auto result = ui::ShaderProgram::compile(source);
    NUI_CHECK(result.ok());
    NUI_CHECK(result.program);
    NUI_CHECK(result.diagnostics.empty());
    return result.program;
}

std::shared_ptr<const ui::ShaderProgram> pass_through_program() {
    return compile(R"(
        uniform shader child;
        half4 main(float2 p) { return child.eval(p); }
    )");
}

ui::Brush wrap_child(const std::shared_ptr<const ui::ShaderProgram>& program,
                     const ui::Brush& child) {
    ui::ShaderInstance parent{program};
    NUI_CHECK(parent.set_child("child", child) == ui::ShaderSetResult::Ok);
    return ui::Brush{parent};
}

std::array<int, 4> render_pixel(
    const ui::Brush& brush,
    int x = 8,
    int y = 8,
    ui::PaintOptions options = {},
    ui::Color background = {0.0f, 0.0f, 0.0f, 1.0f}) {
    ui::UI tree{PainterProbe{[brush, options, background](ui::Painter& painter) {
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 16.0f, 16.0f},
            0.0f,
            background);
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 16.0f, 16.0f},
            0.0f,
            brush,
            options);
    }}};

    ui::HeadlessRenderer renderer{{20.0f, 20.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    const auto pixel = renderer.pixel(x, y);
    return {pixel.r, pixel.g, pixel.b, pixel.a};
}

std::array<int, 4> render_translated_pixel(
    const ui::Brush& brush,
    int x,
    int y) {
    ui::UI tree{PainterProbe{[brush](ui::Painter& painter) {
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 20.0f, 20.0f},
            0.0f,
            {0.0f, 0.0f, 0.0f, 1.0f});
        auto state = painter.scoped_state();
        painter.translate(2.0f, 1.0f);
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 16.0f, 16.0f},
            0.0f,
            brush);
    }}};

    ui::HeadlessRenderer renderer{{20.0f, 20.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    const auto pixel = renderer.pixel(x, y);
    return {pixel.r, pixel.g, pixel.b, pixel.a};
}

void check_pixel_near(const std::array<int, 4>& a,
                      const std::array<int, 4>& b,
                      int tolerance = 2) {
    for (std::size_t i = 0; i < a.size(); ++i) {
        NUI_CHECK(std::abs(a[i] - b[i]) <= tolerance);
    }
}

void slot_snapshot_and_instance_isolation() {
    const auto parent_program = compile(R"(
        uniform float gain;
        uniform shader albedo;
        uniform shader detail;
        half4 main(float2 p) {
            return gain * albedo.eval(p) + (1.0 - gain) * detail.eval(p);
        }
    )");

    NUI_CHECK(parent_program->children().size() == 2U);
    NUI_CHECK(parent_program->children()[0].name == "albedo");
    NUI_CHECK(parent_program->children()[1].name == "detail");

    ui::ShaderInstance first{parent_program};
    ui::ShaderInstance second{parent_program};
    NUI_CHECK(ui::detail::ShaderInstanceAccess::child_count(first) == 2U);
    NUI_CHECK(ui::detail::ShaderInstanceAccess::child(first, 0) == nullptr);
    NUI_CHECK(ui::detail::ShaderInstanceAccess::child(first, 1) == nullptr);
    NUI_CHECK(ui::detail::ShaderInstanceAccess::depth(first) == 1U);

    const ui::Brush red{ui::Color{1.0f, 0.0f, 0.0f, 1.0f}};
    const ui::Brush blue{ui::Color{0.0f, 0.0f, 1.0f, 1.0f}};
    NUI_CHECK(first.set_float("gain", 1.0f) == ui::ShaderSetResult::Ok);
    NUI_CHECK(second.set_float("gain", 1.0f) == ui::ShaderSetResult::Ok);
    NUI_CHECK(first.set_child("missing", red) == ui::ShaderSetResult::NotFound);
    NUI_CHECK(first.set_child("albedo", red) == ui::ShaderSetResult::Ok);
    NUI_CHECK(second.set_child("albedo", blue) == ui::ShaderSetResult::Ok);

    NUI_CHECK(ui::detail::ShaderInstanceAccess::child(first, 0) != nullptr);
    NUI_CHECK(ui::detail::ShaderInstanceAccess::child(second, 0) != nullptr);
    NUI_CHECK(ui::detail::ShaderInstanceAccess::depth(first) == 1U);
    NUI_CHECK(ui::detail::ShaderInstanceAccess::depth(second) == 1U);

    ui::Brush first_snapshot{first};
    ui::Brush second_snapshot{second};
    NUI_CHECK(ui::detail::ShaderBrushAccess::is_shader(first_snapshot));
    NUI_CHECK(ui::detail::ShaderBrushAccess::child_count(first_snapshot) == 2U);
    NUI_CHECK(ui::detail::ShaderBrushAccess::child(first_snapshot, 0) != nullptr);
    NUI_CHECK(ui::detail::ShaderBrushAccess::child(first_snapshot, 1) == nullptr);
    NUI_CHECK(ui::detail::ShaderBrushAccess::depth(first_snapshot) == 1U);

    NUI_CHECK(first.set_child("albedo", blue) == ui::ShaderSetResult::Ok);
    NUI_CHECK(ui::detail::ShaderBrushAccess::child(first_snapshot, 0) != nullptr);
    NUI_CHECK(render_pixel(first_snapshot)[0] != render_pixel(second_snapshot)[0]);
}

void copy_move_and_source_lifetime() {
    const auto parent_program = pass_through_program();
    const ui::Brush gradient{ui::LinearGradient{
        {0.0f, 0.0f},
        {16.0f, 0.0f},
        ui::Color{1.0f, 0.0f, 0.0f, 1.0f},
        ui::Color{0.0f, 0.0f, 1.0f, 1.0f}}};

    ui::ShaderInstance source{parent_program};
    NUI_CHECK(source.set_child("child", gradient) == ui::ShaderSetResult::Ok);

    ui::ShaderInstance copy{source};
    NUI_CHECK(copy.valid());
    NUI_CHECK(ui::detail::ShaderInstanceAccess::child(copy, 0) != nullptr);

    ui::ShaderInstance assigned{parent_program};
    assigned = source;
    NUI_CHECK(assigned.valid());
    NUI_CHECK(ui::detail::ShaderInstanceAccess::child(assigned, 0) != nullptr);

    ui::ShaderInstance moved{std::move(source)};
    NUI_CHECK(moved.valid());
    NUI_CHECK(!source.valid());
    NUI_CHECK(source.set_child("child", gradient) == ui::ShaderSetResult::NotFound);

    auto* moved_alias = &moved;
    moved = std::move(*moved_alias);
    NUI_CHECK(moved.valid());

    ui::ShaderInstance move_assigned{parent_program};
    move_assigned = std::move(copy);
    NUI_CHECK(move_assigned.valid());
    NUI_CHECK(!copy.valid());

    ui::Brush snapshot = [&] {
        ui::Brush temporary_child{ui::RadialGradient{
            {8.0f, 8.0f},
            8.0f,
            ui::Color{0.0f, 1.0f, 0.0f, 1.0f},
            ui::Color{0.0f, 0.0f, 0.0f, 1.0f}}};
        ui::ShaderInstance temporary_parent{parent_program};
        NUI_CHECK(temporary_parent.set_child("child", temporary_child) ==
                  ui::ShaderSetResult::Ok);
        return ui::Brush{temporary_parent};
    }();

    NUI_CHECK(ui::detail::ShaderBrushAccess::is_shader(snapshot));
    NUI_CHECK(ui::detail::ShaderBrushAccess::child(snapshot, 0) != nullptr);
    const auto pixel = render_pixel(snapshot, 8, 8);
    NUI_CHECK(pixel[1] > pixel[0]);
}

void bounded_depth_and_same_program_snapshot_reuse() {
    const auto leaf_program = compile(R"(
        half4 main(float2 p) { return half4(1.0, 0.0, 0.0, 1.0); }
    )");
    const auto parent_program = pass_through_program();

    ui::ShaderInstance leaf_instance{leaf_program};
    ui::Brush current{leaf_instance};
    NUI_CHECK(ui::detail::ShaderBrushAccess::depth(current) == 1U);

    std::vector<ui::Brush> levels;
    levels.reserve(ui::ShaderInstance::kMaxChildDepth);
    levels.push_back(current);

    for (std::size_t depth = 2U;
         depth <= ui::ShaderInstance::kMaxChildDepth;
         ++depth) {
        ui::ShaderInstance parent{parent_program};
        NUI_CHECK(parent.set_child("child", current) == ui::ShaderSetResult::Ok);
        NUI_CHECK(ui::detail::ShaderInstanceAccess::depth(parent) == depth);
        current = ui::Brush{parent};
        NUI_CHECK(ui::detail::ShaderBrushAccess::depth(current) == depth);
        levels.push_back(current);
    }

    ui::ShaderInstance rejected{parent_program};
    NUI_CHECK(rejected.set_child("child", current) ==
              ui::ShaderSetResult::InvalidValue);
    NUI_CHECK(ui::detail::ShaderInstanceAccess::depth(rejected) == 1U);
    NUI_CHECK(ui::detail::ShaderInstanceAccess::child(rejected, 0) == nullptr);

    const ui::Brush shallow{ui::Color{0.1f, 0.2f, 0.3f, 1.0f}};
    NUI_CHECK(rejected.set_child("child", shallow) == ui::ShaderSetResult::Ok);
    const ui::Brush* previous =
        ui::detail::ShaderInstanceAccess::child(rejected, 0);
    NUI_CHECK(previous != nullptr);
    NUI_CHECK(ui::detail::ShaderInstanceAccess::depth(rejected) == 1U);
    NUI_CHECK(rejected.set_child("child", current) ==
              ui::ShaderSetResult::InvalidValue);
    NUI_CHECK(ui::detail::ShaderInstanceAccess::child(rejected, 0) == previous);
    NUI_CHECK(ui::detail::ShaderInstanceAccess::depth(rejected) == 1U);

    // Reusing an earlier immutable snapshot from the same program is finite,
    // does not create a mutable ownership cycle, and obeys cached depth.
    ui::ShaderInstance reuse{parent_program};
    NUI_CHECK(reuse.set_child("child", levels[3]) == ui::ShaderSetResult::Ok);
    NUI_CHECK(ui::detail::ShaderInstanceAccess::depth(reuse) == 5U);
}

void direct_vs_child_parity() {
    const auto parent_program = pass_through_program();

    const std::vector<ui::Brush> children{
        ui::Brush{ui::Color{0.25f, 0.5f, 0.75f, 1.0f}},
        ui::Brush{ui::LinearGradient{
            {0.0f, 0.0f},
            {16.0f, 0.0f},
            ui::Color{1.0f, 0.0f, 0.0f, 1.0f},
            ui::Color{0.0f, 0.0f, 1.0f, 1.0f}}},
        ui::Brush{ui::RadialGradient{
            {8.0f, 8.0f},
            7.0f,
            ui::Color{0.0f, 1.0f, 0.0f, 1.0f},
            ui::Color{0.0f, 0.0f, 0.0f, 1.0f}}},
        ui::Brush{ui::LinearGradient{
            {0.0f, 0.0f},
            {16.0f, 0.0f},
            {
                ui::GradientStop{0.0f, ui::Color{1.0f, 1.0f, 0.0f, 0.25f}},
                ui::GradientStop{0.0f, ui::Color{0.0f, 0.0f, 1.0f, 0.75f}},
            }}},
        ui::Brush{ui::RadialGradient{
            {8.0f, 8.0f},
            0.0f,
            ui::Color{0.8f, 0.2f, 0.1f, 0.30f},
            ui::Color{0.0f, 0.0f, 0.0f, 0.80f}}},
    };

    for (const auto& child : children) {
        const ui::Brush parent = wrap_child(parent_program, child);
        check_pixel_near(render_pixel(child, 5, 8), render_pixel(parent, 5, 8));
        check_pixel_near(render_pixel(child, 11, 8), render_pixel(parent, 11, 8));
        check_pixel_near(
            render_translated_pixel(child, 7, 9),
            render_translated_pixel(parent, 7, 9));
    }

    const auto leaf_program = compile(R"(
        uniform float red;
        half4 main(float2 p) { return half4(red, p.x / 16.0, 0.25, 1.0); }
    )");
    ui::ShaderInstance leaf{leaf_program};
    NUI_CHECK(leaf.set_float("red", 0.6f) == ui::ShaderSetResult::Ok);
    const ui::Brush shader_child{leaf};
    const ui::Brush parent = wrap_child(parent_program, shader_child);
    check_pixel_near(render_pixel(shader_child, 7, 8), render_pixel(parent, 7, 8));
}

void missing_child_is_transparent_black() {
    const auto parent_program = pass_through_program();
    ui::ShaderInstance parent{parent_program};
    const ui::Brush brush{parent};

    ui::UI tree{PainterProbe{[brush](ui::Painter& painter) {
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 16.0f, 16.0f},
            0.0f,
            {1.0f, 0.0f, 0.0f, 1.0f});
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 16.0f, 16.0f},
            0.0f,
            brush);
    }}};

    ui::HeadlessRenderer renderer{{20.0f, 20.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    const auto pixel = renderer.pixel(8, 8);
    NUI_CHECK(pixel.r > 240);
    NUI_CHECK(pixel.g < 8);
    NUI_CHECK(pixel.b < 8);
}

void transformed_child_coordinates() {
    const ui::Brush gradient{ui::LinearGradient{
        {0.0f, 0.0f},
        {16.0f, 0.0f},
        ui::Color{0.0f, 0.0f, 0.0f, 1.0f},
        ui::Color{0.0f, 1.0f, 0.0f, 1.0f}}};

    const auto passthrough = pass_through_program();
    const auto scaled = compile(R"(
        uniform shader child;
        half4 main(float2 p) { return child.eval(p * 0.5); }
    )");

    const auto direct_parent = wrap_child(passthrough, gradient);
    const auto scaled_parent = wrap_child(scaled, gradient);
    const auto a = render_pixel(direct_parent, 12, 8);
    const auto b = render_pixel(scaled_parent, 12, 8);

    NUI_CHECK(a[1] > b[1] + 50);
    NUI_CHECK(b[1] > 50);
}

void outer_paint_options_and_stroke_apply_once() {
    const auto parent_program = pass_through_program();
    const ui::Brush child{ui::Color{0.8f, 0.4f, 0.2f, 0.75f}};
    const ui::Brush parent = wrap_child(parent_program, child);
    const ui::PaintOptions options{0.5f, ui::BlendMode::SourceOver};

    check_pixel_near(
        render_pixel(child, 8, 8, options),
        render_pixel(parent, 8, 8, options));

    const ui::PaintOptions multiply_options{1.0f, ui::BlendMode::Multiply};
    constexpr ui::Color multiply_background{0.8f, 0.5f, 0.25f, 1.0f};
    const auto direct_multiply =
        render_pixel(child, 8, 8, multiply_options, multiply_background);
    const auto nested_multiply =
        render_pixel(parent, 8, 8, multiply_options, multiply_background);
    check_pixel_near(direct_multiply, nested_multiply);
    NUI_CHECK(direct_multiply[0] > 100 && direct_multiply[0] < 180);
    NUI_CHECK(direct_multiply[1] > 30 && direct_multiply[1] < 100);
    NUI_CHECK(direct_multiply[2] < 50);

    auto render_stroke = [](const ui::Brush& brush) {
        ui::Path line;
        line.move_to({1.0f, 8.0f}).line_to({15.0f, 8.0f});
        ui::UI tree{PainterProbe{[brush, line](ui::Painter& painter) {
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 20.0f, 20.0f}, 0.0f,
                {0.0f, 0.0f, 0.0f, 1.0f});
            painter.stroke_path(
                line,
                brush,
                ui::StrokeStyle{
                    4.0f,
                    ui::StrokeCap::Butt,
                    ui::StrokeJoin::Miter,
                    4.0f});
        }}};
        ui::HeadlessRenderer renderer{{20.0f, 20.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        const auto p = renderer.pixel(8, 8);
        return std::array<int, 4>{p.r, p.g, p.b, p.a};
    };

    check_pixel_near(render_stroke(child), render_stroke(parent));
}

void nested_layout_color_semantics() {
    const auto leaf_program = compile(R"(
        layout(color) uniform float4 tint;
        half4 main(float2 p) { return tint; }
    )");
    ui::ShaderInstance leaf{leaf_program};
    NUI_CHECK(leaf.set_color("tint", {0.5f, 0.5f, 0.5f, 1.0f}) ==
              ui::ShaderSetResult::Ok);
    const ui::Brush leaf_brush{leaf};

    const auto parent_program = pass_through_program();
    const ui::Brush parent_brush = wrap_child(parent_program, leaf_brush);

    const auto info = SkImageInfo::Make(
        2,
        1,
        kRGBA_F32_SkColorType,
        kPremul_SkAlphaType,
        SkColorSpace::MakeSRGBLinear());
    auto surface = SkSurfaces::Raster(info);
    NUI_CHECK(surface);
    auto* canvas = surface->getCanvas();
    NUI_CHECK(canvas != nullptr);
    canvas->clear(SK_ColorBLACK);

    {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect({0.0f, 0.0f, 1.0f, 1.0f}, 0.0f, leaf_brush);
        painter.fill_rounded_rect({1.0f, 0.0f, 1.0f, 1.0f}, 0.0f, parent_brush);
    }

    SkPixmap pixels;
    NUI_CHECK(surface->peekPixels(&pixels));
    const SkColor4f direct = pixels.getColor4f(0, 0);
    const SkColor4f nested = pixels.getColor4f(1, 0);
    NUI_CHECK(std::abs(direct.fR - nested.fR) < 0.001f);
    NUI_CHECK(std::abs(direct.fG - nested.fG) < 0.001f);
    NUI_CHECK(std::abs(direct.fB - nested.fB) < 0.001f);
}

void two_renderer_nested_isolation() {
    const auto parent_program = pass_through_program();
    const auto leaf_program = compile(R"(
        half4 main(float2 p) { return half4(0.2, 0.7, 0.3, 1.0); }
    )");
    ui::ShaderInstance leaf{leaf_program};
    const ui::Brush nested = wrap_child(parent_program, ui::Brush{leaf});

    auto make_tree = [nested] {
        return ui::UI{PainterProbe{[nested](ui::Painter& painter) {
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 16.0f, 16.0f}, 0.0f, nested);
        }}};
    };

    auto second_tree = make_tree();
    ui::HeadlessRenderer second{{20.0f, 20.0f}, 1.0f};
    {
        auto first_tree = make_tree();
        ui::HeadlessRenderer first{{20.0f, 20.0f}, 1.0f};
        NUI_CHECK(first.render(first_tree));
        NUI_CHECK(first.pixel(8, 8).g > 150);
    }
    NUI_CHECK(second.render(second_tree));
    NUI_CHECK(second.pixel(8, 8).g > 150);
    NUI_CHECK(second.render(second_tree));
}

} // namespace

int main() {
    try {
        slot_snapshot_and_instance_isolation();
        copy_move_and_source_lifetime();
        bounded_depth_and_same_program_snapshot_reuse();
        direct_vs_child_parity();
        missing_child_is_transparent_black();
        transformed_child_coordinates();
        outer_paint_options_and_stroke_apply_once();
        nested_layout_color_semantics();
        two_renderer_nested_isolation();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL T082 shader children: " << error.what() << '\n';
        return 1;
    }
}
