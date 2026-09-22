#include <nativeui/paint.hpp>

#include "src/detail/image_texture_test_seams.hpp"

#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace allocation_probe {

bool fail_allocations = false;
std::size_t allocation_count = 0;

[[nodiscard]] void* allocate_unaligned(std::size_t size) noexcept {
    return std::malloc(size == 0 ? 1U : size);
}

[[nodiscard]] void* allocate_aligned(std::size_t size,
                                     std::size_t alignment) noexcept {
    if (alignment == 0 || (alignment & (alignment - 1U)) != 0U) return nullptr;

    constexpr std::size_t pointer_bytes = sizeof(void*);
    const auto max_size = (std::numeric_limits<std::size_t>::max)();
    if (alignment - 1U > max_size - pointer_bytes) return nullptr;

    const std::size_t overhead = pointer_bytes + alignment - 1U;
    const std::size_t payload_size = size == 0 ? 1U : size;
    if (payload_size > max_size - overhead) return nullptr;

    void* raw = std::malloc(payload_size + overhead);
    if (!raw) return nullptr;

    const auto raw_address = reinterpret_cast<std::uintptr_t>(raw);
    const auto candidate = raw_address + pointer_bytes;
    const auto aligned_address =
        (candidate + alignment - 1U) &
        ~static_cast<std::uintptr_t>(alignment - 1U);
    auto* aligned = reinterpret_cast<std::byte*>(aligned_address);
    std::memcpy(aligned - pointer_bytes, &raw, pointer_bytes);
    return aligned;
}

void deallocate_aligned(void* pointer) noexcept {
    if (!pointer) return;
    void* raw = nullptr;
    auto* aligned = static_cast<std::byte*>(pointer);
    std::memcpy(&raw, aligned - sizeof(void*), sizeof(raw));
    std::free(raw);
}

struct ScopedFailure {
    ScopedFailure() noexcept { fail_allocations = true; }
    ScopedFailure(const ScopedFailure&) = delete;
    ScopedFailure& operator=(const ScopedFailure&) = delete;
    ~ScopedFailure() noexcept { fail_allocations = false; }
};

} // namespace allocation_probe

void* operator new(std::size_t size) {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) throw std::bad_alloc{};
    if (void* pointer = allocation_probe::allocate_unaligned(size)) return pointer;
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) throw std::bad_alloc{};
    if (void* pointer = allocation_probe::allocate_unaligned(size)) return pointer;
    throw std::bad_alloc{};
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) return nullptr;
    return allocation_probe::allocate_unaligned(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) return nullptr;
    return allocation_probe::allocate_unaligned(size);
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }

void* operator new(std::size_t size, std::align_val_t alignment) {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) throw std::bad_alloc{};
    if (void* pointer = allocation_probe::allocate_aligned(
            size, static_cast<std::size_t>(alignment))) {
        return pointer;
    }
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) throw std::bad_alloc{};
    if (void* pointer = allocation_probe::allocate_aligned(
            size, static_cast<std::size_t>(alignment))) {
        return pointer;
    }
    throw std::bad_alloc{};
}

void* operator new(std::size_t size,
                   std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) return nullptr;
    return allocation_probe::allocate_aligned(
        size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size,
                     std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) return nullptr;
    return allocation_probe::allocate_aligned(
        size, static_cast<std::size_t>(alignment));
}

void operator delete(void* pointer, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(pointer);
}
void operator delete[](void* pointer, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(pointer);
}
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(pointer);
}
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(pointer);
}
void operator delete(void* pointer, std::align_val_t, const std::nothrow_t&) noexcept {
    allocation_probe::deallocate_aligned(pointer);
}
void operator delete[](void* pointer, std::align_val_t, const std::nothrow_t&) noexcept {
    allocation_probe::deallocate_aligned(pointer);
}

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

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

[[nodiscard]] SkColor pixel(const sk_sp<SkSurface>& surface, int x, int y) {
    SkPixmap pixmap;
    check(surface->peekPixels(&pixmap), "raster surface did not expose pixels");
    return pixmap.getColor(x, y);
}

[[nodiscard]] bool black(SkColor color) noexcept {
    return SkColorGetR(color) < 8 &&
           SkColorGetG(color) < 8 &&
           SkColorGetB(color) < 8;
}

[[nodiscard]] bool red(SkColor color) noexcept {
    return SkColorGetR(color) > 180 &&
           SkColorGetG(color) < 60 &&
           SkColorGetB(color) < 60;
}

void value_operations_allocate_nothing() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    check(image.valid(), "allocation test image did not decode");

    const auto decode_before =
        ui::detail::image_decode_call_count_for_test();
    const auto materialization_before =
        ui::detail::image_texture_materialization_call_count_for_test();
    const auto before = allocation_probe::allocation_count;
    bool state_ok = true;
    {
        allocation_probe::ScopedFailure fail;

        ui::ImageTexture texture{image, {0.0f, 0.0f, 8.0f, 8.0f}};
        for (int i = 0; i < 1024; ++i) {
            const auto x = (i & 1) == 0
                ? ui::TextureTileMode::Repeat
                : ui::TextureTileMode::Mirror;
            const auto y = (i & 2) == 0
                ? ui::TextureTileMode::Decal
                : ui::TextureTileMode::Clamp;
            auto* returned = &texture.set_tile_mode(x, y);
            state_ok = state_ok &&
                returned == &texture &&
                texture.tile_mode_x() == x &&
                texture.tile_mode_y() == y;

            ui::TextureSampling sample;
            const auto filter = (i & 4) == 0
                ? ui::TextureFilter::Nearest
                : ui::TextureFilter::Linear;
            const auto mipmap = (i % 3) == 0
                ? ui::TextureMipmap::None
                : ((i % 3) == 1
                    ? ui::TextureMipmap::Nearest
                    : ui::TextureMipmap::Linear);
            sample.set_filter(filter).set_mipmap(mipmap);
            returned = &texture.set_sampling(sample);
            const auto roundtrip = texture.sampling();

            const ui::Transform2D transform{
                1.0f, (i & 8) == 0 ? 0.125f : -0.125f,
                static_cast<float>(i & 3),
                0.0f, 1.0f, static_cast<float>((i >> 2) & 3)};
            returned = &texture.set_transform(transform);
            const auto transform_roundtrip = texture.transform();

            state_ok = state_ok &&
                returned == &texture &&
                roundtrip.filter() == filter &&
                roundtrip.mipmap() == mipmap &&
                transform_roundtrip.m00 == transform.m00 &&
                transform_roundtrip.m01 == transform.m01 &&
                transform_roundtrip.m02 == transform.m02 &&
                transform_roundtrip.m10 == transform.m10 &&
                transform_roundtrip.m11 == transform.m11 &&
                transform_roundtrip.m12 == transform.m12 &&
                texture.valid();
        }

        ui::ImageTexture copied_texture{texture};
        ui::ImageTexture copy_assigned_texture;
        copy_assigned_texture = texture;
        ui::ImageTexture moved_texture{std::move(copied_texture)};
        ui::ImageTexture move_assigned_texture;
        move_assigned_texture = std::move(copy_assigned_texture);

        ui::Brush brush{texture};
        ui::Brush copied_brush{brush};
        ui::Brush copy_assigned_brush{ui::Color{}};
        copy_assigned_brush = brush;
        ui::Brush moved_brush{std::move(copied_brush)};
        ui::Brush move_assigned_brush{ui::Color{}};
        move_assigned_brush = std::move(copy_assigned_brush);
        ui::Brush invalid{ui::ImageTexture{}};

        state_ok = state_ok &&
            moved_texture.tile_mode_x() == texture.tile_mode_x() &&
            moved_texture.tile_mode_y() == texture.tile_mode_y() &&
            moved_texture.sampling().filter() == texture.sampling().filter() &&
            moved_texture.sampling().mipmap() == texture.sampling().mipmap() &&
            moved_texture.transform().m01 == texture.transform().m01 &&
            moved_texture.transform().m02 == texture.transform().m02 &&
            move_assigned_texture.tile_mode_x() == texture.tile_mode_x() &&
            move_assigned_texture.tile_mode_y() == texture.tile_mode_y() &&
            move_assigned_texture.sampling().filter() == texture.sampling().filter() &&
            move_assigned_texture.sampling().mipmap() == texture.sampling().mipmap() &&
            move_assigned_texture.transform().m01 == texture.transform().m01 &&
            move_assigned_texture.transform().m12 == texture.transform().m12;

        ui::ImageTexture invalid_transform_texture = texture;
        invalid_transform_texture.set_transform(ui::Transform2D{
            1.0f, 0.0f, std::numeric_limits<float>::infinity(),
            0.0f, 1.0f, 0.0f});
        ui::Brush invalid_transform_brush{invalid_transform_texture};
        state_ok = state_ok &&
            !invalid_transform_texture.valid() &&
            invalid_transform_texture.image() == image &&
            invalid_transform_texture.transform().m02 ==
                std::numeric_limits<float>::infinity();

        (void)moved_brush;
        (void)move_assigned_brush;
        (void)invalid;
        (void)invalid_transform_brush;
    }
    check(state_ok, "ImageTexture value state was not stable");
    check(allocation_probe::allocation_count == before,
          "ImageTexture/Brush value operation allocated");
    check(ui::detail::image_decode_call_count_for_test() == decode_before,
          "ImageTexture value mutation decoded image data");
    check(ui::detail::image_texture_materialization_call_count_for_test() ==
              materialization_before,
          "ImageTexture value mutation materialized backend resources");
}

void nonrepresentable_mapping_fails_visibly_and_recovers() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    check(image.valid(), "mapping failure image did not decode");

    const float min_normal = std::numeric_limits<float>::min();
    const float huge_origin = std::numeric_limits<float>::max() * 0.5f;
    const std::array<ui::ImageTexture, 2> mappings{
        // Finite logical inputs whose scale overflows float.
        ui::ImageTexture{
            image,
            {0.0f, 0.0f, min_normal, 1.0f},
            {0.0f, 0.0f, 8.0f, 8.0f}},
        // Finite positive destination dimensions whose x + width rounds back to
        // x at this magnitude, producing a finite but non-invertible backend
        // matrix. The logical value remains valid and must fail visibly at paint.
        ui::ImageTexture{
            image,
            {0.0f, 0.0f, 1.0f, 1.0f},
            {huge_origin, 0.0f, 0.25f, 8.0f}},
    };
    for (const auto& texture : mappings) {
        check(texture.valid(),
              "finite positive extreme mapping was incorrectly canonicalized invalid");
    }

    const auto info = SkImageInfo::MakeN32Premul(8, 8);
    auto surface = SkSurfaces::Raster(info);
    check(static_cast<bool>(surface), "mapping failure raster surface creation failed");
    auto* canvas = surface->getCanvas();
    check(canvas != nullptr, "mapping failure raster canvas is null");

    for (const auto& texture : mappings) {
        canvas->clear(SK_ColorBLACK);
        bool failed = false;
        try {
            ui::Painter painter{*canvas};
            painter.fill_rounded_rect(
                {0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, ui::Brush{texture});
        } catch (const std::runtime_error&) {
            failed = true;
        }
        check(failed,
              "non-representable ImageTexture mapping did not fail visibly");
        check(black(pixel(surface, 1, 1)),
              "non-representable mapping submitted a partial primitive");
    }

    canvas->clear(SK_ColorBLACK);
    const ui::Brush normal{
        ui::ImageTexture{image, {0.0f, 0.0f, 8.0f, 8.0f}}};
    {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, normal);
    }
    check(red(pixel(surface, 1, 1)),
          "normal image paint did not recover after mapping failure");
}

void decode_materialization_and_failure_recovery() {
    const auto decode_before = ui::detail::image_decode_call_count_for_test();
    const auto image = ui::Image::decode(kTinyRgbaPng);
    check(image.valid(), "fault test image did not decode");
    check(ui::detail::image_decode_call_count_for_test() == decode_before + 1U,
          "decode instrumentation did not observe Image::decode");
    check(!ui::detail::image_backing_is_lazy_for_test(image),
          "Image::decode published a lazy backing that can decode during paint");

    // Keep the legacy 8x8 recovery geometry while enabling mip materialization.
    // This isolates the new no-redecode assertion from the existing pixel oracle.
    ui::ImageTexture mip_texture{image, {0.0f, 0.0f, 8.0f, 8.0f}};
    ui::TextureSampling mip_sampling;
    mip_sampling.set_filter(ui::TextureFilter::Linear)
        .set_mipmap(ui::TextureMipmap::Linear);
    mip_texture.set_sampling(mip_sampling);
    mip_texture.set_transform(
        ui::Transform2D::translation(1.0f, 0.0f) *
        ui::Transform2D::scaling(0.75f, 1.0f));
    check(mip_texture.valid(), "valid transformed mip texture became invalid");
    const ui::Brush brush{mip_texture};

    const auto info = SkImageInfo::MakeN32Premul(8, 8);
    auto surface = SkSurfaces::Raster(info);
    check(static_cast<bool>(surface), "raster surface creation failed");
    auto* canvas = surface->getCanvas();
    check(canvas != nullptr, "raster canvas is null");

    const auto materialization_before =
        ui::detail::image_texture_materialization_call_count_for_test();
    {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect({0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, brush);
        painter.fill_rounded_rect({0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, brush);
    }
    check(ui::detail::image_texture_materialization_call_count_for_test() ==
              materialization_before + 2U,
          "pre-T097 draws did not materialize independently");

    const ui::Brush invalid{ui::ImageTexture{}};

    ui::ImageTexture invalid_transform_texture{
        image, {0.0f, 0.0f, 8.0f, 8.0f}};
    invalid_transform_texture.set_transform(ui::Transform2D{
        1.0f, 0.0f, std::numeric_limits<float>::infinity(),
        0.0f, 1.0f, 0.0f});
    check(!invalid_transform_texture.valid(),
          "non-finite transformed texture remained valid");
    const ui::Brush invalid_transform{invalid_transform_texture};

    const auto invalid_before =
        ui::detail::image_texture_materialization_call_count_for_test();
    {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect({0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, invalid);
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, invalid_transform);
    }
    check(ui::detail::image_texture_materialization_call_count_for_test() ==
              invalid_before,
          "invalid ImageTexture Brush attempted backend materialization");
    check(ui::detail::image_decode_call_count_for_test() == decode_before + 1U,
          "mipmapped painting unexpectedly called Image::decode");

    // Recover the same logical ImageTexture from semantic transform invalidity
    // without touching the immutable decoded Image backing.
    invalid_transform_texture.set_transform(ui::Transform2D::identity());
    check(invalid_transform_texture.valid(),
          "valid transform did not restore ImageTexture validity");
    const ui::Brush recovered_transform{invalid_transform_texture};
    const auto recovery_materialization_before =
        ui::detail::image_texture_materialization_call_count_for_test();
    canvas->clear(SK_ColorBLACK);
    {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, recovered_transform);
    }
    check(red(pixel(surface, 1, 1)),
          "valid transform did not restore texture rendering");
    check(ui::detail::image_texture_materialization_call_count_for_test() ==
              recovery_materialization_before + 1U,
          "recovered transformed texture did not use normal materialization");
    check(ui::detail::image_decode_call_count_for_test() == decode_before + 1U,
          "invalid-to-valid transform recovery re-decoded Image data");

    canvas->clear(SK_ColorBLACK);
    ui::detail::set_image_texture_materialization_failure_for_test(
        ui::detail::ImageTextureMaterializationFailurePoint::BeforeShader);
    bool allocation_failed = false;
    try {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect({0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, brush);
    } catch (const std::bad_alloc&) {
        allocation_failed = true;
    }
    ui::detail::set_image_texture_materialization_failure_for_test(
        ui::detail::ImageTextureMaterializationFailurePoint::None);
    check(allocation_failed, "injected image materialization allocation failure was hidden");
    check(black(pixel(surface, 1, 1)),
          "failed materialization submitted a partial primitive");

    {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect({0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, brush);
    }
    check(red(pixel(surface, 1, 1)),
          "normal image paint did not recover after allocation failure");

    canvas->clear(SK_ColorBLACK);
    ui::detail::set_image_texture_materialization_failure_for_test(
        ui::detail::ImageTextureMaterializationFailurePoint::ForceNullShader);
    bool null_failed = false;
    try {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect({0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, brush);
    } catch (const std::runtime_error&) {
        null_failed = true;
    }
    ui::detail::set_image_texture_materialization_failure_for_test(
        ui::detail::ImageTextureMaterializationFailurePoint::None);
    check(null_failed, "null backend image shader silently fell back");
    check(black(pixel(surface, 1, 1)),
          "null image shader failure submitted a partial primitive");

    {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect({0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, brush);
    }
    check(red(pixel(surface, 1, 1)),
          "normal image paint did not recover after null materialization");
    check(ui::detail::image_decode_call_count_for_test() == decode_before + 1U,
          "mipmapped failure recovery unexpectedly re-decoded image bytes");
}

} // namespace

int main() {
    try {
        value_operations_allocate_nothing();
        nonrepresentable_mapping_fails_visibly_and_recovers();
        decode_materialization_and_failure_recovery();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL T083 ImageTexture fault/value tests: "
                  << error.what() << '\n';
        return 1;
    }
}
