#include <nativeui/detail/semantic_proxy.hpp>
#include <nativeui/detail/semantic_snapshot.hpp>
#include <nativeui/semantics.hpp>

#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

struct FakeNativeProxy {
    explicit FakeNativeProxy(ui::detail::SemanticSnapshotProxy source)
        : source(std::move(source)) {}

    ui::detail::SemanticSnapshotProxy source;
};

ui::SemanticTreeSnapshot ordinary_snapshot() {
    ui::SemanticTreeSnapshot tree;
    tree.root = 42;

    ui::SemanticNodeSnapshot node;
    node.id = 42;
    node.info.role = ui::SemanticRole::Button;
    node.info.name = "Apply";
    node.info.focusable = true;
    node.info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};
    tree.nodes.push_back(std::move(node));
    return tree;
}

ui::SemanticTreeSnapshot virtual_snapshot(
    ui::VirtualSemanticChildren::MetadataSnapshot metadata) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 7;

    ui::SemanticNodeSnapshot list;
    list.id = 7;
    list.info.role = ui::SemanticRole::ListView;
    list.info.name = "Items";
    list.bounds = {0.0f, 0.0f, 100.0f, 40.0f};
    list.virtual_children = ui::VirtualSemanticChildren::from_metadata(
        1,
        std::move(metadata),
        ui::VirtualSemanticItemToken{20},
        list.bounds,
        20.0f,
        0.0f);
    tree.nodes.push_back(std::move(list));
    return tree;
}

void ordinary_identity_is_stable_and_cache_values_are_weak() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    T068_CHECK(!publisher->publish(ordinary_snapshot()).empty());

    ui::detail::SemanticProxyCache<FakeNativeProxy> cache{publisher};
    int factory_calls = 0;
    const auto factory = [&](ui::detail::SemanticSnapshotProxy source) {
        ++factory_calls;
        return std::make_shared<FakeNativeProxy>(std::move(source));
    };

    auto first = cache.ordinary(42, factory);
    auto second = cache.ordinary(42, factory);
    T068_CHECK(first != nullptr);
    T068_CHECK(first.get() == second.get());
    T068_CHECK(factory_calls == 1);
    T068_CHECK(cache.tracked_identities() == 1);

    std::weak_ptr<FakeNativeProxy> weak = first;
    first.reset();
    second.reset();
    T068_CHECK(weak.expired());

    auto replacement = cache.ordinary(42, factory);
    T068_CHECK(replacement != nullptr);
    T068_CHECK(factory_calls == 2);
    T068_CHECK(cache.tracked_identities() == 1);

    ui::SemanticTreeSnapshot removed;
    T068_CHECK(!publisher->publish(std::move(removed)).empty());
    T068_CHECK(!replacement->source.read().has_value());
    replacement.reset();
    T068_CHECK(cache.prune_expired() == 1);
    T068_CHECK(cache.tracked_identities() == 0);

    // A removed identity cannot manufacture a fresh native proxy solely from a
    // stale platform request after its last live proxy has gone away.
    auto stale = cache.ordinary(42, factory);
    T068_CHECK(stale == nullptr);
    T068_CHECK(factory_calls == 2);
}

void virtual_identity_uses_list_and_token_and_is_per_view() {
    auto metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    metadata->push_back({10, "Ten", "", true, false,
                         ui::SemanticCheckedState::NotApplicable,
                         {ui::SemanticAction::Select, ui::SemanticAction::Focus}});
    metadata->push_back({20, "Twenty", "", true, false,
                         ui::SemanticCheckedState::NotApplicable,
                         {ui::SemanticAction::Select, ui::SemanticAction::Focus}});

    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    T068_CHECK(!publisher->publish(virtual_snapshot(metadata)).empty());

    ui::detail::SemanticProxyCache<FakeNativeProxy> first_view{publisher};
    ui::detail::SemanticProxyCache<FakeNativeProxy> second_view{publisher};
    const auto factory = [](ui::detail::SemanticSnapshotProxy source) {
        return std::make_shared<FakeNativeProxy>(std::move(source));
    };

    const auto twenty_a = first_view.virtual_item(7, 20, factory);
    const auto twenty_b = first_view.virtual_item(7, 20, factory);
    const auto ten = first_view.virtual_item(7, 10, factory);
    const auto twenty_other_view = second_view.virtual_item(7, 20, factory);

    T068_CHECK(twenty_a != nullptr);
    T068_CHECK(twenty_a.get() == twenty_b.get());
    T068_CHECK(ten != nullptr);
    T068_CHECK(ten.get() != twenty_a.get());
    T068_CHECK(twenty_other_view != nullptr);
    T068_CHECK(twenty_other_view.get() != twenty_a.get());
    T068_CHECK(first_view.tracked_identities() == 2);
    T068_CHECK(second_view.tracked_identities() == 1);

    T068_CHECK(twenty_a->source.read().has_value());
    T068_CHECK(twenty_a->source.read()->virtual_token() ==
               std::optional<ui::VirtualSemanticItemToken>{20});
}

void concurrent_lookup_returns_one_canonical_proxy() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    T068_CHECK(!publisher->publish(ordinary_snapshot()).empty());

    ui::detail::SemanticProxyCache<FakeNativeProxy> cache{publisher};
    std::atomic<int> factory_calls{0};
    std::array<std::shared_ptr<FakeNativeProxy>, 8> results;
    std::vector<std::thread> workers;
    workers.reserve(results.size());

    for (std::size_t index = 0; index < results.size(); ++index) {
        workers.emplace_back([&, index] {
            results[index] = cache.ordinary(
                42,
                [&](ui::detail::SemanticSnapshotProxy source) {
                    factory_calls.fetch_add(1, std::memory_order_relaxed);
                    return std::make_shared<FakeNativeProxy>(std::move(source));
                });
        });
    }
    for (auto& worker : workers) worker.join();

    T068_CHECK(results[0] != nullptr);
    for (const auto& result : results) {
        T068_CHECK(result.get() == results[0].get());
    }
    T068_CHECK(factory_calls.load(std::memory_order_relaxed) == 1);
}

} // namespace

int main() {
    try {
        ordinary_identity_is_stable_and_cache_values_are_weak();
        virtual_identity_uses_list_and_token_and_is_per_view();
        concurrent_lookup_returns_one_canonical_proxy();
        std::cout << "PASS t068 semantic proxy cache\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic proxy cache: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
