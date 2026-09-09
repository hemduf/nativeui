#include "test_support.hpp"

#include <nativeui/svg.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kResourceSvgA =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="2" height="2" viewBox="0 0 2 2"><rect width="2" height="2" fill="#f00"/></svg>)svg";
constexpr std::string_view kResourceSvgB =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="3" height="4" viewBox="0 0 3 4"><rect width="3" height="4" fill="#00f"/></svg>)svg";

std::vector<std::byte> byte_vector(std::string_view text) {
    const auto* first = reinterpret_cast<const std::byte*>(text.data());
    return {first, first + text.size()};
}

class FixedProvider final : public ui::ResourceProvider {
public:
    explicit FixedProvider(std::string_view svg)
        : svg_(svg) {}

    [[nodiscard]] std::optional<std::vector<std::byte>> load(
        std::string_view resource_id) override {
        ++calls;
        if (resource_id != "same-resource") return std::nullopt;
        return byte_vector(svg_);
    }

    std::string_view svg_;
    int calls{};
};

struct ProbeStats {
    int mount{};
    int activate{};
    int deactivate{};
    int unmount{};
    int focus_in{};
    int focus_out{};
    int pointer_down{};
    int pointer_move{};
    int pointer_cancel{};
    int state_changes{};
    int stale_callbacks{};
    std::string application_label;
    std::weak_ptr<int> lifetime;
};

[[noreturn]] void stress_fail(
    std::string_view fixture,
    int cycle,
    std::string_view transition,
    std::string_view message) {
    throw test::Failure(
        std::string(fixture) + " cycle=" + std::to_string(cycle) +
        " transition=" + std::string(transition) + ": " + std::string(message));
}

void require(
    bool condition,
    std::string_view fixture,
    int cycle,
    std::string_view transition,
    std::string_view message) {
    if (!condition) stress_fail(fixture, cycle, transition, message);
}

class StressProbeComponent final : public ui::Component {
public:
    StressProbeComponent(
        std::shared_ptr<ProbeStats> stats,
        ui::State<int>& observed,
        std::string label)
        : stats_(std::move(stats)),
          observed_(observed),
          lifetime_(std::make_shared<int>(1)) {
        stats_->application_label = std::move(label);
        stats_->lifetime = lifetime_;
    }

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {96.0f, 48.0f};
    }

    void mount(ui::MountContext& context) override {
        ++stats_->mount;
        const auto weak_lifetime = std::weak_ptr<int>{lifetime_};
        auto invalidate = context.invalidator();
        subscription_ = observed_.observe(
            [stats = stats_, weak_lifetime, invalidate = std::move(invalidate)](const int&) {
                if (weak_lifetime.expired()) {
                    ++stats->stale_callbacks;
                    return;
                }
                ++stats->state_changes;
                invalidate();
            });
    }

    void activate(ui::LifecycleContext&) override { ++stats_->activate; }
    void deactivate(ui::LifecycleContext&) override { ++stats_->deactivate; }
    void unmount(ui::LifecycleContext&) override { ++stats_->unmount; }

    void focus_changed(bool focused, ui::FocusContext& context) override {
        if (focused) {
            ++stats_->focus_in;
            context.set_text_input(true, context.bounds());
        } else {
            ++stats_->focus_out;
            context.set_text_input(false);
        }
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        switch (event.type) {
        case ui::InputType::PointerDown:
            ++stats_->pointer_down;
            context.capture_pointer();
            context.request_clipboard_text();
            context.invalidate();
            return ui::EventResult::Handled;
        case ui::InputType::PointerMove:
            ++stats_->pointer_move;
            return ui::EventResult::Handled;
        case ui::InputType::PointerCancel:
            ++stats_->pointer_cancel;
            return ui::EventResult::Handled;
        default:
            return ui::EventResult::Ignored;
        }
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<ProbeStats> stats_;
    ui::State<int>& observed_;
    std::shared_ptr<int> lifetime_;
    ui::State<int>::Subscription subscription_;
};

class StressProbe {
public:
    StressProbe(
        std::shared_ptr<ProbeStats> stats,
        ui::State<int>& observed,
        std::string label = "stress probe")
        : stats_(std::move(stats)), observed_(&observed), label_(std::move(label)) {}

    ui::Spec spec() && {
        auto stats = std::move(stats_);
        auto* observed = observed_;
        auto label = std::move(label_);
        return ui::Spec{
            [stats = std::move(stats), observed, label = std::move(label)]() mutable {
                return std::make_unique<StressProbeComponent>(
                    stats, *observed, std::move(label));
            },
            {}};
    }

private:
    std::shared_ptr<ProbeStats> stats_;
    ui::State<int>* observed_{};
    std::string label_;
};

void headless_tree_lifecycle_1000() {
    constexpr std::string_view fixture = "headless_tree_lifecycle_1000";

    for (int cycle = 0; cycle < 1000; ++cycle) {
        ui::State<int> observed{0};
        auto stats = std::make_shared<ProbeStats>();
        test::MockPlatform platform;
        int invalidation_callbacks = 0;

        {
            ui::UI tree{StressProbe{stats, observed}};
            tree.set_invalidation_callback([&](ui::Rect) { ++invalidation_callbacks; });
            tree.resize({160.0f, 96.0f});
            tree.activate(platform);

            require(stats->mount == 1, fixture, cycle, "activate", "mount count != 1");
            require(stats->activate == 1, fixture, cycle, "activate", "activate count != 1");
            require(stats->focus_in == 1, fixture, cycle, "activate", "focus was not acquired");
            require(platform.text_input_active, fixture, cycle, "activate", "text input was not activated");

            const auto pointer_result = tree.dispatch(
                test::pointer(ui::InputType::PointerDown, 12.0f, 12.0f), platform);
            require(pointer_result == ui::EventResult::Handled,
                    fixture, cycle, "pointer-down", "pointer down was not handled");
            require(stats->pointer_down == 1,
                    fixture, cycle, "pointer-down", "pointer down count != 1");
            require(platform.paste_request_count == 1,
                    fixture, cycle, "clipboard-request", "paste request count != 1");
            require(invalidation_callbacks > 0,
                    fixture, cycle, "pointer-down", "component invalidation callback did not reach the tree");

            // Multiple invalidations may coalesce while the tree is already dirty.
            // Exercise the state-owned invalidator without requiring a second callback.
            observed.set(1);
            require(stats->state_changes == 1,
                    fixture, cycle, "state-change", "state observer count != 1");
            require(tree.dirty(),
                    fixture, cycle, "state-change", "state invalidation did not leave the tree dirty");

            tree.deactivate(platform);
            require(stats->pointer_cancel == 1,
                    fixture, cycle, "deactivate", "active capture was not cancelled exactly once");
            require(stats->focus_out == 1,
                    fixture, cycle, "deactivate", "focus was not released exactly once");
            require(stats->deactivate == 1,
                    fixture, cycle, "deactivate", "deactivate count != 1");
            require(!platform.text_input_active,
                    fixture, cycle, "deactivate", "text input remained active");

            tree.deactivate(platform);
            require(stats->pointer_cancel == 1,
                    fixture, cycle, "repeat-deactivate", "stale capture was cancelled twice");
            require(stats->focus_out == 1,
                    fixture, cycle, "repeat-deactivate", "focus-out repeated");
            require(stats->deactivate == 1,
                    fixture, cycle, "repeat-deactivate", "deactivate repeated");
        }

        require(stats->unmount == 1, fixture, cycle, "destroy", "unmount count != 1");
        require(stats->lifetime.expired(), fixture, cycle, "destroy", "component lifetime sentinel survived destruction");

        const int observed_before_destroyed_set = stats->state_changes;
        observed.set(2);
        require(stats->state_changes == observed_before_destroyed_set,
                fixture, cycle, "post-destroy-state", "state callback executed after owner destruction");
        require(stats->stale_callbacks == 0,
                fixture, cycle, "post-destroy-state", "weak sentinel detected a stale callback");
    }
}

void headless_two_tree_isolation_50() {
    constexpr std::string_view fixture = "headless_two_tree_isolation_50";

    for (int cycle = 0; cycle < 50; ++cycle) {
        ui::State<int> state_a{7};
        ui::State<int> state_b{7};
        auto stats_a = std::make_shared<ProbeStats>();
        auto stats_b = std::make_shared<ProbeStats>();
        test::MockPlatform platform_a;
        test::MockPlatform platform_b;
        int invalidations_a = 0;
        int invalidations_b = 0;

        // Deliberately use the same resource ID through independent providers
        // and caches. Clearing A must not evict or mutate B.
        FixedProvider provider_a{kResourceSvgA};
        FixedProvider provider_b{kResourceSvgB};
        ui::SvgCache cache_a{provider_a};
        ui::SvgCache cache_b{provider_b};
        const auto resource_a = cache_a.load("same-resource");
        const auto resource_b = cache_b.load("same-resource");
        require(
            static_cast<bool>(resource_a) && static_cast<bool>(resource_b),
            fixture,
            cycle,
            "resource-load",
            "same resource ID did not resolve independently in A/B");
        const auto size_a = resource_a.icon.intrinsic_size();
        const auto size_b = resource_b.icon.intrinsic_size();
        require(
            size_a.w == 2.0f && size_a.h == 2.0f &&
                size_b.w == 3.0f && size_b.h == 4.0f,
            fixture,
            cycle,
            "resource-load",
            "A/B resource providers were not isolated");
        require(
            provider_a.calls == 1 && provider_b.calls == 1 &&
                cache_a.size() == 1 && cache_b.size() == 1,
            fixture,
            cycle,
            "resource-load",
            "A/B cache bookkeeping was not independent");
        cache_a.clear();
        require(
            cache_a.size() == 0 && cache_b.size() == 1,
            fixture,
            cycle,
            "resource-clear-a",
            "clearing A mutated B resource cache");

        auto tree_a = std::make_unique<ui::UI>(
            StressProbe{stats_a, state_a, "same application label"});
        auto tree_b = std::make_unique<ui::UI>(
            StressProbe{stats_b, state_b, "same application label"});
        tree_a->set_invalidation_callback([&](ui::Rect) { ++invalidations_a; });
        tree_b->set_invalidation_callback([&](ui::Rect) { ++invalidations_b; });
        tree_a->resize({160.0f, 96.0f});
        tree_b->resize({160.0f, 96.0f});
        tree_a->activate(platform_a);
        tree_b->activate(platform_b);
        tree_a->dispatch(test::pointer(ui::InputType::PointerDown, 10.0f, 10.0f), platform_a);
        tree_b->dispatch(test::pointer(ui::InputType::PointerDown, 10.0f, 10.0f), platform_b);

        require(
            stats_a->application_label == "same application label" &&
                stats_b->application_label == "same application label" &&
                state_a.get() == state_b.get(),
            fixture,
            cycle,
            "same-looking-data",
            "A/B same-looking application labels/control values diverged");
        require(
            invalidations_a > 0 && invalidations_b > 0,
            fixture,
            cycle,
            "invalidation",
            "A/B did not maintain independent invalidation callbacks");

        state_a.set(8);
        state_b.set(8);
        require(stats_a->state_changes == 1 && stats_b->state_changes == 1,
                fixture, cycle, "initial-state", "independent state observers did not fire once each");

        tree_a->deactivate(platform_a);
        const int a_invalidations_before_destroy = invalidations_a;
        tree_a.reset();
        require(stats_a->pointer_cancel == 1 && stats_a->focus_out == 1,
                fixture, cycle, "destroy-a", "A did not tear down capture/focus exactly once");
        require(stats_b->pointer_cancel == 0 && stats_b->focus_out == 0,
                fixture, cycle, "destroy-a", "destroying A mutated B interaction state");

        const int b_changes_before = stats_b->state_changes;
        state_a.set(9);
        state_b.set(9);
        require(stats_a->state_changes == 1,
                fixture, cycle, "continue-b", "A state callback survived tree destruction");
        require(invalidations_a == a_invalidations_before_destroy,
                fixture, cycle, "continue-b", "A invalidation callback survived tree destruction");
        require(stats_b->state_changes == b_changes_before + 1,
                fixture, cycle, "continue-b", "B state observer stopped after A destruction");

        int survivor_invalidation_replays = 0;
        tree_b->set_invalidation_callback(
            [&](ui::Rect) { ++survivor_invalidation_replays; });
        require(survivor_invalidation_replays > 0,
                fixture, cycle, "continue-b", "B dirty/invalidation state was cleared by A destruction");

        const auto move_result = tree_b->dispatch(
            test::pointer(ui::InputType::PointerMove, 400.0f, 400.0f), platform_b);
        require(move_result == ui::EventResult::Handled && stats_b->pointer_move == 1,
                fixture, cycle, "continue-b", "B capture stopped after A destruction");

        tree_b->deactivate(platform_b);
        tree_b.reset();
        require(stats_b->pointer_cancel == 1 && stats_b->focus_out == 1,
                fixture, cycle, "destroy-b", "B did not tear down capture/focus exactly once");
        require(stats_a->stale_callbacks == 0 && stats_b->stale_callbacks == 0,
                fixture, cycle, "complete", "stale callback sentinel fired");
    }
}

void suite() {
    headless_tree_lifecycle_1000();
    headless_two_tree_isolation_50();
}

} // namespace

int main() { return test::run("t042_lifecycle_stress", &suite); }
