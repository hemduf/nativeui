#include <nativeui/nativeui.hpp>

#include <string_view>

void advanced_rendering_gradient_snippet(ui::Painter& painter)
{
    ui::LinearGradient gradient{
        {0.0f, 0.0f},
        {240.0f, 0.0f},
        {
            {0.0f, {0.10f, 0.14f, 0.22f, 1.0f}},
            {1.0f, {0.42f, 0.58f, 0.90f, 1.0f}},
        }};

    painter.fill_rounded_rect(
        {0.0f, 0.0f, 240.0f, 48.0f},
        8.0f,
        ui::Brush{gradient});
}

ui::Brush advanced_rendering_shader_snippet()
{
    constexpr std::string_view source = R"(
        uniform float gain;
        layout(color) uniform half4 tint;

        half4 main(float2 p) {
            return half4(tint.rgb * gain, tint.a);
        }
    )";

    auto compiled = ui::ShaderProgram::compile(source);
    if (!compiled.ok())
        return ui::Brush{ui::Color{0.0f, 0.0f, 0.0f, 0.0f}};

    ui::ShaderInstance instance{compiled.program};
    if (instance.set_float("gain", 0.75f) != ui::ShaderSetResult::Ok)
        return ui::Brush{ui::Color{0.0f, 0.0f, 0.0f, 0.0f}};
    if (instance.set_color("tint", {0.20f, 0.75f, 1.0f, 1.0f}) !=
        ui::ShaderSetResult::Ok)
        return ui::Brush{ui::Color{0.0f, 0.0f, 0.0f, 0.0f}};

    return ui::Brush{instance};
}

int main()
{
    const auto brush = advanced_rendering_shader_snippet();
    (void)brush;
    return 0;
}
