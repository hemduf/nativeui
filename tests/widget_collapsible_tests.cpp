#include "test_support.hpp"
#include <nativeui/collapsible.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>

namespace {
using namespace std::chrono_literals;

struct ContentObservation {
    ui::NodeId mounted_id{};
    ui::Rect bounds{};
    int mounts{};
    int unmounts{};
    int presses{};
    bool focused{};
    bool fail_mount{};
};

class ContentComponent final : public ui::Component {
public:
    explicit ContentComponent(std::shared_ptr<ContentObservation> state) : state_(std::move(state)) {}
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f,80.0f};
    }
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    void mount(ui::MountContext& context) override {
        ++state_->mounts;
        state_->mounted_id = context.node_id();
        if (state_->fail_mount) throw std::runtime_error("disclosure content mount");
    }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void focus_changed(bool value, ui::FocusContext&) override { state_->focused = value; }
    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type != ui::InputType::PointerDown) return ui::EventResult::Ignored;
        ++state_->presses;
        return ui::EventResult::Handled;
    }
    void paint(ui::PaintContext& context) const override { state_->bounds = context.bounds(); }
private:
    std::shared_ptr<ContentObservation> state_;
};

class Content {
public:
    explicit Content(std::shared_ptr<ContentObservation> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        auto state = state_;
        return {[state] { return std::make_unique<ContentComponent>(state); }, {}};
    }
private:
    std::shared_ptr<ContentObservation> state_;
};

ui::InputEvent release(ui::Key key) {
    auto event = test::key(key);
    event.type = ui::InputType::KeyUp;
    return event;
}

void press_key(ui::UI& tree, ui::Key key, test::MockPlatform& platform) {
    tree.dispatch(test::key(key),platform);
    tree.dispatch(release(key),platform);
}

struct ClockHarness {
    std::shared_ptr<ui::detail::ManualDispatcherClock> clock{
        std::make_shared<ui::detail::ManualDispatcherClock>()};
    ui::detail::DispatcherOwner owner{{},clock};
    test::MockPlatform platform;
    ClockHarness() { platform.dispatcher_value = owner.dispatcher(); }
    void advance(std::chrono::milliseconds duration) {
        clock->advance(duration);
        (void)owner.checkpoint();
    }
};

void key_and_pointer_toggle_only_on_valid_release() {
    ui::State<bool> open{false};
    int changes = 0;
    ui::UI tree{ui::Collapsible{"Advanced",open,ui::Spacer{120.0f,80.0f}}
        .style(ui::CollapsibleStyle{.reduced_motion=true})
        .on_change([&](bool value) { NUI_CHECK(value == open.get()); ++changes; })};
    test::MockPlatform platform;
    tree.resize({240.0f,160.0f});
    tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Space),platform);
    tree.dispatch(test::key(ui::Key::Space),platform);
    NUI_CHECK(!open.get() && changes == 0);
    tree.dispatch(release(ui::Key::Space),platform);
    NUI_CHECK(open.get() && changes == 1);
    tree.dispatch(release(ui::Key::Space),platform);
    NUI_CHECK(changes == 1);
    tree.dispatch(test::key(ui::Key::Escape),platform);
    NUI_CHECK(open.get());
    tree.dispatch(test::key(ui::Key::Left),platform);
    NUI_CHECK(!open.get() && changes == 2);
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(open.get() && changes == 3);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,8.0f,8.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,500.0f,500.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,500.0f,500.0f),platform);
    NUI_CHECK(open.get() && changes == 3);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,8.0f,8.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,8.0f,8.0f),platform);
    NUI_CHECK(!open.get() && changes == 4);
}

void retain_and_unmount_policies_preserve_their_identity_contracts() {
    ui::State<bool> retained_open{false};
    auto retained = std::make_shared<ContentObservation>();
    ui::UI first{ui::Collapsible{"Retain",retained_open,Content{retained}}
        .style(ui::CollapsibleStyle{.reduced_motion=true})};
    NUI_CHECK(retained->mounts == 1);
    const auto retained_id = retained->mounted_id;
    first.resize({240.0f,160.0f});
    retained_open.set(true);
    first.resize({240.0f,160.0f});
    retained_open.set(false);
    first.resize({240.0f,160.0f});
    NUI_CHECK(retained->mounts == 1 && retained->unmounts == 0);
    NUI_CHECK(retained->mounted_id == retained_id);

    ui::State<bool> lazy_open{false};
    auto lazy = std::make_shared<ContentObservation>();
    ui::UI second{ui::Collapsible{"Unmount",lazy_open,Content{lazy}}
        .content_policy(ui::DisclosureContentPolicy::UnmountWhenClosed)
        .style(ui::CollapsibleStyle{.reduced_motion=true})};
    NUI_CHECK(lazy->mounts == 0);
    second.resize({240.0f,160.0f});
    lazy_open.set(true);
    second.resize({240.0f,160.0f});
    const auto first_id = lazy->mounted_id;
    NUI_CHECK(lazy->mounts == 1);
    lazy_open.set(false);
    second.resize({240.0f,160.0f});
    NUI_CHECK(lazy->unmounts == 1);
    lazy_open.set(true);
    second.resize({240.0f,160.0f});
    NUI_CHECK(lazy->mounts == 2 && lazy->mounted_id != first_id);
}

void closing_focused_content_restores_header_and_suppresses_hits_immediately() {
    ui::State<bool> open{true};
    auto content = std::make_shared<ContentObservation>();
    ui::UI tree{ui::Collapsible{"Focus",open,Content{content}}};
    ClockHarness harness;
    tree.resize({240.0f,180.0f});
    tree.activate(harness.platform);
    tree.dispatch(test::key(ui::Key::Tab),harness.platform);
    NUI_CHECK(content->focused);
    ui::HeadlessRenderer renderer{{240.0f,180.0f},1.0f};
    NUI_CHECK(renderer.render(tree));
    const auto old_content = content->bounds;
    open.set(false);
    tree.resize({240.0f,180.0f});
    NUI_CHECK(!content->focused);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,
        old_content.x + old_content.w * 0.5f, old_content.y + old_content.h * 0.5f),harness.platform);
    NUI_CHECK(content->presses == 0);
    press_key(tree,ui::Key::Space,harness.platform);
    NUI_CHECK(open.get()); // Header is the authoritative restored target.
}

void animation_uses_instance_clock_reverses_and_cancels_when_hidden_or_removed() {
    ClockHarness harness;
    ui::State<bool> open{false};
    ui::State<ui::VisibilityMode> visible{ui::VisibilityMode::Visible};
    int invalidations = 0;
    auto tree = std::make_unique<ui::UI>(ui::Visibility{visible.binding(),
        ui::Collapsible{"Animation",open,ui::Spacer{120.0f,80.0f}}});
    tree->resize({240.0f,180.0f});
    tree->activate(harness.platform);
    tree->set_invalidation_callback([&] { ++invalidations; });
    const auto closed_height = tree->measure().preferred.h;
    open.set(true);
    NUI_CHECK(harness.owner.active_timer_count() > 0);
    harness.advance(75ms);
    const auto halfway = tree->measure().preferred.h;
    NUI_CHECK(halfway > closed_height && halfway < closed_height + 100.0f);
    open.set(false);
    const auto before_reverse = tree->measure().preferred.h;
    NUI_CHECK_NEAR(before_reverse,halfway,0.1f);
    harness.advance(150ms);
    NUI_CHECK_NEAR(tree->measure().preferred.h,closed_height,0.01f);
    NUI_CHECK(harness.owner.active_timer_count() == 0);
    open.set(true);
    NUI_CHECK(harness.owner.active_timer_count() > 0);
    visible.set(ui::VisibilityMode::Hidden);
    NUI_CHECK(harness.owner.active_timer_count() == 0);
    const auto hidden_invalidations = invalidations;
    harness.advance(250ms);
    NUI_CHECK(invalidations == hidden_invalidations);
    visible.set(ui::VisibilityMode::Visible);
    open.set(false);
    open.set(true);
    tree.reset();
    NUI_CHECK(harness.owner.active_timer_count() == 0);
    const auto retired_invalidations = invalidations;
    harness.advance(250ms);
    NUI_CHECK(invalidations == retired_invalidations);
}

void reentrant_and_throwing_callbacks_do_not_replay_and_next_press_recovers() {
    ui::State<bool> open{false};
    int callbacks = 0;
    bool replace = true;
    ui::UI tree{ui::Collapsible{"Callbacks",open,ui::Spacer{120.0f,80.0f}}
        .style(ui::CollapsibleStyle{.reduced_motion=true})
        .on_change([&](bool) {
            ++callbacks;
            if (replace) { open.set(false); return; }
            if (callbacks == 2) throw std::runtime_error("disclosure callback");
        })};
    test::MockPlatform platform;
    tree.resize({240.0f,160.0f});
    tree.activate(platform);
    press_key(tree,ui::Key::Enter,platform);
    NUI_CHECK(!open.get() && callbacks == 1);
    replace = false;
    bool threw = false;
    try { press_key(tree,ui::Key::Enter,platform); }
    catch (const std::runtime_error&) { threw = true; }
    NUI_CHECK(threw && open.get() && callbacks == 2);
    tree.dispatch(release(ui::Key::Enter),platform);
    NUI_CHECK(callbacks == 2);
    press_key(tree,ui::Key::Space,platform);
    NUI_CHECK(!open.get() && callbacks == 3);
}

void lazy_mount_failure_recovers_without_a_ghost_child() {
    ui::State<bool> open{false};
    auto content = std::make_shared<ContentObservation>();
    ui::UI tree{ui::Collapsible{"Failure",open,Content{content}}
        .content_policy(ui::DisclosureContentPolicy::UnmountWhenClosed)
        .style(ui::CollapsibleStyle{.reduced_motion=true})};
    tree.resize({240.0f,160.0f});
    content->fail_mount = true;
    open.set(true);
    bool threw = false;
    try { tree.resize({240.0f,160.0f}); }
    catch (const std::runtime_error&) { threw = true; }
    NUI_CHECK(threw && content->mounts == 1 && content->unmounts == 1);
    content->fail_mount = false;
    tree.resize({240.0f,160.0f});
    NUI_CHECK(content->mounts == 2);
    ui::HeadlessRenderer renderer{{240.0f,160.0f},1.0f};
    NUI_CHECK(renderer.render(tree));
}

void suite() {
    key_and_pointer_toggle_only_on_valid_release();
    retain_and_unmount_policies_preserve_their_identity_contracts();
    closing_focused_content_restores_header_and_suppresses_hits_immediately();
    animation_uses_instance_clock_reverses_and_cancels_when_hidden_or_removed();
    reentrant_and_throwing_callbacks_do_not_replay_and_next_press_recovers();
    lazy_mount_failure_recovers_without_a_ghost_child();
}

} // namespace

int main() { return test::run("widget_collapsible",&suite); }
