#include "render_resource_allocation_probe.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#if defined(_WIN32)
#  include <malloc.h>
#endif

namespace {

std::atomic<bool> g_count_allocations{false};
std::atomic<std::size_t> g_allocation_count{0};

void record_allocation() noexcept {
    if (g_count_allocations.load(std::memory_order_relaxed)) {
        g_allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace

namespace test::render_resource_allocations {

void begin() noexcept {
    g_allocation_count.store(0, std::memory_order_relaxed);
    g_count_allocations.store(true, std::memory_order_relaxed);
}

void end() noexcept {
    g_count_allocations.store(false, std::memory_order_relaxed);
}

std::size_t count() noexcept {
    return g_allocation_count.load(std::memory_order_relaxed);
}

} // namespace test::render_resource_allocations

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
    const auto requested = size == 0 ? 1U : size;
    if (align == 0U ||
        requested > std::numeric_limits<std::size_t>::max() - (align - 1U)) {
        throw std::bad_alloc{};
    }
    const auto rounded = ((requested + align - 1U) / align) * align;
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
