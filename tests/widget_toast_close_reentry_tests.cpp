#include "test_support.hpp"
#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/toast.hpp>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
void frame(ui::UI &tree) {
    tree.resize({320.0f, 240.0f});
    ui::HeadlessRenderer renderer{{320.0f, 240.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
}
bool focused(ui::UI &tree, std::string_view name) {
    for (ui::NodeId id = 1; id < 500; ++id) {
        const auto info = tree.component_semantics(id);
        if (info && info->name == name && info->focused)
            return true;
    }
    return false;
}
void reentrant_show_during_empty_stack_close_is_coherent() {
    auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
    ui::detail::DispatcherOwner owner{{}, clock};
    test::MockPlatform platform;
    platform.dispatcher_value = owner.dispatcher();
    ui::UI tree{ui::Button{"Root", [] {}}};
    frame(tree);
    tree.activate(platform);
    ui::Toast messages{tree, owner.dispatcher()};
    int first_calls = 0, second_calls = 0, closing = 0;
    ui::ToastShowResult replacement;
    ui::ToastHandle second_handle;
    auto first = messages.show({.message = "First", .action_label = "First action", .action = [&] {
                                    ++first_calls;
                                    NUI_CHECK(!second_handle.valid());
                                    // The second accepted action is still pending in the
                                    // controller. A normal frame therefore preserves the retained
                                    // stack participant while completing the accumulated
                                    // paint/layout notification.
                                    frame(tree);
                                    NUI_CHECK(tree.overlay_entries().size() == 1);
                                    tree.set_invalidation_callback([&](ui::Rect) {
                                        if (closing)
                                            return;
                                        ++closing;
                                        replacement = messages.show({.message = "During close"});
                                    });
                                }});
    auto second = messages.show(
        {.message = "Second", .action_label = "Second action", .action = [&] { ++second_calls; }});
    second_handle = second.handle;
    frame(tree);
    tree.dispatch(test::key(ui::Key::Tab), platform);
    NUI_CHECK(focused(tree, "First action"));
    frame(tree);
    bool armed = true;
    tree.set_invalidation_callback([&](ui::Rect) {
        if (first.handle.valid() || !std::exchange(armed, false))
            return;
        throw std::runtime_error("first action retirement");
    });
    // Keep the first accepted action unstarted so activation of the second
    // naturally drains both callbacks in order through the public controller.
    ui::detail::DispatcherTestAccess::fail_next_post(owner.dispatcher());
    bool caught = false;
    try {
        tree.dispatch(test::key(ui::Key::Enter), platform);
    } catch (const std::runtime_error &error) {
        NUI_CHECK(std::string_view{error.what()} == "first action retirement");
        caught = true;
    }
    NUI_CHECK(caught && !armed && !first.handle.valid() && second_handle.valid());
    NUI_CHECK(first_calls == 0 && second_calls == 0);
    tree.clear_invalidation_callback();
    frame(tree);
    // No dispatcher checkpoint has begun the recovery callback.
    for (int pass = 0; pass < 3 && !focused(tree, "Second action"); ++pass)
        tree.dispatch(test::key(ui::Key::Tab), platform);
    NUI_CHECK(focused(tree, "Second action"));
    frame(tree);
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(first_calls == 1 && second_calls == 1 && closing == 1);
    tree.clear_invalidation_callback();
    // Reentrant close may reject publication, or accept a still-owned message;
    // it must never report Shown for a handle erased by the outer transaction.
    if (replacement.status == ui::ToastShowStatus::Shown) {
        NUI_CHECK(replacement.handle.valid());
        frame(tree);
        NUI_CHECK(replacement.handle.valid());
        NUI_CHECK(messages.dismiss(replacement.handle));
    } else {
        NUI_CHECK(replacement.status == ui::ToastShowStatus::Unavailable);
        NUI_CHECK(!replacement.handle.valid() && owner.active_timer_count() == 0);
    }
    (void)owner.checkpoint();
    NUI_CHECK(first_calls == 1 && second_calls == 1);
    frame(tree);
    auto recovery = messages.show({.message = "Recovery"});
    NUI_CHECK(recovery.handle.valid());
    NUI_CHECK(messages.dismiss(recovery.handle));
}
} // namespace
int main() {
    return test::run("toast_close_reentry", &reentrant_show_during_empty_stack_close_is_coherent);
}
