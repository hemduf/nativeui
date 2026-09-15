#include "example_support.hpp"

namespace {

struct DemoState {
    ui::State<std::string> notes{"alpha"};
};

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T140 — Binding-backed TextArea"},
            ui::Label{
                "TextArea uses Binding<std::string> internally while State<std::string>& stays source compatible."
            }.size(12.0f),
            ui::TextArea{"Notes", state.notes.binding()}
                .placeholder("Type notes")
                .max_length(256),
        }.gap(12.0f).padding(16.0f)};
}

ui::InputEvent text(std::string value) {
    ui::InputEvent event{};
    event.type = ui::InputType::TextInput;
    event.text = std::move(value);
    return event;
}

int self_test() {
    DemoState left;
    DemoState right;

    int left_notifications = 0;
    int right_notifications = 0;
    bool invalidation_visible_at_publication = false;
    auto left_binding = left.notes.binding();
    auto right_binding = right.notes.binding();

    auto left_tree = make_ui(left);
    auto right_tree = make_ui(right);
    example::Platform left_platform;
    example::Platform right_platform;
    left_tree.resize({620.0f, 320.0f});
    right_tree.resize({620.0f, 320.0f});
    left_tree.activate(left_platform);
    right_tree.activate(right_platform);

    ui::HeadlessRenderer left_renderer{{620.0f, 320.0f}, 1.0f};
    ui::HeadlessRenderer right_renderer{{620.0f, 320.0f}, 1.0f};
    if (!left_renderer.render(left_tree)) return example::fail("initial left TextArea render failed");
    if (!right_renderer.render(right_tree)) return example::fail("initial right TextArea render failed");
    if (left_tree.layout_dirty() || left_tree.paint_dirty() ||
        right_tree.layout_dirty() || right_tree.paint_dirty()) {
        return example::fail("initial TextArea render did not settle invalidation state");
    }

    auto left_subscription = left_binding.observe(
        [&left_notifications, &left_tree, &invalidation_visible_at_publication](const std::string&) {
            ++left_notifications;
            if (left_notifications == 1) {
                invalidation_visible_at_publication =
                    left_tree.paint_dirty() && !left_tree.layout_dirty();
            }
        });
    auto right_subscription = right_binding.observe(
        [&right_notifications](const std::string&) { ++right_notifications; });

    left_tree.dispatch(text("!"), left_platform);
    if (left.notes.get() != "alpha!" || right.notes.get() != "alpha") {
        return example::fail("Binding TextArea edit crossed independent State sources");
    }
    if (left_notifications != 1 || right_notifications != 0) {
        return example::fail("Binding TextArea edit notification count/isolation failed");
    }
    if (!invalidation_visible_at_publication) {
        return example::fail("TextArea published Binding before completing paint invalidation");
    }

    if (!left_renderer.render(left_tree)) return example::fail("edited TextArea render failed");
    if (left_tree.layout_dirty() || left_tree.paint_dirty()) {
        return example::fail("TextArea render did not settle invalidation state");
    }
    if (right_tree.layout_dirty() || right_tree.paint_dirty()) {
        return example::fail("independent TextArea became dirty before external mutation");
    }

    left_binding.set("external");
    if (left.notes.get() != "external" || left_notifications != 2 || right_notifications != 0) {
        return example::fail("external Binding TextArea update notification/isolation failed");
    }
    if (!left_tree.paint_dirty() || left_tree.layout_dirty()) {
        return example::fail("external TextArea Binding update broadened beyond paint invalidation");
    }
    if (right_tree.paint_dirty() || right_tree.layout_dirty()) {
        return example::fail("external TextArea Binding update dirtied independent UI");
    }

    ui::State<std::string> legacy_notes{"legacy"};
    [[maybe_unused]] auto legacy_text_area = ui::TextArea{"Legacy", legacy_notes}.spec();

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T140 Binding TextArea", {620.0f, 320.0f});
}