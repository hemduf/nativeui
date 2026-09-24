#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/semantic_native_view_bridge.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

ui::SemanticTreeSnapshot button_snapshot(bool enabled = true) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 42;

    ui::SemanticNodeSnapshot node;
    node.id = 42;
    node.info.role = ui::SemanticRole::Button;
    node.info.name = "Target";
    node.info.enabled = enabled;
    node.info.actions = {ui::SemanticAction::Activate};
    tree.nodes.push_back(std::move(node));
    return tree;
}

class RecordingTarget final : public ui::detail::SemanticActionTarget {
public:
    RecordingTarget(std::shared_ptr<int> count, bool enabled = true)
        : count_(std::move(count)) {
        info_.role = ui::SemanticRole::Button;
        info_.name = "Target";
        info_.enabled = enabled;
        info_.actions = {ui::SemanticAction::Activate};
    }

    std::optional<ui::SemanticInfo> current_semantics(
        const ui::detail::SemanticIdentity& identity) const override {
        if (identity.node_id != 42 || identity.virtual_token) {
            return std::nullopt;
        }
        return info_;
    }

    bool dispatch_semantic_action(
        const ui::detail::SemanticIdentity& identity,
        const ui::detail::SemanticActionRequest& request) override {
        if (identity.node_id != 42 || identity.virtual_token ||
            request.action != ui::SemanticAction::Activate) {
            return false;
        }
        ++*count_;
        return true;
    }

private:
    std::shared_ptr<int> count_;
    ui::SemanticInfo info_;
};

ui::detail::SemanticActionRequest activate_request() {
    ui::detail::SemanticActionRequest request;
    request.action = ui::SemanticAction::Activate;
    return request;
}

void bridge_routes_against_its_own_snapshot_and_target() {
    ui::detail::DispatcherOwner dispatcher_owner;
    ui::detail::SemanticNativeViewBridge bridge;
    auto count = std::make_shared<int>(0);
    auto target = std::make_shared<RecordingTarget>(count);

    bridge.bind_actions(dispatcher_owner.dispatcher(), target);
    target.reset();
    T068_CHECK(!bridge.publish(button_snapshot()).empty());

    auto router = bridge.make_ordinary_router(42);
    T068_CHECK(router.post(activate_request()));
    T068_CHECK(*count == 0);
    T068_CHECK(dispatcher_owner.checkpoint() == 1);
    T068_CHECK(*count == 1);
}

void bridge_exposes_lifetime_safe_native_action_endpoint() {
    ui::detail::DispatcherOwner dispatcher_owner;
    ui::detail::SemanticNativeViewBridge bridge;
    auto count = std::make_shared<int>(0);
    auto target = std::make_shared<RecordingTarget>(count);

    bridge.bind_actions(dispatcher_owner.dispatcher(), target);
    target.reset();
    T068_CHECK(!bridge.publish(button_snapshot()).empty());

    auto weak_endpoint = bridge.native_action_endpoint();
    auto endpoint = weak_endpoint.lock();
    T068_CHECK(endpoint != nullptr);

    const ui::detail::SemanticIdentity identity{42, std::nullopt};
    T068_CHECK(endpoint->post(identity, activate_request()));
    T068_CHECK(*count == 0);
    T068_CHECK(dispatcher_owner.checkpoint() == 1);
    T068_CHECK(*count == 1);

    bridge.unbind_actions();
    T068_CHECK(!endpoint->post(identity, activate_request()));
    T068_CHECK(dispatcher_owner.checkpoint() == 0);
    T068_CHECK(*count == 1);

    endpoint.reset();
    T068_CHECK(weak_endpoint.expired());
}

void sibling_views_keep_query_proxies_isolated() {
    ui::detail::SemanticNativeViewBridge bridge_a;
    ui::detail::SemanticNativeViewBridge bridge_b;

    T068_CHECK(!bridge_a.publish(button_snapshot(true)).empty());
    T068_CHECK(!bridge_b.publish(button_snapshot(false)).empty());

    const auto proxy_a = bridge_a.make_ordinary_proxy(42);
    const auto proxy_b = bridge_b.make_ordinary_proxy(42);
    const auto read_a = proxy_a.read();
    const auto read_b = proxy_b.read();
    T068_CHECK(read_a.has_value());
    T068_CHECK(read_b.has_value());
    T068_CHECK(read_a->info().enabled);
    T068_CHECK(!read_b->info().enabled);
    T068_CHECK(!bridge_a.make_ordinary_proxy(99).read().has_value());
}

void destroying_one_view_invalidates_only_its_domain() {
    ui::detail::DispatcherOwner dispatcher_a;
    ui::detail::DispatcherOwner dispatcher_b;
    auto bridge_a = std::make_unique<ui::detail::SemanticNativeViewBridge>();
    ui::detail::SemanticNativeViewBridge bridge_b;
    auto count_a = std::make_shared<int>(0);
    auto count_b = std::make_shared<int>(0);
    auto target_a = std::make_shared<RecordingTarget>(count_a);
    auto target_b = std::make_shared<RecordingTarget>(count_b);

    bridge_a->bind_actions(dispatcher_a.dispatcher(), target_a);
    bridge_b.bind_actions(dispatcher_b.dispatcher(), target_b);
    target_a.reset();
    target_b.reset();
    T068_CHECK(!bridge_a->publish(button_snapshot()).empty());
    T068_CHECK(!bridge_b.publish(button_snapshot()).empty());

    auto proxy_a = bridge_a->make_ordinary_proxy(42);
    auto proxy_b = bridge_b.make_ordinary_proxy(42);
    auto router_a = bridge_a->make_ordinary_router(42);
    auto router_b = bridge_b.make_ordinary_router(42);
    T068_CHECK(router_a.post(activate_request()));
    T068_CHECK(router_b.post(activate_request()));

    bridge_a.reset();
    T068_CHECK(!proxy_a.read().has_value());
    T068_CHECK(proxy_b.read().has_value());
    T068_CHECK(!router_a.post(activate_request()));

    T068_CHECK(dispatcher_a.checkpoint() == 1);
    T068_CHECK(dispatcher_b.checkpoint() == 1);
    T068_CHECK(*count_a == 0);
    T068_CHECK(*count_b == 1);

    T068_CHECK(router_b.post(activate_request()));
    T068_CHECK(dispatcher_b.checkpoint() == 1);
    T068_CHECK(*count_b == 2);
}

void sibling_views_keep_snapshot_and_action_domains_isolated() {
    ui::detail::DispatcherOwner dispatcher_a;
    ui::detail::DispatcherOwner dispatcher_b;
    ui::detail::SemanticNativeViewBridge bridge_a;
    ui::detail::SemanticNativeViewBridge bridge_b;
    auto count_a = std::make_shared<int>(0);
    auto count_b = std::make_shared<int>(0);
    auto target_a = std::make_shared<RecordingTarget>(count_a);
    auto target_b = std::make_shared<RecordingTarget>(count_b);

    bridge_a.bind_actions(dispatcher_a.dispatcher(), target_a);
    bridge_b.bind_actions(dispatcher_b.dispatcher(), target_b);
    target_a.reset();
    target_b.reset();
    T068_CHECK(!bridge_a.publish(button_snapshot(true)).empty());
    T068_CHECK(!bridge_b.publish(button_snapshot(false)).empty());

    auto router_a = bridge_a.make_ordinary_router(42);
    auto router_b = bridge_b.make_ordinary_router(42);
    T068_CHECK(router_a.post(activate_request()));
    T068_CHECK(!router_b.post(activate_request()));
    T068_CHECK(dispatcher_a.checkpoint() == 1);
    T068_CHECK(dispatcher_b.checkpoint() == 0);
    T068_CHECK(*count_a == 1);
    T068_CHECK(*count_b == 0);
}

void rebind_cancels_old_queued_work_without_switching_targets() {
    ui::detail::DispatcherOwner dispatcher_owner;
    ui::detail::SemanticNativeViewBridge bridge;
    auto old_count = std::make_shared<int>(0);
    auto new_count = std::make_shared<int>(0);
    auto old_target = std::make_shared<RecordingTarget>(old_count);
    auto new_target = std::make_shared<RecordingTarget>(new_count);

    bridge.bind_actions(dispatcher_owner.dispatcher(), old_target);
    old_target.reset();
    T068_CHECK(!bridge.publish(button_snapshot()).empty());
    auto old_router = bridge.make_ordinary_router(42);
    T068_CHECK(old_router.post(activate_request()));

    bridge.bind_actions(dispatcher_owner.dispatcher(), new_target);
    new_target.reset();
    auto new_router = bridge.make_ordinary_router(42);
    T068_CHECK(new_router.post(activate_request()));

    T068_CHECK(dispatcher_owner.checkpoint() == 2);
    T068_CHECK(*old_count == 0);
    T068_CHECK(*new_count == 1);
    T068_CHECK(!old_router.post(activate_request()));
}

void unbind_rejects_queued_and_future_actions_but_keeps_snapshot_readable() {
    ui::detail::DispatcherOwner dispatcher_owner;
    ui::detail::SemanticNativeViewBridge bridge;
    auto count = std::make_shared<int>(0);
    auto target = std::make_shared<RecordingTarget>(count);

    bridge.bind_actions(dispatcher_owner.dispatcher(), target);
    target.reset();
    T068_CHECK(!bridge.publish(button_snapshot()).empty());
    auto router = bridge.make_ordinary_router(42);
    T068_CHECK(router.post(activate_request()));

    bridge.unbind_actions();
    T068_CHECK(dispatcher_owner.checkpoint() == 1);
    T068_CHECK(*count == 0);
    T068_CHECK(!router.post(activate_request()));

    const auto snapshot = bridge.current();
    T068_CHECK(snapshot);
    T068_CHECK(snapshot->nodes.size() == 1);
    T068_CHECK(snapshot->nodes.front().id == 42);
}

void shutdown_makes_future_queries_and_actions_defunct_but_retains_inflight_read() {
    ui::detail::DispatcherOwner dispatcher_owner;
    ui::detail::SemanticNativeViewBridge bridge;
    auto count = std::make_shared<int>(0);
    auto target = std::make_shared<RecordingTarget>(count);

    bridge.bind_actions(dispatcher_owner.dispatcher(), target);
    target.reset();
    T068_CHECK(!bridge.publish(button_snapshot()).empty());

    auto retained_publisher = bridge.publisher();
    T068_CHECK(retained_publisher);
    auto proxy = bridge.make_ordinary_proxy(42);
    auto retained_read = proxy.read();
    T068_CHECK(retained_read.has_value());
    T068_CHECK(retained_read->info().role == ui::SemanticRole::Button);

    auto router = bridge.make_ordinary_router(42);
    T068_CHECK(router.post(activate_request()));

    bridge.shutdown();

    T068_CHECK(!bridge.publisher());
    T068_CHECK(!bridge.current());
    T068_CHECK(!retained_publisher->current());
    T068_CHECK(!proxy.read().has_value());
    T068_CHECK(!router.post(activate_request()));
    T068_CHECK(dispatcher_owner.checkpoint() == 1);
    T068_CHECK(*count == 0);

    // The callback began its read before teardown and therefore keeps this
    // immutable generation alive independently of the retired publisher.
    T068_CHECK(retained_read->node_id() == 42);
    T068_CHECK(retained_read->info().name == "Target");

    // Shutdown is terminal: publication and action rebinding cannot resurrect
    // a native view whose semantic domain has already been retired, even if an
    // internal collaborator temporarily retained the old publisher strongly.
    bridge.stage(button_snapshot());
    T068_CHECK(!bridge.has_pending_publication());
    T068_CHECK(bridge.publish(button_snapshot()).empty());
    T068_CHECK(retained_publisher->publish(button_snapshot()).empty());
    T068_CHECK(!retained_publisher->current());
    T068_CHECK(!bridge.current());

    auto rebound_target = std::make_shared<RecordingTarget>(count);
    bridge.bind_actions(dispatcher_owner.dispatcher(), rebound_target);
    auto rebound_router = bridge.make_ordinary_router(42);
    T068_CHECK(!rebound_router.post(activate_request()));
    T068_CHECK(dispatcher_owner.checkpoint() == 0);
}

void latest_publication_is_revalidated_before_queued_dispatch() {
    ui::detail::DispatcherOwner dispatcher_owner;
    ui::detail::SemanticNativeViewBridge bridge;
    auto count = std::make_shared<int>(0);
    auto target = std::make_shared<RecordingTarget>(count);

    bridge.bind_actions(dispatcher_owner.dispatcher(), target);
    target.reset();
    T068_CHECK(!bridge.publish(button_snapshot(true)).empty());
    auto router = bridge.make_ordinary_router(42);
    T068_CHECK(router.post(activate_request()));

    T068_CHECK(!bridge.publish(button_snapshot(false)).empty());
    T068_CHECK(dispatcher_owner.checkpoint() == 1);
    T068_CHECK(*count == 0);
}

void lifetime_binding_rejects_work_after_owner_death() {
    ui::detail::DispatcherOwner dispatcher_owner;
    ui::detail::SemanticNativeViewBridge bridge;
    auto owner_lifetime = std::make_shared<int>(0);
    int current_calls = 0;
    int dispatch_calls = 0;

    bridge.bind_lifetime_actions(
        dispatcher_owner.dispatcher(),
        std::weak_ptr<const void>{owner_lifetime},
        [&](const ui::detail::SemanticIdentity& identity)
            -> std::optional<ui::SemanticInfo> {
            ++current_calls;
            if (identity.node_id != 42 || identity.virtual_token) {
                return std::nullopt;
            }
            return button_snapshot().nodes.front().info;
        },
        [&](const ui::detail::SemanticIdentity& identity,
            const ui::detail::SemanticActionRequest& request) {
            ++dispatch_calls;
            return identity.node_id == 42 && !identity.virtual_token &&
                   request.action == ui::SemanticAction::Activate;
        });
    T068_CHECK(!bridge.publish(button_snapshot()).empty());

    auto router = bridge.make_ordinary_router(42);
    T068_CHECK(router.post(activate_request()));
    T068_CHECK(dispatcher_owner.checkpoint() == 1);
    T068_CHECK(current_calls == 1);
    T068_CHECK(dispatch_calls == 1);

    T068_CHECK(router.post(activate_request()));
    owner_lifetime.reset();
    T068_CHECK(dispatcher_owner.checkpoint() == 1);
    T068_CHECK(current_calls == 1);
    T068_CHECK(dispatch_calls == 1);

    const auto snapshot = bridge.current();
    T068_CHECK(snapshot);
    T068_CHECK(snapshot->nodes.size() == 1);
}

void suite() {
    bridge_routes_against_its_own_snapshot_and_target();
    bridge_exposes_lifetime_safe_native_action_endpoint();
    sibling_views_keep_query_proxies_isolated();
    destroying_one_view_invalidates_only_its_domain();
    sibling_views_keep_snapshot_and_action_domains_isolated();
    rebind_cancels_old_queued_work_without_switching_targets();
    unbind_rejects_queued_and_future_actions_but_keeps_snapshot_readable();
    shutdown_makes_future_queries_and_actions_defunct_but_retains_inflight_read();
    latest_publication_is_revalidated_before_queued_dispatch();
    lifetime_binding_rejects_work_after_owner_death();
}

} // namespace

int main() {
    try {
        suite();
        std::cout << "PASS t068 semantic native view bridge\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic native view bridge: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
