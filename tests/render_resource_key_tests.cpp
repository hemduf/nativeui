#include "src/detail/image_texture_cache_key.hpp"
#include "test_support.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

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

[[nodiscard]] ui::ImageTexture base_texture(const ui::Image& image) {
    ui::ImageTexture texture{
        image,
        {0.0f, 0.0f, 2.0f, 2.0f},
        {3.0f, 4.0f, 16.0f, 12.0f}};
    texture.set_tile_mode(
        ui::TextureTileMode::Repeat, ui::TextureTileMode::Mirror);
    ui::TextureSampling sampling;
    sampling.set_filter(ui::TextureFilter::Nearest)
        .set_mipmap(ui::TextureMipmap::Linear);
    texture.set_sampling(sampling);
    texture.set_transform(ui::Transform2D{
        1.0f, 0.25f, 2.0f,
        -0.1f, 1.0f, -3.0f});
    return texture;
}

void shared_image_and_copy_hit_identity() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    const auto a = ui::detail::image_texture_cache_key(base_texture(image));
    const auto b = ui::detail::image_texture_cache_key(base_texture(image));
    NUI_CHECK(a && b);
    NUI_CHECK(*a == *b);
    NUI_CHECK(
        ui::detail::ImageTextureCacheKeyHash{}(*a) ==
        ui::detail::ImageTextureCacheKeyHash{}(*b));
}

void independently_decoded_images_do_not_alias() {
    const auto a_image = ui::Image::decode(kTinyRgbaPng);
    const auto b_image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(a_image.valid() && b_image.valid());

    const auto a = ui::detail::image_texture_cache_key(base_texture(a_image));
    const auto b = ui::detail::image_texture_cache_key(base_texture(b_image));
    NUI_CHECK(a && b);
    NUI_CHECK(!(*a == *b));
}

void every_semantic_field_participates() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());
    const auto baseline = ui::detail::image_texture_cache_key(base_texture(image));
    NUI_CHECK(baseline);

    auto source = base_texture(image);
    source = ui::ImageTexture{
        image,
        {0.0f, 0.0f, 1.0f, 2.0f},
        {3.0f, 4.0f, 16.0f, 12.0f}};
    NUI_CHECK(ui::detail::image_texture_cache_key(source) != baseline);

    auto destination = base_texture(image);
    destination = ui::ImageTexture{
        image,
        {0.0f, 0.0f, 2.0f, 2.0f},
        {4.0f, 4.0f, 16.0f, 12.0f}};
    NUI_CHECK(ui::detail::image_texture_cache_key(destination) != baseline);

    auto tile = base_texture(image);
    tile.set_tile_mode(ui::TextureTileMode::Clamp, ui::TextureTileMode::Mirror);
    NUI_CHECK(ui::detail::image_texture_cache_key(tile) != baseline);

    auto sampling = base_texture(image);
    ui::TextureSampling linear;
    linear.set_filter(ui::TextureFilter::Linear)
        .set_mipmap(ui::TextureMipmap::Linear);
    sampling.set_sampling(linear);
    NUI_CHECK(ui::detail::image_texture_cache_key(sampling) != baseline);

    auto transform = base_texture(image);
    transform.set_transform(ui::Transform2D::translation(8.0f, -3.0f));
    NUI_CHECK(ui::detail::image_texture_cache_key(transform) != baseline);

    auto interpretation = base_texture(image);
    interpretation.set_interpretation(ui::TextureInterpretation::Data);
    NUI_CHECK(ui::detail::image_texture_cache_key(interpretation) != baseline);
}

void signed_zero_is_semantically_equal_and_hash_consistent() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    auto positive = base_texture(image);
    positive.set_transform(ui::Transform2D{
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f});
    auto negative = base_texture(image);
    negative.set_transform(ui::Transform2D{
        1.0f, -0.0f, -0.0f,
        0.0f, 1.0f, -0.0f});

    const auto a = ui::detail::image_texture_cache_key(positive);
    const auto b = ui::detail::image_texture_cache_key(negative);
    NUI_CHECK(a && b);
    NUI_CHECK(*a == *b);
    NUI_CHECK(
        ui::detail::ImageTextureCacheKeyHash{}(*a) ==
        ui::detail::ImageTextureCacheKeyHash{}(*b));
}

void invalid_transform_is_never_a_retained_key() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    const float payload_nan =
        std::bit_cast<float>(std::uint32_t{0x7fc12345U});
    auto texture = base_texture(image);
    texture.set_transform(ui::Transform2D{
        1.0f, payload_nan, 0.0f,
        0.0f, 1.0f, 0.0f});
    NUI_CHECK(!texture.valid());
    NUI_CHECK(!ui::detail::image_texture_cache_key(texture));

    texture = base_texture(image);
    texture.set_transform(ui::Transform2D{
        1.0f, 0.0f, std::numeric_limits<float>::infinity(),
        0.0f, 1.0f, 0.0f});
    NUI_CHECK(!texture.valid());
    NUI_CHECK(!ui::detail::image_texture_cache_key(texture));
}

} // namespace

int main() {
    shared_image_and_copy_hit_identity();
    independently_decoded_images_do_not_alias();
    every_semantic_field_participates();
    signed_zero_is_semantically_equal_and_hash_consistent();
    invalid_transform_is_never_a_retained_key();
    return 0;
}
