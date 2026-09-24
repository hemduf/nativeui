#include "test_support.hpp"

#include "../src/detail/semantic_native_bounds.hpp"

#include <nativeui/component_tree.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/semantic_tree_action_access.hpp>

#include <algorithm>
#include <memory>
#include <new>
#include <optional>
#include <utility>
#include <vector>

namespace {

struct ProbeState final {
    int value{};
};

class NativeCheckpointProbe final : public ui::Component,
                                    public ui::detail::SemanticActionHandler {
public:
    explicit NativeCheckpointProbe(std::shared_ptr<ProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 24.0f};
    }

    [[nodiscard]] ui::SemanticInfo semantics() const override {
        ui::SemanticInfo info;
        info.role = ui::SemanticRole::Slider;
        info.name = "Native checkpoint probe";
        info.numeric_value = static_cast<double>(state_->value);
        info.value_range = ui::SemanticValueRange{0.0, 10.0, 1.0};
        info.enabled = true;
        info.actions = {ui::SemanticAction::Increment};
        return info;
    }

    [[nodiscard]] bool perform_semantic_action(
        const ui::detail::SemanticActionRequest& request) override {
        if (request.action != ui::SemanticAction::Increment) {
            return false;
        }
        ++state_->value;
        return true;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<ProbeState> state_;
};

[[nodiscard]] bool has_change(const std::vector<ui::SemanticChange>& changes,
                              ui::SemanticChange expected) {
    return std::find(changes.begin(), changes.end(), expected) != changes.end();
}

[[nodiscard]] ui::detail::SemanticNativeGeometry geometry(
    float scale,
    float x,
    float y) noexcept {
    return {scale, {x, y}};
}

void retained_native_checkpoint_pairs_post_dispatch_state_and_geometry() {
    auto state = std::make_shared<ProbeState>();
    ui::Spec spec;
    spec.factory = [state] {
        return std::make_unique<NativeCheckpointProbe>(state);
    };
    auto root = ui::compile(std::move(spec));
    const auto node_id = static_cast<ui::SemanticId>(root->id);

    ui::Tree tree{std::move(root)};
    tree.mount();

    auto dispatcher_owner = std::make_shared<ui::detail::DispatcherOwner>();
    auto view_owner = std::make_shared<int>(0);
    const std::weak_ptr<const void> view_lifetime{view_owner};
    auto domain = std::make_shared<ui::detail::SemanticRetainedViewDomain>(
        dispatcher_owner->dispatcher(), view_lifetime, tree);

    const auto first_geometry = geometry(1.25f, 40.0f, 60.0f);
    auto initial = ui::detail::SemanticRetainedViewCheckpoint::drain_and_publish_native(
        dispatcher_owner, view_lifetime, domain, first_geometry);
    NUI_CHECK(initial.has_value());
    NUI_CHECK(initial->publication != nullptr);
    NUI_CHECK(initial->publication->geometry == first_geometry);
    NUI_CHECK(has_change(initial->changes, ui::SemanticChange::StructureChanged));

    const auto first_native = domain->bridge().native_current();
    NUI_CHECK(first_native != nullptr);
    NUI_CHECK(first_native->geometry == first_geometry);
    const auto first_semantic_generation = first_native->semantic_generation();

    auto router = domain->bridge().make_ordinary_router(node_id);
    ui::detail::SemanticActionRequest increment;
    increment.action = ui::SemanticAction::Increment;
    NUI_CHECK(router.post(increment));
    NUI_CHECK(state->value == 0);

    const auto moved_geometry = geometry(1.5f, 101.0f, 203.0f);
    auto updated = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native_with_capture(
            dispatcher_owner,
            view_lifetime,
            domain,
            [&]() noexcept {
                NUI_CHECK(state->value == 1);
                return moved_geometry;
            });
    NUI_CHECK(updated.has_value());
    NUI_CHECK(state->value == 1);
    NUI_CHECK(updated->publication != nullptr);
    NUI_CHECK(updated->publication->geometry == moved_geometry);
    NUI_CHECK(updated->semantic_generation() == first_semantic_generation + 1);
    NUI_CHECK(has_change(updated->changes, ui::SemanticChange::ValueChanged));
    NUI_CHECK(has_change(updated->changes, ui::SemanticChange::BoundsChanged));

    // An already retained native publication is immutable and keeps the exact
    // semantic/geometry pair from its own generation.
    NUI_CHECK(first_native->geometry == first_geometry);
    NUI_CHECK(first_native->semantic_generation() == first_semantic_generation);
}

void retained_native_checkpoint_samples_t043_capture_source_after_dispatch() {
    auto state = std::make_shared<ProbeState>();
    ui::Spec spec;
    spec.factory = [state] {
        return std::make_unique<NativeCheckpointProbe>(state);
    };
    ui::Tree tree{ui::compile(std::move(spec))};
    tree.mount();

    auto dispatcher_owner = std::make_shared<ui::detail::DispatcherOwner>();
    auto view_owner = std::make_shared<int>(0);
    const std::weak_ptr<const void> view_lifetime{view_owner};
    auto domain = std::make_shared<ui::detail::SemanticRetainedViewDomain>(
        dispatcher_owner->dispatcher(), view_lifetime, tree);

    ui::detail::ViewGeometryState view_geometry{{80.0f, 24.0f}};
    NUI_CHECK(view_geometry.observe_scale(1.25f));
    NUI_CHECK(view_geometry.observe_physical_screen_origin({40.0f, 60.0f}));
    const auto capture_source = view_geometry.retain_native_geometry_capture_state();
    NUI_CHECK(capture_source != nullptr);
    const ui::detail::SemanticNativeGeometryCaptureLease capture_geometry{capture_source};

    int capture_count = 0;
    auto initial = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native_with_capture(
            dispatcher_owner,
            view_lifetime,
            domain,
            [&]() noexcept {
                ++capture_count;
                return capture_geometry();
            });
    NUI_CHECK(initial.has_value());
    NUI_CHECK(capture_count == 1);
    NUI_CHECK(initial->publication != nullptr);
    NUI_CHECK(initial->publication->geometry == geometry(1.25f, 40.0f, 60.0f));
    const auto semantic_generation = initial->semantic_generation();

    // Model the real T043 observation path as accepted T065 work. The retained
    // source must be sampled only after that work drains, so native readers see
    // the post-dispatch scale/origin pair rather than stale pre-dispatch state.
    NUI_CHECK(dispatcher_owner->dispatcher().post([&view_geometry] {
        (void)view_geometry.observe_scale(2.0f);
        (void)view_geometry.observe_physical_screen_origin({-120.0f, 305.0f});
    }));

    capture_count = 0;
    auto moved = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native_with_capture(
            dispatcher_owner,
            view_lifetime,
            domain,
            [&]() noexcept {
                ++capture_count;
                return capture_geometry();
            });
    NUI_CHECK(moved.has_value());
    NUI_CHECK(capture_count == 1);
    NUI_CHECK(moved->publication != nullptr);
    NUI_CHECK(moved->publication->geometry == geometry(2.0f, -120.0f, 305.0f));
    NUI_CHECK(moved->semantic_generation() == semantic_generation);
    NUI_CHECK(moved->changes.size() == 1);
    NUI_CHECK(moved->changes.front() == ui::SemanticChange::BoundsChanged);

    // The reusable lease keeps sampling the same detached per-view source and
    // therefore observes the latest authoritative pair without reading ViewCore.
    NUI_CHECK(capture_geometry() == geometry(2.0f, -120.0f, 305.0f));
}

void retained_native_checkpoint_refreshes_platform_origin_after_dispatch() {
    auto state = std::make_shared<ProbeState>();
    ui::Spec spec;
    spec.factory = [state] {
        return std::make_unique<NativeCheckpointProbe>(state);
    };
    ui::Tree tree{ui::compile(std::move(spec))};
    tree.mount();

    auto dispatcher_owner = std::make_shared<ui::detail::DispatcherOwner>();
    auto view_owner = std::make_shared<int>(0);
    const std::weak_ptr<const void> view_lifetime{view_owner};
    auto domain = std::make_shared<ui::detail::SemanticRetainedViewDomain>(
        dispatcher_owner->dispatcher(), view_lifetime, tree);

    ui::detail::ViewGeometryState view_geometry{{80.0f, 24.0f}};
    NUI_CHECK(view_geometry.observe_scale(1.5f));
    NUI_CHECK(view_geometry.observe_physical_screen_origin({20.0f, 30.0f}));
    const auto capture_source = view_geometry.retain_native_geometry_capture_state();
    const ui::detail::SemanticNativeGeometryCaptureLease capture_geometry{capture_source};

    auto initial = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native_with_capture(
            dispatcher_owner,
            view_lifetime,
            domain,
            [&]() noexcept { return capture_geometry(); });
    NUI_CHECK(initial.has_value());
    NUI_CHECK(initial->publication != nullptr);
    NUI_CHECK(initial->publication->geometry == geometry(1.5f, 20.0f, 30.0f));
    const auto semantic_generation = initial->semantic_generation();

    // Embedded parents can move without producing a child configure event.
    // Model the host movement as T065 work that changes only the native source
    // queried by the capture callback. The callback must run after the drain,
    // refresh the retained T043 origin, then publish that exact post-dispatch
    // screen-space value without advancing the logical semantic generation.
    ui::Point platform_screen_origin{20.0f, 30.0f};
    NUI_CHECK(dispatcher_owner->dispatcher().post([&platform_screen_origin] {
        platform_screen_origin = {-310.0f, 415.0f};
    }));

    int refresh_count = 0;
    auto moved = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native_with_capture(
            dispatcher_owner,
            view_lifetime,
            domain,
            [&]() noexcept {
                ++refresh_count;
                NUI_CHECK(view_geometry.observe_physical_screen_origin(
                    platform_screen_origin));
                return capture_geometry();
            });
    NUI_CHECK(moved.has_value());
    NUI_CHECK(refresh_count == 1);
    NUI_CHECK(moved->publication != nullptr);
    NUI_CHECK(moved->publication->geometry == geometry(1.5f, -310.0f, 415.0f));
    NUI_CHECK(moved->semantic_generation() == semantic_generation);
    NUI_CHECK(moved->changes.size() == 1);
    NUI_CHECK(moved->changes.front() == ui::SemanticChange::BoundsChanged);
    const auto retained_origin = view_geometry.physical_screen_origin();
    NUI_CHECK_NEAR(retained_origin.x, platform_screen_origin.x, 0.0001f);
    NUI_CHECK_NEAR(retained_origin.y, platform_screen_origin.y, 0.0001f);
}

void native_geometry_capture_is_suppressed_after_reentrant_view_death() {
    auto state = std::make_shared<ProbeState>();
    ui::Spec spec;
    spec.factory = [state] {
        return std::make_unique<NativeCheckpointProbe>(state);
    };
    ui::Tree tree{ui::compile(std::move(spec))};
    tree.mount();

    auto dispatcher_owner = std::make_shared<ui::detail::DispatcherOwner>();
    auto view_owner = std::make_shared<int>(0);
    const std::weak_ptr<const void> view_lifetime{view_owner};
    auto domain = std::make_shared<ui::detail::SemanticRetainedViewDomain>(
        dispatcher_owner->dispatcher(), view_lifetime, tree);

    bool capture_called = false;
    NUI_CHECK(dispatcher_owner->dispatcher().post([&view_owner] {
        view_owner.reset();
    }));

    const auto publication = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native_with_capture(
            dispatcher_owner,
            view_lifetime,
            domain,
            [&]() noexcept {
                capture_called = true;
                return geometry(2.0f, 300.0f, 400.0f);
            });

    NUI_CHECK(!publication.has_value());
    NUI_CHECK(!capture_called);
    NUI_CHECK(view_lifetime.expired());
}

void native_checkpoint_retry_keeps_committed_geometry_before_newer_capture() {
    auto state = std::make_shared<ProbeState>();
    ui::Spec spec;
    spec.factory = [state] {
        return std::make_unique<NativeCheckpointProbe>(state);
    };
    auto root = ui::compile(std::move(spec));
    const auto node_id = static_cast<ui::SemanticId>(root->id);

    ui::Tree tree{std::move(root)};
    tree.mount();

    auto dispatcher_owner = std::make_shared<ui::detail::DispatcherOwner>();
    auto view_owner = std::make_shared<int>(0);
    const std::weak_ptr<const void> view_lifetime{view_owner};
    auto domain = std::make_shared<ui::detail::SemanticRetainedViewDomain>(
        dispatcher_owner->dispatcher(), view_lifetime, tree);

    const auto first_geometry = geometry(1.0f, 10.0f, 20.0f);
    NUI_CHECK(ui::detail::SemanticRetainedViewCheckpoint::drain_and_publish_native(
                  dispatcher_owner, view_lifetime, domain, first_geometry)
                  .has_value());
    const auto first_native = domain->bridge().native_current();
    NUI_CHECK(first_native != nullptr);

    auto router = domain->bridge().make_ordinary_router(node_id);
    ui::detail::SemanticActionRequest increment;
    increment.action = ui::SemanticAction::Increment;
    NUI_CHECK(router.post(increment));

    const auto failed_geometry = geometry(1.5f, 100.0f, 200.0f);
    domain->bridge().fail_next_native_publish_at_for_test(
        ui::detail::SemanticNativePublicationState::FailurePointForTest::PublicationAllocation);

    bool threw = false;
    try {
        (void)ui::detail::SemanticRetainedViewCheckpoint::drain_and_publish_native(
            dispatcher_owner, view_lifetime, domain, failed_geometry);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(state->value == 1);
    NUI_CHECK(domain->bridge().has_pending_native_publication());
    NUI_CHECK(domain->bridge().native_current().get() == first_native.get());

    // The durable retry already owns the exact geometry from the failed batch,
    // so a newer platform capture must not run at all until that batch commits.
    const auto newer_geometry = geometry(2.0f, 300.0f, 400.0f);
    int capture_count = 0;
    auto retry = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native_with_capture(
            dispatcher_owner,
            view_lifetime,
            domain,
            [&]() noexcept {
                ++capture_count;
                return newer_geometry;
            });
    NUI_CHECK(retry.has_value());
    NUI_CHECK(capture_count == 0);
    NUI_CHECK(retry->publication != nullptr);
    NUI_CHECK(retry->publication->geometry == failed_geometry);
    NUI_CHECK(!domain->bridge().has_pending_native_publication());

    // Once the exact failed batch commits, the next checkpoint may publish the
    // newer transform as a bounds-only native generation without advancing the
    // logical semantic generation.
    const auto semantic_generation = retry->semantic_generation();
    auto geometry_only = ui::detail::SemanticRetainedViewCheckpoint::drain_and_publish_native(
        dispatcher_owner, view_lifetime, domain, newer_geometry);
    NUI_CHECK(geometry_only.has_value());
    NUI_CHECK(geometry_only->publication != nullptr);
    NUI_CHECK(geometry_only->publication->geometry == newer_geometry);
    NUI_CHECK(geometry_only->semantic_generation() == semantic_generation);
    NUI_CHECK(geometry_only->changes.size() == 1);
    NUI_CHECK(geometry_only->changes.front() == ui::SemanticChange::BoundsChanged);
}

} // namespace

int main() {
    retained_native_checkpoint_pairs_post_dispatch_state_and_geometry();
    retained_native_checkpoint_samples_t043_capture_source_after_dispatch();
    retained_native_checkpoint_refreshes_platform_origin_after_dispatch();
    native_geometry_capture_is_suppressed_after_reentrant_view_death();
    native_checkpoint_retry_keeps_committed_geometry_before_newer_capture();
    return 0;
}
