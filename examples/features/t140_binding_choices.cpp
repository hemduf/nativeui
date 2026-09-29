#include "example_support.hpp"

namespace {

struct DemoState {
    ui::State<bool> checked{false};
    ui::State<int> choice{1};
};

ui::UI make_ui(DemoState& state) {
    auto group = ui::RadioGroup<int>{state.choice.binding()};
    return ui::UI{
        ui::Column{
            ui::Header{"T140 — Binding-backed choice widgets"},
            ui::Label{
                "Checkbox and RadioGroup use Binding<T> internally while State<T>& stays source compatible."
            }.size(12.0f),
            ui::Checkbox{state.checked.binding(), "Binding checkbox"},
            ui::RadioButton{group, 1, "One"},
            ui::RadioButton{group, 2, "Two"},
            ui::Checkbox{state.checked, "Legacy State<T>& overload"},
        }.gap(12.0f).padding(16.0f)};
}

int self_test() {
    DemoState state;
    auto tree = make_ui(state);
    tree.resize({600.0f, 360.0f});

    ui::HeadlessRenderer renderer{{600.0f, 360.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("initial choice Binding render failed");

    auto checked = state.checked.binding();
    auto choice = state.choice.binding();
    checked.set(true);
    choice.set(2);
    if (!state.checked.get()) return example::fail("Checkbox Binding write did not reach State");
    if (state.choice.get() != 2) return example::fail("Radio Binding write did not reach State");
    if (!renderer.render(tree)) return example::fail("choice Binding rerender failed");

    ui::State<bool> left{false};
    ui::State<bool> right{false};
    int left_notifications = 0;
    int right_notifications = 0;
    auto left_binding = left.binding();
    auto right_binding = right.binding();
    auto left_subscription = left_binding.observe(
        [&left_notifications](const bool&) { ++left_notifications; });
    auto right_subscription = right_binding.observe(
        [&right_notifications](const bool&) { ++right_notifications; });
    left_binding.set(true);
    if (!left.get() || right.get()) return example::fail("independent Binding values crossed");
    if (left_notifications != 1 || right_notifications != 0) {
        return example::fail("independent Binding notifications crossed");
    }

    ui::State<bool> legacy_checked{false};
    ui::State<int> legacy_choice{1};
    [[maybe_unused]] auto legacy_checkbox = ui::Checkbox{legacy_checked, "Legacy"}.spec();
    auto legacy_group = ui::RadioGroup<int>{legacy_choice};
    [[maybe_unused]] auto legacy_radio = ui::RadioButton{legacy_group, 1, "Legacy one"}.spec();

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T140 Binding choice widgets", {600.0f, 360.0f});
}
