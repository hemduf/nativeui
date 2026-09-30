#include "test_support.hpp"
#include "src/detail/shader_brush_access.hpp"

#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

static_assert(noexcept(ui::Brush{ui::Color{}}));
static_assert(std::is_copy_constructible_v<ui::Brush>);
static_assert(std::is_copy_assignable_v<ui::Brush>);
static_assert(std::is_nothrow_move_constructible_v<ui::Brush>);
static_assert(std::is_nothrow_move_assignable_v<ui::Brush>);
static_assert(std::is_nothrow_destructible_v<ui::Brush>);

namespace {

class PainterProbeComponent final : public ui::Component {
public:
    explicit PainterProbeComponent(std::function<void(ui::Painter&)> draw)
        : draw_(std::move(draw)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {64.0f, 24.0f};
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

std::shared_ptr<const ui::ShaderProgram> compile_program(std::string_view source) {
    const auto compiled = ui::ShaderProgram::compile(source);
    NUI_CHECK(compiled.ok());
    NUI_CHECK(compiled.program);
    NUI_CHECK(compiled.diagnostics.empty());
    return compiled.program;
}

float read_float(std::span<const std::byte> bytes, std::size_t offset = 0) {
    NUI_CHECK(offset <= bytes.size());
    NUI_CHECK(sizeof(float) <= bytes.size() - offset);
    float value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

void snapshot_value_semantics() {
    const auto program = compile_program(R"(
        uniform float value;
        half4 main(float2 p) { return half4(value, p.x / 64.0, 0.0, 1.0); }
    )");

    ui::ShaderInstance source{program};
    NUI_CHECK(source.set_float("value", 0.25f) == ui::ShaderSetResult::Ok);
    ui::Brush first{source};

    NUI_CHECK(ui::detail::ShaderBrushAccess::is_shader(first));
    NUI_CHECK(ui::detail::ShaderBrushAccess::program(first) == program.get());
    NUI_CHECK(std::abs(
        read_float(ui::detail::ShaderBrushAccess::binding_bytes(first)) - 0.25f) <
        0.0001f);

    NUI_CHECK(source.set_float("value", 0.75f) == ui::ShaderSetResult::Ok);
    ui::Brush second{source};
    NUI_CHECK(std::abs(
        read_float(ui::detail::ShaderBrushAccess::binding_bytes(first)) - 0.25f) <
        0.0001f);
    NUI_CHECK(std::abs(
        read_float(ui::detail::ShaderBrushAccess::binding_bytes(second)) - 0.75f) <
        0.0001f);

    ui::Brush copied{first};
    NUI_CHECK(ui::detail::ShaderBrushAccess::is_shader(copied));

    ui::Brush assigned{ui::Color{0.0f, 0.0f, 1.0f, 1.0f}};
    assigned = first;
    NUI_CHECK(ui::detail::ShaderBrushAccess::is_shader(assigned));
    NUI_CHECK(std::abs(
        read_float(ui::detail::ShaderBrushAccess::binding_bytes(assigned)) - 0.25f) <
        0.0001f);

    ui::Brush moved{std::move(copied)};
    NUI_CHECK(ui::detail::ShaderBrushAccess::is_shader(moved));
    NUI_CHECK(ui::detail::ShaderBrushAccess::is_transparent_solid(copied));

    ui::Brush move_assigned{ui::Color{0.0f, 1.0f, 0.0f, 1.0f}};
    move_assigned = std::move(assigned);
    NUI_CHECK(ui::detail::ShaderBrushAccess::is_shader(move_assigned));
    NUI_CHECK(ui::detail::ShaderBrushAccess::is_transparent_solid(assigned));

    auto* moved_alias = &moved;
    moved = std::move(*moved_alias);
    NUI_CHECK(ui::detail::ShaderBrushAccess::is_transparent_solid(moved));

    ui::ShaderInstance moved_source_after_snapshot{std::move(source)};
    NUI_CHECK(moved_source_after_snapshot.valid());
    NUI_CHECK(!source.valid());
    NUI_CHECK(std::abs(
        read_float(ui::detail::ShaderBrushAccess::binding_bytes(first)) - 0.25f) <
        0.0001f);
    NUI_CHECK(std::abs(
        read_float(ui::detail::ShaderBrushAccess::binding_bytes(second)) - 0.75f) <
        0.0001f);

    ui::Brush survivor = [&] {
        ui::ShaderInstance temporary{program};
        NUI_CHECK(
            temporary.set_float("value", 0.5f) == ui::ShaderSetResult::Ok);
        return ui::Brush{temporary};
    }();
    NUI_CHECK(ui::detail::ShaderBrushAccess::program(survivor) == program.get());
    NUI_CHECK(std::abs(
        read_float(ui::detail::ShaderBrushAccess::binding_bytes(survivor)) - 0.5f) <
        0.0001f);

    ui::ShaderInstance move_source{program};
    ui::ShaderInstance live{std::move(move_source)};
    NUI_CHECK(live.valid());
    NUI_CHECK(!move_source.valid());
    ui::Brush inert{move_source};
    NUI_CHECK(ui::detail::ShaderBrushAccess::is_transparent_solid(inert));
}

void same_program_two_instance_isolation() {
    const auto program = compile_program(R"(
        uniform float value;
        half4 main(float2 p) { return half4(value, 0.0, 0.0, 1.0); }
    )");

    ui::ShaderInstance first_instance{program};
    ui::ShaderInstance second_instance{program};
    NUI_CHECK(first_instance.set_float("value", 0.2f) == ui::ShaderSetResult::Ok);
    NUI_CHECK(second_instance.set_float("value", 0.8f) == ui::ShaderSetResult::Ok);

    const ui::Brush first{first_instance};
    const ui::Brush second{second_instance};

    NUI_CHECK(ui::detail::ShaderBrushAccess::program(first) == program.get());
    NUI_CHECK(ui::detail::ShaderBrushAccess::program(second) == program.get());
    NUI_CHECK(std::abs(
        read_float(ui::detail::ShaderBrushAccess::binding_bytes(first)) - 0.2f) <
        0.0001f);
    NUI_CHECK(std::abs(
        read_float(ui::detail::ShaderBrushAccess::binding_bytes(second)) - 0.8f) <
        0.0001f);

    NUI_CHECK(first_instance.set_float("value", 0.4f) == ui::ShaderSetResult::Ok);
    NUI_CHECK(second_instance.set_float("value", 0.6f) == ui::ShaderSetResult::Ok);
    NUI_CHECK(std::abs(
        read_float(ui::detail::ShaderBrushAccess::binding_bytes(first)) - 0.2f) <
        0.0001f);
    NUI_CHECK(std::abs(
        read_float(ui::detail::ShaderBrushAccess::binding_bytes(second)) - 0.8f) <
        0.0001f);
}

void render_fill_stroke_and_local_coordinates() {
    const auto program = compile_program(R"(
        uniform float red;
        half4 main(float2 p) {
            return half4(red, p.x / 64.0, 0.0, 1.0);
        }
    )");
    ui::ShaderInstance shader{program};
    NUI_CHECK(shader.set_float("red", 1.0f) == ui::ShaderSetResult::Ok);
    const ui::Brush brush{shader};

    ui::Path triangle;
    triangle.move_to({32.0f, 2.0f})
        .line_to({44.0f, 2.0f})
        .line_to({38.0f, 14.0f})
        .close();

    ui::Path line;
    line.move_to({50.0f, 8.0f}).line_to({62.0f, 8.0f});

    ui::UI tree{PainterProbe{[brush, triangle, line](ui::Painter& painter) {
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 64.0f, 24.0f}, 0.0f, {0.0f, 0.0f, 0.0f, 1.0f});
        painter.fill_rounded_rect({0.0f, 0.0f, 14.0f, 16.0f}, 0.0f, brush);
        painter.circle({24.0f, 8.0f}, 6.0f, brush);
        painter.fill_path(triangle, brush);
        painter.stroke_path(
            line,
            brush,
            ui::StrokeStyle{4.0f, ui::StrokeCap::Butt, ui::StrokeJoin::Miter, 4.0f});
    }}};

    ui::HeadlessRenderer renderer{{64.0f, 24.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto left = renderer.pixel(4, 8);
    const auto circle = renderer.pixel(24, 8);
    const auto path = renderer.pixel(38, 6);
    const auto stroke = renderer.pixel(56, 8);

    NUI_CHECK(left.r > 220 && left.g < 40);
    NUI_CHECK(circle.r > 220 && circle.g > left.g + 40);
    NUI_CHECK(path.r > 220 && path.g > circle.g);
    NUI_CHECK(stroke.r > 220 && stroke.g > path.g);
}

void transform_preserves_painter_local_coordinates() {
    const auto program = compile_program(R"(
        half4 main(float2 p) {
            return half4(1.0, p.x / 32.0, 0.0, 1.0);
        }
    )");
    ui::ShaderInstance shader{program};
    const ui::Brush brush{shader};

    ui::UI tree{PainterProbe{[brush](ui::Painter& painter) {
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 64.0f, 16.0f}, 0.0f, {0.0f, 0.0f, 0.0f, 1.0f});
        painter.fill_rounded_rect({0.0f, 0.0f, 16.0f, 16.0f}, 0.0f, brush);
        {
            auto state = painter.scoped_state();
            painter.translate(32.0f, 0.0f);
            painter.fill_rounded_rect({0.0f, 0.0f, 16.0f, 16.0f}, 0.0f, brush);
        }
    }}};

    ui::HeadlessRenderer renderer{{64.0f, 24.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto untranslated = renderer.pixel(8, 8);
    const auto translated = renderer.pixel(40, 8);
    NUI_CHECK(untranslated.r > 220 && translated.r > 220);
    const int green_delta =
        std::abs(static_cast<int>(untranslated.g) - static_cast<int>(translated.g));
    NUI_CHECK(green_delta <= 3);
}

void paint_options_and_zero_uniforms() {
    const auto program = compile_program(R"(
        uniform float red;
        half4 main(float2 p) {
            return half4(red, 0.5, 1.0, 1.0);
        }
    )");
    ui::ShaderInstance shader{program};
    const ui::Brush zero_brush{shader};

    NUI_CHECK(shader.set_float("red", 0.5f) == ui::ShaderSetResult::Ok);
    const ui::Brush brush{shader};

    ui::UI tree{PainterProbe{[zero_brush, brush](ui::Painter& painter) {
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 64.0f, 24.0f}, 0.0f, {0.0f, 0.0f, 0.0f, 1.0f});
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 16.0f, 16.0f}, 0.0f, zero_brush);
        painter.fill_rounded_rect(
            {20.0f, 0.0f, 16.0f, 16.0f},
            0.0f,
            brush,
            ui::PaintOptions{0.25f, ui::BlendMode::SourceOver});
        painter.fill_rounded_rect(
            {40.0f, 0.0f, 16.0f, 16.0f},
            0.0f,
            {0.8f, 0.5f, 0.25f, 1.0f});
        painter.fill_rounded_rect(
            {40.0f, 0.0f, 16.0f, 16.0f},
            0.0f,
            brush,
            ui::PaintOptions{1.0f, ui::BlendMode::Multiply});
    }}};

    ui::HeadlessRenderer renderer{{64.0f, 24.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto unset = renderer.pixel(8, 8);
    NUI_CHECK(unset.r < 8);
    NUI_CHECK(unset.g > 110 && unset.g < 145);
    NUI_CHECK(unset.b > 240);

    const auto translucent = renderer.pixel(28, 8);
    NUI_CHECK(translucent.r > 20 && translucent.r < 45);
    NUI_CHECK(translucent.g > 20 && translucent.g < 45);
    NUI_CHECK(translucent.b > 50 && translucent.b < 80);

    const auto multiplied = renderer.pixel(48, 8);
    NUI_CHECK(multiplied.r > 80 && multiplied.r < 125);
    NUI_CHECK(multiplied.g > 45 && multiplied.g < 90);
    NUI_CHECK(multiplied.b > 45 && multiplied.b < 90);
}

void layout_color_preserves_backend_transform() {
    const auto color_program = compile_program(R"(
        layout(color) uniform float4 tint;
        half4 main(float2 p) { return tint; }
    )");
    const auto float_program = compile_program(R"(
        uniform float4 tint;
        half4 main(float2 p) { return tint; }
    )");

    ui::ShaderInstance color_shader{color_program};
    ui::ShaderInstance float_shader{float_program};
    NUI_CHECK(color_shader.set_color("tint", {0.5f, 0.5f, 0.5f, 1.0f}) ==
              ui::ShaderSetResult::Ok);
    NUI_CHECK(float_shader.set_float4("tint", {0.5f, 0.5f, 0.5f, 1.0f}) ==
              ui::ShaderSetResult::Ok);

    const ui::Brush color_brush{color_shader};
    const ui::Brush float_brush{float_shader};

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
        painter.fill_rounded_rect({0.0f, 0.0f, 1.0f, 1.0f}, 0.0f, color_brush);
        painter.fill_rounded_rect({1.0f, 0.0f, 1.0f, 1.0f}, 0.0f, float_brush);
    }

    SkPixmap pixels;
    NUI_CHECK(surface->peekPixels(&pixels));
    const SkColor4f managed = pixels.getColor4f(0, 0);
    const SkColor4f numeric = pixels.getColor4f(1, 0);

    NUI_CHECK(managed.fR < numeric.fR - 0.10f);
    NUI_CHECK(numeric.fR > 0.45f && numeric.fR < 0.55f);
}

void two_renderer_isolation() {
    const auto program = compile_program(R"(
        half4 main(float2 p) { return half4(1.0, 0.0, 0.0, 1.0); }
    )");
    ui::ShaderInstance shader{program};
    const ui::Brush brush{shader};

    auto make_tree = [brush] {
        return ui::UI{PainterProbe{[brush](ui::Painter& painter) {
            painter.fill_rounded_rect({0.0f, 0.0f, 32.0f, 16.0f}, 0.0f, brush);
        }}};
    };

    auto second_tree = make_tree();
    ui::HeadlessRenderer second{{32.0f, 16.0f}, 1.0f};

    {
        auto first_tree = make_tree();
        ui::HeadlessRenderer first{{32.0f, 16.0f}, 1.0f};
        NUI_CHECK(first.render(first_tree));
        NUI_CHECK(first.pixel(8, 8).r > 220);
    }

    NUI_CHECK(second.render(second_tree));
    NUI_CHECK(second.pixel(8, 8).r > 220);
    NUI_CHECK(second.render(second_tree));
    NUI_CHECK(second.pixel(8, 8).r > 220);
}

} // namespace

int main() {
    try {
        snapshot_value_semantics();
        same_program_two_instance_isolation();
        render_fill_stroke_and_local_coordinates();
        transform_preserves_painter_local_coordinates();
        paint_options_and_zero_uniforms();
        layout_color_preserves_backend_transform();
        two_renderer_isolation();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL T081 shader Brush: " << error.what() << '\n';
        return 1;
    }
}
