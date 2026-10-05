#include "test_support.hpp"
#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/toast.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
struct Clock {
    std::shared_ptr<ui::detail::ManualDispatcherClock> clock{
        std::make_shared<ui::detail::ManualDispatcherClock>()};
    ui::detail::DispatcherOwner owner{{}, clock};
    test::MockPlatform platform;
    Clock() { platform.dispatcher_value = owner.dispatcher(); }
};
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
void accepted_action_survives_post_refusal_then_normal_frame() {
    Clock time;
    int actions = 0;
    ui::UI tree{ui::Button{"Root", [] {}}};
    frame(tree);
    tree.activate(time.platform);
    ui::Toast messages{tree, time.owner.dispatcher()};
    auto pending = messages.show(
        {.message = "Accepted action", .action_label = "Run", .action = [&] { ++actions; }});
    frame(tree);
    tree.dispatch(test::key(ui::Key::Tab), time.platform);
    NUI_CHECK(focused(tree, "Run"));
    frame(tree); // Clean focus styling so the next invalidation reaches the injected boundary.
    NUI_CHECK(focused(tree, "Run"));
    bool armed = true;
    tree.set_invalidation_callback([&](ui::Rect) {
        if (pending.handle.valid() || !std::exchange(armed, false))
            return;
        throw std::runtime_error("after action retirement");
    });
    // First recovery post throws before enqueue; the accepted action remains
    // owned, and no synchronous callback-stack fallback is permitted.
    ui::detail::DispatcherTestAccess::fail_next_post(time.owner.dispatcher());
    bool caught = false;
    try {
        tree.dispatch(test::key(ui::Key::Enter), time.platform);
    } catch (const std::runtime_error &error) {
        NUI_CHECK(std::string_view{error.what()} == "after action retirement");
        caught = true;
    }
    NUI_CHECK(caught && !armed && !pending.handle.valid() && actions == 0);
    NUI_CHECK(time.owner.active_timer_count() == 0 && time.owner.pending_task_count() == 0);
    tree.clear_invalidation_callback();
    const auto dispatcher = time.owner.dispatcher();
    for (std::size_t i = 0; i < ui::kDispatcherMaxPendingTasks; ++i)
        NUI_CHECK(dispatcher.post([] {}));
    NUI_CHECK(!dispatcher.post([] {}));
    // The retained checkpoint sees a full queue. It must preserve the owner's
    // retry participant, even though no visual messages remain.
    frame(tree);
    NUI_CHECK(actions == 0);
    while (time.owner.pending_task_count())
        (void)time.owner.checkpoint();
    NUI_CHECK(actions == 0);
    frame(tree);
    (void)time.owner.checkpoint();
    NUI_CHECK(actions == 1);
    frame(tree);
    (void)time.owner.checkpoint();
    NUI_CHECK(actions == 1 && tree.overlay_entries().empty());
    auto recovered = messages.show({.message = "Recovery"});
    NUI_CHECK(recovered.handle.valid());
    NUI_CHECK(messages.dismiss(recovered.handle));
}
void quiet_deactivate_invalidation_throw_then_dispatch_recovery() {
    Clock time;
    int root_calls = 0, actions = 0;
    ui::UI tree{ui::Button{"Root", [&] { ++root_calls; }}};
    frame(tree);
    tree.activate(time.platform);
    ui::Toast messages{tree, time.owner.dispatcher()};
    auto shown = messages.show(
        {.message = "Deactivate", .action_label = "Run", .action = [&] { ++actions; }});
    frame(tree);
    tree.dispatch(test::key(ui::Key::Tab), time.platform);
    NUI_CHECK(focused(tree, "Run"));
    frame(tree); // Clean focus styling so the next invalidation reaches the injected boundary.
    NUI_CHECK(focused(tree, "Run"));
    bool armed = true;
    tree.set_invalidation_callback([&](ui::Rect) {
        if (shown.handle.valid() || !std::exchange(armed, false))
            return;
        throw std::runtime_error("quiet structure refresh");
    });
    bool caught = false;
    try {
        tree.deactivate(time.platform);
    } catch (const std::runtime_error &error) {
        NUI_CHECK(std::string_view{error.what()} == "quiet structure refresh");
        caught = true;
    }
    NUI_CHECK(caught && !armed && !shown.handle.valid() && actions == 0);
    NUI_CHECK(time.owner.active_timer_count() == 0 && tree.overlay_entries().empty());
    tree.clear_invalidation_callback();
    frame(tree);
    NUI_CHECK(focused(tree, "Root"));
    tree.dispatch(test::key(ui::Key::Enter), time.platform);
    NUI_CHECK(root_calls == 1 && actions == 0);
    auto next = messages.show({.message = "Available after failed deactivation"});
    NUI_CHECK(next.handle.valid());
    tree.deactivate(time.platform);
    NUI_CHECK(!next.handle.valid() && time.owner.active_timer_count() == 0);
    tree.activate(time.platform);
    frame(tree);
    tree.dispatch(test::key(ui::Key::Enter), time.platform);
    NUI_CHECK(root_calls == 2 && actions == 0);
}
void suite() {
    accepted_action_survives_post_refusal_then_normal_frame();
    quiet_deactivate_invalidation_throw_then_dispatch_recovery();
}
} // namespace
int main(int argc, char **argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "all";
    if (mode == "queue")
        return test::run("toast_action_queue",
                         &accepted_action_survives_post_refusal_then_normal_frame);
    if (mode == "quiet")
        return test::run("toast_quiet_deactivate",
                         &quiet_deactivate_invalidation_throw_then_dispatch_recovery);
    return test::run("toast_recovery", &suite);
}
