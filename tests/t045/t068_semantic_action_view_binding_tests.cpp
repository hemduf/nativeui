#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/semantic_action_view_binding.hpp>
#include <nativeui/detail/semantic_snapshot.hpp>

#include <cstdlib>
#include <functional>
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

ui::SemanticTreeSnapshot button_snapshot() {
    ui::SemanticTreeSnapshot tree;
    tree.root = 42;

    ui::SemanticNodeSnapshot node;
    node.id = 42;
    node.info.role = ui::SemanticRole::Button;
    node.info.name = "Target";
    node.info.enabled = true;
    node.info.actions = {ui::SemanticAction::Activate};
    tree.nodes.push_back(std::move(node));
    return tree;
}

std::shared_ptr<ui::detail::SemanticSnapshotPublisher> published_button() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    T068_CHECK(!publisher->publish(button_snapshot()).empty());
    return publisher;
}

class RecordingTarget final : public ui::detail::SemanticActionTarget {
public:
    explicit RecordingTarget(
        std::shared_ptr<int> dispatch_count,
        std::function<void()> on_dispatch = {},
        std::function<void()> on_current_semantics = {})
        : dispatch_count_(std::move(dispatch_count)),
          on_dispatch_(std::move(on_dispatch)),
          on_current_semantics_(std::move(on_current_semantics)) {
        info_.role = ui::SemanticRole::Button;
        info_.name = "Target";
        info_.enabled = true;
        info_.actions = {ui::SemanticAction::Activate};
    }

    std::optional<ui::SemanticInfo> current_semantics(
        const ui::detail::SemanticIdentity& identity) const override {
        if (identity.node_id != 42 || identity.virtual_token) {
            return std::nullopt;
        }
        if (on_current_semantics_) on_current_semantics_();
        return info_;
    }

    bool dispatch_semantic_action(
        const ui::detail::SemanticIdentity& identity,
        const ui::detail::SemanticActionRequest& request) override {
        if (identity.node_id != 42 || identity.virtual_token ||
            request.action != ui::SemanticAction::Activate) {
            return false;
        }
        ++*dispatch_count_;
        if (on_dispatch_) on_dispatch_();
        return true;
    }

private:
    std::shared_ptr<int> dispatch_count_;
    std::function<void()> on_dispatch_;
    std::function<void()> on_current_semantics_;
    ui::SemanticInfo info_;
};

ui::detail::SemanticActionRequest activate_request() {
    ui::detail::SemanticActionRequest request;
    request.action = ui::SemanticAction::Activate;
    return request;
}

void binding_routes_only_at_the_t065_checkpoint() {
    auto publisher = published_button();
    ui::detail::DispatcherOwner dispatcher_owner;
    auto dispatch_count = std::make_shared<int>(0);
    auto target = std::make_shared<RecordingTarget>(dispatch_count);

    ui::detail::SemanticActionViewBinding binding{
        dispatcher_owner.dispatcher(), target};
    target.reset();

    auto router = binding.make_router(
        ui::detail::SemanticSnapshotProxy::ordinary(publisher, 42));
    T068_CHECK(router.post(activate_request()));
    T068_CHECK(*dispatch_count == 0);
    T068_CHECK(dispatcher_owner.checkpoint() == 1);
    T068_CHECK(*dispatch_count == 1);
}

void reset_invalidates_already_queued_and_future_work() {
    auto publisher = published_button();
    ui::detail::DispatcherOwner dispatcher_owner;
    auto dispatch_count = std::make_shared<int>(0);
    auto target = std::make_shared<RecordingTarget>(dispatch_count);

    ui::detail::SemanticActionViewBinding binding{
        dispatcher_owner.dispatcher(), target};
    target.reset();
    auto router = binding.make_router(
        ui::detail::SemanticSnapshotProxy::ordinary(publisher, 42));

    T068_CHECK(router.post(activate_request()));
    binding.reset();

    T068_CHECK(dispatcher_owner.checkpoint() == 1);
    T068_CHECK(*dispatch_count == 0);
    T068_CHECK(!router.post(activate_request()));
}

void reset_during_live_recheck_blocks_dispatch() {
    auto publisher = published_button();
    ui::detail::DispatcherOwner dispatcher_owner;
    auto dispatch_count = std::make_shared<int>(0);
    ui::detail::SemanticActionViewBinding binding;

    auto target = std::make_shared<RecordingTarget>(
        dispatch_count,
        std::function<void()>{},
        [&] { binding.reset(); });
    binding = ui::detail::SemanticActionViewBinding{
        dispatcher_owner.dispatcher(), target};
    target.reset();
    auto router = binding.make_router(
        ui::detail::SemanticSnapshotProxy::ordinary(publisher, 42));

    T068_CHECK(router.post(activate_request()));
    T068_CHECK(dispatcher_owner.checkpoint() == 1);
    T068_CHECK(*dispatch_count == 0);
    T068_CHECK(!router.post(activate_request()));
}

void reentrant_reset_rejects_nested_future_work() {
    auto publisher = published_button();
    ui::detail::DispatcherOwner dispatcher_owner;
    auto dispatch_count = std::make_shared<int>(0);
    ui::detail::SemanticActionViewBinding binding;
    std::optional<ui::detail::SemanticActionRouter> router;
    bool nested_post_accepted = true;

    auto target = std::make_shared<RecordingTarget>(
        dispatch_count,
        [&] {
            binding.reset();
            nested_post_accepted = router->post(activate_request());
        });
    binding = ui::detail::SemanticActionViewBinding{
        dispatcher_owner.dispatcher(), target};
    target.reset();
    router.emplace(binding.make_router(
        ui::detail::SemanticSnapshotProxy::ordinary(publisher, 42)));

    T068_CHECK(router->post(activate_request()));
    T068_CHECK(dispatcher_owner.checkpoint() == 1);
    T068_CHECK(*dispatch_count == 1);
    T068_CHECK(!nested_post_accepted);
    T068_CHECK(dispatcher_owner.checkpoint() == 0);
}

void independent_view_bindings_do_not_share_target_lifetime() {
    auto publisher_a = published_button();
    auto publisher_b = published_button();
    ui::detail::DispatcherOwner dispatcher_a;
    ui::detail::DispatcherOwner dispatcher_b;
    auto count_a = std::make_shared<int>(0);
    auto count_b = std::make_shared<int>(0);
    auto target_a = std::make_shared<RecordingTarget>(count_a);
    auto target_b = std::make_shared<RecordingTarget>(count_b);

    ui::detail::SemanticActionViewBinding binding_a{
        dispatcher_a.dispatcher(), target_a};
    ui::detail::SemanticActionViewBinding binding_b{
        dispatcher_b.dispatcher(), target_b};
    target_a.reset();
    target_b.reset();

    auto router_a = binding_a.make_router(
        ui::detail::SemanticSnapshotProxy::ordinary(publisher_a, 42));
    auto router_b = binding_b.make_router(
        ui::detail::SemanticSnapshotProxy::ordinary(publisher_b, 42));

    binding_a.reset();
    T068_CHECK(!router_a.post(activate_request()));
    T068_CHECK(router_b.post(activate_request()));
    T068_CHECK(dispatcher_a.checkpoint() == 0);
    T068_CHECK(dispatcher_b.checkpoint() == 1);
    T068_CHECK(*count_a == 0);
    T068_CHECK(*count_b == 1);
}

void platform_endpoint_routes_stable_identity_only_through_t065() {
    auto publisher = published_button();
    ui::detail::DispatcherOwner dispatcher_owner;
    auto dispatch_count = std::make_shared<int>(0);
    auto target = std::make_shared<RecordingTarget>(dispatch_count);

    ui::detail::SemanticActionViewBinding binding{
        dispatcher_owner.dispatcher(), target, publisher};
    target.reset();

    const auto endpoint = binding.endpoint().lock();
    T068_CHECK(endpoint != nullptr);

    const ui::detail::SemanticIdentity identity{42, std::nullopt};
    T068_CHECK(endpoint->post(identity, activate_request()));
    T068_CHECK(*dispatch_count == 0);
    T068_CHECK(dispatcher_owner.checkpoint() == 1);
    T068_CHECK(*dispatch_count == 1);

    const ui::detail::SemanticIdentity missing{43, std::nullopt};
    T068_CHECK(!endpoint->post(missing, activate_request()));
    T068_CHECK(dispatcher_owner.checkpoint() == 0);
}

void platform_endpoint_fails_closed_after_binding_retirement() {
    auto publisher = published_button();
    ui::detail::DispatcherOwner dispatcher_owner;
    auto dispatch_count = std::make_shared<int>(0);
    auto target = std::make_shared<RecordingTarget>(dispatch_count);

    ui::detail::SemanticActionViewBinding binding{
        dispatcher_owner.dispatcher(), target, publisher};
    target.reset();

    auto weak_endpoint = binding.endpoint();
    auto in_flight_endpoint = weak_endpoint.lock();
    T068_CHECK(in_flight_endpoint != nullptr);
    T068_CHECK(in_flight_endpoint->available());

    binding.reset();
    T068_CHECK(!in_flight_endpoint->available());
    const ui::detail::SemanticIdentity identity{42, std::nullopt};
    T068_CHECK(!in_flight_endpoint->post(identity, activate_request()));
    T068_CHECK(dispatcher_owner.checkpoint() == 0);
    T068_CHECK(*dispatch_count == 0);

    in_flight_endpoint.reset();
    T068_CHECK(weak_endpoint.expired());
}

void endpoint_isolation_matches_view_binding_isolation() {
    auto publisher_a = published_button();
    auto publisher_b = published_button();
    ui::detail::DispatcherOwner dispatcher_a;
    ui::detail::DispatcherOwner dispatcher_b;
    auto count_a = std::make_shared<int>(0);
    auto count_b = std::make_shared<int>(0);
    auto target_a = std::make_shared<RecordingTarget>(count_a);
    auto target_b = std::make_shared<RecordingTarget>(count_b);

    ui::detail::SemanticActionViewBinding binding_a{
        dispatcher_a.dispatcher(), target_a, publisher_a};
    ui::detail::SemanticActionViewBinding binding_b{
        dispatcher_b.dispatcher(), target_b, publisher_b};
    target_a.reset();
    target_b.reset();

    const auto endpoint_a = binding_a.endpoint().lock();
    const auto endpoint_b = binding_b.endpoint().lock();
    T068_CHECK(endpoint_a != nullptr);
    T068_CHECK(endpoint_b != nullptr);
    T068_CHECK(endpoint_a.get() != endpoint_b.get());

    binding_a.reset();
    const ui::detail::SemanticIdentity identity{42, std::nullopt};
    T068_CHECK(!endpoint_a->post(identity, activate_request()));
    T068_CHECK(endpoint_b->post(identity, activate_request()));
    T068_CHECK(dispatcher_a.checkpoint() == 0);
    T068_CHECK(dispatcher_b.checkpoint() == 1);
    T068_CHECK(*count_a == 0);
    T068_CHECK(*count_b == 1);
}

void empty_binding_fails_closed() {
    auto publisher = published_button();
    ui::detail::SemanticActionViewBinding binding;
    auto router = binding.make_router(
        ui::detail::SemanticSnapshotProxy::ordinary(publisher, 42));

    T068_CHECK(!router.post(activate_request()));
    T068_CHECK(binding.endpoint().expired());
}

void suite() {
    binding_routes_only_at_the_t065_checkpoint();
    reset_invalidates_already_queued_and_future_work();
    reset_during_live_recheck_blocks_dispatch();
    reentrant_reset_rejects_nested_future_work();
    independent_view_bindings_do_not_share_target_lifetime();
    platform_endpoint_routes_stable_identity_only_through_t065();
    platform_endpoint_fails_closed_after_binding_retirement();
    endpoint_isolation_matches_view_binding_isolation();
    empty_binding_fails_closed();
}

} // namespace

int main() {
    try {
        suite();
        std::cout << "PASS t068 semantic action view binding\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic action view binding: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
