#include <nativeui/detail/raster_cache_epoch.hpp>

#include <cstdint>
#include <cstdlib>
#include <new>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

// Allocation fault instrumentation is confined to this test executable.
namespace {
std::size_t allocations{};
bool fail_next_allocation{};
}
void* operator new(std::size_t size) {
    if (fail_next_allocation) {
        fail_next_allocation = false;
        throw std::bad_alloc{};
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        ++allocations;
        return memory;
    }
    throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

namespace ui::detail {
struct RasterCacheEpochTestAccess {
    static void near_exhaustion(RasterCacheEpoch& epoch) noexcept {
        epoch.generation_ = std::numeric_limits<std::uint64_t>::max() - 1;
    }
};
}

namespace {
using Epoch = ui::detail::RasterCacheEpoch;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void cold_warm_and_coalescing() {
    Epoch epoch;
    require(epoch.stale(), "fresh content is stale");
    require(!epoch.reusable({}), "empty token cannot hit");
    require(!epoch.commit({}), "empty token cannot commit");
    const auto first = epoch.capture();
    require(!epoch.reusable(first), "capture does not publish content");
    require(epoch.commit(first), "successful initial publication");
    require(epoch.reusable(first), "published token is reusable");
    epoch.invalidate();
    require(epoch.stale(), "changed content is stale");
    require(!epoch.reusable(first), "old content is no longer reusable");
    require(!epoch.commit(first), "old publication cannot clear new damage");
    const auto generation = epoch.generation();
    epoch.invalidate();
    epoch.invalidate();
    require(epoch.generation() == generation, "pending invalidations coalesce");
    const auto next = epoch.capture();
    require(epoch.commit(next), "later normal update recovers");
    require(epoch.reusable(next), "later content reusable");
}

void invalidation_during_capture() {
    Epoch epoch;
    const auto candidate = epoch.capture();
    epoch.invalidate();
    require(!epoch.commit(candidate), "in-paint invalidation survives publication");
    require(epoch.stale(), "rejected commit stays stale");
    const auto retry = epoch.capture();
    epoch.invalidate();
    require(!epoch.commit(retry), "repeated reentrant invalidation is not lost");
    const auto complete = epoch.capture();
    require(epoch.commit(complete), "normal update following reentrancy succeeds");
    epoch.invalidate();
    {
        const auto abandoned = epoch.capture();
        (void)abandoned;
    }
    require(epoch.stale(), "abandoned paint does not publish clean content");
    require(epoch.commit(epoch.capture()), "abandoned update can be retried");
}

void identities_and_lifetime() {
    Epoch a;
    Epoch b;
    const auto a_token = a.capture();
    const auto b_token = b.capture();
    require(!b.commit(a_token), "equal generations from distinct owners do not alias");
    require(a.commit(a_token) && b.commit(b_token), "independent publications");
    a.invalidate();
    require(b.reusable(b_token), "other instance remains reusable");

    Epoch::Token retired;
    {
        Epoch temporary;
        retired = temporary.capture();
        require(!retired.expired(), "token is live before owner retirement");
        require(temporary.commit(retired), "temporary publication");
    }
    require(retired.expired(), "token does not retain removed boundary");
    Epoch replacement;
    require(!replacement.commit(retired), "new owner cannot accept dead token");
    require(b.reusable(b_token), "survivor unaffected by owner destruction");
}

void overlapping_readers() {
    Epoch epoch;
    const auto first_view = epoch.capture();
    const auto second_view = epoch.capture();
    require(epoch.commit(first_view), "first consumer publishes");
    require(epoch.commit(second_view), "second consumer may publish same unchanged generation");
    require(epoch.reusable(first_view) && epoch.reusable(second_view), "both entries current");
    epoch.invalidate();
    require(!epoch.reusable(first_view) && !epoch.reusable(second_view), "change invalidates both");
}

void exhaustion_is_fail_closed() {
    Epoch epoch;
    ui::detail::RasterCacheEpochTestAccess::near_exhaustion(epoch);
    const auto before = epoch.capture();
    require(epoch.commit(before), "penultimate generation publishes");
    epoch.invalidate();
    const auto last = epoch.capture();
    require(epoch.commit(last), "maximum generation publishes once");
    epoch.invalidate();
    require(epoch.stale(), "exhaustion is stale, never wrapped");
    require(!epoch.reusable(last), "exhausted epoch cannot hit last generation");
    require(!epoch.commit(last), "exhausted epoch cannot commit last generation");
    require(epoch.capture().expired(), "exhausted epoch cannot issue usable token");
    for (int i = 0; i < 100; ++i) epoch.invalidate();
    require(epoch.generation() == std::numeric_limits<std::uint64_t>::max(),
            "generation never wraps");
    Epoch replacement;
    require(replacement.commit(replacement.capture()), "fresh lifetime recovers independently");
}

void identity_capture_does_not_start_paint() {
    Epoch epoch;
    const auto generation = epoch.generation();
    const auto identity = epoch.lifetime_token();
    epoch.invalidate();
    require(epoch.generation() == generation, "callback identity does not start a render");
    require(epoch.same_lifetime(identity), "callback identity survives content changes");
    require(!epoch.commit(identity), "identity-only token cannot commit");
    require(epoch.commit(epoch.capture()), "ordinary capture still publishes");
    require(!epoch.commit(identity), "identity-only token cannot impersonate a capture");
}

void allocation_failure_and_warm_operations() {
    fail_next_allocation = true;
    bool caught = false;
    try { Epoch attempted; } catch (const std::bad_alloc&) { caught = true; }
    require(caught && !fail_next_allocation, "identity allocation fails visibly");
    Epoch epoch;
    const auto count = allocations;
    for (int i = 0; i < 1000; ++i) {
        const auto token = epoch.capture();
        require(epoch.commit(token), "update after failed construction succeeds");
        require(epoch.reusable(token), "warm lookup succeeds");
        epoch.invalidate();
        epoch.invalidate();
    }
    require(allocations == count, "capture, commit, hit and invalidation allocate nothing");
}

static_assert(!std::is_copy_constructible_v<Epoch>);
static_assert(!std::is_move_constructible_v<Epoch>);
static_assert(std::is_nothrow_destructible_v<Epoch>);
static_assert(noexcept(std::declval<Epoch&>().invalidate()));
static_assert(noexcept(std::declval<Epoch&>().capture()));
static_assert(noexcept(std::declval<Epoch&>().commit(std::declval<const Epoch::Token&>())));
static_assert(noexcept(std::declval<const Epoch&>().reusable(std::declval<const Epoch::Token&>())));
}

int main() {
    try {
        cold_warm_and_coalescing();
        invalidation_during_capture();
        identities_and_lifetime();
        overlapping_readers();
        exhaustion_is_fail_closed();
        identity_capture_does_not_start_paint();
        allocation_failure_and_warm_operations();
        std::cout << "7 raster cache epoch contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
