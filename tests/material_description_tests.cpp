#include <nativeui/material.hpp>

#include "src/detail/material_access.hpp"
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
thread_local bool fail_next_allocation = false;
thread_local std::size_t allocation_count = 0;

void before_allocation() {
    if (fail_next_allocation) {
        fail_next_allocation = false;
        throw std::bad_alloc{};
    }
    if (track_allocations) ++allocation_count;
}

void* allocate_memory(std::size_t size) {
    before_allocation();
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}

void* allocate_aligned_memory(std::size_t size, std::size_t alignment) {
    before_allocation();
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
void operator delete(void* memory, const std::nothrow_t&) noexcept {
    std::free(memory);
}
void operator delete[](void* memory, const std::nothrow_t&) noexcept {
    std::free(memory);
}
void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocate_aligned_memory(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocate_aligned_memory(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* memory, std::align_val_t) noexcept {
    free_aligned_memory(memory);
}
void operator delete[](void* memory, std::align_val_t) noexcept {
    free_aligned_memory(memory);
}
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept {
    free_aligned_memory(memory);
}
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept {
    free_aligned_memory(memory);
}

namespace {

static_assert(std::is_nothrow_default_constructible_v<ui::Material>);
static_assert(std::is_nothrow_move_constructible_v<ui::Material>);
static_assert(std::is_nothrow_move_assignable_v<ui::Material>);
static_assert(std::is_nothrow_destructible_v<ui::Material>);
static_assert(noexcept(std::declval<ui::Material&>().set_roughness(0.0f)));
static_assert(noexcept(std::declval<ui::Material&>().set_metallic(0.0f)));
static_assert(noexcept(std::declval<ui::Material&>().clear_emissive()));

float scalar_constant(const ui::ScalarSource& source) {
    NUI_CHECK(ui::detail::ScalarSourceAccess::is_constant(source));
    return ui::detail::ScalarSourceAccess::constant_value(source);
}

void check_color(ui::Color actual, ui::Color expected) {
    NUI_CHECK(actual.r == expected.r);
    NUI_CHECK(actual.g == expected.g);
    NUI_CHECK(actual.b == expected.b);
    NUI_CHECK(actual.a == expected.a);
}

void default_and_emissive_contract() {
    ui::Material material;
    const auto* albedo = ui::detail::MaterialAccess::solid_color(material.albedo());
    NUI_CHECK(albedo != nullptr);
    check_color(*albedo, {1.0f, 1.0f, 1.0f, 1.0f});
    NUI_CHECK(scalar_constant(material.roughness()) == 0.5f);
    NUI_CHECK(scalar_constant(material.metallic()) == 0.0f);
    NUI_CHECK(!material.has_emissive());

    material.set_emissive(
        ui::Brush{ui::Color{0.2f, 0.3f, 0.4f, 0.5f}},
        ui::ScalarSource{0.0f});
    NUI_CHECK(material.has_emissive());
    NUI_CHECK(scalar_constant(material.emissive_intensity()) == 0.0f);

    material.set_emissive(
        ui::Brush{ui::Color{0.4f, 0.3f, 0.2f, 1.0f}},
        8.0f);
    NUI_CHECK(material.has_emissive());
    NUI_CHECK(scalar_constant(material.emissive_intensity()) == 8.0f);

    material.clear_emissive();
    NUI_CHECK(!material.has_emissive());
    const auto* emissive =
        ui::detail::MaterialAccess::solid_color(material.emissive_color());
    NUI_CHECK(emissive != nullptr);
    check_color(*emissive, {0.0f, 0.0f, 0.0f, 0.0f});
    NUI_CHECK(scalar_constant(material.emissive_intensity()) == 0.0f);
}

void producer_values_are_not_preclamped() {
    ui::Material material;
    material.set_roughness(-2.0f).set_metallic(8.0f);
    NUI_CHECK(scalar_constant(material.roughness()) == -2.0f);
    NUI_CHECK(scalar_constant(material.metallic()) == 8.0f);

    material.set_roughness(std::numeric_limits<float>::infinity());
    material.set_metallic(std::numeric_limits<float>::quiet_NaN());
    NUI_CHECK(std::isinf(scalar_constant(material.roughness())));
    NUI_CHECK(std::isnan(scalar_constant(material.metallic())));
}

void move_contract() {
    ui::Material source{
        ui::Brush{ui::Color{0.25f, 0.5f, 0.75f, 1.0f}}};
    source.set_roughness(0.2f)
          .set_metallic(0.9f)
          .set_emissive(
              ui::Brush{ui::Color{1.0f, 0.5f, 0.25f, 1.0f}},
              ui::ScalarSource{4.0f});

    ui::Material moved{std::move(source)};
    NUI_CHECK(moved.has_emissive());
    NUI_CHECK(scalar_constant(moved.roughness()) == 0.2f);
    NUI_CHECK(scalar_constant(moved.metallic()) == 0.9f);
    NUI_CHECK(scalar_constant(moved.emissive_intensity()) == 4.0f);

    const auto* source_albedo =
        ui::detail::MaterialAccess::solid_color(source.albedo());
    NUI_CHECK(source_albedo != nullptr);
    check_color(*source_albedo, {1.0f, 1.0f, 1.0f, 1.0f});
    NUI_CHECK(scalar_constant(source.roughness()) == 0.5f);
    NUI_CHECK(scalar_constant(source.metallic()) == 0.0f);
    NUI_CHECK(!source.has_emissive());

    auto* self = &moved;
    moved = std::move(*self);
    const auto* moved_albedo =
        ui::detail::MaterialAccess::solid_color(moved.albedo());
    NUI_CHECK(moved_albedo != nullptr);
    check_color(*moved_albedo, {1.0f, 1.0f, 1.0f, 1.0f});
    NUI_CHECK(scalar_constant(moved.roughness()) == 0.5f);
    NUI_CHECK(scalar_constant(moved.metallic()) == 0.0f);
    NUI_CHECK(!moved.has_emissive());
}

void allocation_free_value_contract() {
    allocation_count = 0;
    track_allocations = true;
    {
        ui::Material material;
        material.set_roughness(-2.0f)
                .set_metallic(8.0f);
        material.clear_emissive();
    }
    track_allocations = false;

    NUI_CHECK(allocation_count == 0);
}

void copy_assignment_failure_contract() {
    ui::LinearGradient gradient{
        {0.0f, 0.0f},
        {16.0f, 0.0f},
        ui::Color{0.1f, 0.2f, 0.3f, 1.0f},
        ui::Color{0.7f, 0.8f, 0.9f, 1.0f}};
    ui::Material source{ui::Brush{std::move(gradient)}};
    source.set_roughness(0.2f).set_metallic(0.8f);

    ui::Material destination{
        ui::Brush{ui::Color{0.9f, 0.1f, 0.2f, 1.0f}}};
    destination.set_roughness(0.7f).set_metallic(0.3f);

    bool failed = false;
    fail_next_allocation = true;
    try {
        destination = source;
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    fail_next_allocation = false;

    NUI_CHECK(failed);
    const auto* unchanged =
        ui::detail::MaterialAccess::solid_color(destination.albedo());
    NUI_CHECK(unchanged != nullptr);
    check_color(*unchanged, {0.9f, 0.1f, 0.2f, 1.0f});
    NUI_CHECK(scalar_constant(destination.roughness()) == 0.7f);
    NUI_CHECK(scalar_constant(destination.metallic()) == 0.3f);

    destination = source;
    NUI_CHECK(ui::detail::MaterialAccess::solid_color(destination.albedo()) == nullptr);
    NUI_CHECK(scalar_constant(destination.roughness()) == 0.2f);
    NUI_CHECK(scalar_constant(destination.metallic()) == 0.8f);
}

void independent_material_contract() {
    ui::Material second{
        ui::Brush{ui::Color{0.1f, 0.7f, 0.2f, 1.0f}}};
    second.set_roughness(0.4f).set_metallic(0.6f);

    {
        ui::Material first{
            ui::Brush{ui::Color{0.8f, 0.2f, 0.1f, 1.0f}}};
        first.set_roughness(0.2f)
             .set_metallic(0.9f)
             .set_emissive(
                 ui::Brush{ui::Color{1.0f, 0.4f, 0.2f, 1.0f}},
                 4.0f);
        first.clear_emissive();
    }

    const auto* color =
        ui::detail::MaterialAccess::solid_color(second.albedo());
    NUI_CHECK(color != nullptr);
    check_color(*color, {0.1f, 0.7f, 0.2f, 1.0f});
    NUI_CHECK(scalar_constant(second.roughness()) == 0.4f);
    NUI_CHECK(scalar_constant(second.metallic()) == 0.6f);
    NUI_CHECK(!second.has_emissive());
}

void sanitation_contract() {
    using A = ui::detail::MaterialAccess;
    check_color(
        A::sanitize_albedo(
            {-1.0f, 0.25f, 2.0f, std::numeric_limits<float>::infinity()}),
        {0.0f, 0.25f, 1.0f, 0.0f});

    NUI_CHECK(A::sanitize_roughness(-1.0f) == 0.045f);
    NUI_CHECK(A::sanitize_roughness(0.5f) == 0.5f);
    NUI_CHECK(A::sanitize_roughness(2.0f) == 1.0f);
    NUI_CHECK(A::sanitize_roughness(
                  std::numeric_limits<float>::quiet_NaN()) == 0.5f);

    NUI_CHECK(A::sanitize_metallic(-1.0f) == 0.0f);
    NUI_CHECK(A::sanitize_metallic(0.4f) == 0.4f);
    NUI_CHECK(A::sanitize_metallic(2.0f) == 1.0f);
    NUI_CHECK(A::sanitize_metallic(
                  -std::numeric_limits<float>::infinity()) == 0.0f);

    NUI_CHECK(A::sanitize_emissive_channel(-1.0f) == 0.0f);
    NUI_CHECK(A::sanitize_emissive_channel(0.7f) == 0.7f);
    NUI_CHECK(A::sanitize_emissive_channel(3.0f) == 1.0f);
    NUI_CHECK(A::sanitize_emissive_channel(
                  std::numeric_limits<float>::quiet_NaN()) == 0.0f);

    NUI_CHECK(A::sanitize_emissive_intensity(-1.0f) == 0.0f);
    NUI_CHECK(A::sanitize_emissive_intensity(8.0f) == 8.0f);
    NUI_CHECK(A::sanitize_emissive_intensity(128.0f) == 64.0f);
    NUI_CHECK(A::sanitize_emissive_intensity(
                  std::numeric_limits<float>::infinity()) == 0.0f);
}

} // namespace

int main() {
    default_and_emissive_contract();
    producer_values_are_not_preclamped();
    move_contract();
    allocation_free_value_contract();
    copy_assignment_failure_contract();
    independent_material_contract();
    sanitation_contract();
    return 0;
}
