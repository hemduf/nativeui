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
#if defined(_WIN32)
#  include <malloc.h>
#endif

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

void linear_gradient_warm_hit_allocates_zero() {
    const ui::LinearGradient gradient{
        {0.0f, 0.0f},
        {32.0f, 0.0f},
        {
            {0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
            {0.5f, {0.0f, 1.0f, 0.0f, 1.0f}},
            {1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},
        }};

    ui::detail::RenderResourceMaterializationContext context;
    std::size_t creates = 0;
    auto factory = [&] {
        ++creates;
        return ui::detail::GradientCacheAccess::materialize(gradient);
    };

    context.begin_frame();
    auto cold = context.acquire_linear_gradient(gradient, factory);
    NUI_CHECK(cold && cold.retained && !cold.hit);
    NUI_CHECK(creates == 1);
    cold = {};
    context.end_frame();

    context.begin_frame();
    ui::detail::RenderResourceMaterializationContext::ImageTextureAcquisition warm;
    std::size_t allocations = 0;
    {
        AllocationScope guard;
        warm = context.acquire_linear_gradient(gradient, factory);
        allocations = guard.allocations();
    }

    NUI_CHECK(warm && warm.hit && warm.retained);
    NUI_CHECK(allocations == 0);
    NUI_CHECK(creates == 1);
    context.end_frame();
}

void image_texture_warm_hit_allocates_zero() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());
    const auto value = texture(image);

    ui::detail::RenderResourceMaterializationContext context;
    std::size_t creates = 0;
    auto factory = [&] {
        ++creates;
        return SkShaders::Color(SK_ColorRED);
    };
    context.begin_frame();
    auto cold = context.acquire_image_texture(value, factory);
    NUI_CHECK(cold && cold.retained);
    NUI_CHECK(creates == 1);
    cold = {};
    context.end_frame();

    context.begin_frame();
    ui::detail::RenderResourceMaterializationContext::ImageTextureAcquisition warm;
    std::size_t allocations = 0;
    {
        AllocationScope guard;
        warm = context.acquire_image_texture(value, factory);
        allocations = guard.allocations();
    }

    NUI_CHECK(warm && warm.hit && warm.retained);
    NUI_CHECK(allocations == 0);
    NUI_CHECK(creates == 1);
    context.end_frame();
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
    std::size_t creates = 0;
    auto factory = [snapshot, &creates] {
        ++creates;
        return ui::detail::materialize_shader_brush(*snapshot);
    };
    context.begin_frame();
    auto cold = context.acquire_runtime_shader(*snapshot, factory);
    NUI_CHECK(cold && cold.retained);
    NUI_CHECK(creates == 1);
    cold = {};
    context.end_frame();

    context.begin_frame();
    ui::detail::RenderResourceMaterializationContext::ImageTextureAcquisition warm;
    std::size_t allocations = 0;
    {
        AllocationScope guard;
        warm = context.acquire_runtime_shader(*snapshot, factory);
        allocations = guard.allocations();
    }

    NUI_CHECK(warm && warm.hit && warm.retained);
    NUI_CHECK(allocations == 0);
    NUI_CHECK(creates == 1);
    context.end_frame();
}

void effect_warm_hit_allocates_zero() {
    const auto effect = ui::Effect::drop_shadow(
        {3.0f, -2.0f}, 5.0f, {0.2f, 0.3f, 0.4f, 0.8f});

    ui::detail::RenderResourceMaterializationContext context;
    std::size_t creates = 0;
    auto factory = [&effect, &creates] {
        ++creates;
        return ui::detail::EffectCacheAccess::materialize(effect);
    };
    context.begin_frame();
    auto cold = context.acquire_effect(effect, factory);
    NUI_CHECK(cold && cold.retained);
    NUI_CHECK(creates == 1);
    cold = {};
    context.end_frame();

    context.begin_frame();
    ui::detail::RenderResourceMaterializationContext::EffectAcquisition warm;
    std::size_t allocations = 0;
    {
        AllocationScope guard;
        warm = context.acquire_effect(effect, factory);
        allocations = guard.allocations();
    }

    NUI_CHECK(warm && warm.hit && warm.retained);
    NUI_CHECK(allocations == 0);
    NUI_CHECK(creates == 1);
    context.end_frame();
}

void deep_shader_child_warm_hit_allocates_zero() {
    const auto leaf_program = ui::ShaderProgram::compile(R"(
        uniform float value;
        half4 main(float2) {
            return half4(value, value, value, 1.0);
        }
    )");
    const auto parent_program = ui::ShaderProgram::compile(R"(
        uniform shader child;
        half4 main(float2 p) {
            return child.eval(p);
        }
    )");
    NUI_CHECK(leaf_program.ok() && parent_program.ok());

    ui::ShaderInstance leaf{leaf_program.program};
    NUI_CHECK(leaf.set_float("value", 0.25f) == ui::ShaderSetResult::Ok);

    ui::ShaderInstance middle{parent_program.program};
    NUI_CHECK(middle.set_child("child", ui::Brush{leaf}) ==
              ui::ShaderSetResult::Ok);

    ui::ShaderInstance root{parent_program.program};
    NUI_CHECK(root.set_child("child", ui::Brush{middle}) ==
              ui::ShaderSetResult::Ok);

    const ui::Brush brush{root};
    const auto* snapshot = ui::detail::ShaderBrushAccess::snapshot(brush);
    NUI_CHECK(snapshot && *snapshot);

    ui::detail::RenderResourceMaterializationContext context;
    std::size_t creates = 0;
    auto factory = [snapshot, &creates] {
        ++creates;
        return ui::detail::materialize_shader_brush(*snapshot);
    };

    context.begin_frame();
    auto cold = context.acquire_runtime_shader(*snapshot, factory);
    NUI_CHECK(cold && cold.retained && !cold.hit);
    NUI_CHECK(creates == 1);
    cold = {};
    context.end_frame();

    context.begin_frame();
    ui::detail::RenderResourceMaterializationContext::ImageTextureAcquisition warm;
    std::size_t allocations = 0;
    {
        AllocationScope guard;
        warm = context.acquire_runtime_shader(*snapshot, factory);
        allocations = guard.allocations();
    }

    NUI_CHECK(warm && warm.hit && warm.retained);
    NUI_CHECK(allocations == 0);
    NUI_CHECK(creates == 1);
    context.end_frame();
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

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](size);
    } catch (...) {
        return nullptr;
    }
}

void operator delete(void* memory, const std::nothrow_t&) noexcept {
    ::operator delete(memory);
}

void operator delete[](void* memory, const std::nothrow_t&) noexcept {
    ::operator delete[](memory);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    const auto align = static_cast<std::size_t>(alignment);
#if defined(_WIN32)
    if (void* memory = _aligned_malloc(size == 0 ? 1 : size, align)) {
        record_allocation();
        return memory;
    }
#else
    const auto requested = size == 0 ? 1 : size;
    const auto rounded = ((requested + align - 1) / align) * align;
    if (void* memory = std::aligned_alloc(align, rounded)) {
        record_allocation();
        return memory;
    }
#endif
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}

void* operator new(
    std::size_t size,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size, alignment);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](
    std::size_t size,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](size, alignment);
    } catch (...) {
        return nullptr;
    }
}

void operator delete(void* memory, std::align_val_t) noexcept {
#if defined(_WIN32)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

void operator delete[](void* memory, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}

void operator delete(
    void* memory, std::size_t, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}

void operator delete[](
    void* memory, std::size_t, std::align_val_t alignment) noexcept {
    ::operator delete[](memory, alignment);
}

void operator delete(
    void* memory,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept {
    ::operator delete(memory, alignment);
}

void operator delete[](
    void* memory,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept {
    ::operator delete[](memory, alignment);
}

void allocation_guard_covers_aligned_nothrow_new() {
    void* memory = nullptr;
    std::size_t allocations = 0;
    {
        AllocationScope guard;
        memory = ::operator new(
            64U, std::align_val_t{64U}, std::nothrow);
        allocations = guard.allocations();
    }

    NUI_CHECK(memory);
    NUI_CHECK(allocations == 1U);
    ::operator delete(
        memory, std::align_val_t{64U}, std::nothrow);
}

int main() {
    allocation_guard_covers_aligned_nothrow_new();
    linear_gradient_warm_hit_allocates_zero();
    image_texture_warm_hit_allocates_zero();
    runtime_shader_warm_hit_allocates_zero();
    effect_warm_hit_allocates_zero();
    deep_shader_child_warm_hit_allocates_zero();
    return 0;
}
