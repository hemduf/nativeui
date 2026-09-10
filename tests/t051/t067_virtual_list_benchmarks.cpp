#include <nativeui/detail/virtual_list_model.hpp>
#include <nativeui/detail/virtual_list_window.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr float kRowHeight = 20.0f;
constexpr float kViewportHeight = 400.0f;
constexpr std::size_t kOverscan = 2;
constexpr std::size_t kIterations = 1000;
constexpr std::size_t kMaximumMaterialized = 25;

struct BenchmarkResult {
    std::size_t item_count{};
    std::int64_t elapsed_ns{};
    std::size_t factory_calls{};
    std::size_t max_materialized{};
    std::size_t metadata_rebuilds{};
};

bool run_case(std::size_t item_count, BenchmarkResult& result) {
    using Model = ui::detail::VirtualListDatasetModel<int>;
    using Window = ui::detail::VirtualListMaterializationWindow<int, int>;

    if (item_count <= 20) return false;

    std::vector<Model::Item> items;
    items.reserve(item_count);
    for (std::size_t index = 0; index < item_count; ++index) {
        items.emplace_back(static_cast<int>(index), "row " + std::to_string(index));
    }

    Model model;
    if (!model.replace(std::move(items))) return false;

    const auto metadata_before = model.metadata_snapshot();
    const auto generation_before = model.generation();

    std::size_t factory_calls = 0;
    Window window{model, kRowHeight, [&](const Model::Item& item) {
        ++factory_calls;
        return item.key;
    }};

    if (!window.update(0.0f, kViewportHeight, kOverscan)) return false;
    std::size_t max_materialized = window.items().size();

    const std::size_t scrollable_rows = item_count - 20;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < kIterations; ++iteration) {
        const std::size_t row_index = (iteration * 97u) % scrollable_rows;
        const float scroll_y = static_cast<float>(row_index) * kRowHeight + 5.0f;
        if (!window.update(scroll_y, kViewportHeight, kOverscan)) return false;
        max_materialized = std::max(max_materialized, window.items().size());
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start);

    const auto metadata_after = model.metadata_snapshot();
    const auto generation_after = model.generation();
    const std::size_t metadata_rebuilds =
        (metadata_after.get() == metadata_before.get() && generation_after == generation_before)
            ? 0u
            : 1u;

    result = BenchmarkResult{
        item_count,
        elapsed.count(),
        factory_calls,
        max_materialized,
        metadata_rebuilds,
    };

    return max_materialized <= kMaximumMaterialized && metadata_rebuilds == 0;
}

} // namespace

int main() {
    const std::size_t sizes[] = {1000u, 10000u, 100000u};
    bool ok = true;

    for (const auto item_count : sizes) {
        BenchmarkResult result;
        const bool case_ok = run_case(item_count, result);
        ok = case_ok && ok;
        std::cout << "t067_virtual_list"
                  << " items=" << result.item_count
                  << " iterations=" << kIterations
                  << " elapsed_ns=" << result.elapsed_ns
                  << " factory_calls=" << result.factory_calls
                  << " max_materialized=" << result.max_materialized
                  << " metadata_rebuilds=" << result.metadata_rebuilds << '\n';
    }

    return ok ? 0 : 1;
}
