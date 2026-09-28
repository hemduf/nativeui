#include "example_support.hpp"

#include <string>

namespace {

struct DemoState {
    ui::State<float> amount{0.5f};
    ui::State<bool> enabled{false};
    ui::State<std::string> name{"NativeUI"};
};

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T139 — Binding-backed widgets"},
            ui::Label{
                "Binding<T> is a first-class widget input while State<T>& remains source compatible."
            }.size(12.0f),
            ui::Knob{"Amount", state.amount.binding()}.range(0.0f, 1.0f),
            ui::Toggle{"Enabled", state.enabled.binding()},
            ui::TextInput{"Name", state.name.binding()}.placeholder("Name"),
            ui::Toggle{"Legacy State<T>& overload", state.enabled},
        }.gap(12.0f).padding(16.0f)};
}

int self_test() {
    DemoState state;
    auto tree = make_ui(state);
    tree.resize({560.0f, 360.0f});

    ui::HeadlessRenderer renderer{{560.0f, 360.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("initial Binding widget render failed");

    auto amount = state.amount.binding();
    auto enabled = state.enabled.binding();
    auto name = state.name.binding();
    amount.set(0.75f);
    enabled.set(true);
    name.set("Bound");

    if (state.amount.get() != 0.75f) return example::fail("Binding<float> write did not reach State");
    if (!state.enabled.get()) return example::fail("Binding<bool> write did not reach State");
    if (state.name.get() != "Bound") return example::fail("Binding<string> write did not reach State");
    if (!renderer.render(tree)) return example::fail("Binding widget rerender failed");

    ui::State<float> legacy_amount{0.25f};
    ui::State<bool> legacy_enabled{false};
    ui::State<std::string> legacy_name{"legacy"};
    [[maybe_unused]] auto legacy_knob = ui::Knob{"Legacy knob", legacy_amount}.spec();
    [[maybe_unused]] auto legacy_toggle = ui::Toggle{"Legacy toggle", legacy_enabled}.spec();
    [[maybe_unused]] auto legacy_text = ui::TextInput{"Legacy text", legacy_name}.spec();

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T139 Binding-backed widgets", {560.0f, 360.0f});
}
