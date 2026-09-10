#include "example_support.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

struct Item {
    int key{};
    std::string label;

    bool operator==(const Item&) const = default;
};

struct DemoState {
    ui::State<bool> details{true};
    ui::State<int> page{1};
    ui::State<std::vector<Item>> items{{
        Item{1, "Alpha"},
        Item{2, "Beta"},
        Item{3, "Gamma"},
    }};
};

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T058 — dynamic composition"},
            ui::Row{
                ui::Button{"Toggle details", [&state] { state.details.set(!state.details.get()); }},
                ui::Button{"Next page", [&state] {
                    state.page.set(state.page.get() == 1 ? 2 : 1);
                }},
                ui::Button{"Rotate keys", [&state] {
                    auto next = state.items.get();
                    if (!next.empty()) std::rotate(next.begin(), next.begin() + 1, next.end());
                    state.items.set(std::move(next));
                }}
            }.gap(10.0f),
            ui::If{
                state.details,
                ui::Label{"Conditional content is reconciled at a safe top-level checkpoint."}
                    .size(13.0f)
            },
            ui::Switch<int>{state.page}
                .when(1, ui::Label{"Switch branch: page one"})
                .when(2, ui::Label{"Switch branch: page two"})
                .otherwise(ui::Label{"Switch fallback"}),
            ui::ForEach<Item>{
                state.items,
                [](const Item& item) { return item.key; },
                [](const Item& item) { return ui::Label{item.label}; }
            },
            ui::Label{
                "ForEach keeps retained identity by key across reorder; changing a key replaces that item."
            }.size(12.0f).color(ui::colors::textMuted)
        }.gap(12.0f)
    };
}

int self_test() {
    DemoState state;
    auto tree = make_ui(state);
    ui::HeadlessRenderer renderer{{760.0f, 360.0f}, 1.0f};

    if (!renderer.render(tree)) return example::fail("initial dynamic render failed");

    state.details.set(false);
    if (!renderer.render(tree)) return example::fail("conditional removal render failed");

    state.page.set(2);
    if (!renderer.render(tree)) return example::fail("switch branch render failed");

    auto reordered = state.items.get();
    std::rotate(reordered.begin(), reordered.begin() + 1, reordered.end());
    state.items.set(std::move(reordered));
    if (!renderer.render(tree)) return example::fail("keyed reorder render failed");

    // Duplicate-key snapshots are rejected atomically; the retained valid tree
    // must remain renderable and a later valid snapshot must recover normally.
    state.items.set({Item{1, "duplicate-a"}, Item{1, "duplicate-b"}});
    if (!renderer.render(tree)) return example::fail("duplicate-key rejection render failed");

    state.items.set({Item{4, "Delta"}, Item{2, "Beta"}});
    if (!renderer.render(tree)) return example::fail("keyed recovery render failed");

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T058 Dynamic Composition", {760.0f, 360.0f});
}
