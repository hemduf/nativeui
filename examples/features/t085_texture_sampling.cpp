#include "example_support.hpp"

#include <nativeui/headless.hpp>

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace {

constexpr std::array<std::byte, 75> kTinyRgbaPng{
    std::byte{137}, std::byte{80}, std::byte{78}, std::byte{71}, std::byte{13}, std::byte{10},
    std::byte{26}, std::byte{10}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{13},
    std::byte{73}, std::byte{72}, std::byte{68}, std::byte{82}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{2},
    std::byte{8}, std::byte{6}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{114},
    std::byte{182}, std::byte{13}, std::byte{36}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{18}, std::byte{73}, std::byte{68}, std::byte{65}, std::byte{84}, std::byte{120},
    std::byte{218}, std::byte{99}, std::byte{248}, std::byte{207}, std::byte{192}, std::byte{240},
    std::byte{31}, std::byte{12}, std::byte{129}, std::byte{52}, std::byte{24}, std::byte{0},
    std::byte{0}, std::byte{73}, std::byte{200}, std::byte{9}, std::byte{247}, std::byte{3},
    std::byte{217}, std::byte{100}, std::byte{241}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{73}, std::byte{69}, std::byte{78}, std::byte{68}, std::byte{174},
    std::byte{66}, std::byte{96}, std::byte{130},
};

static_assert(noexcept(std::declval<ui::TextureSampling&>().set_filter(
    ui::TextureFilter::Nearest)));
static_assert(noexcept(std::declval<ui::TextureSampling&>().set_mipmap(
    ui::TextureMipmap::Linear)));
static_assert(noexcept(std::declval<const ui::TextureSampling&>().filter()));
static_assert(noexcept(std::declval<const ui::TextureSampling&>().mipmap()));
static_assert(noexcept(std::declval<ui::ImageTexture&>().set_sampling(
    ui::TextureSampling{})));
static_assert(noexcept(std::declval<const ui::ImageTexture&>().sampling()));
static_assert(std::is_trivially_copyable_v<ui::TextureSampling>);

int self_test() {
    ui::TextureSampling sampling;
    if (sampling.filter() != ui::TextureFilter::Linear) {
        return example::fail("T085 default filter must be Linear");
    }
    if (sampling.mipmap() != ui::TextureMipmap::None) {
        return example::fail("T085 default mipmap policy must be None");
    }

    auto* returned = &sampling.set_filter(ui::TextureFilter::Nearest)
                         .set_mipmap(ui::TextureMipmap::Linear);
    if (returned != &sampling ||
        sampling.filter() != ui::TextureFilter::Nearest ||
        sampling.mipmap() != ui::TextureMipmap::Linear) {
        return example::fail("T085 TextureSampling value mutation failed");
    }

    const auto image = ui::Image::decode(kTinyRgbaPng);
    if (!image.valid()) return example::fail("T085 sample image did not decode");

    ui::ImageTexture texture{image, {0.0f, 0.0f, 2.0f, 2.0f},
                             {8.0f, 8.0f, 16.0f, 16.0f}};
    if (texture.sampling().filter() != ui::TextureFilter::Linear ||
        texture.sampling().mipmap() != ui::TextureMipmap::None) {
        return example::fail("T085 ImageTexture defaults changed T083 behavior");
    }

    texture.set_sampling(sampling);
    if (texture.sampling().filter() != ui::TextureFilter::Nearest ||
        texture.sampling().mipmap() != ui::TextureMipmap::Linear) {
        return example::fail("T085 ImageTexture sampling state did not publish");
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    return self_test();
}
