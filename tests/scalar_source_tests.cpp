#include <nativeui/scalar_source.hpp>

#include "src/detail/scalar_source_access.hpp"
#include "test_support.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace {

thread_local bool track_allocations = false;
thread_local std::size_t allocation_count = 0;

void note_allocation() noexcept {
    if (track_allocations) ++allocation_count;
}

void* allocate_memory(std::size_t size) {
    note_allocation();
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}

void* allocate_aligned_memory(std::size_t size, std::size_t alignment) {
    note_allocation();
#if defined(_MSC_VER)
    if (void* memory = _aligned_malloc(size == 0 ? 1 : size, alignment)) {
        return memory;
    }
#else
    void* memory = nullptr;
    if (posix_memalign(&memory, alignment, size == 0 ? 1 : size) == 0) {
        return memory;
    }
#endif
    throw std::bad_alloc{};
}

void free_aligned_memory(void* memory) noexcept {
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

} // namespace

void* operator new(std::size_t size) { return allocate_memory(size); }
void* operator new[](std::size_t size) { return allocate_memory(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try { return allocate_memory(size); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try { return allocate_memory(size); } catch (...) { return nullptr; }
}
void operator delete(void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocate_aligned_memory(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocate_aligned_memory(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* memory, std::align_val_t) noexcept { free_aligned_memory(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { free_aligned_memory(memory); }
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept { free_aligned_memory(memory); }
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept { free_aligned_memory(memory); }

namespace {

static_assert(std::is_nothrow_default_constructible_v<ui::ScalarSource>);
static_assert(std::is_nothrow_constructible_v<ui::ScalarSource, float>);
static_assert(noexcept(ui::ScalarSource::constant(0.0f)));
static_assert(std::is_nothrow_move_constructible_v<ui::ScalarSource>);
static_assert(std::is_nothrow_move_assignable_v<ui::ScalarSource>);
static_assert(std::is_nothrow_destructible_v<ui::ScalarSource>);

void constant_contract() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float values[] = {
        -8.0f,
        -0.0f,
        0.0f,
        0.5f,
        1.0f,
        8.0f,
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        nan,
    };

    for (float value : values) {
        ui::ScalarSource direct{value};
        ui::ScalarSource factory = ui::ScalarSource::constant(value);
        NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(direct));
        NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(factory));

        const float a = ui::detail::ScalarSourceAccess::constant_value(direct);
        const float b = ui::detail::ScalarSourceAccess::constant_value(factory);
        if (std::isnan(value)) {
            NUI_CHECK(std::isnan(a));
            NUI_CHECK(std::isnan(b));
        } else {
            NUI_CHECK(a == value);
            NUI_CHECK(b == value);
            if (value == 0.0f) {
                NUI_CHECK(std::signbit(a) == std::signbit(value));
                NUI_CHECK(std::signbit(b) == std::signbit(value));
            }
        }
    }

    ui::ScalarSource zero;
    NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(zero));
    NUI_CHECK(ui::detail::ScalarSourceAccess::constant_value(zero) == 0.0f);
}

void allocation_free_value_contract() {
    auto brush_backed = ui::ScalarSource::from_brush(
        ui::Brush{ui::Color{0.2f, 0.3f, 0.4f, 1.0f}},
        ui::ScalarChannel::Blue);

    allocation_count = 0;
    track_allocations = true;
    {
        ui::ScalarSource zero;
        ui::ScalarSource direct{8.0f};
        auto factory = ui::ScalarSource::constant(-2.0f);
        ui::ScalarSource moved{std::move(brush_backed)};
        ui::ScalarSource assigned;
        assigned = std::move(moved);
        (void)zero;
        (void)direct;
        (void)factory;
        (void)assigned;
    }
    track_allocations = false;

    NUI_CHECK(allocation_count == 0);
}

void brush_channel_contract() {
    const ui::Brush brush{ui::Color{0.25f, 0.5f, 2.0f, 0.75f}};

    for (auto channel : {
             ui::ScalarChannel::Red,
             ui::ScalarChannel::Green,
             ui::ScalarChannel::Blue,
             ui::ScalarChannel::Alpha}) {
        auto source = ui::ScalarSource::from_brush(brush, channel);
        NUI_CHECK(!ui::detail::ScalarSourceAccess::is_constant(source));
        NUI_CHECK(ui::detail::ScalarSourceAccess::brush(source) != nullptr);
        NUI_CHECK(ui::detail::ScalarSourceAccess::channel(source) == channel);
    }

    auto invalid = ui::ScalarSource::from_brush(
        brush, static_cast<ui::ScalarChannel>(255));
    NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(invalid));
    NUI_CHECK(ui::detail::ScalarSourceAccess::constant_value(invalid) == 0.0f);
}

void move_contract() {
    auto source = ui::ScalarSource::from_brush(
        ui::Brush{ui::Color{0.2f, 0.3f, 0.4f, 1.0f}},
        ui::ScalarChannel::Blue);
    ui::ScalarSource moved{std::move(source)};

    NUI_CHECK(!ui::detail::ScalarSourceAccess::is_constant(moved));
    NUI_CHECK(ui::detail::ScalarSourceAccess::channel(moved) ==
              ui::ScalarChannel::Blue);
    NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(source));
    NUI_CHECK(ui::detail::ScalarSourceAccess::constant_value(source) == 0.0f);

    auto* self = &moved;
    moved = std::move(*self);
    NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(moved));
    NUI_CHECK(ui::detail::ScalarSourceAccess::constant_value(moved) == 0.0f);
}

void copy_assignment_contract() {
    ui::ScalarSource original = ui::ScalarSource::from_brush(
        ui::Brush{ui::Color{0.1f, 0.2f, 0.3f, 1.0f}},
        ui::ScalarChannel::Green);
    ui::ScalarSource copy{original};
    NUI_CHECK(ui::detail::ScalarSourceAccess::channel(copy) ==
              ui::ScalarChannel::Green);

    ui::ScalarSource replacement{8.0f};
    replacement = original;
    NUI_CHECK(!ui::detail::ScalarSourceAccess::is_constant(replacement));
    NUI_CHECK(ui::detail::ScalarSourceAccess::channel(replacement) ==
              ui::ScalarChannel::Green);

    original = original;
    NUI_CHECK(ui::detail::ScalarSourceAccess::channel(original) ==
              ui::ScalarChannel::Green);
}

void noise_factory_contract() {
    ui::NoiseSource inert;
    auto zero = ui::ScalarSource::from_noise(inert);
    NUI_CHECK(ui::detail::ScalarSourceAccess::brush(zero) != nullptr);
    NUI_CHECK(ui::detail::ScalarSourceAccess::channel(zero) ==
              ui::ScalarChannel::Red);
}

} // namespace

int main() {
    constant_contract();
    allocation_free_value_contract();
    brush_channel_contract();
    move_contract();
    copy_assignment_contract();
    noise_factory_contract();
    return 0;
}
