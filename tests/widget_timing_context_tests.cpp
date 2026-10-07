#include "test_support.hpp"
#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>

namespace {
using namespace std::chrono_literals;

struct Activity {
    ui::Dispatcher dispatcher;
    ui::TimerHandle timer;
    int ticks{};
    std::uint64_t generation{};
    int transitions_during_disabled_measure{-1};
    int transitions{};
    int activations{};
    int deactivations{};
    int cancellations{};
    bool active{};
    bool throw_activation{};
    bool throw_deactivation{};
    bool valid_in_activation{};
    bool valid_in_deactivation{};
};

class ActivityComponent final : public ui::Component, public ui::detail::ThemeBinding {
public:
    explicit ActivityComponent(std::shared_ptr<Activity> state) : state_(std::move(state)) {}
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        if (!effective_enabled()) state_->transitions_during_disabled_measure = state_->transitions;
        return {effective_enabled() ? 60.0f : 80.0f, 20.0f};
    }
    void paint(ui::PaintContext&) const override {}
    void activate(ui::LifecycleContext& context) override {
        state_->dispatcher = context.dispatcher();
        state_->valid_in_activation = state_->dispatcher.valid();
        ++state_->activations;
        state_->active = true;
        arm();
        if (state_->throw_activation) throw std::runtime_error("activation fault");
    }
    void deactivate(ui::LifecycleContext& context) override {
        ++state_->deactivations;
        state_->valid_in_deactivation = context.dispatcher().valid();
        state_->active = false;
        stop();
        if (state_->throw_deactivation) throw std::runtime_error("deactivation fault");
    }
    void unmount(ui::LifecycleContext&) override { state_->active = false; stop(); }
private:
    void effective_availability_changed(const ui::ComponentAvailability&,
                                        const ui::ComponentAvailability& next) noexcept override {
        ++state_->transitions;
        stop();
        if (state_->active && next.interactive() && !next.read_only) {
            try { arm(); } catch (...) { /* Rejected scheduling keeps a static activity. */ }
        }
    }
    void arm() {
        if (!effective_availability().interactive() || effective_read_only()) return;
        std::weak_ptr<Activity> weak = state_;
        const auto generation = ++state_->generation;
        state_->timer = state_->dispatcher.schedule_after(10ms, [weak, generation] {
            if (const auto state = weak.lock(); state && state->active && state->generation == generation)
                ++state->ticks;
        });
    }
    void stop() noexcept {
        ++state_->generation;
        const auto timer = std::exchange(state_->timer, {});
        if (!timer.valid()) return;
        try { if (state_->dispatcher.cancel(timer)) ++state_->cancellations; } catch (...) {}
    }
    std::shared_ptr<Activity> state_;
};

ui::Spec probe(const std::shared_ptr<Activity>& state, std::vector<ui::Spec> children = {}) {
    return ui::Spec{[state] { return std::make_unique<ActivityComponent>(state); }, std::move(children)};
}

struct Harness {
    std::shared_ptr<ui::detail::ManualDispatcherClock> clock{
        std::make_shared<ui::detail::ManualDispatcherClock>()};
    ui::detail::DispatcherOwner owner{{}, clock};
    test::MockPlatform platform;
    Harness() { platform.dispatcher_value = owner.dispatcher(); }
    std::size_t tick() { clock->advance(10ms); return owner.checkpoint(); }
};

void activation_and_rollback() {
    Harness h;
    auto parent = std::make_shared<Activity>();
    auto first = std::make_shared<Activity>();
    auto last = std::make_shared<Activity>();
    last->throw_activation = true;
    ui::UI tree{probe(parent, {probe(first), probe(last)})};
    tree.resize({200, 100});
    bool caught = false;
    try { tree.activate(h.platform); } catch (const std::runtime_error& error) {
        caught = std::string_view{error.what()} == "activation fault";
    }
    NUI_CHECK(caught);
    NUI_CHECK(parent->valid_in_activation && first->valid_in_activation && last->valid_in_activation);
    NUI_CHECK(parent->valid_in_deactivation && first->valid_in_deactivation && last->valid_in_deactivation);
    NUI_CHECK(parent->deactivations == 1 && first->deactivations == 1 && last->deactivations == 1);
    NUI_CHECK(h.owner.active_timer_count() == 0);
    NUI_CHECK(h.tick() == 0);
    NUI_CHECK(first->ticks == 0 && last->ticks == 0);
    last->throw_activation = false;
    tree.activate(h.platform);
    NUI_CHECK(h.owner.active_timer_count() == 3);
    NUI_CHECK(first->ticks == 0 && last->ticks == 0);
    NUI_CHECK(h.tick() == 3);
    NUI_CHECK(parent->ticks == 1 && first->ticks == 1 && last->ticks == 1);
    tree.deactivate(h.platform);
    NUI_CHECK(parent->deactivations == 2 && first->deactivations == 2 && last->deactivations == 2);
}

void availability_and_dynamic_insertion() {
    Harness h;
    auto state = std::make_shared<Activity>();
    ui::State<bool> present{false};
    ui::State<bool> enabled{true};
    ui::State<bool> locked{false};
    ui::State<ui::VisibilityMode> visible{ui::VisibilityMode::Visible};
    ui::UI tree{ui::Enabled{enabled, ui::ReadOnly{locked,
        ui::Visibility{visible, ui::If{present, probe(state)}}}}};
    tree.resize({200, 100});
    tree.activate(h.platform);
    present.set(true);
    (void)tree.measure();
    NUI_CHECK(state->valid_in_activation);
    NUI_CHECK(h.owner.active_timer_count() == 1);
    auto expect_stopped = [&] {
        NUI_CHECK(h.owner.active_timer_count() == 0);
        NUI_CHECK(h.tick() == 0);
    };
    auto expect_resume = [&] {
        NUI_CHECK(h.owner.active_timer_count() == 1);
        NUI_CHECK(h.tick() == 1);
    };
    auto rearm = [&] {
        visible.set(ui::VisibilityMode::Hidden);
        visible.set(ui::VisibilityMode::Visible);
        NUI_CHECK(h.owner.active_timer_count() == 1);
    };
    visible.set(ui::VisibilityMode::Hidden);
    expect_stopped();
    visible.set(ui::VisibilityMode::Visible);
    expect_resume();
    rearm();
    visible.set(ui::VisibilityMode::Collapsed);
    expect_stopped();
    visible.set(ui::VisibilityMode::Visible);
    expect_resume();
    rearm();
    const auto transitions = state->transitions;
    enabled.set(false);
    NUI_CHECK(state->transitions_during_disabled_measure == transitions);
    NUI_CHECK(state->transitions == transitions + 1);
    expect_stopped();
    enabled.set(true);
    expect_resume();
    rearm();
    locked.set(true);
    expect_stopped();
    locked.set(false);
    expect_resume();
    NUI_CHECK(state->ticks == 4);
    rearm();
    present.set(false);
    (void)tree.measure();
    expect_stopped();
    NUI_CHECK(state->ticks == 4);
}

void failed_dynamic_activation_preserves_sibling() {
    Harness h;
    auto established = std::make_shared<Activity>();
    auto added = std::make_shared<Activity>();
    added->throw_activation = true;
    ui::State<bool> present{false};
    ui::UI tree{ui::Column{probe(established), ui::If{present, probe(added)}}};
    tree.resize({200, 100});
    tree.activate(h.platform);
    NUI_CHECK(h.owner.active_timer_count() == 1);
    present.set(true);
    bool caught = false;
    try { (void)tree.measure(); } catch (const std::runtime_error& error) {
        caught = std::string_view{error.what()} == "activation fault";
    }
    NUI_CHECK(caught);
    NUI_CHECK(added->deactivations == 1);
    NUI_CHECK(established->deactivations == 0);
    NUI_CHECK(h.owner.active_timer_count() == 1);
    NUI_CHECK(h.tick() == 1);
    NUI_CHECK(established->ticks == 1 && added->ticks == 0);
    present.set(false);
    (void)tree.measure();
    added->throw_activation = false;
    present.set(true);
    (void)tree.measure();
    NUI_CHECK(h.owner.active_timer_count() == 1);
    NUI_CHECK(h.tick() == 1);
    NUI_CHECK(established->ticks == 1 && added->ticks == 1);
}

void teardown_and_independent_dispatchers() {
    Harness a;
    Harness b;
    auto first = std::make_shared<Activity>();
    auto last = std::make_shared<Activity>();
    last->throw_deactivation = true;
    auto tree = std::make_unique<ui::UI>(ui::Column{probe(first), probe(last)});
    tree->resize({200, 100});
    tree->activate(a.platform);
    bool caught = false;
    try { tree->deactivate(a.platform); } catch (const std::runtime_error& error) {
        caught = std::string_view{error.what()} == "deactivation fault";
    }
    NUI_CHECK(caught);
    NUI_CHECK(first->deactivations == 1 && last->deactivations == 1);
    NUI_CHECK(a.owner.active_timer_count() == 0);
    NUI_CHECK(a.tick() == 0);
    NUI_CHECK(first->ticks == 0 && last->ticks == 0);
    last->throw_deactivation = false;
    tree->activate(b.platform);
    NUI_CHECK(b.owner.active_timer_count() == 2);
    NUI_CHECK(a.tick() == 0);
    NUI_CHECK(first->ticks == 0 && last->ticks == 0);
    NUI_CHECK(b.tick() == 2);
    NUI_CHECK(first->ticks == 1 && last->ticks == 1);
    tree->deactivate(b.platform);
    tree->activate(b.platform);
    NUI_CHECK(b.owner.active_timer_count() == 2);
    tree.reset();
    NUI_CHECK(b.owner.active_timer_count() == 0);
    NUI_CHECK(b.tick() == 0);
    NUI_CHECK(first->ticks == 1 && last->ticks == 1);
}

class NoTimingPlatform final : public ui::PlatformServices {
public:
    void set_clipboard_text(std::string_view) override {}
    void request_clipboard_text() override {}
};

void legacy_context_and_missing_timing() {
    ui::LifecycleContext context{1, {}, [] {}, [] {}};
    NUI_CHECK(!context.dispatcher().valid());
    auto state = std::make_shared<Activity>();
    NoTimingPlatform platform;
    ui::UI tree{probe(state)};
    tree.resize({200, 100});
    tree.activate(platform);
    NUI_CHECK(!state->valid_in_activation && !state->timer.valid());
}
} // namespace

int main() {
    legacy_context_and_missing_timing();
    activation_and_rollback();
    availability_and_dynamic_insertion();
    failed_dynamic_activation_preserves_sibling();
    teardown_and_independent_dispatchers();
    return 0;
}
