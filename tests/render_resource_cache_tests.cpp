#include "src/detail/render_resource_cache.hpp"
#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {

struct Resource {
    explicit Resource(int value_in) : value(value_in) {}
    int value{};
};

using Cache = ui::detail::RenderResourceCache<int, Resource>;

void retained_hit_reuses_resource() {
    Cache cache{{.max_entries = 4, .max_accounted_bytes = 64}};
    int creates = 0;

    auto first = cache.acquire(7, 8, [&] {
        ++creates;
        return std::make_shared<Resource>(17);
    });
    NUI_CHECK(first);
    NUI_CHECK(!first.hit);
    NUI_CHECK(first.retained);
    NUI_CHECK(first.resource->value == 17);

    auto second = cache.acquire(7, 8, [&] {
        ++creates;
        return std::make_shared<Resource>(99);
    });
    NUI_CHECK(second);
    NUI_CHECK(second.hit);
    NUI_CHECK(second.retained);
    NUI_CHECK(second.resource == first.resource);
    NUI_CHECK(creates == 1);
    NUI_CHECK(cache.retained_entries() == 1);
    NUI_CHECK(cache.retained_accounted_bytes() == 8);
}

void entry_cap_is_exact_and_lru_is_deterministic() {
    Cache cache{{.max_entries = 512, .max_accounted_bytes = 4096}};

    for (int key = 0; key < 512; ++key) {
        auto resource = cache.acquire(key, 1, [key] {
            return std::make_shared<Resource>(key);
        });
        NUI_CHECK(resource && resource.retained && !resource.hit);
    }
    NUI_CHECK(cache.retained_entries() == 512);
    NUI_CHECK(cache.contains(0));
    NUI_CHECK(cache.contains(511));

    auto touch_oldest = cache.acquire(0, 1, [] {
        return std::make_shared<Resource>(-1);
    });
    NUI_CHECK(touch_oldest.hit);
    touch_oldest.resource.reset();

    auto overflow = cache.acquire(512, 1, [] {
        return std::make_shared<Resource>(512);
    });
    NUI_CHECK(overflow && overflow.retained && !overflow.hit);
    NUI_CHECK(cache.retained_entries() == 512);
    NUI_CHECK(cache.contains(0));
    NUI_CHECK(!cache.contains(1));
    NUI_CHECK(cache.contains(512));
}

void byte_budget_is_exact_and_oversize_is_transient() {
    constexpr std::size_t budget = 128U * 1024U * 1024U;
    Cache cache{{.max_entries = 8, .max_accounted_bytes = budget}};

    auto below = cache.acquire(1, budget - 1, [] {
        return std::make_shared<Resource>(1);
    });
    NUI_CHECK(below && below.retained);
    below.resource.reset();

    auto exact = cache.acquire(2, 1, [] {
        return std::make_shared<Resource>(2);
    });
    NUI_CHECK(exact && exact.retained);
    exact.resource.reset();
    NUI_CHECK(cache.retained_accounted_bytes() == budget);

    auto above = cache.acquire(3, budget + 1, [] {
        return std::make_shared<Resource>(3);
    });
    NUI_CHECK(above);
    NUI_CHECK(!above.retained);
    NUI_CHECK(!above.hit);
    NUI_CHECK(cache.retained_accounted_bytes() == budget);
    NUI_CHECK(!cache.contains(3));
}

void checked_accounting_rejects_wraparound() {
    Cache cache{{.max_entries = 8, .max_accounted_bytes = 1024}};
    auto impossible = cache.acquire(
        1, std::numeric_limits<std::size_t>::max(), [] {
            return std::make_shared<Resource>(1);
        });
    NUI_CHECK(impossible);
    NUI_CHECK(!impossible.retained);
    NUI_CHECK(cache.retained_entries() == 0);
    NUI_CHECK(cache.retained_accounted_bytes() == 0);
}

void full_entry_cap_keeps_all_active_resources_pinned() {
    Cache cache{{.max_entries = 512, .max_accounted_bytes = 4096}};
    std::array<Cache::Acquisition, 512> active{};

    for (int key = 0; key < 512; ++key) {
        active[static_cast<std::size_t>(key)] = cache.acquire(key, 1, [key] {
            return std::make_shared<Resource>(key);
        });
        const auto& acquisition = active[static_cast<std::size_t>(key)];
        NUI_CHECK(acquisition && acquisition.retained && !acquisition.hit);
        if (key == 510) NUI_CHECK(cache.retained_entries() == 511);
    }

    NUI_CHECK(cache.retained_entries() == 512);
    NUI_CHECK(cache.retained_accounted_bytes() == 512);

    auto transient = cache.acquire(512, 1, [] {
        return std::make_shared<Resource>(512);
    });
    NUI_CHECK(transient);
    NUI_CHECK(!transient.retained);
    NUI_CHECK(!transient.hit);
    NUI_CHECK(cache.retained_entries() == 512);
    NUI_CHECK(cache.retained_accounted_bytes() == 512);
    NUI_CHECK(!cache.contains(512));

    active.front().resource.reset();
    transient.resource.reset();

    auto replacement = cache.acquire(512, 1, [] {
        return std::make_shared<Resource>(1512);
    });
    NUI_CHECK(replacement && replacement.retained && !replacement.hit);
    NUI_CHECK(cache.retained_entries() == 512);
    NUI_CHECK(cache.retained_accounted_bytes() == 512);
    NUI_CHECK(!cache.contains(0));
    NUI_CHECK(cache.contains(512));
}

void active_entries_are_pinned_and_new_resource_stays_transient() {
    Cache cache{{.max_entries = 1, .max_accounted_bytes = 16}};

    auto active = cache.acquire(1, 8, [] {
        return std::make_shared<Resource>(1);
    });
    NUI_CHECK(active && active.retained);

    auto transient = cache.acquire(2, 8, [] {
        return std::make_shared<Resource>(2);
    });
    NUI_CHECK(transient);
    NUI_CHECK(!transient.retained);
    NUI_CHECK(!cache.contains(2));
    NUI_CHECK(cache.contains(1));
    NUI_CHECK(cache.retained_entries() == 1);
    NUI_CHECK(cache.retained_accounted_bytes() == 8);

    active.resource.reset();
    transient.resource.reset();

    auto replacement = cache.acquire(2, 8, [] {
        return std::make_shared<Resource>(22);
    });
    NUI_CHECK(replacement && replacement.retained);
    NUI_CHECK(!cache.contains(1));
    NUI_CHECK(cache.contains(2));
    NUI_CHECK(cache.retained_entries() == 1);
}

struct ConstantHash {
    [[nodiscard]] std::size_t operator()(int) const noexcept {
        return 0;
    }
};

using CollisionCache =
    ui::detail::RenderResourceCache<int, Resource, ConstantHash>;

void hash_collision_still_uses_full_key_equality() {
    CollisionCache cache{{.max_entries = 4, .max_accounted_bytes = 64}};

    auto one = cache.acquire(1, 4, [] {
        return std::make_shared<Resource>(10);
    });
    auto two = cache.acquire(2, 4, [] {
        return std::make_shared<Resource>(20);
    });
    NUI_CHECK(one && two);
    NUI_CHECK(one.resource != two.resource);

    auto again = cache.acquire(2, 4, [] {
        return std::make_shared<Resource>(999);
    });
    NUI_CHECK(again.hit);
    NUI_CHECK(again.resource->value == 20);
}

void factory_failure_does_not_mutate_cache() {
    Cache cache{{.max_entries = 4, .max_accounted_bytes = 64}};
    bool threw = false;
    try {
        static_cast<void>(cache.acquire(1, 4, []() -> std::shared_ptr<Resource> {
            throw std::runtime_error("injected resource creation failure");
        }));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(cache.retained_entries() == 0);
    NUI_CHECK(cache.retained_accounted_bytes() == 0);

    auto recovered = cache.acquire(1, 4, [] {
        return std::make_shared<Resource>(7);
    });
    NUI_CHECK(recovered && recovered.retained);
}

void factory_failure_preserves_full_cache_and_lru() {
    Cache cache{{.max_entries = 2, .max_accounted_bytes = 16}};

    auto one = cache.acquire(1, 8, [] {
        return std::make_shared<Resource>(1);
    });
    auto two = cache.acquire(2, 8, [] {
        return std::make_shared<Resource>(2);
    });
    NUI_CHECK(one && two && one.retained && two.retained);
    one.resource.reset();
    two.resource.reset();

    auto touch_one = cache.acquire(1, 8, [] {
        return std::make_shared<Resource>(101);
    });
    NUI_CHECK(touch_one && touch_one.hit);
    touch_one.resource.reset();

    bool threw = false;
    try {
        static_cast<void>(cache.acquire(3, 8, []() -> std::shared_ptr<Resource> {
            throw std::runtime_error("injected full-cache creation failure");
        }));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(cache.retained_entries() == 2);
    NUI_CHECK(cache.retained_accounted_bytes() == 16);
    NUI_CHECK(cache.contains(1));
    NUI_CHECK(cache.contains(2));
    NUI_CHECK(!cache.contains(3));

    auto three = cache.acquire(3, 8, [] {
        return std::make_shared<Resource>(3);
    });
    NUI_CHECK(three && three.retained && !three.hit);
    NUI_CHECK(cache.contains(1));
    NUI_CHECK(!cache.contains(2));
    NUI_CHECK(cache.contains(3));
}

void cache_instances_are_lifetime_independent() {
    Cache a{{.max_entries = 2, .max_accounted_bytes = 32}};
    Cache b{{.max_entries = 2, .max_accounted_bytes = 32}};

    auto a_resource = a.acquire(1, 8, [] {
        return std::make_shared<Resource>(11);
    });
    auto b_resource = b.acquire(1, 8, [] {
        return std::make_shared<Resource>(22);
    });
    NUI_CHECK(a_resource && b_resource);
    NUI_CHECK(a_resource.resource != b_resource.resource);

    a.clear();
    NUI_CHECK(a.retained_entries() == 0);
    NUI_CHECK(b.retained_entries() == 1);
    NUI_CHECK(b.contains(1));
    NUI_CHECK(b_resource.resource->value == 22);
}

} // namespace

int main() {
    retained_hit_reuses_resource();
    entry_cap_is_exact_and_lru_is_deterministic();
    byte_budget_is_exact_and_oversize_is_transient();
    checked_accounting_rejects_wraparound();
    full_entry_cap_keeps_all_active_resources_pinned();
    active_entries_are_pinned_and_new_resource_stays_transient();
    hash_collision_still_uses_full_key_equality();
    factory_failure_does_not_mutate_cache();
    factory_failure_preserves_full_cache_and_lru();
    cache_instances_are_lifetime_independent();
    return 0;
}
