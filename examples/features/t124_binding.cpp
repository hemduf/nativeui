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

ui::StyleScopeOverrides scoped_surface(float r, float g, float b) {
    ui::StyleScopeOverrides value;
    value.palette.surface = ui::Color{r, g, b, 1.0f};
    return value;
}

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
    if (!first_tree.layout_dirty() || !first_tree.paint_dirty()) {
        return example::fail("Binding If update missed structural layout+paint invalidation");
    }
    if (second_tree.dirty()) {
        return example::fail("Binding If update leaked invalidation into an independent UI");
    }
    if (!first_renderer.render(first_tree)) {
        return example::fail("Binding If update render failed");
    }

    first.page.set(2);
    if (!first_tree.layout_dirty() || !first_tree.paint_dirty()) {
        return example::fail("Binding Switch update missed structural layout+paint invalidation");
    }
    if (!first_renderer.render(first_tree)) {
        return example::fail("Binding Switch update render failed");
    }

    auto reordered = first.items.get();
    std::rotate(reordered.begin(), reordered.begin() + 1, reordered.end());
    first.items.set(std::move(reordered));
    if (!first_tree.layout_dirty() || !first_tree.paint_dirty()) {
        return example::fail("Binding ForEach update missed structural layout+paint invalidation");
    }
    if (!first_renderer.render(first_tree)) {
        return example::fail("Binding ForEach update render failed");
    }

    first.focus_active.set(false);
    if (!first_tree.dirty()) {
        return example::fail("Binding FocusScope update did not invalidate its retained UI");
    }
    if (second_tree.dirty()) {
        return example::fail("Binding updates leaked invalidation into an independent UI");
    }
    if (!first_renderer.render(first_tree) || !second_renderer.render(second_tree)) {
        return example::fail("Binding retained-consumer update render failed");
    }
    return 0;
}

int verify_binding_style_scope_invalidation_and_isolation() {
    ui::State<ui::StyleScopeOverrides> first{scoped_surface(0.12f, 0.18f, 0.25f)};
    ui::State<ui::StyleScopeOverrides> second{scoped_surface(0.22f, 0.28f, 0.35f)};

    ui::UI first_tree{ui::StyleScope{first.binding(), ui::Button{"First", [] {}}}};
    ui::UI second_tree{ui::StyleScope{second.binding(), ui::Button{"Second", [] {}}}};
    ui::HeadlessRenderer first_renderer{{220.0f, 90.0f}, 1.0f};
    ui::HeadlessRenderer second_renderer{{220.0f, 90.0f}, 1.0f};

    if (!first_renderer.render(first_tree) || !second_renderer.render(second_tree)) {
        return example::fail("Binding StyleScope baseline render failed");
    }
    if (first_tree.dirty() || second_tree.dirty()) {
        return example::fail("Binding StyleScope baseline left a UI dirty");
    }

    auto paint_only = first.get();
    paint_only.palette.surface = ui::Color{0.36f, 0.16f, 0.10f, 1.0f};
    first.set(paint_only);
    if (first_tree.layout_dirty() || !first_tree.paint_dirty()) {
        return example::fail("Binding StyleScope paint-only update changed invalidation class");
    }
    if (second_tree.dirty()) {
        return example::fail("Binding StyleScope invalidation leaked into an independent UI");
    }
    if (!first_renderer.render(first_tree)) {
        return example::fail("Binding StyleScope paint-only render failed");
    }

    first.set(paint_only);
    if (first_tree.dirty()) {
        return example::fail("equal Binding StyleScope replacement invalidated the UI");
    }

    auto layout = first.get();
    layout.controls.control_height = 82.0f;
    first.set(layout);
    if (!first_tree.layout_dirty() || !first_tree.paint_dirty()) {
        return example::fail("Binding StyleScope layout update missed layout+paint invalidation");
    }
    if (second_tree.dirty()) {
        return example::fail("Binding StyleScope layout update leaked into an independent UI");
    }
    return 0;
}

int verify_state_and_binding_owner_teardown() {
    std::optional<ui::Spec> retained_legacy_root;
    std::optional<ui::Spec> retained_binding_scope;
    {
        ui::State<bool> visible{true};
        ui::State<int> page{1};
        ui::State<std::vector<Item>> items{{Item{1, "One"}, Item{2, "Two"}}};
        ui::State<bool> focus_active{true};
        ui::State<ui::StyleScopeOverrides> legacy_scope{scoped_surface(0.18f, 0.24f, 0.30f)};
        ui::State<ui::StyleScopeOverrides> binding_scope{scoped_surface(0.30f, 0.24f, 0.18f)};

        retained_legacy_root.emplace(
            ui::Column{
                ui::If{visible, ui::Label{"Legacy If"}},
                ui::Switch<int>{page}
                    .when(1, ui::Label{"Legacy Switch"})
                    .otherwise(ui::Label{"Legacy fallback"}),
                ui::ForEach<Item>{
                    items,
                    [](const Item& item) { return item.key; },
                    [](const Item& item) { return ui::Label{item.label}; }},
                ui::FocusScope{focus_active, ui::Button{"Legacy FocusScope", [] {}}},
                ui::StyleScope{legacy_scope, ui::Button{"Legacy StyleScope", [] {}}}
            }.gap(6.0f).spec());

        retained_binding_scope.emplace(
            ui::StyleScope{
                binding_scope.binding(),
                ui::Button{"Binding StyleScope", [] {}}}.spec());
    }

    ui::UI legacy_tree{SpecRoot{std::move(*retained_legacy_root)}};
    ui::HeadlessRenderer legacy_renderer{{420.0f, 260.0f}, 1.0f};
    if (!legacy_renderer.render(legacy_tree)) {
        return example::fail("legacy State syntax retained dangling owner state");
    }

    ui::UI binding_tree{SpecRoot{std::move(*retained_binding_scope)}};
    ui::HeadlessRenderer binding_renderer{{220.0f, 90.0f}, 1.0f};
    if (!binding_renderer.render(binding_tree)) {
        return example::fail("Binding StyleScope retained dangling owner state");
    }
    return 0;
}

int self_test() {
    if (const int result = verify_binding_invalidation_and_isolation(); result != 0) return result;
    if (const int result = verify_binding_style_scope_invalidation_and_isolation(); result != 0) return result;
    if (const int result = verify_state_and_binding_owner_teardown(); result != 0) return result;
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    Model model;
    auto tree = make_binding_ui(model);
    return example::run_window(tree, "NativeUI T124 Binding", {560.0f, 320.0f});
}
