#include "test_support.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/semantic_tree_action_access.hpp>
#include <nativeui/widgets.hpp>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {

struct ReentrantSemanticState final {
    std::shared_ptr<int> owner_lifetime{std::make_shared<int>(0)};
    bool retire_on_semantics{};
};

class ReentrantSemanticComponent final : public ui::Component {
public:
    explicit ReentrantSemanticComponent(std::shared_ptr<ReentrantSemanticState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {40.0f, 20.0f};
    }

    [[nodiscard]] ui::SemanticInfo semantics() const override {
        ui::SemanticInfo info;
        info.role = ui::SemanticRole::Button;
        info.name = "Reentrant semantic probe";
        info.enabled = true;
        info.actions = {ui::SemanticAction::Activate};

        if (state_->retire_on_semantics) {
            state_->owner_lifetime.reset();
        }
        return info;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<ReentrantSemanticState> state_;
};

struct MutableSemanticState final {
    int value{};
};

class IncrementingSemanticComponent final : public ui::Component,
                                            public ui::detail::SemanticActionHandler {
public:
    explicit IncrementingSemanticComponent(std::shared_ptr<MutableSemanticState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 20.0f};
    }

    [[nodiscard]] ui::SemanticInfo semantics() const override {
        ui::SemanticInfo info;
        info.role = ui::SemanticRole::Slider;
        info.name = "Checkpoint order probe";
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
    std::shared_ptr<MutableSemanticState> state_;
};

void retained_action_bridge_routes_through_live_tree() {
    int activations = 0;
    auto root = ui::compile(ui::make_spec(ui::Button{"Apply", [&] { ++activations; }}));
    const auto button_id = static_cast<ui::SemanticId>(root->id);

    ui::Tree tree{std::move(root)};
    tree.mount();

    const ui::detail::SemanticIdentity identity{button_id, std::nullopt};
    const auto initial = ui::detail::SemanticTreeActionAccess::current_semantics(
        tree, identity);
    NUI_CHECK(initial.has_value());
    NUI_CHECK(initial->role == ui::SemanticRole::Button);
    NUI_CHECK(initial->supports(ui::SemanticAction::Activate));

    auto snapshot = ui::detail::SemanticTreeActionAccess::build_snapshot(tree);
    NUI_CHECK(snapshot.has_value());
    NUI_CHECK(snapshot->root == button_id);
    NUI_CHECK(snapshot->nodes.size() == 1);
    NUI_CHECK(snapshot->nodes.front().id == button_id);
    NUI_CHECK(snapshot->nodes.front().info.role == ui::SemanticRole::Button);

    ui::detail::DispatcherOwner dispatcher_owner;
    auto owner_lifetime = std::make_shared<int>(0);
    ui::detail::SemanticRetainedViewDomain domain{
        dispatcher_owner.dispatcher(),
        std::weak_ptr<const void>{owner_lifetime},
        tree};

    auto publication = domain.checkpoint_snapshot();
    NUI_CHECK(publication.has_value());
    NUI_CHECK(!domain.bridge().has_pending_publication());
    NUI_CHECK(domain.bridge().current() != nullptr);

    auto proxy = domain.bridge().make_ordinary_proxy(button_id);
    auto router = domain.bridge().make_ordinary_router(button_id);
    ui::detail::SemanticActionRequest activate;
    activate.action = ui::SemanticAction::Activate;

    NUI_CHECK(proxy.read().has_value());
    NUI_CHECK(router.post(activate));
    NUI_CHECK(activations == 0);
    NUI_CHECK(dispatcher_owner.checkpoint() == 1);
    NUI_CHECK(activations == 1);

    NUI_CHECK(router.post(activate));
    owner_lifetime.reset();
    NUI_CHECK(dispatcher_owner.checkpoint() == 1);
    NUI_CHECK(activations == 1);

    // The outer checkpoint must fail closed before borrowing the retained Tree
    // again once the owner lifetime is gone. Retiring the domain at that point
    // also makes previously issued native query/action handles defunct.
    NUI_CHECK(!domain.checkpoint_snapshot().has_value());
    NUI_CHECK(!proxy.read().has_value());
    NUI_CHECK(!router.post(activate));
}

void retained_view_domain_rejects_expired_owner_at_construction() {
    auto root = ui::compile(ui::make_spec(ui::Button{"Apply", [] {}}));
    const auto button_id = static_cast<ui::SemanticId>(root->id);

    ui::Tree tree{std::move(root)};
    tree.mount();

    ui::detail::DispatcherOwner dispatcher_owner;
    std::weak_ptr<const void> expired_owner;
    {
        auto owner_lifetime = std::make_shared<int>(0);
        expired_owner = owner_lifetime;
    }

    ui::detail::SemanticRetainedViewDomain domain{
        dispatcher_owner.dispatcher(), expired_owner, tree};

    // Construction is a publication boundary too: a dead owner must not expose
    // a transiently active publisher and wait for the first outer pump to retire.
    NUI_CHECK(domain.bridge().publisher() == nullptr);
    NUI_CHECK(domain.bridge().current() == nullptr);
    NUI_CHECK(!domain.checkpoint_snapshot().has_value());
    NUI_CHECK(!domain.bridge().make_ordinary_proxy(button_id).read().has_value());
}

void retained_checkpoint_observes_dispatcher_mutation_after_drain() {
    auto state = std::make_shared<MutableSemanticState>();
    ui::Spec spec;
    spec.factory = [state] {
        return std::make_unique<IncrementingSemanticComponent>(state);
    };
    auto root = ui::compile(std::move(spec));
    const auto node_id = static_cast<ui::SemanticId>(root->id);

    ui::Tree tree{std::move(root)};
    tree.mount();

    auto dispatcher_owner = std::make_shared<ui::detail::DispatcherOwner>();
    auto owner_lifetime = std::make_shared<int>(0);
    const std::weak_ptr<const void> view_lifetime{owner_lifetime};
    auto domain = std::make_shared<ui::detail::SemanticRetainedViewDomain>(
        dispatcher_owner->dispatcher(),
        view_lifetime,
        tree);

    NUI_CHECK(domain->checkpoint_snapshot().has_value());
    auto proxy = domain->bridge().make_ordinary_proxy(node_id);
    auto router = domain->bridge().make_ordinary_router(node_id);

    auto initial = proxy.read();
    NUI_CHECK(initial.has_value());
    NUI_CHECK(initial->info().numeric_value == std::optional<double>{0.0});
    const auto initial_generation = initial->generation();

    ui::detail::SemanticActionRequest increment;
    increment.action = ui::SemanticAction::Increment;
    NUI_CHECK(router.post(increment));

    // A semantic checkpoint before the existing T065 drain can only observe
    // pre-action retained state and therefore cannot publish the accepted native
    // mutation yet. Production uses SemanticRetainedViewCheckpoint below to
    // enforce the inverse ordering at the native-view pump boundary.
    auto before_drain = domain->checkpoint_snapshot();
    NUI_CHECK(before_drain.has_value());
    NUI_CHECK(before_drain->empty());
    auto still_old = proxy.read();
    NUI_CHECK(still_old.has_value());
    NUI_CHECK(still_old->generation() == initial_generation);
    NUI_CHECK(still_old->info().numeric_value == std::optional<double>{0.0});

    auto after_drain = ui::detail::SemanticRetainedViewCheckpoint::drain_and_publish(
        dispatcher_owner, view_lifetime, domain);
    NUI_CHECK(after_drain.has_value());
    NUI_CHECK(state->value == 1);
    NUI_CHECK(after_drain->changes.size() == 1);
    NUI_CHECK(after_drain->changes.front() == ui::SemanticChange::ValueChanged);

    auto updated = proxy.read();
    NUI_CHECK(updated.has_value());
    NUI_CHECK(updated->generation() == initial_generation + 1);
    NUI_CHECK(updated->info().numeric_value == std::optional<double>{1.0});
}

void semantic_checkpoint_retires_after_reentrant_owner_death() {
    auto state = std::make_shared<ReentrantSemanticState>();

    ui::Spec spec;
    spec.factory = [state] {
        return std::make_unique<ReentrantSemanticComponent>(state);
    };
    auto root = ui::compile(std::move(spec));
    const auto node_id = static_cast<ui::SemanticId>(root->id);

    ui::Tree tree{std::move(root)};
    tree.mount();

    ui::detail::DispatcherOwner dispatcher_owner;
    ui::detail::SemanticRetainedViewDomain domain{
        dispatcher_owner.dispatcher(),
        std::weak_ptr<const void>{state->owner_lifetime},
        tree};

    NUI_CHECK(domain.checkpoint_snapshot().has_value());
    auto proxy = domain.bridge().make_ordinary_proxy(node_id);
    NUI_CHECK(proxy.read().has_value());

    // Component::semantics() is application code and can re-enter surrounding
    // view teardown. The checkpoint must not keep the owner token alive while
    // projecting the retained tree or publish a candidate after that teardown.
    state->retire_on_semantics = true;
    NUI_CHECK(!domain.checkpoint_snapshot().has_value());
    NUI_CHECK(!state->owner_lifetime);
    NUI_CHECK(domain.bridge().current() == nullptr);
    NUI_CHECK(!proxy.read().has_value());
}

void retained_view_domain_raii_teardown_keeps_inflight_read_safe() {
    int activations = 0;
    auto root = ui::compile(ui::make_spec(ui::Button{"Apply", [&] { ++activations; }}));
    const auto button_id = static_cast<ui::SemanticId>(root->id);

    ui::Tree tree{std::move(root)};
    tree.mount();

    ui::detail::DispatcherOwner dispatcher_owner;
    auto owner_lifetime = std::make_shared<int>(0);
    std::optional<ui::detail::SemanticSnapshotProxy> proxy;
    std::optional<ui::detail::SemanticActionRouter> router;
    std::optional<ui::detail::SemanticSnapshotRead> inflight_read;
    ui::detail::SemanticActionRequest activate;
    activate.action = ui::SemanticAction::Activate;

    {
        ui::detail::SemanticRetainedViewDomain domain{
            dispatcher_owner.dispatcher(),
            std::weak_ptr<const void>{owner_lifetime},
            tree};

        NUI_CHECK(domain.checkpoint_snapshot().has_value());
        proxy = domain.bridge().make_ordinary_proxy(button_id);
        router = domain.bridge().make_ordinary_router(button_id);
        inflight_read = proxy->read();

        NUI_CHECK(inflight_read.has_value());
        NUI_CHECK(router->post(activate));
        NUI_CHECK(activations == 0);
    }

    // ViewCore will own this domain by RAII. Destruction must publish endpoint
    // death even when its wider UI owner remains alive: weak native handles are
    // immediately defunct and already queued work becomes a no-op. A callback
    // that retained one immutable generation before teardown remains safe.
    NUI_CHECK(inflight_read.has_value());
    NUI_CHECK(inflight_read->node_id() == button_id);
    NUI_CHECK(inflight_read->info().name == "Apply");
    NUI_CHECK(!proxy->read().has_value());
    NUI_CHECK(!router->post(activate));
    NUI_CHECK(dispatcher_owner.checkpoint() == 1);
    NUI_CHECK(activations == 0);
}

void retained_view_checkpoint_stops_after_reentrant_view_teardown() {
    auto root = ui::compile(ui::make_spec(ui::Button{"Apply", [] {}}));
    ui::Tree tree{std::move(root)};
    tree.mount();

    auto dispatcher_owner = std::make_shared<ui::detail::DispatcherOwner>();
    auto view_owner = std::make_shared<int>(0);
    const std::weak_ptr<const void> view_lifetime{view_owner};
    auto domain = std::make_shared<ui::detail::SemanticRetainedViewDomain>(
        dispatcher_owner->dispatcher(), view_lifetime, tree);
    std::weak_ptr<ui::detail::SemanticRetainedViewDomain> domain_lifetime = domain;
    NUI_CHECK(domain->checkpoint_snapshot().has_value());

    auto dispatcher = dispatcher_owner->dispatcher();
    NUI_CHECK(dispatcher.post([&] {
        // Production view teardown publishes lifetime death before releasing its
        // semantic domain. drain_and_publish() owns an independent domain lease,
        // so this release cannot invalidate its stack while the T065 drain runs.
        view_owner.reset();
        domain.reset();
    }));

    auto publication = ui::detail::SemanticRetainedViewCheckpoint::drain_and_publish(
        dispatcher_owner, view_lifetime, domain);
    NUI_CHECK(!publication.has_value());
    NUI_CHECK(!domain);
    NUI_CHECK(domain_lifetime.expired());
    NUI_CHECK(dispatcher_owner->pending_task_count() == 0);
}

void retained_view_checkpoint_drains_dispatcher_after_semantic_retirement() {
    auto root = ui::compile(ui::make_spec(ui::Button{"Apply", [] {}}));
    ui::Tree tree{std::move(root)};
    tree.mount();

    auto dispatcher_owner = std::make_shared<ui::detail::DispatcherOwner>();
    auto view_owner = std::make_shared<int>(0);
    const std::weak_ptr<const void> view_lifetime{view_owner};
    auto domain = std::make_shared<ui::detail::SemanticRetainedViewDomain>(
        dispatcher_owner->dispatcher(), view_lifetime, tree);
    NUI_CHECK(domain->checkpoint_snapshot().has_value());

    domain->shutdown();
    view_owner.reset();
    NUI_CHECK(domain->bridge().publisher() == nullptr);

    int callbacks = 0;
    auto dispatcher = dispatcher_owner->dispatcher();
    NUI_CHECK(dispatcher.post([&] { ++callbacks; }));

    // Semantic retirement must not suppress the owning T065 checkpoint. The
    // dispatcher can contain unrelated accepted work even when accessibility is
    // already defunct; only semantic publication is gated by view lifetime.
    auto publication = ui::detail::SemanticRetainedViewCheckpoint::drain_and_publish(
        dispatcher_owner, view_lifetime, domain);
    NUI_CHECK(!publication.has_value());
    NUI_CHECK(callbacks == 1);
    NUI_CHECK(dispatcher_owner->pending_task_count() == 0);
}

void sibling_retained_view_domains_isolate_shutdown() {
    int activations = 0;
    auto root = ui::compile(ui::make_spec(ui::Button{"Apply", [&] { ++activations; }}));
    const auto button_id = static_cast<ui::SemanticId>(root->id);

    ui::Tree tree{std::move(root)};
    tree.mount();

    ui::detail::DispatcherOwner dispatcher_a;
    ui::detail::DispatcherOwner dispatcher_b;
    auto owner_lifetime = std::make_shared<int>(0);
    ui::detail::SemanticRetainedViewDomain domain_a{
        dispatcher_a.dispatcher(),
        std::weak_ptr<const void>{owner_lifetime},
        tree};
    ui::detail::SemanticRetainedViewDomain domain_b{
        dispatcher_b.dispatcher(),
        std::weak_ptr<const void>{owner_lifetime},
        tree};

    NUI_CHECK(domain_a.checkpoint_snapshot().has_value());
    NUI_CHECK(domain_b.checkpoint_snapshot().has_value());

    auto proxy_a = domain_a.bridge().make_ordinary_proxy(button_id);
    auto proxy_b = domain_b.bridge().make_ordinary_proxy(button_id);
    auto router_a = domain_a.bridge().make_ordinary_router(button_id);
    auto router_b = domain_b.bridge().make_ordinary_router(button_id);
    ui::detail::SemanticActionRequest activate;
    activate.action = ui::SemanticAction::Activate;

    NUI_CHECK(proxy_a.read().has_value());
    NUI_CHECK(proxy_b.read().has_value());
    NUI_CHECK(router_a.post(activate));
    NUI_CHECK(router_b.post(activate));

    domain_a.shutdown();
    domain_a.shutdown();
    NUI_CHECK(!proxy_a.read().has_value());
    NUI_CHECK(proxy_b.read().has_value());
    NUI_CHECK(!router_a.post(activate));
    NUI_CHECK(!domain_a.checkpoint_snapshot().has_value());
    NUI_CHECK(domain_b.checkpoint_snapshot().has_value());

    NUI_CHECK(dispatcher_a.checkpoint() == 1);
    NUI_CHECK(dispatcher_b.checkpoint() == 1);
    NUI_CHECK(activations == 1);

    NUI_CHECK(router_b.post(activate));
    NUI_CHECK(dispatcher_b.checkpoint() == 1);
    NUI_CHECK(activations == 2);

    NUI_CHECK(router_b.post(activate));
    owner_lifetime.reset();
    NUI_CHECK(dispatcher_b.checkpoint() == 1);
    NUI_CHECK(activations == 2);
}

void suite() {
    retained_action_bridge_routes_through_live_tree();
    retained_view_domain_rejects_expired_owner_at_construction();
    retained_checkpoint_observes_dispatcher_mutation_after_drain();
    semantic_checkpoint_retires_after_reentrant_owner_death();
    retained_view_domain_raii_teardown_keeps_inflight_read_safe();
    retained_view_checkpoint_stops_after_reentrant_view_teardown();
    retained_view_checkpoint_drains_dispatcher_after_semantic_retirement();
    sibling_retained_view_domains_isolate_shutdown();
}

} // namespace

int main() { return test::run("t068_retained_action_bridge", &suite); }
