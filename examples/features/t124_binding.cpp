#include "example_support.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Item {
    int key{};
    std::string label;

    bool operator==(const Item&) const = default;
};

struct Model {
    ui::State<bool> visible{true};
    ui::State<int> page{1};
    ui::State<std::vector<Item>> items{{
        Item{1, "Alpha"},
        Item{2, "Beta"},
        Item{3, "Gamma"},
    }};
    ui::State<bool> focus_active{true};
};

struct SpecRoot {
    ui::Spec value;
    ui::Spec spec() && { return std::move(value); }
};

ui::UI make_binding_ui(Model& model) {
    return ui::UI{
        ui::Column{
            ui::Header{"T124 — Binding retained consumers"},
            ui::If{
                model.visible.binding(),
                ui::Label{"Binding-driven conditional content"}},
            ui::Switch<int>{model.page.binding()}
                .when(1, ui::Label{"Binding switch: page one"})
                .when(2, ui::Label{"Binding switch: page two"})
                .otherwise(ui::Label{"Binding switch: fallback"}),
            ui::ForEach<Item>{
                model.items.binding(),
                [](const Item& item) { return item.key; },
                [](const Item& item) { return ui::Label{item.label}; }},
            ui::FocusScope{
                model.focus_active.binding(),
                ui::Button{"Binding focus scope", [] {}}}
                .trap(false)
        }.gap(8.0f)};
}

int verify_binding_invalidation_and_isolation() {
    Model first;
    Model second;
    auto first_tree = make_binding_ui(first);
    auto second_tree = make_binding_ui(second);
    ui::HeadlessRenderer first_renderer{{520.0f, 280.0f}, 1.0f};
    ui::HeadlessRenderer second_renderer{{520.0f, 280.0f}, 1.0f};

    if (!first_renderer.render(first_tree) || !second_renderer.render(second_tree)) {
        return example::fail("Binding retained-consumer baseline render failed");
    }
    if (first_tree.dirty() || second_tree.dirty()) {
        return example::fail("baseline Binding renders left a UI dirty");
    }

    first.visible.set(false);
    first.page.set(2);
    auto reordered = first.items.get();
    std::rotate(reordered.begin(), reordered.begin() + 1, reordered.end());
    first.items.set(std::move(reordered));
    first.focus_active.set(false);

    if (!first_tree.dirty()) {
        return example::fail("Binding updates did not invalidate their retained UI");
    }
    if (second_tree.dirty()) {
        return example::fail("Binding updates leaked invalidation into an independent UI");
    }
    if (!first_renderer.render(first_tree) || !second_renderer.render(second_tree)) {
        return example::fail("Binding retained-consumer update render failed");
    }
    return 0;
}

int verify_legacy_state_teardown() {
    std::optional<ui::Spec> retained_root;
    {
        ui::State<bool> visible{true};
        ui::State<int> page{1};
        ui::State<std::vector<Item>> items{{Item{1, "One"}, Item{2, "Two"}}};
        ui::State<bool> focus_active{true};

        retained_root.emplace(
            ui::Column{
                ui::If{visible, ui::Label{"Legacy If"}},
                ui::Switch<int>{page}
                    .when(1, ui::Label{"Legacy Switch"})
                    .otherwise(ui::Label{"Legacy fallback"}),
                ui::ForEach<Item>{
                    items,
                    [](const Item& item) { return item.key; },
                    [](const Item& item) { return ui::Label{item.label}; }},
                ui::FocusScope{focus_active, ui::Button{"Legacy FocusScope", [] {}}}
            }.gap(6.0f).spec());
    }

    ui::UI tree{SpecRoot{std::move(*retained_root)}};
    ui::HeadlessRenderer renderer{{420.0f, 220.0f}, 1.0f};
    if (!renderer.render(tree)) {
        return example::fail("legacy State syntax retained dangling owner state");
    }
    return 0;
}

int self_test() {
    if (const int result = verify_binding_invalidation_and_isolation(); result != 0) return result;
    if (const int result = verify_legacy_state_teardown(); result != 0) return result;
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    Model model;
    auto tree = make_binding_ui(model);
    return example::run_window(tree, "NativeUI T124 Binding", {560.0f, 320.0f});
}
