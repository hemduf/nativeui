#include "test_support.hpp"

#include <nativeui/nativeui.hpp>

#include <atomic>
#include <cstddef>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

void suite() {
    using VirtualState = ui::VirtualListState<int>;

    ui::State<std::optional<int>> selected{std::nullopt};
    std::size_t row_factory_calls = 0;
    VirtualState state{
        selected,
        20.0f,
        [&row_factory_calls](const VirtualState::Item&) {
            ++row_factory_calls;
            return ui::Spacer{320.0f, 20.0f};
        }};

    std::vector<VirtualState::Item> items;
    items.reserve(100000);
    for (int index = 0; index < 100000; ++index) {
        items.emplace_back(index, "row " + std::to_string(index));
    }
    NUI_CHECK(state.replace(items));
    NUI_CHECK(row_factory_calls == 0);

    const auto generation = state.dataset_generation();
    const auto old_metadata = state.metadata_snapshot();
    NUI_CHECK(old_metadata);
    NUI_CHECK(old_metadata->size() == 100000);

    const ui::Rect list_bounds{10.0f, 20.0f, 320.0f, 400.0f};
    const auto projection = state.semantic_children(list_bounds);
    NUI_CHECK(projection.dataset_generation() == generation);
    NUI_CHECK(projection.metadata_snapshot().get() == old_metadata.get());

    const auto offscreen = projection.item_at(99999);
    NUI_CHECK(offscreen.has_value());
    NUI_CHECK(offscreen->info.name == "row 99999");
    NUI_CHECK_NEAR(offscreen->logical_bounds.x, 10.0f, 0.0001f);
    NUI_CHECK_NEAR(offscreen->logical_bounds.y, 20.0f + 99999.0f * 20.0f, 0.01f);
    NUI_CHECK_NEAR(offscreen->logical_bounds.w, 320.0f, 0.0001f);
    NUI_CHECK_NEAR(offscreen->logical_bounds.h, 20.0f, 0.0001f);
    NUI_CHECK(row_factory_calls == 0);

    // Selection changes only the small scalar semantic projection. They must
    // never rebuild/copy the O(N) immutable metadata snapshot.
    selected.set(99999);
    for (int iteration = 0; iteration < 1000; ++iteration) {
        const auto view = state.semantic_children(list_bounds);
        NUI_CHECK(view.dataset_generation() == generation);
        NUI_CHECK(view.metadata_snapshot().get() == old_metadata.get());
    }
    const auto selected_item = state.semantic_children(list_bounds).item_at(99999);
    NUI_CHECK(selected_item.has_value());
    NUI_CHECK(selected_item->info.selected);
    NUI_CHECK(row_factory_calls == 0);

    // A real dataset metadata change publishes exactly one new immutable
    // generation. A native-side reader may retain the old shared snapshot
    // concurrently while the UI thread publishes the replacement generation.
    std::atomic<bool> reader_started{false};
    std::atomic<bool> release_reader{false};
    std::atomic<bool> reader_ok{true};
    std::thread reader{
        [snapshot = old_metadata, &reader_started, &release_reader, &reader_ok] {
            if (!snapshot || snapshot->empty() || snapshot->back().name != "row 99999") {
                reader_ok.store(false, std::memory_order_relaxed);
            }
            reader_started.store(true, std::memory_order_release);
            while (!release_reader.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            if (!snapshot || snapshot->empty() || snapshot->back().name != "row 99999") {
                reader_ok.store(false, std::memory_order_relaxed);
            }
        }};

    while (!reader_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    items.back().name = "updated row";
    NUI_CHECK(state.replace(items));
    NUI_CHECK(state.dataset_generation() == generation + 1);
    const auto new_metadata = state.metadata_snapshot();
    NUI_CHECK(new_metadata);
    NUI_CHECK(new_metadata.get() != old_metadata.get());
    NUI_CHECK(new_metadata->size() == old_metadata->size());
    NUI_CHECK(old_metadata->back().name == "row 99999");
    NUI_CHECK(new_metadata->back().name == "updated row");
    NUI_CHECK(row_factory_calls == 0);

    release_reader.store(true, std::memory_order_release);
    reader.join();
    NUI_CHECK(reader_ok.load(std::memory_order_relaxed));
}

} // namespace

int main() { return test::run("t067_semantic_api", &suite); }
