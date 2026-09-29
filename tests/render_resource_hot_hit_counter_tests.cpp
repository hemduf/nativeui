#include "src/detail/image_texture_test_seams.hpp"
#include "src/detail/render_resource_materialization.hpp"
#include "src/detail/shader_brush_access.hpp"
#include "src/detail/shader_test_seams.hpp"
#include "test_support.hpp"

#include <array>
#include <cstddef>

namespace ui::detail {
[[nodiscard]] sk_sp<SkShader> materialize_image_texture(
    const ImageTexture& texture);
}

namespace {

constexpr std::array<std::byte, 76> kTinyRgbaPng{
    std::byte{137}, std::byte{80}, std::byte{78}, std::byte{71},
    std::byte{13}, std::byte{10}, std::byte{26}, std::byte{10},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{13},
    std::byte{73}, std::byte{72}, std::byte{68}, std::byte{82},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{2},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{2},
    std::byte{8}, std::byte{6}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{114}, std::byte{182}, std::byte{13},
    std::byte{36}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{19}, std::byte{73}, std::byte{68}, std::byte{65},
    std::byte{84}, std::byte{120}, std::byte{156}, std::byte{99},
    std::byte{248}, std::byte{207}, std::byte{192}, std::byte{240},
    std::byte{31}, std::byte{12}, std::byte{129}, std::byte{52},
    std::byte{16}, std::byte{48}, std::byte{0}, std::byte{0},
    std::byte{65}, std::byte{201}, std::byte{7}, std::byte{249},
    std::byte{194}, std::byte{177}, std::byte{61}, std::byte{220},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{73}, std::byte{69}, std::byte{78}, std::byte{68},
    std::byte{174}, std::byte{66}, std::byte{96}, std::byte{130},
};

[[nodiscard]] ui::ImageTexture texture(const ui::Image& image) {
    return ui::ImageTexture{
        image,
        {0.0f, 0.0f, 2.0f, 2.0f},
        {0.0f, 0.0f, 16.0f, 16.0f}};
}

void gradient_warm_hit_skips_materialization() {
    const ui::LinearGradient gradient{
        {0.0f, 0.0f},
        {32.0f, 0.0f},
        {
            {0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
            {0.5f, {0.0f, 1.0f, 0.0f, 1.0f}},
            {1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},
        }};

    ui::detail::RenderResourceMaterializationContext context;
    std::size_t creates = 0;
    auto factory = [&] {
        ++creates;
        return ui::detail::GradientCacheAccess::materialize(gradient);
    };

    context.begin_frame();
    auto cold = context.acquire_linear_gradient(gradient, factory);
    NUI_CHECK(cold && cold.retained && !cold.hit);
    cold = {};
    context.end_frame();

    context.begin_frame();
    auto warm = context.acquire_linear_gradient(gradient, factory);
    NUI_CHECK(warm && warm.retained && warm.hit);
    NUI_CHECK(creates == 1U);
    context.end_frame();
}

void image_texture_warm_hit_skips_decode_and_materialization() {
    const auto decode_before =
        ui::detail::image_decode_call_count_for_test();
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());
    NUI_CHECK(
        ui::detail::image_decode_call_count_for_test() ==
        decode_before + 1U);

    const auto value = texture(image);
    ui::detail::RenderResourceMaterializationContext context;

    context.begin_frame();
    auto cold = context.acquire_image_texture(
        value, [&] { return ui::detail::materialize_image_texture(value); });
    NUI_CHECK(cold && cold.retained && !cold.hit);
    const auto decode_after_cold =
        ui::detail::image_decode_call_count_for_test();
    const auto materializations_after_cold =
        ui::detail::image_texture_materialization_call_count_for_test();
    cold = {};
    context.end_frame();

    context.begin_frame();
    auto warm = context.acquire_image_texture(
        value, [&] { return ui::detail::materialize_image_texture(value); });
    NUI_CHECK(warm && warm.retained && warm.hit);
    NUI_CHECK(
        ui::detail::image_decode_call_count_for_test() ==
        decode_after_cold);
    NUI_CHECK(
        ui::detail::image_texture_materialization_call_count_for_test() ==
        materializations_after_cold);
    context.end_frame();
}

void runtime_shader_warm_hit_skips_compile_and_materialization() {
    const auto compile_before =
        ui::detail::shader_compile_call_count_for_test();
    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform float gain;
        half4 main(float2) {
            return half4(gain, gain, gain, 1.0);
        }
    )");
    NUI_CHECK(compiled.ok());
    NUI_CHECK(
        ui::detail::shader_compile_call_count_for_test() ==
        compile_before + 1U);

    ui::ShaderInstance instance{compiled.program};
    NUI_CHECK(instance.set_float("gain", 0.5f) == ui::ShaderSetResult::Ok);
    const ui::Brush brush{instance};
    const auto* snapshot = ui::detail::ShaderBrushAccess::snapshot(brush);
    NUI_CHECK(snapshot && *snapshot);

    ui::detail::RenderResourceMaterializationContext context;
    context.begin_frame();
    auto cold = context.acquire_runtime_shader(
        *snapshot,
        [snapshot] { return ui::detail::materialize_shader_brush(*snapshot); });
    NUI_CHECK(cold && cold.retained && !cold.hit);
    const auto compiles_after_cold =
        ui::detail::shader_compile_call_count_for_test();
    const auto materializations_after_cold =
        ui::detail::shader_materialization_call_count_for_test();
    cold = {};
    context.end_frame();

    context.begin_frame();
    auto warm = context.acquire_runtime_shader(
        *snapshot,
        [snapshot] { return ui::detail::materialize_shader_brush(*snapshot); });
    NUI_CHECK(warm && warm.retained && warm.hit);
    NUI_CHECK(
        ui::detail::shader_compile_call_count_for_test() ==
        compiles_after_cold);
    NUI_CHECK(
        ui::detail::shader_materialization_call_count_for_test() ==
        materializations_after_cold);
    context.end_frame();
}

void effect_warm_hit_skips_materialization() {
    const auto effect = ui::Effect::drop_shadow(
        {3.0f, -2.0f}, 5.0f, {0.2f, 0.3f, 0.4f, 0.8f});
    ui::detail::RenderResourceMaterializationContext context;

    context.begin_frame();
    auto cold = context.acquire_effect(
        effect,
        [&effect] { return ui::detail::EffectCacheAccess::materialize(effect); });
    NUI_CHECK(cold && cold.retained && !cold.hit);
    const auto materializations_after_cold =
        ui::detail::effect_materialization_call_count_for_test();
    NUI_CHECK(materializations_after_cold > 0U);
    cold = {};
    context.end_frame();

    context.begin_frame();
    auto warm = context.acquire_effect(
        effect,
        [&effect] { return ui::detail::EffectCacheAccess::materialize(effect); });
    NUI_CHECK(warm && warm.retained && warm.hit);
    NUI_CHECK(
        ui::detail::effect_materialization_call_count_for_test() ==
        materializations_after_cold);
    context.end_frame();
}

} // namespace

int main() {
    gradient_warm_hit_skips_materialization();
    image_texture_warm_hit_skips_decode_and_materialization();
    runtime_shader_warm_hit_skips_compile_and_materialization();
    effect_warm_hit_skips_materialization();
    return 0;
}
