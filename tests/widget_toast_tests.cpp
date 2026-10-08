#include "test_support.hpp"
#include <nativeui/toast.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>

namespace {
using namespace std::chrono_literals;
struct Clock {
    std::shared_ptr<ui::detail::ManualDispatcherClock> clock =
        std::make_shared<ui::detail::ManualDispatcherClock>();
    ui::detail::DispatcherOwner owner{{}, clock};
    test::MockPlatform platform;
    Clock() { platform.dispatcher_value = owner.dispatcher(); }
    void advance(std::chrono::milliseconds duration) {
        clock->advance(duration);
        (void)owner.checkpoint();
    }
};
void frame(ui::UI &tree, ui::Size size = {320.0f, 240.0f}) {
    tree.resize(size);
    ui::HeadlessRenderer renderer{size, 1.0f};
    NUI_CHECK(renderer.render(tree));
}
std::optional<ui::SemanticInfo> named(ui::UI &tree, std::string_view name) {
    for (ui::NodeId id = 1; id < 500; ++id) {
        auto info = tree.component_semantics(id);
        if (info && info->name == name)
            return info;
    }
    return std::nullopt;
}
void durations_validation_and_timer_lifetime() {
    Clock time;
    ui::UI tree{ui::Button{"Root", [] {}}};
    frame(tree);
    tree.activate(time.platform);
    ui::Toast messages{tree, time.owner.dispatcher()};
    NUI_CHECK(messages.show({}).status == ui::ToastShowStatus::InvalidSpec);
    NUI_CHECK(messages.show({.message = "Empty action", .action_label = "Do"}).status ==
              ui::ToastShowStatus::InvalidSpec);
    NUI_CHECK(messages.show({.message = "Zero", .duration = 0ms}).status ==
              ui::ToastShowStatus::InvalidSpec);
    auto simple = messages.show({.message = "Simple"});
    auto action = messages.show({.message = "Action", .action_label = "Do", .action = [] {}});
    auto custom = messages.show({.message = "Custom", .duration = 1250ms});
    NUI_CHECK(simple.status == ui::ToastShowStatus::Shown && simple.handle.valid());
    NUI_CHECK(action.handle.valid() && custom.handle.valid() &&
              time.owner.active_timer_count() == 3);
    time.advance(1249ms);
    NUI_CHECK(custom.handle.valid());
    time.advance(1ms);
    NUI_CHECK(!custom.handle.valid() && simple.handle.valid());
    time.advance(2749ms);
    NUI_CHECK(simple.handle.valid() && action.handle.valid());
    time.advance(1ms);
    NUI_CHECK(!simple.handle.valid() && action.handle.valid());
    time.advance(3999ms);
    NUI_CHECK(action.handle.valid());
    time.advance(1ms);
    NUI_CHECK(!action.handle.valid() && time.owner.active_timer_count() == 0);
    frame(tree);
    NUI_CHECK(tree.overlay_entries().empty());
}
void dedup_stale_handles_and_one_overlay() {
    Clock time;
    ui::UI tree{ui::Button{"Root", [] {}}};
    frame(tree);
    tree.activate(time.platform);
    ui::Toast messages{tree, time.owner.dispatcher()};
    auto old = messages.show({.message = "Saved"});
    time.advance(2000ms);
    auto replacement = messages.show({.message = "Saved", .duration = 1500ms});
    auto independent = messages.show({.message = "Other"});
    NUI_CHECK(!old.handle.valid() && replacement.handle.valid() &&
              replacement.handle != old.handle);
    NUI_CHECK(!messages.dismiss(old.handle) && tree.overlay_entries().size() == 1 &&
              time.owner.active_timer_count() == 2);
    frame(tree);
    auto label = named(tree, "Saved");
    NUI_CHECK(label && label->role == ui::SemanticRole::Text);
    time.advance(1499ms);
    NUI_CHECK(replacement.handle.valid());
    time.advance(1ms);
    NUI_CHECK(!replacement.handle.valid() && independent.handle.valid());
    NUI_CHECK(messages.dismiss(independent.handle));
    NUI_CHECK(!messages.dismiss(independent.handle));
    frame(tree);
    NUI_CHECK(tree.overlay_entries().empty());
}
void pause_hover_and_focus_preserves_remaining_time() {
    Clock time;
    ui::UI tree{ui::Button{"Root", [] {}}};
    frame(tree);
    tree.activate(time.platform);
    ui::Toast messages{tree, time.owner.dispatcher()};
    auto toast = messages.show(
        {.message = "Paused", .action_label = "Do", .action = [] {}, .duration = 4000ms});
    frame(tree);
    time.advance(1000ms);
    const auto bounds = tree.overlay_entries().front().bounds;
    tree.dispatch(
        test::pointer(ui::InputType::PointerMove, bounds.x + bounds.w * 0.5f, bounds.y + 10.0f),
        time.platform);
    NUI_CHECK(time.owner.active_timer_count() == 0);
    time.advance(10000ms);
    NUI_CHECK(toast.handle.valid());
    tree.dispatch(test::key(ui::Key::Tab), time.platform);
    auto action = named(tree, "Do");
    NUI_CHECK(action && action->focused);
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 0.0f, 0.0f), time.platform);
    time.advance(10000ms);
    NUI_CHECK(toast.handle.valid() && time.owner.active_timer_count() == 0);
    tree.dispatch(test::key(ui::Key::Tab), time.platform);
    NUI_CHECK(time.owner.active_timer_count() == 1);
    time.advance(2999ms);
    NUI_CHECK(toast.handle.valid());
    time.advance(1ms);
    NUI_CHECK(!toast.handle.valid());
}
void action_terminal_once_reentrant_show_and_throw() {
    Clock time;
    int root_calls = 0, actions = 0;
    ui::UI tree{ui::Button{"Root", [&] { ++root_calls; }}};
    frame(tree);
    tree.activate(time.platform);
    ui::Toast messages{tree, time.owner.dispatcher()};
    ui::ToastHandle old;
    ui::ToastShowResult next;
    auto first = messages.show({.message = "Undoable", .action_label = "Undo", .action = [&] {
                                    NUI_CHECK(!old.valid());
                                    ++actions;
                                    next = messages.show({.message = "Undoable"});
                                }});
    old = first.handle;
    frame(tree);
    auto root = named(tree, "Root");
    NUI_CHECK(root && root->focused);
    tree.dispatch(test::key(ui::Key::Tab), time.platform);
    auto action = named(tree, "Undo");
    NUI_CHECK(action && action->focused);
    tree.dispatch(test::key(ui::Key::Enter), time.platform);
    NUI_CHECK(actions == 1 && !old.valid() && next.status == ui::ToastShowStatus::Shown &&
              next.handle.valid());
    NUI_CHECK(messages.dismiss(next.handle));
    frame(tree);
    bool callback_fault = true;
    auto failing = messages.show({.message = "Failure", .action_label = "Run", .action = [&] {
                                      ++actions;
                                      if (callback_fault)
                                          throw std::runtime_error("toast action");
                                  }});
    frame(tree);
    tree.dispatch(test::key(ui::Key::Tab), time.platform);
    bool threw = false;
    try {
        tree.dispatch(test::key(ui::Key::Enter), time.platform);
    } catch (const std::runtime_error &) {
        threw = true;
    }
    NUI_CHECK(threw && actions == 2 && !failing.handle.valid());
    callback_fault = false;
    frame(tree);
    (void)time.owner.checkpoint();
    NUI_CHECK(actions == 2);
    NUI_CHECK(time.owner.active_timer_count() == 0);
    (void)root_calls;
}
void bottom_geometry_outside_hit_and_resize() {
    Clock time;
    int calls = 0;
    ui::UI tree{ui::Button{"Root", [&] { ++calls; }}};
    frame(tree);
    tree.activate(time.platform);
    ui::Toast messages{tree, time.owner.dispatcher()};
    auto value =
        messages.show({.message = "A long message that wraps within a constrained viewport",
                       .action_label = "Confirm",
                       .action = [] {}});
    frame(tree);
    auto entries = tree.overlay_entries();
    NUI_CHECK(entries.size() == 1 &&
              entries.front().placement == ui::OverlayPlacement::ViewportBottomCenter);
    NUI_CHECK_NEAR(entries.front().bounds.y + entries.front().bounds.h, 240.0f, 0.01f);
    NUI_CHECK(entries.front().bounds.w <= 320.0f && entries.front().bounds.h < 240.0f);
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 4.0f, 4.0f), time.platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 4.0f, 4.0f), time.platform);
    NUI_CHECK(calls == 1 && value.handle.valid());
    tree.dispatch(test::key(ui::Key::Escape), time.platform);
    NUI_CHECK(value.handle.valid());
    frame(tree, {200.0f, 160.0f});
    entries = tree.overlay_entries();
    NUI_CHECK(entries.front().bounds.w <= 200.0f);
    NUI_CHECK_NEAR(entries.front().bounds.y + entries.front().bounds.h, 160.0f, 0.01f);
    frame(tree, {30.0f, 20.0f});
    entries = tree.overlay_entries();
    NUI_CHECK(entries.front().bounds.w <= 30.0f && entries.front().bounds.h <= 20.0f);
    NUI_CHECK(entries.front().bounds.x >= 0.0f && entries.front().bounds.y >= 0.0f);
}
void timer_throw_and_refusal_do_not_publish_immortal_messages() {
    Clock time;
    ui::UI tree{ui::Button{"Root", [] {}}};
    frame(tree);
    tree.activate(time.platform);
    ui::Toast messages{tree, time.owner.dispatcher()};
    ui::detail::DispatcherTestAccess::fail_next_timer(time.owner.dispatcher());
    bool threw = false;
    try {
        (void)messages.show({.message = "Failure"});
    } catch (const std::bad_alloc &) {
        threw = true;
    }
    NUI_CHECK(threw && tree.overlay_entries().empty() && time.owner.active_timer_count() == 0);
    auto okay = messages.show({.message = "Recovery"});
    NUI_CHECK(okay.handle.valid());
    NUI_CHECK(messages.dismiss(okay.handle));
    std::vector<ui::TimerHandle> occupied;
    occupied.reserve(ui::kDispatcherMaxActiveTimers);
    for (std::size_t i = 0; i < ui::kDispatcherMaxActiveTimers; ++i) {
        auto timer = time.owner.dispatcher().schedule_after(1000.0s, [] {});
        NUI_CHECK(timer.valid());
        occupied.push_back(timer);
    }
    auto refused = messages.show({.message = "No timer"});
    NUI_CHECK(refused.status == ui::ToastShowStatus::Unavailable && !refused.handle.valid() &&
              tree.overlay_entries().empty());
    for (const auto &timer : occupied)
        NUI_CHECK(time.owner.dispatcher().cancel(timer));
    auto recovered = messages.show({.message = "Accepted"});
    NUI_CHECK(recovered.handle.valid());
}
void capacity_isolation_teardown_and_foreign_handles() {
    Clock a, b;
    auto first = std::make_unique<ui::UI>(ui::Button{"First", [] {}});
    ui::UI second{ui::Button{"Second", [] {}}};
    frame(*first);
    frame(second);
    first->activate(a.platform);
    second.activate(b.platform);
    auto notices = std::make_unique<ui::Toast>(*first, a.owner.dispatcher());
    ui::Toast other{second, b.owner.dispatcher()};
    std::vector<ui::ToastHandle> handles;
    for (int i = 0; i < 32; ++i) {
        auto result = notices->show({.message = "Message " + std::to_string(i)});
        NUI_CHECK(result.handle.valid());
        handles.push_back(result.handle);
    }
    NUI_CHECK(notices->show({.message = "Overflow"}).status == ui::ToastShowStatus::Unavailable);
    auto duplicate = notices->show({.message = "Message 0"});
    NUI_CHECK(duplicate.handle.valid() && !handles[0].valid());
    auto independent = other.show({.message = "Independent"});
    NUI_CHECK(independent.handle.valid());
    NUI_CHECK(!other.dismiss(handles[1]) && !notices->dismiss(independent.handle));
    first.reset();
    NUI_CHECK(!duplicate.handle.valid() && a.owner.active_timer_count() == 0 &&
              independent.handle.valid());
    notices.reset();
    frame(second);
    NUI_CHECK(independent.handle.valid());
}
void deactivation_before_stack_mount_abandons_messages_and_timers() {
    Clock time;
    ui::UI tree{ui::Button{"Root", [] {}}};
    frame(tree);
    tree.activate(time.platform);
    ui::Toast messages{tree, time.owner.dispatcher()};
    auto pending =
        messages.show({.message = "Not mounted", .action_label = "Run", .action = [] {}});
    NUI_CHECK(pending.handle.valid() && time.owner.active_timer_count() == 1);
    // No layout/render after show: the retained Stack is still pending.
    tree.deactivate(time.platform);
    NUI_CHECK(!pending.handle.valid() && time.owner.active_timer_count() == 0 &&
              tree.overlay_entries().empty());
    NUI_CHECK(messages.show({.message = "Inactive"}).status == ui::ToastShowStatus::Unavailable);
    tree.activate(time.platform);
    frame(tree);
    NUI_CHECK(tree.overlay_entries().empty());
    auto fresh = messages.show({.message = "Fresh"});
    NUI_CHECK(fresh.handle.valid());
    frame(tree);
    tree.deactivate(time.platform);
    NUI_CHECK(!fresh.handle.valid() && time.owner.active_timer_count() == 0);
}
struct ActionCopy {
    std::shared_ptr<bool> fault;
    std::shared_ptr<int> calls;
    ActionCopy(std::shared_ptr<bool> f, std::shared_ptr<int> c)
        : fault(std::move(f)), calls(std::move(c)) {}
    ActionCopy(const ActionCopy &other) : fault(other.fault), calls(other.calls) {
        if (*fault)
            throw std::runtime_error("action copy");
    }
    ActionCopy(ActionCopy &&) noexcept = default;
    void operator()() const { ++*calls; }
};
void action_is_moved_after_terminal_retirement_and_destructor_contains_faults() {
    Clock time;
    ui::UI tree{ui::Button{"Root", [] {}}};
    frame(tree);
    tree.activate(time.platform);
    auto messages = std::make_unique<ui::Toast>(tree, time.owner.dispatcher());
    auto fault = std::make_shared<bool>(false);
    auto calls = std::make_shared<int>(0);
    auto value = messages->show(
        {.message = "Owned action", .action_label = "Do", .action = ActionCopy{fault, calls}});
    frame(tree);
    *fault = true;
    tree.dispatch(test::key(ui::Key::Tab), time.platform);
    tree.dispatch(test::key(ui::Key::Enter), time.platform);
    NUI_CHECK(*calls == 1 && !value.handle.valid());
    auto last =
        messages->show({.message = "Abandon", .action_label = "Do", .action = [&] { ++*calls; }});
    frame(tree);
    tree.set_invalidation_callback(
        [](ui::Rect) { throw std::runtime_error("teardown invalidation"); });
    messages.reset();
    NUI_CHECK(!last.handle.valid() && time.owner.active_timer_count() == 0 && *calls == 1);
    tree.set_invalidation_callback(std::function<void(ui::Rect)>{});
    frame(tree);
    NUI_CHECK(tree.overlay_entries().empty());
}
void suite() {
    durations_validation_and_timer_lifetime();
    dedup_stale_handles_and_one_overlay();
    pause_hover_and_focus_preserves_remaining_time();
    action_terminal_once_reentrant_show_and_throw();
    bottom_geometry_outside_hit_and_resize();
    timer_throw_and_refusal_do_not_publish_immortal_messages();
    capacity_isolation_teardown_and_foreign_handles();
    deactivation_before_stack_mount_abandons_messages_and_timers();
    action_is_moved_after_terminal_retirement_and_destructor_contains_faults();
}
} // namespace
int main(int argc, char **argv) {
    if (argc > 1 && std::string_view{argv[1]} == "deactivate")
        return test::run("toast_deactivate_unmounted",
                         &deactivation_before_stack_mount_abandons_messages_and_timers);
    return test::run("toast", &suite);
}
