#include "example_support.hpp"
#include <nativeui/table_view.hpp>
#include <algorithm>

namespace {
int self_test() {
    ui::State<std::vector<ui::CollectionItem<int>>> rows{{{1, "Ada"}, {2, "Lin"}}};
    ui::State<ui::SelectionSnapshot<int>> chosen{{}};
    ui::Selection<int> selection{chosen};
    ui::State<ui::TableLayout> layout{{}};
    ui::State<std::optional<ui::SortOrder>> sort{std::nullopt};
    int requests = 0;
    ui::UI tree{ui::TableView<int>{rows, selection}
                    .columns({{"name", "Name", 100, 60, {}, ui::Align::Start, true, false},
                              {"id", "ID", 80}})
                    .layout(layout)
                    .sort(sort)
                    .on_sort_request([&](const auto &) { ++requests; })};
    example::Platform platform;
    tree.resize({180, 120});
    tree.activate(platform);
    tree.dispatch(example::key(ui::Key::Enter), platform);
    tree.dispatch(example::key(ui::Key::Enter), platform);
    if (!sort.get() || sort.get()->column != "name" ||
        sort.get()->direction != ui::SortDirection::Descending || requests != 2)
        return example::fail("table sort request failed");
    ui::HeadlessRenderer renderer{{180, 120}, 1};
    return renderer.render(tree) ? 0 : example::fail("table render failed");
}
} // namespace
int main(int argc, char **argv) {
    if (example::self_test_requested(argc, argv))
        return self_test();
    ui::State<std::vector<ui::CollectionItem<int>>> rows{
        {{1, "Ada"}, {2, "Lin"}, {3, "Edsger"}, {4, "Grace"}}};
    ui::State<ui::SelectionSnapshot<int>> chosen{{}};
    ui::Selection<int> selection{chosen};
    ui::State<ui::TableLayout> layout{{}};
    ui::State<std::optional<ui::SortOrder>> sort{std::nullopt};
    ui::UI tree{ui::TableView<int>{rows, selection}
                    .columns({{"name", "Name", 220, 80, {}, ui::Align::Start, true, false},
                              {"id", "ID", 100}})
                    .layout(layout)
                    .sort(sort)
                    .cell([](const auto &row, const auto &column) {
                        return ui::make_spec(
                            ui::Label{column == "id" ? std::to_string(row.key) : row.label});
                    })
                    .on_sort_request([&](const auto &order) {
                        auto values = rows.snapshot();
                        std::sort(values.begin(), values.end(), [&](const auto &a, const auto &b) {
                            return order.direction == ui::SortDirection::Ascending
                                       ? a.label < b.label
                                       : a.label > b.label;
                        });
                        rows.set(std::move(values));
                    })};
    return example::run_window(tree, "NativeUI TableView", {500, 350});
}
