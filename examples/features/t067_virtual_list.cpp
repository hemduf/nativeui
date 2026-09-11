#include "example_support.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using VirtualState = ui::VirtualListState<int>;

ui::Canvas row_view(const VirtualState::Item& item) {
    return ui::Canvas{
        {480.0f, 28.0f},
        [label = item.name, enabled = item.enabled](ui::CanvasContext2D& context) {
            context.text(
                {14.0f, context.height() * 0.5f},
                label,
                12.0f,
                enabled ? ui::colors::text : ui::Color{0.39f, 0.42f, 0.47f, 1.0f},
                ui::TextAlign::Left);
        }};
}

std::vector<VirtualState::Item> make_items(std::size_t count) {
    std::vector<VirtualState::Item> items;
    items.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        items.emplace_back(
            static_cast<int>(index),
            "Preset " + std::to_string(index + 1),
            index % 37 != 0);
    }
    return items;
}

struct DemoState {
    ui::State<std::optional<int>> selection{std::nullopt};
    int last_activated{-1};
    VirtualState list;

    explicit DemoState(
        std::size_t item_count = 100000,
        std::size_t* row_factory_calls = nullptr)
        : list(
              selection,
              28.0f,
              [row_factory_calls](const VirtualState::Item& item) {
                  if (row_factory_calls) ++*row_factory_calls;
                  return row_view(item);
              }) {
        (void)list.replace(make_items(item_count));
    }
};

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Label{"T067 — Virtualized ListView"}.size(22.0f).bold(),
            ui::Label{
                "100,000 logical items · fixed 28 px rows · 2-row overscan · immutable semantics"
            }.size(12.0f).color(ui::colors::textMuted),
            ui::Flex{
                std::move(ui::ListView<int>{state.list})
                    .on_activate([&state](const int& key) { state.last_activated = key; })
            }.grow(1.0f).shrink(1.0f),
            ui::Label{"↑ ↓ navigate  ·  Home / End  ·  Enter activates  ·  wheel scrolls"}
                .size(11.0f)
                .color(ui::colors::textMuted)
        }.gap(12.0f).padding(20.0f)
    };
}

int self_test() {
    example::Platform platform;
    ui::State<std::optional<int>> selection{std::nullopt};
    std::size_t row_factory_calls = 0;
    VirtualState state{
        selection,
        20.0f,
        [&row_factory_calls](const VirtualState::Item&) {
            ++row_factory_calls;
            return ui::Spacer{320.0f, 20.0f};
        }};

    if (!state.replace(make_items(100000))) {
        return example::fail("100k dataset replacement failed");
    }
    if (row_factory_calls != 0) {
        return example::fail("dataset replacement materialized visual rows");
    }

    const auto metadata = state.metadata_snapshot();
    const auto generation = state.dataset_generation();
    if (!metadata || metadata->size() != 100000) {
        return example::fail("100k immutable semantic metadata snapshot missing");
    }

    ui::UI tree{ui::ListView<int>{state}};
    tree.resize({320.0f, 200.0f});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{320.0f, 200.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("initial virtual-list render failed");

    // 10 visible rows + default 2-row overscan on the trailing side at offset 0.
    if (row_factory_calls > 12) {
        return example::fail("initial materialization exceeded viewport + overscan bound");
    }

    const auto before_semantic_query = row_factory_calls;
    const auto semantic_item = state.semantic_children({0.0f, 0.0f, 320.0f, 200.0f})
                                   .item_at(99999);
    if (!semantic_item || row_factory_calls != before_semantic_query) {
        return example::fail("offscreen semantic query materialized a visual row");
    }

    if (tree.dispatch(example::key(ui::Key::End), platform) != ui::EventResult::Handled ||
        !selection.get() || *selection.get() != 99999) {
        return example::fail("End did not select the last logical item");
    }
    if (!renderer.render(tree)) return example::fail("last-row render failed");
    if (state.offset().y <= 0.0f) return example::fail("last-row navigation did not scroll");

    if (state.metadata_snapshot().get() != metadata.get() ||
        state.dataset_generation() != generation) {
        return example::fail("scroll/selection rebuilt immutable semantic metadata");
    }

    if (tree.dispatch(example::key(ui::Key::Enter), platform) != ui::EventResult::Handled) {
        return example::fail("virtual-list activation was not handled");
    }

    std::size_t startup_row_factory_calls = 0;
    DemoState startup_state{1000, &startup_row_factory_calls};
    auto startup_tree = make_ui(startup_state);
    startup_tree.resize({560.0f, 620.0f});
    startup_tree.activate(platform);
    ui::HeadlessRenderer startup_renderer{{560.0f, 620.0f}, 1.0f};
    if (!startup_renderer.render(startup_tree)) {
        return example::fail("example startup-layout render failed");
    }
    if (startup_row_factory_calls > 32) {
        return example::fail("example startup materialization exceeded viewport bound");
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T067 Virtualized ListView", {560.0f, 620.0f});
}
