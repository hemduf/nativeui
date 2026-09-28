#include "src/detail/render_resource_materialization.hpp"
#include "test_support.hpp"

#include "include/core/SkColor.h"
#include "include/core/SkShader.h"

#include <array>
#include <cstddef>

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
    NUI_CHECK(creates == 1);
    NUI_CHECK(context.retained_entries() == 1);

    auto second = context.acquire_image_texture(texture(image), factory);
    NUI_CHECK(second);
    NUI_CHECK(creates == 1);
    NUI_CHECK(first == second);
    NUI_CHECK(context.retained_entries() == 1);
    NUI_CHECK(context.retained_accounted_bytes() == 0);
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

    NUI_CHECK(context.acquire_image_texture(color, factory));
    NUI_CHECK(context.acquire_image_texture(data, factory));
    NUI_CHECK(creates == 2);
    NUI_CHECK(context.retained_entries() == 2);
}

void invalid_texture_is_transient_and_never_retained() {
    ui::detail::RenderResourceMaterializationContext context;
    int creates = 0;

    ui::ImageTexture invalid;
    auto shader = context.acquire_image_texture(invalid, [&] {
        ++creates;
        return SkShaders::Color(SK_ColorBLUE);
    });
    NUI_CHECK(shader);
    NUI_CHECK(creates == 1);
    NUI_CHECK(context.retained_entries() == 0);
}

void clear_is_instance_local() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::detail::RenderResourceMaterializationContext a;
    ui::detail::RenderResourceMaterializationContext b;

    NUI_CHECK(a.acquire_image_texture(
        texture(image), [] { return SkShaders::Color(SK_ColorRED); }));
    NUI_CHECK(b.acquire_image_texture(
        texture(image), [] { return SkShaders::Color(SK_ColorRED); }));
    NUI_CHECK(a.retained_entries() == 1);
    NUI_CHECK(b.retained_entries() == 1);

    a.clear();
    NUI_CHECK(a.retained_entries() == 0);
    NUI_CHECK(b.retained_entries() == 1);
}

} // namespace

int main() {
    retained_hit_skips_backend_creation();
    different_semantics_create_independent_entries();
    invalid_texture_is_transient_and_never_retained();
    clear_is_instance_local();
    return 0;
}
