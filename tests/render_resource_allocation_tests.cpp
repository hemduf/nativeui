#include "src/detail/effect_cache_key.hpp"
#include "src/detail/render_resource_materialization.hpp"
#include "src/detail/shader_brush_access.hpp"
#include "test_support.hpp"

#include "include/core/SkColor.h"
#include "include/core/SkShader.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};

void record_allocation() noexcept {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
}

class AllocationScope final {
public:
    AllocationScope() noexcept {
        allocation_count.store(0, std::memory_order_relaxed);
        count_allocations.store(true, std::memory_order_relaxed);
    }

    ~AllocationScope() noexcept {
        count_allocations.store(false, std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t allocations() const noexcept {
        return allocation_count.load(std::memory_order_relaxed);
    }
};

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

void image_texture_warm_hit_allocates_zero() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());
    const auto value = texture(image);

    ui::detail::RenderResourceMaterializationContext context;
    auto factory = [] { return SkShaders::Color(SK_ColorRED); };
    auto cold = context.acquire_image_texture(value, factory);
    NUI_CHECK(cold && cold.retained);
    cold = {};

    ui::detail::RenderResourceMaterializationContext::ImageTextureAcquisition warm;
    AllocationScope guard;
    warm = context.acquire_image_texture(value, factory);
    const auto allocations = guard.allocations();

    NUI_CHECK(warm && warm.hit && warm.retained);
    NUI_CHECK(allocations == 0);
}

void runtime_shader_warm_hit_allocates_zero() {
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
    auto factory = [snapshot] {
        return ui::detail::materialize_shader_brush(*snapshot);
    };
    auto cold = context.acquire_runtime_shader(*snapshot, factory);
    NUI_CHECK(cold && cold.retained);
    cold = {};

    ui::detail::RenderResourceMaterializationContext::ImageTextureAcquisition warm;
    AllocationScope guard;
    warm = context.acquire_runtime_shader(*snapshot, factory);
    const auto allocations = guard.allocations();

    NUI_CHECK(warm && warm.hit && warm.retained);
    NUI_CHECK(allocations == 0);
}

void effect_warm_hit_allocates_zero() {
    const auto effect = ui::Effect::drop_shadow(
        {3.0f, -2.0f}, 5.0f, {0.2f, 0.3f, 0.4f, 0.8f});

    ui::detail::RenderResourceMaterializationContext context;
    auto factory = [&effect] {
        return ui::detail::EffectCacheAccess::materialize(effect);
    };
    auto cold = context.acquire_effect(effect, factory);
    NUI_CHECK(cold && cold.retained);
    cold = {};

    ui::detail::RenderResourceMaterializationContext::EffectAcquisition warm;
    AllocationScope guard;
    warm = context.acquire_effect(effect, factory);
    const auto allocations = guard.allocations();

    NUI_CHECK(warm && warm.hit && warm.retained);
    NUI_CHECK(allocations == 0);
}

} // namespace

void* operator new(std::size_t size) {
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        record_allocation();
        return memory;
    }
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

int main() {
    image_texture_warm_hit_allocates_zero();
    runtime_shader_warm_hit_allocates_zero();
    effect_warm_hit_allocates_zero();
    return 0;
}
