#include "src/detail/painter_private_hooks.hpp"
#include "src/detail/render_resource_materialization.hpp"
#include "src/detail/raster_cache_renderer.hpp"
#include "test_support.hpp"
#include "benchmarks/t051_benchmark_harness.hpp"
#include <nativeui/detail/raster_cache_access.hpp>

#include "include/core/SkColor.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "include/core/SkImageInfo.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <iostream>
#include <limits>
#include <utility>
#include <string_view>
#include <vector>

namespace ui {

struct TreeTestAccess {
    static NodeId root_id(const Tree& tree) {
        return tree.root_ ? tree.root_->id : 0U;
    }

    static void paint_with_resources(
        Tree& tree,
        SkCanvas& canvas,
        PlatformServices& platform,
        const detail::PainterPrivateHooks* hooks) {
        tree.paint_with_resources(canvas, platform, hooks);
    }
};

} // namespace ui

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

[[nodiscard]] ui::ImageTexture texture_at(const ui::Image& image, float x) {
    return ui::ImageTexture{
        image,
        {0.0f, 0.0f, 2.0f, 2.0f},
        {x, 0.0f, 16.0f, 16.0f}};
}

[[nodiscard]] ui::ImageTexture texture(const ui::Image& image) {
    return texture_at(image, 0.0f);
}

struct HookState {
    ui::detail::RenderResourceMaterializationContext resources;
    int shader_creates{};
    int effect_creates{};
    int gradient_creates{};
};

[[nodiscard]] sk_sp<SkShader> cached_shader_hook(
    void* opaque,
    const std::shared_ptr<const ui::detail::ShaderBrushSnapshot>& snapshot) {
    auto& state = *static_cast<HookState*>(opaque);
    auto acquisition = state.resources.acquire_runtime_shader(
        snapshot,
        [&] {
            ++state.shader_creates;
            return ui::detail::materialize_shader_brush(snapshot);
        });
    return acquisition ? std::move(acquisition.shader) : sk_sp<SkShader>{};
}

[[nodiscard]] sk_sp<SkImageFilter> cached_effect_hook(
    void* opaque,
    const ui::Effect& effect) {
    auto& state = *static_cast<HookState*>(opaque);
    auto acquisition = state.resources.acquire_effect(
        effect,
        [&] {
            ++state.effect_creates;
            return ui::detail::EffectCacheAccess::materialize(effect);
        });
    return acquisition ? std::move(acquisition.filter)
                       : sk_sp<SkImageFilter>{};
}

[[nodiscard]] sk_sp<SkShader> cached_linear_gradient_hook(
    void* opaque,
    const ui::LinearGradient& gradient) {
    auto& state = *static_cast<HookState*>(opaque);
    auto acquisition = state.resources.acquire_linear_gradient(
        gradient,
        [&] {
            ++state.gradient_creates;
            return ui::detail::GradientCacheAccess::materialize(gradient);
        });
    return acquisition ? std::move(acquisition.shader) : sk_sp<SkShader>{};
}

void tree_painter_gradient_hits_after_warmup() {
    const ui::Brush brush{ui::LinearGradient{
        {0.0f, 0.0f},
        {32.0f, 0.0f},
        {
            {0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
            {0.5f, {0.0f, 1.0f, 0.0f, 1.0f}},
            {1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},
        }}};

    ui::Tree tree{ui::compile(ui::make_spec(ui::Canvas{
        32.0f, 32.0f, [brush](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 32.0f, 32.0f}, brush);
        }}))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({32.0f, 32.0f});

    const auto info = SkImageInfo::MakeN32Premul(32, 32);
    auto surface = SkSurfaces::Raster(info);
    NUI_CHECK(surface);

    HookState state;
    const ui::detail::PainterPrivateHooks hooks{
        &state,
        nullptr,
        nullptr,
        nullptr,
        &cached_linear_gradient_hook};

    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(state.gradient_creates == 1);
    NUI_CHECK(state.resources.retained_entries() == 1);

    tree.invalidate();
    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(state.gradient_creates == 1);
    NUI_CHECK(state.resources.retained_entries() == 1);
}

void equivalent_gradient_descriptions_hit_semantically() {
    const ui::LinearGradient first{
        {1.0f, 2.0f},
        {20.0f, 8.0f},
        {
            {0.0f, {0.1f, 0.2f, 0.3f, 1.0f}},
            {0.4f, {0.4f, 0.5f, 0.6f, 0.8f}},
            {1.0f, {0.7f, 0.8f, 0.9f, 1.0f}},
        }};
    const ui::LinearGradient same{
        {1.0f, 2.0f},
        {20.0f, 8.0f},
        {
            {0.0f, {0.1f, 0.2f, 0.3f, 1.0f}},
            {0.4f, {0.4f, 0.5f, 0.6f, 0.8f}},
            {1.0f, {0.7f, 0.8f, 0.9f, 1.0f}},
        }};

    ui::detail::RenderResourceMaterializationContext context;
    int creates = 0;
    auto first_resource = context.acquire_linear_gradient(first, [&] {
        ++creates;
        return ui::detail::GradientCacheAccess::materialize(first);
    });
    auto same_resource = context.acquire_linear_gradient(same, [&] {
        ++creates;
        return ui::detail::GradientCacheAccess::materialize(same);
    });

    NUI_CHECK(first_resource && same_resource);
    NUI_CHECK(!first_resource.hit && same_resource.hit);
    NUI_CHECK(first_resource.shader == same_resource.shader);
    NUI_CHECK(creates == 1);
}

void tree_painter_runtime_shader_hits_after_warmup() {
    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform float gain;
        half4 main(float2) {
            return half4(gain, 0.0, 0.0, 1.0);
        }
    )");
    NUI_CHECK(compiled.ok());

    ui::ShaderInstance instance{compiled.program};
    NUI_CHECK(instance.set_float("gain", 0.5f) == ui::ShaderSetResult::Ok);
    const ui::Brush brush{instance};

    ui::Tree tree{ui::compile(ui::make_spec(ui::Canvas{
        32.0f, 32.0f, [brush](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 32.0f, 32.0f}, brush);
        }}))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({32.0f, 32.0f});

    const auto info = SkImageInfo::MakeN32Premul(32, 32);
    auto surface = SkSurfaces::Raster(info);
    NUI_CHECK(surface);

    HookState state;
    const ui::detail::PainterPrivateHooks hooks{
        &state,
        nullptr,
        &cached_shader_hook};

    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(state.shader_creates == 1);
    NUI_CHECK(state.resources.retained_entries() == 1);

    tree.invalidate();
    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(state.shader_creates == 1);
    NUI_CHECK(state.resources.retained_entries() == 1);
}

class EffectProbeComponent final : public ui::Component {
public:
    explicit EffectProbeComponent(ui::Effect effect) : effect_(effect) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {32.0f, 32.0f};
    }

    void paint(ui::PaintContext& context) const override {
        auto layer = context.painter().scoped_layer(
            context.bounds(), effect_);
        context.painter().fill_rounded_rect(
            context.bounds(), 0.0f, ui::Color{1.0f, 0.0f, 0.0f, 1.0f});
    }

private:
    ui::Effect effect_;
};

void tree_painter_effect_hits_after_warmup() {
    const auto effect = ui::Effect::gaussian_blur(4.0f, 4.0f);
    ui::Spec spec{
        [effect] { return std::make_unique<EffectProbeComponent>(effect); },
        {}};
    ui::Tree tree{ui::compile(std::move(spec))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({32.0f, 32.0f});

    const auto info = SkImageInfo::MakeN32Premul(32, 32);
    auto surface = SkSurfaces::Raster(info);
    NUI_CHECK(surface);

    HookState state;
    const ui::detail::PainterPrivateHooks hooks{
        &state,
        nullptr,
        nullptr,
        &cached_effect_hook};

    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(state.effect_creates == 1);
    NUI_CHECK(state.resources.retained_entries() == 1);

    tree.invalidate();
    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(state.effect_creates == 1);
    NUI_CHECK(state.resources.retained_entries() == 1);
}

void retained_hit_skips_backend_creation() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::detail::RenderResourceMaterializationContext context;
    int creates = 0;
    auto factory = [&] {
        ++creates;
        return SkShaders::Color(SK_ColorRED);
    };

    auto first = context.acquire_image_texture(texture(image), factory);
    NUI_CHECK(first);
    NUI_CHECK(first.shader);
    NUI_CHECK(first.frame_lease);
    NUI_CHECK(!first.hit);
    NUI_CHECK(first.retained);
    NUI_CHECK(creates == 1);
    NUI_CHECK(context.retained_entries() == 1);

    auto second = context.acquire_image_texture(texture(image), factory);
    NUI_CHECK(second);
    NUI_CHECK(second.shader);
    NUI_CHECK(second.frame_lease);
    NUI_CHECK(second.hit);
    NUI_CHECK(second.retained);
    NUI_CHECK(creates == 1);
    NUI_CHECK(first.shader == second.shader);
    NUI_CHECK(first.frame_lease == second.frame_lease);
    NUI_CHECK(context.retained_entries() == 1);
    NUI_CHECK(context.retained_accounted_bytes() == 0);
}

void retained_hit_reuses_backend_resource_across_frames() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::detail::RenderResourceMaterializationContext context;
    int creates = 0;
    auto factory = [&] {
        ++creates;
        return SkShaders::Color(SK_ColorRED);
    };

    context.begin_frame();
    {
        auto first = context.acquire_image_texture(texture(image), factory);
        NUI_CHECK(first && first.retained && !first.hit);
        NUI_CHECK(first.frame_lease);
        NUI_CHECK(creates == 1);
    }
    context.end_frame();

    context.begin_frame();
    {
        auto warm = context.acquire_image_texture(texture(image), factory);
        NUI_CHECK(warm && warm.retained && warm.hit);
        NUI_CHECK(warm.frame_lease);
        NUI_CHECK(creates == 1);
    }
    context.end_frame();

    NUI_CHECK(context.retained_entries() == 1);
    NUI_CHECK(context.retained_accounted_bytes() == 0);
}

void frame_lease_survives_cache_clear() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::detail::RenderResourceMaterializationContext context;
    auto acquisition = context.acquire_image_texture(
        texture(image), [] { return SkShaders::Color(SK_ColorRED); });
    NUI_CHECK(acquisition && acquisition.frame_lease);

    const auto shader = acquisition.shader;
    const auto frame_lease = acquisition.frame_lease;
    context.clear();

    NUI_CHECK(context.retained_entries() == 0);
    NUI_CHECK(frame_lease);
    NUI_CHECK(*frame_lease == shader);
    NUI_CHECK(acquisition.shader == shader);
}

void different_semantics_create_independent_entries() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::detail::RenderResourceMaterializationContext context;
    int creates = 0;
    auto factory = [&] {
        ++creates;
        return SkShaders::Color(SK_ColorGREEN);
    };

    auto color = texture(image);
    auto data = color;
    data.set_interpretation(ui::TextureInterpretation::Data);

    auto color_acquisition = context.acquire_image_texture(color, factory);
    auto data_acquisition = context.acquire_image_texture(data, factory);
    NUI_CHECK(color_acquisition && data_acquisition);
    NUI_CHECK(creates == 2);
    NUI_CHECK(context.retained_entries() == 2);
}

void renderer_owned_texture_storage_is_accounted() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::detail::RenderResourceMaterializationContext context;
    auto factory = [] { return SkShaders::Color(SK_ColorGREEN); };

    auto direct = context.acquire_image_texture(texture(image), factory);
    NUI_CHECK(direct && direct.retained);
    NUI_CHECK(context.retained_accounted_bytes() == 0);

    context.clear();
    auto mipmapped = texture(image);
    ui::TextureSampling sampling;
    sampling.set_mipmap(ui::TextureMipmap::Linear);
    mipmapped.set_sampling(sampling);
    auto mip = context.acquire_image_texture(mipmapped, factory);
    NUI_CHECK(mip && mip.retained);
    NUI_CHECK(context.retained_accounted_bytes() == 32);

    context.clear();
    ui::ImageTexture isolated{
        image,
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 16.0f, 16.0f}};
    isolated.set_tile_mode(
        ui::TextureTileMode::Repeat, ui::TextureTileMode::Repeat);
    auto tile = context.acquire_image_texture(isolated, factory);
    NUI_CHECK(tile && tile.retained);
    NUI_CHECK(context.retained_accounted_bytes() == 4);

    context.clear();
    auto shared_full = texture(image);
    shared_full.set_tile_mode(
        ui::TextureTileMode::Repeat, ui::TextureTileMode::Repeat);
    auto repeat = context.acquire_image_texture(shared_full, factory);
    NUI_CHECK(repeat && repeat.retained);
    NUI_CHECK(context.retained_accounted_bytes() == 0);
}

void runtime_shader_shares_the_same_budget_and_hits() {
    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform float gain;
        half4 main(float2) {
            return half4(gain, gain, gain, 1.0);
        }
    )");
    NUI_CHECK(compiled.ok());

    ui::ShaderInstance instance{compiled.program};
    NUI_CHECK(instance.set_float("gain", 0.5f) == ui::ShaderSetResult::Ok);
    const ui::Brush brush{instance};
    const auto* snapshot = ui::detail::ShaderBrushAccess::snapshot(brush);
    NUI_CHECK(snapshot && *snapshot);

    ui::detail::RenderResourceMaterializationContext context;
    int creates = 0;
    auto factory = [&] {
        ++creates;
        return ui::detail::materialize_shader_brush(*snapshot);
    };

    context.begin_frame();
    auto first = context.acquire_runtime_shader(*snapshot, factory);
    NUI_CHECK(first);
    NUI_CHECK(creates == 1);
    NUI_CHECK(context.retained_entries() == 1);
    context.end_frame();

    context.begin_frame();
    auto second = context.acquire_runtime_shader(*snapshot, factory);
    NUI_CHECK(second);
    NUI_CHECK(creates == 1);
    NUI_CHECK(first.shader == second.shader);
    NUI_CHECK(context.retained_entries() == 1);
    context.end_frame();
}

void image_and_runtime_shader_share_one_entry_budget() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    const auto compiled = ui::ShaderProgram::compile(R"(
        half4 main(float2) { return half4(1.0); }
    )");
    NUI_CHECK(compiled.ok());
    ui::ShaderInstance instance{compiled.program};
    const ui::Brush brush{instance};
    const auto* snapshot = ui::detail::ShaderBrushAccess::snapshot(brush);
    NUI_CHECK(snapshot && *snapshot);

    ui::detail::RenderResourceMaterializationContext context;
    context.begin_frame();
    NUI_CHECK(context.acquire_image_texture(
        texture(image), [] { return SkShaders::Color(SK_ColorRED); }));
    NUI_CHECK(context.acquire_runtime_shader(
        *snapshot, [snapshot] {
            return ui::detail::materialize_shader_brush(*snapshot);
        }));
    NUI_CHECK(context.retained_entries() == 2);
    context.end_frame();
}

void runtime_shader_accounts_nested_image_storage() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    auto mipmapped = texture(image);
    ui::TextureSampling sampling;
    sampling.set_mipmap(ui::TextureMipmap::Linear);
    mipmapped.set_sampling(sampling);

    const auto parent_program = ui::ShaderProgram::compile(R"(
        uniform shader child;
        half4 main(float2 p) { return child.eval(p); }
    )");
    NUI_CHECK(parent_program.ok());

    ui::ShaderInstance inner{parent_program.program};
    NUI_CHECK(inner.set_child("child", ui::Brush{mipmapped}) ==
              ui::ShaderSetResult::Ok);

    ui::ShaderInstance outer{parent_program.program};
    NUI_CHECK(outer.set_child("child", ui::Brush{inner}) ==
              ui::ShaderSetResult::Ok);

    const ui::Brush brush{outer};
    const auto* snapshot = ui::detail::ShaderBrushAccess::snapshot(brush);
    NUI_CHECK(snapshot && *snapshot);

    ui::detail::RenderResourceMaterializationContext context;
    auto acquisition = context.acquire_runtime_shader(
        *snapshot, [snapshot] {
            return ui::detail::materialize_shader_brush(*snapshot);
        });
    NUI_CHECK(acquisition && acquisition.retained);
    NUI_CHECK(context.retained_entries() == 1);
    NUI_CHECK(context.retained_accounted_bytes() == 32);
}

void effects_share_the_unified_cache_budget() {
    ui::detail::RenderResourceMaterializationContext context;
    const auto blur = ui::Effect::gaussian_blur(4.0f, 6.0f);
    const auto shadow = ui::Effect::drop_shadow(
        {3.0f, -2.0f}, 5.0f, {0.1f, 0.2f, 0.3f, 0.7f});

    int blur_creates = 0;
    auto first = context.acquire_effect(blur, [&] {
        ++blur_creates;
        return ui::detail::EffectCacheAccess::materialize(blur);
    });
    auto warm = context.acquire_effect(blur, [&] {
        ++blur_creates;
        return ui::detail::EffectCacheAccess::materialize(blur);
    });
    NUI_CHECK(first && warm);
    NUI_CHECK(blur_creates == 1);
    NUI_CHECK(first.filter == warm.filter);

    auto distinct = context.acquire_effect(shadow, [&] {
        return ui::detail::EffectCacheAccess::materialize(shadow);
    });
    NUI_CHECK(distinct);
    NUI_CHECK(context.retained_entries() == 2);
    NUI_CHECK(context.retained_accounted_bytes() == 0);
}

void image_and_runtime_shader_share_hard_entry_limit() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    const auto compiled = ui::ShaderProgram::compile(R"(
        half4 main(float2) { return half4(1.0); }
    )");
    NUI_CHECK(compiled.ok());
    ui::ShaderInstance instance{compiled.program};
    const ui::Brush brush{instance};
    const auto* snapshot = ui::detail::ShaderBrushAccess::snapshot(brush);
    NUI_CHECK(snapshot && *snapshot);

    ui::detail::RenderResourceMaterializationContext context;
    context.begin_frame();

    auto shader = context.acquire_runtime_shader(
        *snapshot, [snapshot] {
            return ui::detail::materialize_shader_brush(*snapshot);
        });
    NUI_CHECK(shader && shader.retained && !shader.hit);

    for (std::size_t index = 0;
         index + 1U <
             ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries;
         ++index) {
        auto image_resource = context.acquire_image_texture(
            texture_at(image, static_cast<float>(index)),
            [] { return SkShaders::Color(SK_ColorRED); });
        NUI_CHECK(image_resource && image_resource.retained && !image_resource.hit);
    }

    NUI_CHECK(
        context.retained_entries() ==
        ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries);

    const auto overflow_key = texture_at(
        image,
        static_cast<float>(
            ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries));
    auto transient = context.acquire_image_texture(
        overflow_key, [] { return SkShaders::Color(SK_ColorBLUE); });
    NUI_CHECK(transient && !transient.retained && !transient.hit);
    NUI_CHECK(
        context.retained_entries() ==
        ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries);

    context.end_frame();

    transient = {};
    auto retained_after_frame = context.acquire_image_texture(
        overflow_key, [] { return SkShaders::Color(SK_ColorGREEN); });
    NUI_CHECK(retained_after_frame && retained_after_frame.retained);
    NUI_CHECK(
        context.retained_entries() ==
        ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries);
}

void invalid_texture_is_transient_and_never_retained() {
    ui::detail::RenderResourceMaterializationContext context;
    int creates = 0;

    ui::ImageTexture invalid;
    auto acquisition = context.acquire_image_texture(invalid, [&] {
        ++creates;
        return SkShaders::Color(SK_ColorBLUE);
    });
    NUI_CHECK(acquisition);
    NUI_CHECK(acquisition.shader);
    NUI_CHECK(!acquisition.frame_lease);
    NUI_CHECK(!acquisition.hit);
    NUI_CHECK(!acquisition.retained);
    NUI_CHECK(creates == 1);
    NUI_CHECK(context.retained_entries() == 0);
}

void frame_scope_pins_resources_until_completion() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::detail::RenderResourceMaterializationContext context;
    context.begin_frame();

    for (std::size_t index = 0;
         index < ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries;
         ++index) {
        auto acquisition = context.acquire_image_texture(
            texture_at(image, static_cast<float>(index)),
            [] { return SkShaders::Color(SK_ColorRED); });
        NUI_CHECK(acquisition && acquisition.retained && !acquisition.hit);
    }

    NUI_CHECK(
        context.retained_entries() ==
        ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries);

    {
        auto transient = context.acquire_image_texture(
            texture_at(
                image,
                static_cast<float>(
                    ui::detail::RenderResourceMaterializationContext::
                        kMaxRetainedEntries)),
            [] { return SkShaders::Color(SK_ColorBLUE); });
        NUI_CHECK(transient);
        NUI_CHECK(!transient.retained);
        NUI_CHECK(!transient.hit);
    }

    context.end_frame();

    auto replacement = context.acquire_image_texture(
        texture_at(
            image,
            static_cast<float>(
                ui::detail::RenderResourceMaterializationContext::
                    kMaxRetainedEntries)),
        [] { return SkShaders::Color(SK_ColorGREEN); });
    NUI_CHECK(replacement && replacement.retained && !replacement.hit);
    NUI_CHECK(
        context.retained_entries() ==
        ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries);
}

void transient_frame_bookkeeping_is_released_at_frame_boundary() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::detail::RenderResourceMaterializationContext context;
    context.begin_frame();

    for (std::size_t index = 0;
         index < ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries;
         ++index) {
        auto retained = context.acquire_image_texture(
            texture_at(image, static_cast<float>(index)),
            [] { return SkShaders::Color(SK_ColorRED); });
        NUI_CHECK(retained && retained.retained);
    }

    for (std::size_t index = 0; index < 64U; ++index) {
        auto transient = context.acquire_image_texture(
            texture_at(
                image,
                static_cast<float>(
                    ui::detail::RenderResourceMaterializationContext::
                        kMaxRetainedEntries + index)),
            [] { return SkShaders::Color(SK_ColorBLUE); });
        NUI_CHECK(transient && !transient.retained);
    }

    NUI_CHECK(context.transient_frame_lease_capacity_for_test() >= 64U);
    context.end_frame();
    NUI_CHECK(context.transient_frame_lease_capacity_for_test() == 0U);
    NUI_CHECK(
        context.retained_entries() ==
        ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries);
}

void clear_releases_context_owned_frame_leases() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::detail::RenderResourceMaterializationContext context;
    context.begin_frame();

    std::weak_ptr<const sk_sp<SkShader>> lease;
    {
        auto acquisition = context.acquire_image_texture(
            texture(image), [] { return SkShaders::Color(SK_ColorRED); });
        NUI_CHECK(acquisition && acquisition.retained && acquisition.frame_lease);
        lease = acquisition.frame_lease;
    }

    NUI_CHECK(!lease.expired());
    context.clear();

    NUI_CHECK(context.retained_entries() == 0);
    NUI_CHECK(context.retained_accounted_bytes() == 0);
    NUI_CHECK(lease.expired());
}

void clear_is_instance_local() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::detail::RenderResourceMaterializationContext a;
    ui::detail::RenderResourceMaterializationContext b;

    auto a_acquisition = a.acquire_image_texture(
        texture(image), [] { return SkShaders::Color(SK_ColorRED); });
    auto b_acquisition = b.acquire_image_texture(
        texture(image), [] { return SkShaders::Color(SK_ColorRED); });
    NUI_CHECK(a_acquisition && b_acquisition);
    NUI_CHECK(a.retained_entries() == 1);
    NUI_CHECK(b.retained_entries() == 1);

    a.clear();
    NUI_CHECK(a.retained_entries() == 0);
    NUI_CHECK(b.retained_entries() == 1);
    NUI_CHECK(b_acquisition.frame_lease);
}



struct RasterHookState {
    ui::detail::RenderResourceMaterializationContext resources;
    int creates{};
    bool fail_after_validate_once{};
    bool fail_shader_materialization_once{};
    enum class Fault { None, Surface, Submit, Snapshot };
    Fault fault{Fault::None};
    std::function<void()> after_snapshot;
    int hits{};
    float device_scale{1.0f};
};

[[nodiscard]] sk_sp<SkShader> raster_test_shader_hook(
    void* opaque,
    const std::shared_ptr<const ui::detail::ShaderBrushSnapshot>& snapshot) {
    auto& state = *static_cast<RasterHookState*>(opaque);
    if (state.fail_shader_materialization_once) {
        state.fail_shader_materialization_once = false;
        throw std::bad_alloc{};
    }
    auto acquisition = state.resources.acquire_runtime_shader(
        snapshot, [&] { return ui::detail::materialize_shader_brush(snapshot); });
    return acquisition ? std::move(acquisition.shader) : sk_sp<SkShader>{};
}

bool raster_test_hook(
    void* opaque,
    const ui::detail::RasterCachePaintRequest& request,
    SkCanvas& destination,
    void* callback_state,
    ui::detail::RasterCachePaintCallback paint_callback,
    ui::detail::RasterCacheValidateCallback validate_callback,
    ui::detail::RasterCacheCommitCallback commit_callback) {
    auto& state = *static_cast<RasterHookState*>(opaque);
    const ui::detail::PainterPrivateHooks nested_hooks{
        &state, nullptr, &raster_test_shader_hook, nullptr, nullptr, nullptr, nullptr};
    const ui::detail::RasterCacheBackend backend{
        .state = &state,
        .device_scale = state.device_scale,
        .max_surface_size = 16777216,
        .painter_hooks = &nested_hooks,
        .create_surface = [](void* raw, const SkImageInfo& info) -> sk_sp<SkSurface> {
            auto& hook = *static_cast<RasterHookState*>(raw);
            if (hook.fault == RasterHookState::Fault::Surface) {
                hook.fault = RasterHookState::Fault::None;
                throw std::bad_alloc{};
            }
            ++hook.creates;
            return SkSurfaces::Raster(info);
        },
        .submit_surface = [](void* raw, SkSurface&) {
            auto& hook = *static_cast<RasterHookState*>(raw);
            if (hook.fault == RasterHookState::Fault::Submit) {
                hook.fault = RasterHookState::Fault::None;
                throw std::runtime_error("injected raster submission failure");
            }
        },
        .snapshot_surface = [](void* raw, SkSurface& surface) -> sk_sp<SkImage> {
            auto& hook = *static_cast<RasterHookState*>(raw);
            if (hook.fault == RasterHookState::Fault::Snapshot) {
                hook.fault = RasterHookState::Fault::None;
                throw std::bad_alloc{};
            }
            auto image = surface.makeImageSnapshot();
            if (hook.after_snapshot) {
                auto callback = std::exchange(hook.after_snapshot, {});
                callback();
            }
            return image;
        },
        .before_retention = [](void* raw) {
            auto& hook = *static_cast<RasterHookState*>(raw);
            if (std::exchange(hook.fail_after_validate_once, false)) {
                throw std::bad_alloc{};
            }
        },
        .did_hit = [](void* raw) noexcept {
            ++static_cast<RasterHookState*>(raw)->hits;
        }};
    return ui::detail::paint_raster_cache_boundary(
        state.resources, backend, request, destination, callback_state,
        paint_callback, validate_callback, commit_callback);
}

class PaintCounterComponent final : public ui::Component {
public:
    explicit PaintCounterComponent(std::shared_ptr<int> paints)
        : paints_(std::move(paints)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {32.0f, 32.0f};
    }

    void paint(ui::PaintContext& context) const override {
        ++*paints_;
        context.painter().fill_rounded_rect(
            context.bounds(), 0.0f, ui::Color{1.0f, 0.0f, 0.0f, 1.0f});
    }

private:
    std::shared_ptr<int> paints_;
};

void tree_raster_boundary_cold_warm_stale_warm() {
    auto paints = std::make_shared<int>(0);
    ui::Spec spec{
        [paints] { return std::make_unique<PaintCounterComponent>(paints); },
        {}};
    ui::Tree tree{ui::compile(std::move(spec))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({32.0f, 32.0f});
    const auto root_id = ui::TreeTestAccess::root_id(tree);
    NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(tree, root_id));

    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
    NUI_CHECK(surface);
    RasterHookState state;
    const ui::detail::PainterPrivateHooks hooks{
        &state, nullptr, nullptr, nullptr, nullptr, nullptr, &raster_test_hook};

    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 1);
    NUI_CHECK(state.creates == 1);
    NUI_CHECK(state.resources.retained_entries() == 1);

    tree.invalidate();
    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 1);
    NUI_CHECK(state.creates == 1);

    auto invalidate = ui::detail::RasterCacheAccess::invalidator(tree, root_id);
    NUI_CHECK(static_cast<bool>(invalidate));
    invalidate();
    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 2);
    NUI_CHECK(state.creates == 2);

    tree.invalidate();
    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 2);
    NUI_CHECK(state.creates == 2);
}


class ReentrantInvalidationComponent final : public ui::Component {
public:
    ReentrantInvalidationComponent(
        std::shared_ptr<int> paints,
        std::shared_ptr<std::function<void()>> invalidate_once)
        : paints_(std::move(paints)),
          invalidate_once_(std::move(invalidate_once)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {32.0f, 32.0f};
    }

    void paint(ui::PaintContext& context) const override {
        ++*paints_;
        context.painter().fill_rounded_rect(
            context.bounds(), 0.0f, ui::Color{0.0f, 0.0f, 1.0f, 1.0f});
        if (*invalidate_once_) {
            auto callback = std::move(*invalidate_once_);
            *invalidate_once_ = {};
            callback();
        }
    }

private:
    std::shared_ptr<int> paints_;
    std::shared_ptr<std::function<void()>> invalidate_once_;
};

void reentrant_invalidation_cannot_publish_captured_generation() {
    auto paints = std::make_shared<int>(0);
    auto invalidate_once = std::make_shared<std::function<void()>>();
    ui::Spec spec{
        [paints, invalidate_once] {
            return std::make_unique<ReentrantInvalidationComponent>(
                paints, invalidate_once);
        },
        {}};
    ui::Tree tree{ui::compile(std::move(spec))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({32.0f, 32.0f});
    const auto root_id = ui::TreeTestAccess::root_id(tree);
    NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(tree, root_id));
    *invalidate_once = ui::detail::RasterCacheAccess::invalidator(tree, root_id);
    NUI_CHECK(static_cast<bool>(*invalidate_once));

    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
    NUI_CHECK(surface);
    RasterHookState state;
    const ui::detail::PainterPrivateHooks hooks{
        &state, nullptr, nullptr, nullptr, nullptr, nullptr, &raster_test_hook};

    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 1);
    NUI_CHECK(state.resources.retained_entries() == 0);

    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 2);
    NUI_CHECK(state.resources.retained_entries() == 1);

    tree.invalidate();
    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 2);
}


void raster_retention_failure_leaves_boundary_stale_and_retryable() {
    auto paints = std::make_shared<int>(0);
    ui::Spec spec{
        [paints] { return std::make_unique<PaintCounterComponent>(paints); },
        {}};
    ui::Tree tree{ui::compile(std::move(spec))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({32.0f, 32.0f});
    const auto root_id = ui::TreeTestAccess::root_id(tree);
    NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(tree, root_id));

    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
    NUI_CHECK(surface);
    RasterHookState state;
    state.fail_after_validate_once = true;
    const ui::detail::PainterPrivateHooks hooks{
        &state, nullptr, nullptr, nullptr, nullptr, nullptr, &raster_test_hook};

    bool threw = false;
    state.resources.begin_frame();
    try {
        ui::TreeTestAccess::paint_with_resources(
            tree, *surface->getCanvas(), platform, &hooks);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    state.resources.end_frame();
    NUI_CHECK(threw);
    NUI_CHECK(*paints == 1);
    NUI_CHECK(state.resources.retained_entries() == 0);

    const auto after_failure =
        ui::detail::RasterCacheAccess::capture(tree, root_id);
    NUI_CHECK(!after_failure.expired());
    NUI_CHECK(!ui::detail::RasterCacheAccess::reusable(
        tree, root_id, after_failure));

    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 2);
    NUI_CHECK(state.resources.retained_entries() == 1);

    tree.invalidate();
    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 2);
}


class ThrowOncePaintComponent final : public ui::Component {
public:
    ThrowOncePaintComponent(
        std::shared_ptr<int> paints,
        std::shared_ptr<bool> throw_once)
        : paints_(std::move(paints)),
          throw_once_(std::move(throw_once)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {32.0f, 32.0f};
    }

    void paint(ui::PaintContext& context) const override {
        ++*paints_;
        if (*throw_once_) {
            *throw_once_ = false;
            throw std::runtime_error("injected cached subtree paint failure");
        }
        context.painter().fill_rounded_rect(
            context.bounds(), 0.0f, ui::Color{0.5f, 0.25f, 0.75f, 1.0f});
    }

private:
    std::shared_ptr<int> paints_;
    std::shared_ptr<bool> throw_once_;
};

void subtree_paint_failure_leaves_boundary_retryable() {
    auto paints = std::make_shared<int>(0);
    auto throw_once = std::make_shared<bool>(true);
    ui::Tree tree{ui::compile(ui::Spec{
        [paints, throw_once] {
            return std::make_unique<ThrowOncePaintComponent>(
                paints, throw_once);
        },
        {}})};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({32.0f, 32.0f});
    const auto root_id = ui::TreeTestAccess::root_id(tree);
    NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(tree, root_id));

    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
    NUI_CHECK(surface);
    RasterHookState state;
    const ui::detail::PainterPrivateHooks hooks{
        &state, nullptr, nullptr, nullptr, nullptr, nullptr, &raster_test_hook};

    bool threw = false;
    state.resources.begin_frame();
    try {
        ui::TreeTestAccess::paint_with_resources(
            tree, *surface->getCanvas(), platform, &hooks);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    state.resources.end_frame();
    NUI_CHECK(threw);
    NUI_CHECK(*paints == 1);
    NUI_CHECK(state.resources.retained_entries() == 0);

    const auto failed_token =
        ui::detail::RasterCacheAccess::capture(tree, root_id);
    NUI_CHECK(!ui::detail::RasterCacheAccess::reusable(
        tree, root_id, failed_token));

    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 2);
    NUI_CHECK(state.resources.retained_entries() == 1);

    tree.invalidate();
    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 2);
}

class ShaderPaintComponent final : public ui::Component {
public:
    ShaderPaintComponent(ui::Brush brush, std::shared_ptr<int> paints)
        : brush_(std::move(brush)), paints_(std::move(paints)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {32.0f, 32.0f};
    }

    void paint(ui::PaintContext& context) const override {
        ++*paints_;
        context.painter().fill_rounded_rect(
            context.bounds(), 0.0f, brush_);
    }

private:
    ui::Brush brush_;
    std::shared_ptr<int> paints_;
};

void subtree_materialization_failure_leaves_boundary_retryable() {
    const auto compiled = ui::ShaderProgram::compile(R"(
        half4 main(float2) {
            return half4(0.25, 0.5, 0.75, 1.0);
        }
    )");
    NUI_CHECK(compiled.ok());
    ui::ShaderInstance shader{compiled.program};
    ui::Brush brush{shader};
    auto paints = std::make_shared<int>(0);

    ui::Tree tree{ui::compile(ui::Spec{
        [brush, paints] {
            return std::make_unique<ShaderPaintComponent>(brush, paints);
        },
        {}})};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({32.0f, 32.0f});
    const auto root_id = ui::TreeTestAccess::root_id(tree);
    NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(tree, root_id));

    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
    NUI_CHECK(surface);
    RasterHookState state;
    state.fail_shader_materialization_once = true;
    const ui::detail::PainterPrivateHooks hooks{
        &state, nullptr, nullptr, nullptr, nullptr, nullptr, &raster_test_hook};

    bool threw = false;
    state.resources.begin_frame();
    try {
        ui::TreeTestAccess::paint_with_resources(
            tree, *surface->getCanvas(), platform, &hooks);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    state.resources.end_frame();
    NUI_CHECK(threw);
    NUI_CHECK(*paints == 1);
    NUI_CHECK(state.resources.retained_entries() == 0);

    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 2);
    NUI_CHECK(state.resources.retained_entries() == 2);

    tree.invalidate();
    state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        tree, *surface->getCanvas(), platform, &hooks);
    state.resources.end_frame();
    NUI_CHECK(*paints == 2);
}

class TransformPaintComponent final : public ui::Component {
public:
    explicit TransformPaintComponent(std::shared_ptr<int> paints)
        : paints_(std::move(paints)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {32.0f, 32.0f};
    }

    void paint(ui::PaintContext& context) const override {
        ++*paints_;
        const auto restore = context.painter().scoped_state();
        context.painter().translate(1.0f, 0.0f);
        context.painter().fill_rounded_rect(
            context.bounds(), 0.0f, ui::Color{0.0f, 1.0f, 0.0f, 1.0f});
    }

private:
    std::shared_ptr<int> paints_;
};

void unqualified_internal_transform_bypasses_retention() {
    auto paints = std::make_shared<int>(0);
    ui::Spec spec{
        [paints] { return std::make_unique<TransformPaintComponent>(paints); },
        {}};
    ui::Tree tree{ui::compile(std::move(spec))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({32.0f, 32.0f});
    NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(
        tree, ui::TreeTestAccess::root_id(tree)));

    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
    NUI_CHECK(surface);
    RasterHookState state;
    const ui::detail::PainterPrivateHooks hooks{
        &state, nullptr, nullptr, nullptr, nullptr, nullptr, &raster_test_hook};

    for (int frame = 0; frame < 2; ++frame) {
        tree.invalidate();
        state.resources.begin_frame();
        ui::TreeTestAccess::paint_with_resources(
            tree, *surface->getCanvas(), platform, &hooks);
        state.resources.end_frame();
    }
    NUI_CHECK(*paints == 2);
    NUI_CHECK(state.creates == 2);
    NUI_CHECK(state.resources.retained_entries() == 0);
}



void unqualified_effect_bypasses_raster_retention() {
    const auto effect = ui::Effect::gaussian_blur(2.0f, 2.0f);
    ui::Spec spec{
        [effect] { return std::make_unique<EffectProbeComponent>(effect); },
        {}};
    ui::Tree tree{ui::compile(std::move(spec))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({32.0f, 32.0f});
    NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(
        tree, ui::TreeTestAccess::root_id(tree)));

    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
    NUI_CHECK(surface);
    RasterHookState state;
    const ui::detail::PainterPrivateHooks hooks{
        &state, nullptr, nullptr, nullptr, nullptr, nullptr, &raster_test_hook};

    for (int frame = 0; frame < 2; ++frame) {
        tree.invalidate();
        state.resources.begin_frame();
        ui::TreeTestAccess::paint_with_resources(
            tree, *surface->getCanvas(), platform, &hooks);
        state.resources.end_frame();
    }
    NUI_CHECK(state.creates == 2);
    NUI_CHECK(state.resources.retained_entries() == 0);
}


void retained_raster_uses_lifetime_identity_and_shared_budget() {
    ui::detail::RenderResourceMaterializationContext context;
    ui::detail::RasterCacheEpoch first_epoch;
    const auto first_token = first_epoch.capture();

    const auto info = SkImageInfo::MakeN32Premul(4, 4);
    auto surface = SkSurfaces::Raster(info);
    NUI_CHECK(surface);
    surface->getCanvas()->clear(SK_ColorRED);
    auto image = surface->makeImageSnapshot();
    NUI_CHECK(image);

    const ui::detail::RenderResourceMaterializationContext::RasterCacheKey key{
        7U, first_token, {0.0f, 0.0f, 4.0f, 4.0f}, {0.0f, 0.0f, 4.0f, 4.0f}, 1.0f};
    auto retained = context.retain_raster(key, 64U, image);
    NUI_CHECK(retained);
    NUI_CHECK(context.retained_entries() == 1);
    NUI_CHECK(context.retained_accounted_bytes() == 64U);
    NUI_CHECK(context.find_raster(key) == retained.image);

    ui::detail::RasterCacheEpoch remounted_epoch;
    const auto remounted_token = remounted_epoch.capture();
    const ui::detail::RenderResourceMaterializationContext::RasterCacheKey remounted{
        7U, remounted_token, {0.0f, 0.0f, 4.0f, 4.0f}, {0.0f, 0.0f, 4.0f, 4.0f}, 1.0f};
    NUI_CHECK(!context.find_raster(remounted));

    const auto compiled = ui::ShaderProgram::compile(R"(
        half4 main(float2) { return half4(1.0); }
    )");
    NUI_CHECK(compiled.ok());
    ui::ShaderInstance instance{compiled.program};
    const ui::Brush brush{instance};
    const auto* snapshot = ui::detail::ShaderBrushAccess::snapshot(brush);
    NUI_CHECK(snapshot && *snapshot);
    auto shader = context.acquire_runtime_shader(
        *snapshot,
        [&] { return ui::detail::materialize_shader_brush(*snapshot); });
    NUI_CHECK(shader);
    NUI_CHECK(context.retained_entries() == 2);
    NUI_CHECK(context.retained_accounted_bytes() >= 64U);
}

void raster_signature_separates_scale_and_local_extent() {
    ui::detail::RenderResourceMaterializationContext context;
    ui::detail::RasterCacheEpoch epoch;
    const auto token = epoch.capture();

    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
    NUI_CHECK(surface);
    auto image = surface->makeImageSnapshot();
    NUI_CHECK(image);

    const ui::detail::RenderResourceMaterializationContext::RasterCacheKey base{
        11U, token, {-1.0f, -1.0f, 2.0f, 2.0f}, {10.0f, 10.0f, 2.0f, 2.0f}, 1.0f};
    NUI_CHECK(context.retain_raster(base, 16U, image));

    const ui::detail::RenderResourceMaterializationContext::RasterCacheKey scale{
        11U, token, {-1.0f, -1.0f, 2.0f, 2.0f}, {10.0f, 10.0f, 2.0f, 2.0f}, 2.0f};
    const ui::detail::RenderResourceMaterializationContext::RasterCacheKey extent{
        11U, token, {0.0f, 0.0f, 2.0f, 2.0f},
        {10.0f, 10.0f, 2.0f, 2.0f}, 1.0f};
    const ui::detail::RenderResourceMaterializationContext::RasterCacheKey placement{
        11U, token, {-1.0f, -1.0f, 2.0f, 2.0f},
        {11.0f, 10.0f, 2.0f, 2.0f}, 1.0f};
    NUI_CHECK(!context.find_raster(scale));
    NUI_CHECK(!context.find_raster(extent));
    NUI_CHECK(!context.find_raster(placement));
}


std::vector<std::uint32_t> read_surface_pixels(
    SkSurface& surface, int width, int height) {
    std::vector<std::uint32_t> pixels(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    const auto info = SkImageInfo::MakeN32Premul(width, height);
    NUI_CHECK(surface.readPixels(
        info,
        pixels.data(),
        static_cast<std::size_t>(width) * sizeof(std::uint32_t),
        0,
        0));
    return pixels;
}

void cold_and_warm_raster_pixels_match_reference() {
    auto make_tree = [] {
        return ui::Tree{ui::compile(ui::Spec{
            [] {
                return std::make_unique<PaintCounterComponent>(
                    std::make_shared<int>(0));
            },
            {}})};
    };
    test::MockPlatform platform;

    for (const float scale : {1.0f, 1.25f, 2.0f}) {
        const int dimension = static_cast<int>(32.0f * scale);

        auto reference_tree = make_tree();
        reference_tree.mount();
        reference_tree.layout({32.0f, 32.0f});
        auto reference_surface =
            SkSurfaces::Raster(SkImageInfo::MakeN32Premul(dimension, dimension));
        NUI_CHECK(reference_surface);
        reference_surface->getCanvas()->clear(SK_ColorTRANSPARENT);
        reference_surface->getCanvas()->scale(scale, scale);
        ui::TreeTestAccess::paint_with_resources(
            reference_tree, *reference_surface->getCanvas(), platform, nullptr);
        const auto reference =
            read_surface_pixels(*reference_surface, dimension, dimension);

        auto cached_tree = make_tree();
        cached_tree.mount();
        cached_tree.layout({32.0f, 32.0f});
        const auto root_id = ui::TreeTestAccess::root_id(cached_tree);
        NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(
            cached_tree, root_id));

        RasterHookState state;
        state.device_scale = scale;
        const ui::detail::PainterPrivateHooks hooks{
            &state, nullptr, nullptr, nullptr, nullptr, nullptr, &raster_test_hook};

        auto cold_surface =
            SkSurfaces::Raster(SkImageInfo::MakeN32Premul(dimension, dimension));
        NUI_CHECK(cold_surface);
        cold_surface->getCanvas()->clear(SK_ColorTRANSPARENT);
        cold_surface->getCanvas()->scale(scale, scale);
        state.resources.begin_frame();
        ui::TreeTestAccess::paint_with_resources(
            cached_tree, *cold_surface->getCanvas(), platform, &hooks);
        state.resources.end_frame();
        const auto cold = read_surface_pixels(*cold_surface, dimension, dimension);
        NUI_CHECK(cold == reference);
        NUI_CHECK(state.resources.retained_accounted_bytes() ==
                  static_cast<std::size_t>(dimension * dimension * 4));

        cached_tree.invalidate();
        auto warm_surface =
            SkSurfaces::Raster(SkImageInfo::MakeN32Premul(dimension, dimension));
        NUI_CHECK(warm_surface);
        warm_surface->getCanvas()->clear(SK_ColorTRANSPARENT);
        warm_surface->getCanvas()->scale(scale, scale);
        state.resources.begin_frame();
        ui::TreeTestAccess::paint_with_resources(
            cached_tree, *warm_surface->getCanvas(), platform, &hooks);
        state.resources.end_frame();
        const auto warm = read_surface_pixels(*warm_surface, dimension, dimension);
        NUI_CHECK(warm == reference);
        NUI_CHECK(warm == cold);
        NUI_CHECK(state.creates == 1 && state.hits == 1);
    }
}

void remount_same_node_id_gets_new_raster_lifetime() {
    auto paints = std::make_shared<int>(0);
    ui::Tree tree{ui::compile(ui::Spec{
        [paints] { return std::make_unique<PaintCounterComponent>(paints); },
        {}})};
    tree.mount();
    tree.layout({32.0f, 32.0f});
    const auto first_id = ui::TreeTestAccess::root_id(tree);
    NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(tree, first_id));
    const auto first_token =
        ui::detail::RasterCacheAccess::capture(tree, first_id);
    NUI_CHECK(!first_token.expired());

    tree.unmount();
    tree.mount();
    tree.layout({32.0f, 32.0f});
    const auto second_id = ui::TreeTestAccess::root_id(tree);
    NUI_CHECK(second_id == first_id);
    NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(tree, second_id));
    const auto second_token =
        ui::detail::RasterCacheAccess::capture(tree, second_id);
    NUI_CHECK(!second_token.expired());
    NUI_CHECK(!first_token.same_lifetime(second_token));

    ui::detail::RenderResourceMaterializationContext context;
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1, 1));
    NUI_CHECK(surface);
    auto image = surface->makeImageSnapshot();
    NUI_CHECK(image);
    const ui::detail::RenderResourceMaterializationContext::RasterCacheKey old_key{
        first_id, first_token,
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
        1.0f};
    const ui::detail::RenderResourceMaterializationContext::RasterCacheKey new_key{
        second_id, second_token,
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
        1.0f};
    NUI_CHECK(context.retain_raster(old_key, 4U, image));
    NUI_CHECK(!context.find_raster(new_key));
}

void oversize_raster_is_transient() {
    ui::detail::RenderResourceMaterializationContext context;
    ui::detail::RasterCacheEpoch epoch;
    const auto token = epoch.capture();
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1, 1));
    NUI_CHECK(surface);
    auto image = surface->makeImageSnapshot();
    NUI_CHECK(image);
    const ui::detail::RenderResourceMaterializationContext::RasterCacheKey key{
        41U, token,
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
        1.0f};

    context.begin_frame();
    auto acquisition = context.retain_raster(
        key, ui::detail::kRenderResourceOverBudgetBytes, image);
    NUI_CHECK(acquisition);
    NUI_CHECK(!acquisition.retained);
    NUI_CHECK(!acquisition.hit);
    NUI_CHECK(context.retained_entries() == 0);
    NUI_CHECK(context.retained_accounted_bytes() == 0);
    context.end_frame();
}

void raster_shader_effect_share_budget_and_frame_pinning() {
    ui::detail::RenderResourceMaterializationContext context;
    ui::detail::RasterCacheEpoch epoch;
    const auto token = epoch.capture();
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1, 1));
    NUI_CHECK(surface);
    auto image = surface->makeImageSnapshot();
    NUI_CHECK(image);

    context.begin_frame();
    constexpr std::size_t raster_entries =
        ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries - 2U;
    for (std::size_t index = 0; index < raster_entries; ++index) {
        const ui::detail::RenderResourceMaterializationContext::RasterCacheKey key{
            static_cast<ui::NodeId>(1000U + index),
            token,
            {0.0f, 0.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, 1.0f, 1.0f},
            1.0f};
        auto acquisition = context.retain_raster(key, 4U, image);
        NUI_CHECK(acquisition && acquisition.retained && !acquisition.hit);
    }

    const ui::LinearGradient gradient{
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {
            {0.0f, {0.0f, 0.0f, 0.0f, 1.0f}},
            {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}},
        }};
    auto gradient_resource = context.acquire_linear_gradient(
        gradient,
        [&] { return ui::detail::GradientCacheAccess::materialize(gradient); });
    NUI_CHECK(gradient_resource && gradient_resource.retained);

    const auto effect = ui::Effect::gaussian_blur(1.0f, 1.0f);
    auto effect_resource = context.acquire_effect(
        effect,
        [&] { return ui::detail::EffectCacheAccess::materialize(effect); });
    NUI_CHECK(effect_resource && effect_resource.retained);
    NUI_CHECK(
        context.retained_entries() ==
        ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries);

    const ui::detail::RenderResourceMaterializationContext::RasterCacheKey overflow{
        999999U, token,
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
        1.0f};
    auto transient = context.retain_raster(overflow, 4U, image);
    NUI_CHECK(transient);
    NUI_CHECK(!transient.retained);
    NUI_CHECK(
        context.retained_entries() ==
        ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries);
    context.end_frame();

    transient = {};
    auto after_frame = context.retain_raster(overflow, 4U, image);
    NUI_CHECK(after_frame && after_frame.retained && !after_frame.hit);
    NUI_CHECK(
        context.retained_entries() ==
        ui::detail::RenderResourceMaterializationContext::kMaxRetainedEntries);
}


struct MutableRasterState final {
    ui::NodeId id{};
    ui::Color color{1.0f, 0.0f, 0.0f, 1.0f};
    int paints{};
    std::function<void()> invalidate;
};

class MutableRasterComponent final : public ui::Component {
public:
    explicit MutableRasterComponent(std::shared_ptr<MutableRasterState> state)
        : state_(std::move(state)) {}
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {32.0f, 32.0f};
    }
    void mount(ui::MountContext& context) override {
        state_->id = context.node_id();
        state_->invalidate = context.invalidator();
    }
    void paint(ui::PaintContext& context) const override {
        ++state_->paints;
        context.painter().fill_rounded_rect(context.bounds(), 0.0f, state_->color);
    }
private:
    std::shared_ptr<MutableRasterState> state_;
};

ui::Spec mutable_raster(std::shared_ptr<MutableRasterState> state) {
    return {[state = std::move(state)] {
        return std::make_unique<MutableRasterComponent>(state);
    }, {}};
}

void failed_new_generation_never_composites_old_complete_pixels() {
    for (const auto fault : {RasterHookState::Fault::Surface,
                             RasterHookState::Fault::Submit,
                             RasterHookState::Fault::Snapshot}) {
        auto content = std::make_shared<MutableRasterState>();
        ui::Tree tree{ui::compile(mutable_raster(content))};
        tree.mount();
        tree.layout({32.0f, 32.0f});
        NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(tree, content->id));
        test::MockPlatform platform;
        auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
        NUI_CHECK(surface);
        RasterHookState state;
        const ui::detail::PainterPrivateHooks hooks{
            &state, nullptr, nullptr, nullptr, nullptr, nullptr, &raster_test_hook};
        const auto paint = [&] {
            state.resources.begin_frame();
            try {
                ui::TreeTestAccess::paint_with_resources(
                    tree, *surface->getCanvas(), platform, &hooks);
            } catch (...) {
                state.resources.end_frame();
                throw;
            }
            state.resources.end_frame();
        };
        paint();
        const auto red = read_surface_pixels(*surface, 32, 32);
        NUI_CHECK(SkColorGetR(red.front()) == 255);
        NUI_CHECK(state.resources.retained_entries() == 1);

        content->color = {0.0f, 0.0f, 1.0f, 1.0f};
        content->invalidate();
        surface->getCanvas()->clear(SK_ColorGREEN);
        const auto untouched = read_surface_pixels(*surface, 32, 32);
        const int saves = surface->getCanvas()->getSaveCount();
        state.fault = fault;
        bool threw = false;
        try { paint(); } catch (const std::exception&) { threw = true; }
        NUI_CHECK(threw);
        NUI_CHECK(tree.dirty());
        NUI_CHECK(surface->getCanvas()->getSaveCount() == saves);
        NUI_CHECK(surface->getCanvas()->getLocalToDeviceAs3x3().isIdentity());
        NUI_CHECK(read_surface_pixels(*surface, 32, 32) == untouched);
        NUI_CHECK(state.resources.retained_entries() == 1);
        NUI_CHECK(state.hits == 0);
        const auto token = ui::detail::RasterCacheAccess::capture(tree, content->id);
        NUI_CHECK(!ui::detail::RasterCacheAccess::reusable(tree, content->id, token));

        paint();
        const auto blue = read_surface_pixels(*surface, 32, 32);
        NUI_CHECK(SkColorGetB(blue.front()) == 255);
        NUI_CHECK(SkColorGetR(blue.front()) == 0);
        NUI_CHECK(blue != red);
        const int paints = content->paints;
        tree.invalidate();
        surface->getCanvas()->clear(SK_ColorBLACK);
        paint();
        NUI_CHECK(content->paints == paints);
        NUI_CHECK(state.hits == 1);
        NUI_CHECK(read_surface_pixels(*surface, 32, 32) == blue);
    }
}

void invalidation_after_snapshot_prevents_epoch_publication() {
    auto content = std::make_shared<MutableRasterState>();
    ui::Tree tree{ui::compile(mutable_raster(content))};
    tree.mount();
    tree.layout({32.0f, 32.0f});
    NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(tree, content->id));
    const auto captured = ui::detail::RasterCacheAccess::capture(tree, content->id);
    test::MockPlatform platform;
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
    NUI_CHECK(surface);
    RasterHookState state;
    state.after_snapshot = [content] {
        content->color = {0.0f, 0.0f, 1.0f, 1.0f};
        content->invalidate();
    };
    const ui::detail::PainterPrivateHooks hooks{
        &state, nullptr, nullptr, nullptr, nullptr, nullptr, &raster_test_hook};
    const auto paint = [&] {
        state.resources.begin_frame();
        ui::TreeTestAccess::paint_with_resources(
            tree, *surface->getCanvas(), platform, &hooks);
        state.resources.end_frame();
    };
    paint();
    NUI_CHECK(content->paints == 1);
    NUI_CHECK(tree.dirty());
    NUI_CHECK(state.resources.retained_entries() == 0);
    NUI_CHECK(!ui::detail::RasterCacheAccess::reusable(tree, content->id, captured));
    paint();
    NUI_CHECK(content->paints == 2);
    NUI_CHECK(state.resources.retained_entries() == 1);
    const auto blue = read_surface_pixels(*surface, 32, 32);
    NUI_CHECK(SkColorGetB(blue.front()) == 255);
    tree.invalidate();
    paint();
    NUI_CHECK(content->paints == 2);
    NUI_CHECK(state.hits == 1);
}

void nested_boundaries_conservatively_bypass_both_levels() {
    auto content = std::make_shared<MutableRasterState>();
    ui::Tree tree{ui::compile(ui::make_spec(ui::Stack{mutable_raster(content)}))};
    tree.mount();
    tree.layout({32.0f, 32.0f});
    NUI_CHECK(ui::detail::RasterCacheAccess::register_root(tree));
    NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(tree, content->id));
    test::MockPlatform platform;
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
    NUI_CHECK(surface);
    RasterHookState state;
    const ui::detail::PainterPrivateHooks hooks{
        &state, nullptr, nullptr, nullptr, nullptr, nullptr, &raster_test_hook};
    for (int frame = 0; frame < 2; ++frame) {
        tree.invalidate();
        state.resources.begin_frame();
        ui::TreeTestAccess::paint_with_resources(
            tree, *surface->getCanvas(), platform, &hooks);
        state.resources.end_frame();
    }
    NUI_CHECK(content->paints == 2);
    NUI_CHECK(state.creates == 0);
    NUI_CHECK(state.hits == 0);
    NUI_CHECK(state.resources.retained_entries() == 0);
}

void raster_byte_pressure_preserves_active_frames_then_evicts() {
    ui::detail::RenderResourceMaterializationContext context;
    ui::detail::RasterCacheEpoch epoch;
    const auto token = epoch.capture();
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1, 1));
    NUI_CHECK(surface);
    auto image = surface->makeImageSnapshot();
    const ui::detail::RenderResourceMaterializationContext::RasterCacheKey first{
        1U, token, {0, 0, 1, 1}, {0, 0, 1, 1}, 1.0f};
    auto second = first;
    second.node_id = 2U;
    // Synthetic accounted sizes exercise the exact byte boundary without
    // allocating 128 MiB in every portable test process.
    constexpr auto half = ui::detail::kRenderResourceMaxAccountedBytes / 2U;
    context.begin_frame();
    NUI_CHECK(context.retain_raster(first, half, image).retained);
    NUI_CHECK(context.retain_raster(second, half, image).retained);

    const auto decoded = ui::Image::decode(kTinyRgbaPng);
    auto mipmapped = texture(decoded);
    ui::TextureSampling sampling;
    sampling.set_mipmap(ui::TextureMipmap::Linear);
    mipmapped.set_sampling(sampling);
    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform shader child;
        half4 main(float2 p) { return child.eval(p); }
    )");
    NUI_CHECK(compiled.ok());
    ui::ShaderInstance shader{compiled.program};
    NUI_CHECK(shader.set_child("child", ui::Brush{mipmapped}) == ui::ShaderSetResult::Ok);
    const ui::Brush brush{shader};
    const auto& snapshot = *ui::detail::ShaderBrushAccess::snapshot(brush);
    const auto acquire_shader = [&] {
        return context.acquire_runtime_shader(snapshot, [&] {
            return ui::detail::materialize_shader_brush(snapshot);
        });
    };
    auto transient = acquire_shader();
    NUI_CHECK(transient && !transient.retained);
    const auto effect = ui::Effect::gaussian_blur(1, 1);
    auto filter = context.acquire_effect(effect, [&] {
        return ui::detail::EffectCacheAccess::materialize(effect);
    });
    NUI_CHECK(filter && filter.retained);
    NUI_CHECK(context.find_raster(first));
    NUI_CHECK(context.find_raster(second));
    NUI_CHECK(context.retained_accounted_bytes() == half * 2U);
    NUI_CHECK(context.retained_entries() == 3);
    context.end_frame();
    transient = {};
    filter = {};

    context.begin_frame();
    const auto retained = acquire_shader();
    NUI_CHECK(retained && retained.retained && !retained.hit);
    NUI_CHECK(!context.find_raster(first));
    NUI_CHECK(context.find_raster(second));
    NUI_CHECK(context.retained_accounted_bytes() == half + 32U);
    context.end_frame();
    NUI_CHECK(image && image->width() == 1);
}

void raster_storage_arithmetic_is_exact_and_saturates() {
    using ui::detail::raster_retained_storage_bytes;
    NUI_CHECK(raster_retained_storage_bytes(7, 11) == 308U);
    NUI_CHECK(raster_retained_storage_bytes(32768, 1024) ==
              ui::detail::kRenderResourceMaxAccountedBytes);
    NUI_CHECK(raster_retained_storage_bytes(32769, 1024) ==
              ui::detail::kRenderResourceOverBudgetBytes);
    NUI_CHECK(raster_retained_storage_bytes(0, 11) ==
              ui::detail::kRenderResourceOverBudgetBytes);
    NUI_CHECK(raster_retained_storage_bytes((std::numeric_limits<int>::max)(),
                                           (std::numeric_limits<int>::max)()) ==
              ui::detail::kRenderResourceOverBudgetBytes);
}

void unsupported_raster_geometry_bypasses_before_acquisition() {
    ui::detail::RenderResourceMaterializationContext resources;
    ui::detail::RasterCacheEpoch epoch;
    struct Counters { int creates{}; int paints{}; int commits{}; } counters;
    const ui::detail::RasterCacheBackend backend{
        .state = &counters,
        .create_surface = [](void* raw, const SkImageInfo& info) {
            ++static_cast<Counters*>(raw)->creates;
            return SkSurfaces::Raster(info);
        }};
    ui::detail::RasterCachePaintRequest request{
        1U, epoch.capture(), {0.0f, 0.0f, 4.0f, 4.0f},
        {0.0f, 0.0f, 4.0f, 4.0f}, false};
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 8));
    NUI_CHECK(surface);
    const auto paint = [](void* raw, SkCanvas& canvas,
                          const ui::detail::PainterPrivateHooks*) {
        ++static_cast<Counters*>(raw)->paints;
        canvas.clear(SK_ColorRED);
        return true;
    };
    const auto validate = [](void*) noexcept { return true; };
    const auto commit = [](void* raw) noexcept {
        ++static_cast<Counters*>(raw)->commits;
        return true;
    };
    const auto attempt = [&](const ui::detail::RasterCacheBackend& chosen) {
        return ui::detail::paint_raster_cache_boundary(
            resources, chosen, request, *surface->getCanvas(), &counters,
            paint, validate, commit);
    };
    resources.begin_frame();
    surface->getCanvas()->translate(1.0f, 0.0f);
    NUI_CHECK(!attempt(backend));
    surface->getCanvas()->resetMatrix();
    auto limited = backend;
    limited.max_surface_size = 3;
    NUI_CHECK(!attempt(limited));
    limited.device_scale = 0.0f;
    NUI_CHECK(!attempt(limited));
    request.scene_extent.x = (std::numeric_limits<float>::infinity)();
    NUI_CHECK(!attempt(backend));
    request.scene_extent.x = 16777216.0f;
    NUI_CHECK(!attempt(backend));
    request.scene_extent.x = 0.0f;
    request.local_extent.w = -4.0f;
    NUI_CHECK(!attempt(backend));
    NUI_CHECK(counters.creates == 0 && counters.paints == 0 && counters.commits == 0);
    NUI_CHECK(resources.retained_entries() == 0);
    request.local_extent.w = 4.0f;
    NUI_CHECK(attempt(backend));
    NUI_CHECK(counters.creates == 1 && counters.paints == 1 && counters.commits == 1);
    NUI_CHECK(resources.retained_accounted_bytes() == 64U);
    resources.end_frame();
}

void run_subtree_raster_benchmark() {
    // Compare identical retained trees on one renderer/backend. Cold and warm
    // use the same isolated transaction; uncached measures the traversal cost.
    for (const std::size_t children : {32U, 256U}) {
        for (const std::string_view mode : {"uncached", "cold", "warm"}) {
            auto paints = std::make_shared<int>(0);
            ui::Spec spec{[] { return std::make_unique<ui::StackComponent>(); }, {}};
            for (std::size_t child = 0; child < children; ++child) {
                spec.children.push_back(ui::Spec{
                    [paints] { return std::make_unique<PaintCounterComponent>(paints); }, {}});
            }
            ui::Tree tree{ui::compile(std::move(spec))};
            tree.mount();
            tree.layout({32.0f, 32.0f});
            const auto root = ui::TreeTestAccess::root_id(tree);
            NUI_CHECK(ui::detail::RasterCacheAccess::register_boundary(tree, root));
            auto stale = ui::detail::RasterCacheAccess::invalidator(tree, root);
            RasterHookState state;
            test::MockPlatform platform;
            auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
            NUI_CHECK(surface);
            const ui::detail::PainterPrivateHooks hooks{
                &state, nullptr, nullptr, nullptr, nullptr, nullptr, &raster_test_hook};
            const auto operation = [&] {
                if (mode == "cold") stale();
                tree.invalidate();
                state.resources.begin_frame();
                ui::TreeTestAccess::paint_with_resources(
                    tree, *surface->getCanvas(), platform,
                    mode == "uncached" ? nullptr : &hooks);
                state.resources.end_frame();
            };
            operation();
            const auto timing = nativeui::bench::run_fixed_protocol(100U, [] {}, operation);
            if (mode == "warm") {
                NUI_CHECK(*paints == static_cast<int>(children));
                NUI_CHECK(state.creates == 1 && state.hits == 3500);
            }
            std::cout << "subtree_raster mode=" << mode
                      << " nodes=" << children + 1U
                      << " median_ns=" << timing.summary.median_ns_per_op
                      << " p95_ns=" << timing.summary.p95_ns_per_op
                      << " paints=" << *paints << " creates=" << state.creates
                      << " hits=" << state.hits << '\n';
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--benchmark") {
        run_subtree_raster_benchmark();
        return 0;
    }
    tree_painter_gradient_hits_after_warmup();
    equivalent_gradient_descriptions_hit_semantically();
    tree_painter_runtime_shader_hits_after_warmup();
    tree_painter_effect_hits_after_warmup();
    retained_hit_skips_backend_creation();
    retained_hit_reuses_backend_resource_across_frames();
    frame_lease_survives_cache_clear();
    different_semantics_create_independent_entries();
    renderer_owned_texture_storage_is_accounted();
    runtime_shader_shares_the_same_budget_and_hits();
    image_and_runtime_shader_share_one_entry_budget();
    image_and_runtime_shader_share_hard_entry_limit();
    runtime_shader_accounts_nested_image_storage();
    effects_share_the_unified_cache_budget();
    invalid_texture_is_transient_and_never_retained();
    frame_scope_pins_resources_until_completion();
    transient_frame_bookkeeping_is_released_at_frame_boundary();
    clear_releases_context_owned_frame_leases();
    clear_is_instance_local();
    tree_raster_boundary_cold_warm_stale_warm();
    reentrant_invalidation_cannot_publish_captured_generation();
    raster_retention_failure_leaves_boundary_stale_and_retryable();
    subtree_paint_failure_leaves_boundary_retryable();
    subtree_materialization_failure_leaves_boundary_retryable();
    unqualified_internal_transform_bypasses_retention();
    unqualified_effect_bypasses_raster_retention();
    retained_raster_uses_lifetime_identity_and_shared_budget();
    raster_signature_separates_scale_and_local_extent();
    cold_and_warm_raster_pixels_match_reference();
    remount_same_node_id_gets_new_raster_lifetime();
    oversize_raster_is_transient();
    raster_shader_effect_share_budget_and_frame_pinning();
    failed_new_generation_never_composites_old_complete_pixels();
    invalidation_after_snapshot_prevents_epoch_publication();
    nested_boundaries_conservatively_bypass_both_levels();
    raster_byte_pressure_preserves_active_frames_then_evicts();
    raster_storage_arithmetic_is_exact_and_saturates();
    unsupported_raster_geometry_bypasses_before_acquisition();
    return 0;
}
