#include "render_resource_allocation_probe.hpp"

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#if defined(__linux__) && __has_include(<execinfo.h>)
#  define NATIVEUI_ALLOCATION_BACKTRACE 1
#  include <execinfo.h>
#endif
#if defined(_WIN32)
#  include <malloc.h>
#endif

namespace {

std::atomic<bool> g_count_allocations{false};
std::atomic<std::size_t> g_allocation_count{0};
#if defined(NATIVEUI_ALLOCATION_BACKTRACE)
std::atomic<bool> g_capture_trace{false};
std::array<void*, 32> g_first_allocation_trace{};
int g_first_allocation_trace_size{};
#endif

void record_allocation() noexcept {
    if (g_count_allocations.load(std::memory_order_relaxed)) {
        g_allocation_count.fetch_add(1, std::memory_order_relaxed);
#if defined(NATIVEUI_ALLOCATION_BACKTRACE)
        if (g_capture_trace.exchange(false, std::memory_order_relaxed)) {
            g_first_allocation_trace_size = ::backtrace(
                g_first_allocation_trace.data(),
                static_cast<int>(g_first_allocation_trace.size()));
        }
#endif
    }
}

} // namespace

namespace test::render_resource_allocations {

void begin() noexcept {
#if defined(NATIVEUI_ALLOCATION_BACKTRACE)
    g_first_allocation_trace_size = 0;
    g_capture_trace.store(std::getenv("NATIVEUI_TRACE_ALLOCATIONS") != nullptr,
                          std::memory_order_relaxed);
#endif
    g_allocation_count.store(0, std::memory_order_relaxed);
    g_count_allocations.store(true, std::memory_order_relaxed);
}

void end() noexcept {
    g_count_allocations.store(false, std::memory_order_relaxed);
}

bool pause_counting() noexcept {
    return g_count_allocations.exchange(false, std::memory_order_relaxed);
}

void resume_counting(bool previously_enabled) noexcept {
    g_count_allocations.store(previously_enabled, std::memory_order_relaxed);
}

std::size_t count() noexcept {
    return g_allocation_count.load(std::memory_order_relaxed);
}

void print_first_allocation_trace() noexcept {
#if defined(NATIVEUI_ALLOCATION_BACKTRACE)
    if (g_first_allocation_trace_size > 0) {
        ::backtrace_symbols_fd(g_first_allocation_trace.data(),
                               g_first_allocation_trace_size, 2);
    }
#endif
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
