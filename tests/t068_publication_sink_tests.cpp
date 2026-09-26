#include "test_support.hpp"

#include "../src/detail/semantic_native_bounds.hpp"

#include <nativeui/component_tree.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/semantic_tree_action_access.hpp>

#include <algorithm>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

struct ProbeState final {
    int value{};
};

class PublicationProbe final : public ui::Component,
                                public ui::detail::SemanticActionHandler {
public:
    explicit PublicationProbe(std::shared_ptr<ProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 24.0f};
    }

    [[nodiscard]] ui::SemanticInfo semantics() const override {
        ui::SemanticInfo info;
        info.role = ui::SemanticRole::Slider;
        info.name = "Publication sink probe";
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

/// Records every sink delivery so tests can assert exact-once delivery,
/// pointer identity with the committed publication and copied geometry.
class RecordingSink final : public ui::detail::SemanticNativeNotificationSink {
public:
    void on_native_publication(
        const ui::detail::SemanticNativePublicationBatch& batch) override {
        ++calls;
        last_publication = batch.publication;
        last_changes = batch.changes;
        last_geometry = batch.publication
            ? batch.publication->geometry
            : ui::detail::SemanticNativeGeometry{};
    }

    int calls{};
    std::shared_ptr<const ui::detail::SemanticNativePublicationSnapshot>
        last_publication;
    std::vector<ui::SemanticChange> last_changes;
    ui::detail::SemanticNativeGeometry last_geometry{};
};

/// Retires the whole domain from inside delivery. The in-flight delivery must
/// still complete safely and the log must contain no later event.
class RetiringSink final : public ui::detail::SemanticNativeNotificationSink {
public:
    explicit RetiringSink(ui::detail::SemanticRetainedViewDomain* domain) noexcept
        : domain_(domain) {}

    void on_native_publication(
        const ui::detail::SemanticNativePublicationBatch&) override {
        ++calls;
        if (domain_) domain_->shutdown();
    }

    int calls{};

private:
    ui::detail::SemanticRetainedViewDomain* domain_{};
};

/// Throws on the configured delivery index and must otherwise behave like the
/// recording sink, so the recovery assertion proves later deliveries still run.
class ThrowingSink final : public ui::detail::SemanticNativeNotificationSink {
public:
    void on_native_publication(
        const ui::detail::SemanticNativePublicationBatch& batch) override {
        ++calls;
        last_publication = batch.publication;
        last_changes = batch.changes;
        if (calls == throw_on_call) {
            throw std::runtime_error("injected native notification sink failure");
        }
    }

    int calls{};
    int throw_on_call{1};
    std::shared_ptr<const ui::detail::SemanticNativePublicationSnapshot>
        last_publication;
    std::vector<ui::SemanticChange> last_changes;
};

[[nodiscard]] ui::Tree make_probe_tree(
    const std::shared_ptr<ProbeState>& state,
    ui::SemanticId& node_id) {
    ui::Spec spec;
    spec.factory = [state] {
        return std::make_unique<PublicationProbe>(state);
    };
    auto root = ui::compile(std::move(spec));
    node_id = static_cast<ui::SemanticId>(root->id);
    return ui::Tree{std::move(root)};
}

struct Fixture final {
    std::shared_ptr<ProbeState> state{std::make_shared<ProbeState>()};
    ui::SemanticId node_id{};
    ui::Tree tree;
    std::shared_ptr<ui::detail::DispatcherOwner> dispatcher_owner{
        std::make_shared<ui::detail::DispatcherOwner>()};
    std::shared_ptr<int> view_owner{std::make_shared<int>(0)};
    std::weak_ptr<const void> view_lifetime{view_owner};
    std::shared_ptr<ui::detail::SemanticRetainedViewDomain> domain;
    std::shared_ptr<RecordingSink> sink{std::make_shared<RecordingSink>()};

    explicit Fixture()
        : tree(make_probe_tree(state, node_id)),
          domain(std::make_shared<ui::detail::SemanticRetainedViewDomain>(
              dispatcher_owner->dispatcher(), view_lifetime, tree)) {
        tree.mount();
    }

    void post_increment() {
        auto router = domain->bridge().make_ordinary_router(node_id);
        ui::detail::SemanticActionRequest increment;
        increment.action = ui::SemanticAction::Increment;
        NUI_CHECK(router.post(increment));
    }
};

[[nodiscard]] std::optional<ui::detail::SemanticNativePublicationBatch>
checkpoint(Fixture& fixture, ui::detail::SemanticNativeGeometry value) {
    return ui::detail::SemanticRetainedViewCheckpoint::drain_and_publish_native(
        fixture.dispatcher_owner,
        fixture.view_lifetime,
        fixture.domain,
        value);
}

void sink_observes_each_committed_native_batch_exactly_once() {
    Fixture fixture;
    NUI_CHECK(fixture.domain->set_native_notification_sink(fixture.sink));

    const auto first_geometry = geometry(1.0f, 10.0f, 20.0f);
    auto first = checkpoint(fixture, first_geometry);
    NUI_CHECK(first.has_value());
    NUI_CHECK(fixture.sink->calls == 1);
    NUI_CHECK(fixture.sink->last_publication == first->publication);
    NUI_CHECK(fixture.sink->last_geometry == first_geometry);
    NUI_CHECK(has_change(fixture.sink->last_changes, ui::SemanticChange::StructureChanged));

    // An unchanged checkpoint consumes the staged candidate without committing
    // a new native generation, so the previous (superseded) batch is not
    // replayed to the sink.
    auto repeated = checkpoint(fixture, first_geometry);
    NUI_CHECK(repeated.has_value());
    NUI_CHECK(repeated->changes.empty());
    NUI_CHECK(fixture.sink->calls == 1);

    // One committed retained change commits one new native generation and
    // reaches the sink exactly once.
    fixture.post_increment();
    auto changed = checkpoint(fixture, first_geometry);
    NUI_CHECK(changed.has_value());
    NUI_CHECK(fixture.state->value == 1);
    NUI_CHECK(changed->generation() == first->generation() + 1);
    NUI_CHECK(has_change(changed->changes, ui::SemanticChange::ValueChanged));
    NUI_CHECK(fixture.sink->calls == 2);
    NUI_CHECK(fixture.sink->last_publication == changed->publication);
    NUI_CHECK(fixture.sink->last_publication != first->publication);

    // A geometry-only commit is exactly one BoundsChanged delivery and keeps
    // the logical semantic generation unchanged.
    const auto moved_geometry = geometry(2.0f, 33.0f, 44.0f);
    auto moved = checkpoint(fixture, moved_geometry);
    NUI_CHECK(moved.has_value());
    NUI_CHECK(moved->semantic_generation() == changed->semantic_generation());
    NUI_CHECK(moved->changes.size() == 1);
    NUI_CHECK(moved->changes.front() == ui::SemanticChange::BoundsChanged);
    NUI_CHECK(fixture.sink->calls == 3);
    NUI_CHECK(fixture.sink->last_geometry == moved_geometry);
    NUI_CHECK(fixture.domain->bridge().native_current() == moved->publication);

    // The newest commit is not re-delivered by a following idle checkpoint.
    auto idle = checkpoint(fixture, moved_geometry);
    NUI_CHECK(idle.has_value());
    NUI_CHECK(fixture.sink->calls == 3);
}

void durable_retry_delivers_the_exact_pending_batch_once() {
    Fixture fixture;
    NUI_CHECK(fixture.domain->set_native_notification_sink(fixture.sink));

    const auto first_geometry = geometry(1.0f, 10.0f, 20.0f);
    NUI_CHECK(checkpoint(fixture, first_geometry).has_value());
    NUI_CHECK(fixture.sink->calls == 1);
    const auto first_publication = fixture.domain->bridge().native_current();

    fixture.post_increment();
    const auto failed_geometry = geometry(1.5f, 100.0f, 200.0f);
    fixture.domain->bridge().fail_next_native_publish_at_for_test(
        ui::detail::SemanticNativePublicationState::FailurePointForTest::PublicationAllocation);

    bool threw = false;
    try {
        (void)checkpoint(fixture, failed_geometry);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(fixture.sink->calls == 1);
    NUI_CHECK(fixture.domain->bridge().has_pending_native_publication());
    NUI_CHECK(fixture.domain->bridge().native_current() == first_publication);

    // The durable retry owns the exact failed geometry and must deliver that
    // committed batch exactly once before any newer capture is sampled.
    int capture_count = 0;
    auto retry = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native_with_capture(
            fixture.dispatcher_owner,
            fixture.view_lifetime,
            fixture.domain,
            [&]() noexcept {
                ++capture_count;
                return geometry(2.0f, 300.0f, 400.0f);
            });
    NUI_CHECK(retry.has_value());
    NUI_CHECK(capture_count == 0);
    NUI_CHECK(retry->publication != nullptr);
    NUI_CHECK(retry->publication->geometry == failed_geometry);
    NUI_CHECK(fixture.sink->calls == 2);
    NUI_CHECK(fixture.sink->last_publication == retry->publication);
    NUI_CHECK(fixture.sink->last_geometry == failed_geometry);
    NUI_CHECK(has_change(fixture.sink->last_changes, ui::SemanticChange::ValueChanged));
    NUI_CHECK(!fixture.domain->bridge().has_pending_native_publication());

    // Retry delivery is not duplicated by the following checkpoint.
    auto geometry_only = checkpoint(fixture, geometry(2.0f, 300.0f, 400.0f));
    NUI_CHECK(geometry_only.has_value());
    NUI_CHECK(geometry_only->changes.size() == 1);
    NUI_CHECK(geometry_only->changes.front() == ui::SemanticChange::BoundsChanged);
    NUI_CHECK(fixture.sink->calls == 3);
}

void shutdown_and_retirement_never_deliver() {
    Fixture fixture;
    NUI_CHECK(fixture.domain->set_native_notification_sink(fixture.sink));
    NUI_CHECK(checkpoint(fixture, geometry(1.0f, 0.0f, 0.0f)).has_value());
    NUI_CHECK(fixture.sink->calls == 1);

    fixture.domain->shutdown();
    NUI_CHECK(!fixture.domain->set_native_notification_sink(nullptr));
    NUI_CHECK(!fixture.domain->set_native_notification_sink(
        std::make_shared<RecordingSink>()));
    NUI_CHECK(!fixture.domain->checkpoint_native_publication(
                   geometry(2.0f, 5.0f, 6.0f))
                   .has_value());
    NUI_CHECK(!fixture.domain->retry_pending_native_publication().has_value());
    NUI_CHECK(!fixture.domain->checkpoint_publication().has_value());
    NUI_CHECK(fixture.sink->calls == 1);

    // Owner/view death retires the domain from the checkpoint helper before a
    // candidate can be staged or delivered.
    Fixture dying;
    NUI_CHECK(dying.domain->set_native_notification_sink(dying.sink));
    NUI_CHECK(checkpoint(dying, geometry(1.0f, 1.0f, 2.0f)).has_value());
    NUI_CHECK(dying.sink->calls == 1);

    dying.view_owner.reset();
    NUI_CHECK(!checkpoint(dying, geometry(2.0f, 3.0f, 4.0f)).has_value());
    NUI_CHECK(!dying.domain->checkpoint_native_publication(
                   geometry(2.0f, 3.0f, 4.0f))
                   .has_value());
    NUI_CHECK(dying.sink->calls == 1);
    NUI_CHECK(!dying.domain->bridge().publisher());
}

void throwing_sink_is_contained_and_later_publication_recovers() {
    Fixture fixture;
    auto sink = std::make_shared<ThrowingSink>();
    NUI_CHECK(fixture.domain->set_native_notification_sink(sink));

    auto first = checkpoint(fixture, geometry(1.0f, 0.0f, 0.0f));
    NUI_CHECK(first.has_value());
    NUI_CHECK(sink->calls == 1);
    // Containment must not roll back or lose the committed batch.
    NUI_CHECK(fixture.domain->bridge().native_current() == first->publication);
    NUI_CHECK(has_change(first->changes, ui::SemanticChange::StructureChanged));

    fixture.post_increment();
    auto recovered = checkpoint(fixture, geometry(1.0f, 0.0f, 0.0f));
    NUI_CHECK(recovered.has_value());
    NUI_CHECK(recovered->generation() == first->generation() + 1);
    NUI_CHECK(has_change(recovered->changes, ui::SemanticChange::ValueChanged));
    NUI_CHECK(sink->calls == 2);
    NUI_CHECK(sink->last_publication == recovered->publication);
}

void sink_may_retire_the_domain_during_delivery() {
    Fixture fixture;
    auto sink = std::make_shared<RetiringSink>(fixture.domain.get());
    NUI_CHECK(fixture.domain->set_native_notification_sink(sink));

    auto committed = checkpoint(fixture, geometry(1.0f, 2.0f, 3.0f));
    NUI_CHECK(committed.has_value());
    NUI_CHECK(committed->publication != nullptr);
    NUI_CHECK(sink->calls == 1);

    // The in-flight delivery completed after the domain retired itself; the
    // committed immutable batch stays readable and no later checkpoint reaches
    // the sink or resurrects the domain.
    NUI_CHECK(!fixture.domain->bridge().publisher());
    NUI_CHECK(!fixture.domain->set_native_notification_sink(nullptr));
    NUI_CHECK(!checkpoint(fixture, geometry(2.0f, 4.0f, 5.0f)).has_value());
    NUI_CHECK(!fixture.domain->retry_pending_native_publication().has_value());
    NUI_CHECK(sink->calls == 1);
    NUI_CHECK(committed->publication->geometry == geometry(1.0f, 2.0f, 3.0f));
}

void superseded_and_foreign_batches_never_reach_the_sink() {
    Fixture a;
    Fixture b;
    NUI_CHECK(a.domain->set_native_notification_sink(a.sink));
    NUI_CHECK(b.domain->set_native_notification_sink(b.sink));

    auto first_a = checkpoint(a, geometry(1.0f, 1.0f, 1.0f));
    NUI_CHECK(first_a.has_value());
    NUI_CHECK(a.sink->calls == 1);
    NUI_CHECK(b.sink->calls == 0);
    NUI_CHECK(a.sink->last_publication == a.domain->bridge().native_current());
    NUI_CHECK(a.sink->last_publication != b.domain->bridge().native_current());

    auto first_b = checkpoint(b, geometry(3.0f, 7.0f, 8.0f));
    NUI_CHECK(first_b.has_value());
    NUI_CHECK(b.sink->calls == 1);
    NUI_CHECK(a.sink->calls == 1);
    NUI_CHECK(b.sink->last_publication == b.domain->bridge().native_current());
    NUI_CHECK(a.sink->last_publication != b.sink->last_publication);

    // A no-op checkpoint returns the current publication but must not replay it
    // (superseded batch) or forward the other view's batch (foreign batch).
    auto idle = checkpoint(a, geometry(1.0f, 1.0f, 1.0f));
    NUI_CHECK(idle.has_value());
    NUI_CHECK(a.sink->calls == 1);
    NUI_CHECK(a.sink->last_publication != first_b->publication);

    // A domain with no durable pending batch never fabricates a delivery.
    NUI_CHECK(!a.domain->retry_pending_native_publication().has_value());
    NUI_CHECK(a.sink->calls == 1);

    auto moved_a = checkpoint(a, geometry(2.0f, 9.0f, 9.0f));
    NUI_CHECK(moved_a.has_value());
    NUI_CHECK(moved_a->semantic_generation() == first_a->semantic_generation());
    NUI_CHECK(moved_a->changes.size() == 1);
    NUI_CHECK(moved_a->changes.front() == ui::SemanticChange::BoundsChanged);
    NUI_CHECK(a.sink->calls == 2);
    NUI_CHECK(a.sink->last_publication == moved_a->publication);
    NUI_CHECK(a.sink->last_publication != first_a->publication);
    NUI_CHECK(b.sink->calls == 1);
}

void dispatcher_faults_never_deliver_or_poison_publication() {
    Fixture fixture;
    NUI_CHECK(fixture.domain->set_native_notification_sink(fixture.sink));
    NUI_CHECK(checkpoint(fixture, geometry(1.0f, 0.0f, 0.0f)).has_value());
    NUI_CHECK(fixture.sink->calls == 1);

    // A callback that throws during the drain aborts publication before the
    // domain commits; the sink observes nothing and the next checkpoint
    // recovers with the accepted increment.
    NUI_CHECK(fixture.dispatcher_owner->dispatcher().post([] {
        throw std::runtime_error("injected dispatcher callback failure");
    }));
    bool threw = false;
    try {
        (void)checkpoint(fixture, geometry(1.0f, 0.0f, 0.0f));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(fixture.sink->calls == 1);
    NUI_CHECK(!fixture.domain->bridge().has_pending_native_publication());

    fixture.post_increment();
    auto recovered = checkpoint(fixture, geometry(1.0f, 0.0f, 0.0f));
    NUI_CHECK(recovered.has_value());
    NUI_CHECK(has_change(recovered->changes, ui::SemanticChange::ValueChanged));
    NUI_CHECK(fixture.sink->calls == 2);

    // An enqueue that throws before accepting work is not silently converted
    // into a delivery or into lost publication on a later checkpoint.
    ui::detail::DispatcherTestAccess::fail_next_post(
        fixture.dispatcher_owner->dispatcher());
    bool enqueue_threw = false;
    try {
        (void)fixture.dispatcher_owner->dispatcher().post([] {});
    } catch (const std::bad_alloc&) {
        enqueue_threw = true;
    }
    NUI_CHECK(enqueue_threw);
    auto after_enqueue_failure = checkpoint(fixture, geometry(2.0f, 5.0f, 6.0f));
    NUI_CHECK(after_enqueue_failure.has_value());
    NUI_CHECK(after_enqueue_failure->changes.size() == 1);
    NUI_CHECK(fixture.sink->calls == 3);

    // A closed/rejecting dispatcher namespace accepts no work, yet the view's
    // semantic publication still commits exactly once without a stale delivery.
    auto closed_owner = std::make_shared<ui::detail::DispatcherOwner>();
    closed_owner->shutdown();
    NUI_CHECK(!closed_owner->dispatcher().post([] {}));
    auto rejected = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native(
            closed_owner,
            fixture.view_lifetime,
            fixture.domain,
            geometry(3.0f, 7.0f, 8.0f));
    NUI_CHECK(rejected.has_value());
    NUI_CHECK(rejected->changes.size() == 1);
    NUI_CHECK(rejected->changes.front() == ui::SemanticChange::BoundsChanged);
    NUI_CHECK(fixture.sink->calls == 4);
    NUI_CHECK(fixture.sink->last_geometry == geometry(3.0f, 7.0f, 8.0f));
}

void reentrant_view_death_during_drain_suppresses_delivery() {
    Fixture fixture;
    NUI_CHECK(fixture.domain->set_native_notification_sink(fixture.sink));
    NUI_CHECK(checkpoint(fixture, geometry(1.0f, 0.0f, 0.0f)).has_value());
    NUI_CHECK(fixture.sink->calls == 1);

    // Model ViewCore::retire_semantics() from accepted dispatcher work while the
    // pump still holds its independent domain lease.
    auto* domain = fixture.domain.get();
    auto* view_owner = &fixture.view_owner;
    NUI_CHECK(fixture.dispatcher_owner->dispatcher().post([domain, view_owner] {
        view_owner->reset();
        domain->shutdown();
    }));

    bool capture_called = false;
    auto publication = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native_with_capture(
            fixture.dispatcher_owner,
            fixture.view_lifetime,
            fixture.domain,
            [&]() noexcept {
                capture_called = true;
                return geometry(2.0f, 3.0f, 4.0f);
            });

    NUI_CHECK(!publication.has_value());
    NUI_CHECK(!capture_called);
    NUI_CHECK(fixture.view_lifetime.expired());
    NUI_CHECK(fixture.sink->calls == 1);
}

void invalid_physical_origin_preserves_previous_publication() {
    Fixture fixture;
    NUI_CHECK(fixture.domain->set_native_notification_sink(fixture.sink));

    ui::detail::ViewGeometryState view_geometry{{80.0f, 24.0f}};
    NUI_CHECK(view_geometry.observe_scale(1.5f));
    NUI_CHECK(view_geometry.observe_physical_screen_origin({20.0f, 30.0f}));
    const ui::detail::SemanticNativeGeometryCaptureLease capture{
        view_geometry.retain_native_geometry_capture_state()};

    auto first = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native_with_capture(
            fixture.dispatcher_owner,
            fixture.view_lifetime,
            fixture.domain,
            [&]() noexcept { return capture(); });
    NUI_CHECK(first.has_value());
    NUI_CHECK(first->publication->geometry == geometry(1.5f, 20.0f, 30.0f));
    NUI_CHECK(fixture.sink->calls == 1);

    // A rejected platform scale/origin report fails closed: T043 keeps the last
    // valid pair and the checkpoint must not manufacture a delivery for it.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    NUI_CHECK(!view_geometry.observe_physical_screen_origin({nan, 30.0f}));
    NUI_CHECK(!view_geometry.observe_scale(-1.0f));
    auto rejected = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native_with_capture(
            fixture.dispatcher_owner,
            fixture.view_lifetime,
            fixture.domain,
            [&]() noexcept { return capture(); });
    NUI_CHECK(rejected.has_value());
    NUI_CHECK(rejected->changes.empty());
    NUI_CHECK(fixture.sink->calls == 1);
    NUI_CHECK(fixture.domain->bridge().native_current()->geometry ==
              geometry(1.5f, 20.0f, 30.0f));

    // The next valid platform report still commits and delivers exactly once.
    NUI_CHECK(view_geometry.observe_physical_screen_origin({-7.0f, 9.0f}));
    auto recovered = ui::detail::SemanticRetainedViewCheckpoint::
        drain_and_publish_native_with_capture(
            fixture.dispatcher_owner,
            fixture.view_lifetime,
            fixture.domain,
            [&]() noexcept { return capture(); });
    NUI_CHECK(recovered.has_value());
    NUI_CHECK(recovered->changes.size() == 1);
    NUI_CHECK(recovered->changes.front() == ui::SemanticChange::BoundsChanged);
    NUI_CHECK(fixture.sink->calls == 2);
    NUI_CHECK(fixture.sink->last_geometry == geometry(1.5f, -7.0f, 9.0f));
    NUI_CHECK(recovered->semantic_generation() == first->semantic_generation());
}

} // namespace

int main() {
    return test::run("t068_publication_sink_tests", [] {
        sink_observes_each_committed_native_batch_exactly_once();
        durable_retry_delivers_the_exact_pending_batch_once();
        shutdown_and_retirement_never_deliver();
        throwing_sink_is_contained_and_later_publication_recovers();
        sink_may_retire_the_domain_during_delivery();
        superseded_and_foreign_batches_never_reach_the_sink();
        dispatcher_faults_never_deliver_or_poison_publication();
        reentrant_view_death_during_drain_suppresses_delivery();
        invalid_physical_origin_preserves_previous_publication();
    });
}
