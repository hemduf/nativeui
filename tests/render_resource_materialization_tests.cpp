#include "src/detail/painter_private_hooks.hpp"
#include "src/detail/render_resource_materialization.hpp"
#include "test_support.hpp"

#include "include/core/SkColor.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "include/core/SkImageInfo.h"

#include <array>
#include <cstddef>
#include <memory>

namespace ui {

struct TreeTestAccess {
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

} // namespace

int main() {
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
    clear_releases_context_owned_frame_leases();
    clear_is_instance_local();
    return 0;
}
