#include <nativeui/scalar_source.hpp>

#include "src/detail/scalar_source_access.hpp"
#include "test_support.hpp"

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <utility>

namespace allocation_probe {

bool fail_allocations = false;
std::size_t allocation_count = 0;

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
    if (void* pointer = std::malloc(size == 0 ? 1U : size)) return pointer;
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) throw std::bad_alloc{};
    if (void* pointer = std::malloc(size == 0 ? 1U : size)) return pointer;
    throw std::bad_alloc{};
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) return nullptr;
    return std::malloc(size == 0 ? 1U : size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) return nullptr;
    return std::malloc(size == 0 ? 1U : size);
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept {
    std::free(pointer);
}
void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
    std::free(pointer);
}

namespace {

void constant_construction_allocates_nothing() {
    const auto before = allocation_probe::allocation_count;
    {
        allocation_probe::ScopedFailure fail;
        for (int iteration = 0; iteration < 1024; ++iteration) {
            ui::ScalarSource zero;
            ui::ScalarSource direct{8.0f};
            auto constant = ui::ScalarSource::constant(-2.0f);
            NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(zero));
            NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(direct));
            NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(constant));
            NUI_CHECK(ui::detail::ScalarSourceAccess::constant_value(zero) == 0.0f);
            NUI_CHECK(ui::detail::ScalarSourceAccess::constant_value(direct) == 8.0f);
            NUI_CHECK(ui::detail::ScalarSourceAccess::constant_value(constant) == -2.0f);
        }
    }
    NUI_CHECK(allocation_probe::allocation_count == before);
}

void copy_assignment_has_strong_guarantee() {
    ui::LinearGradient gradient{
        {0.0f, 0.0f},
        {16.0f, 0.0f},
        {
            ui::GradientStop{0.0f, {0.1f, 0.2f, 0.3f, 1.0f}},
            ui::GradientStop{0.33f, {0.3f, 0.4f, 0.5f, 1.0f}},
            ui::GradientStop{0.66f, {0.5f, 0.6f, 0.7f, 1.0f}},
            ui::GradientStop{1.0f, {0.7f, 0.8f, 0.9f, 1.0f}},
        },
    };
    const auto source = ui::ScalarSource::from_brush(
        ui::Brush{std::move(gradient)}, ui::ScalarChannel::Green);
    ui::ScalarSource destination{8.0f};

    bool threw = false;
    {
        allocation_probe::ScopedFailure fail;
        try {
            destination = source;
        } catch (const std::bad_alloc&) {
            threw = true;
        }
    }

    NUI_CHECK(threw);
    NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(destination));
    NUI_CHECK(ui::detail::ScalarSourceAccess::constant_value(destination) == 8.0f);

    destination = source;
    NUI_CHECK(!ui::detail::ScalarSourceAccess::is_constant(destination));
    NUI_CHECK(ui::detail::ScalarSourceAccess::channel(destination) ==
              ui::ScalarChannel::Green);
}

void move_and_noise_copy_failure_contracts() {
    auto brush_source = ui::ScalarSource::from_brush(
        ui::Brush{ui::Color{0.2f, 0.4f, 0.8f, 1.0f}},
        ui::ScalarChannel::Blue);

    const auto created = ui::NoiseSource::create(
        ui::NoiseType::Value,
        {.feature_size = 24.0f, .seed = 0x13579bdfu});
    NUI_CHECK(created.ok());
    auto noise_source = ui::ScalarSource::from_noise(created.noise);

    const auto before_move = allocation_probe::allocation_count;
    {
        allocation_probe::ScopedFailure fail;

        ui::ScalarSource moved_brush{std::move(brush_source)};
        NUI_CHECK(!ui::detail::ScalarSourceAccess::is_constant(moved_brush));
        NUI_CHECK(ui::detail::ScalarSourceAccess::channel(moved_brush) ==
                  ui::ScalarChannel::Blue);
        NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(brush_source));

        ui::ScalarSource assigned{4.0f};
        assigned = std::move(moved_brush);
        NUI_CHECK(!ui::detail::ScalarSourceAccess::is_constant(assigned));
        NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(moved_brush));

        ui::ScalarSource moved_noise{std::move(noise_source)};
        NUI_CHECK(!ui::detail::ScalarSourceAccess::is_constant(moved_noise));
        NUI_CHECK(ui::detail::ScalarSourceAccess::channel(moved_noise) ==
                  ui::ScalarChannel::Red);
        NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(noise_source));

        auto* self = &moved_noise;
        moved_noise = std::move(*self);
        NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(moved_noise));
        NUI_CHECK(ui::detail::ScalarSourceAccess::constant_value(moved_noise) ==
                  0.0f);
    }
    NUI_CHECK(allocation_probe::allocation_count == before_move);

    const auto noise_again = ui::NoiseSource::create(
        ui::NoiseType::Perlin,
        {.feature_size = 20.0f, .seed = 0x2468ace0u});
    NUI_CHECK(noise_again.ok());
    const auto source = ui::ScalarSource::from_noise(noise_again.noise);
    ui::ScalarSource destination{8.0f};

    bool threw = false;
    {
        allocation_probe::ScopedFailure fail;
        try {
            destination = source;
        } catch (const std::bad_alloc&) {
            threw = true;
        }
    }

    if (threw) {
        NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(destination));
        NUI_CHECK(ui::detail::ScalarSourceAccess::constant_value(destination) ==
                  8.0f);
    } else {
        NUI_CHECK(!ui::detail::ScalarSourceAccess::is_constant(destination));
        NUI_CHECK(ui::detail::ScalarSourceAccess::channel(destination) ==
                  ui::ScalarChannel::Red);
    }

    destination = source;
    NUI_CHECK(!ui::detail::ScalarSourceAccess::is_constant(destination));
    NUI_CHECK(ui::detail::ScalarSourceAccess::channel(destination) ==
              ui::ScalarChannel::Red);
}

void suite() {
    constant_construction_allocates_nothing();
    copy_assignment_has_strong_guarantee();
    move_and_noise_copy_failure_contracts();
}

} // namespace

int main() {
    return test::run("scalar source fault contracts", suite);
}
