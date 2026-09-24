#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/semantic_action.hpp>
#include <nativeui/detail/semantic_proxy.hpp>
#include <nativeui/detail/semantic_snapshot.hpp>
#include <nativeui/semantics.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

ui::SemanticTreeSnapshot ordinary_snapshot(ui::SemanticRole role,
                                           bool enabled,
                                           bool read_only,
                                           std::vector<ui::SemanticAction> actions) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 42;

    ui::SemanticNodeSnapshot node;
    node.id = 42;
    node.info.role = role;
    node.info.name = "Target";
    node.info.enabled = enabled;
    node.info.read_only = read_only;
    node.info.focusable = true;
    node.info.actions = std::move(actions);
    node.bounds = {0.0f, 0.0f, 80.0f, 24.0f};
    tree.nodes.push_back(std::move(node));
    return tree;
}

class RecordingTarget final : public ui::detail::SemanticActionTarget {
public:
    std::optional<ui::SemanticInfo> current_semantics(
        const ui::detail::SemanticIdentity& identity) const override {
        if (!current_info || identity != expected_identity) {
            return std::nullopt;
        }
        return current_info;
    }

    bool dispatch_semantic_action(
        const ui::detail::SemanticIdentity& identity,
        const ui::detail::SemanticActionRequest& request) override {
        ++dispatch_count;
        last_identity = identity;
        last_request = request;
        callback_thread = std::this_thread::get_id();
        return true;
    }

    ui::detail::SemanticIdentity expected_identity{};
    std::optional<ui::SemanticInfo> current_info;
    int dispatch_count{};
    ui::detail::SemanticIdentity last_identity{};
    ui::detail::SemanticActionRequest last_request{};
    std::thread::id callback_thread{};
};

using RouterTargetPtr = std::shared_ptr<ui::detail::SemanticActionTarget>;

static_assert(!std::is_constructible_v<
              ui::detail::SemanticActionRouter,
              ui::detail::SemanticSnapshotProxy,
              ui::Dispatcher,
              const RouterTargetPtr&>);
static_assert(std::is_constructible_v<
              ui::detail::SemanticActionRouter,
              ui::detail::SemanticSnapshotProxy,
              ui::Dispatcher,
              const RouterTargetPtr&,
              std::weak_ptr<const void>>);

void post_is_noexcept_and_closed_dispatcher_rejects() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    auto snapshot = ordinary_snapshot(
        ui::SemanticRole::Button,
        true,
        false,
        {ui::SemanticAction::Activate});
    T068_CHECK(!publisher->publish(snapshot).empty());

    ui::detail::DispatcherOwner owner;
    auto target = std::make_shared<RecordingTarget>();
    target->expected_identity = {42, std::nullopt};
    target->current_info = snapshot.nodes[0].info;
    auto endpoint_lifetime = std::make_shared<int>(0);
    ui::detail::SemanticActionRouter router{
        ui::detail::SemanticSnapshotProxy::ordinary(publisher, 42),
        owner.dispatcher(),
        target,
        endpoint_lifetime};

    static_assert(noexcept(router.post(ui::detail::SemanticActionRequest{})));

    owner.shutdown();
    ui::detail::SemanticActionRequest activate;
    activate.action = ui::SemanticAction::Activate;
    T068_CHECK(!router.post(std::move(activate)));
    T068_CHECK(target->dispatch_count == 0);
}

void accepted_action_is_marshalled_to_dispatcher_ui_thread() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    auto snapshot = ordinary_snapshot(
        ui::SemanticRole::Button,
        true,
        false,
        {ui::SemanticAction::Activate, ui::SemanticAction::Focus});
    T068_CHECK(!publisher->publish(snapshot).empty());

    ui::detail::DispatcherOwner owner;
    auto target = std::make_shared<RecordingTarget>();
    target->expected_identity = {42, std::nullopt};
    target->current_info = snapshot.nodes[0].info;

    auto endpoint_lifetime = std::make_shared<int>(0);
    ui::detail::SemanticActionRouter router{
        ui::detail::SemanticSnapshotProxy::ordinary(publisher, 42),
        owner.dispatcher(),
        target,
        endpoint_lifetime};

    const auto ui_thread = std::this_thread::get_id();
    bool accepted = false;
    std::thread worker{[&] {
        ui::detail::SemanticActionRequest request;
        request.action = ui::SemanticAction::Activate;
        accepted = router.post(std::move(request));
    }};
    worker.join();

    T068_CHECK(accepted);
    T068_CHECK(target->dispatch_count == 0);
    T068_CHECK(owner.checkpoint() == 1);
    T068_CHECK(target->dispatch_count == 1);
    T068_CHECK(target->last_identity == target->expected_identity);
    T068_CHECK(target->last_request.action == ui::SemanticAction::Activate);
    T068_CHECK(target->callback_thread == ui_thread);
}

void request_must_be_advertised_enabled_and_read_only_safe() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    auto snapshot = ordinary_snapshot(
        ui::SemanticRole::TextInput,
        true,
        true,
        {ui::SemanticAction::SetValue, ui::SemanticAction::Focus});
    T068_CHECK(!publisher->publish(snapshot).empty());

    ui::detail::DispatcherOwner owner;
    auto target = std::make_shared<RecordingTarget>();
    target->expected_identity = {42, std::nullopt};
    target->current_info = snapshot.nodes[0].info;
    auto endpoint_lifetime = std::make_shared<int>(0);
    ui::detail::SemanticActionRouter router{
        ui::detail::SemanticSnapshotProxy::ordinary(publisher, 42),
        owner.dispatcher(),
        target,
        endpoint_lifetime};

    ui::detail::SemanticActionRequest set_value;
    set_value.action = ui::SemanticAction::SetValue;
    set_value.numeric_value = 0.5;
    T068_CHECK(!router.post(set_value));

    ui::detail::SemanticActionRequest unadvertised;
    unadvertised.action = ui::SemanticAction::Toggle;
    T068_CHECK(!router.post(unadvertised));

    ui::detail::SemanticActionRequest focus;
    focus.action = ui::SemanticAction::Focus;
    T068_CHECK(router.post(focus));
    T068_CHECK(owner.checkpoint() == 1);
    T068_CHECK(target->dispatch_count == 1);
    T068_CHECK(target->last_request.action == ui::SemanticAction::Focus);

    auto disabled = snapshot;
    disabled.nodes[0].info.enabled = false;
    T068_CHECK(publisher->publish(std::move(disabled)) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::ValueChanged});
    target->current_info->enabled = false;
    T068_CHECK(!router.post(focus));
}

void live_target_eligibility_is_rechecked_after_enqueue() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    auto snapshot = ordinary_snapshot(
        ui::SemanticRole::Button,
        true,
        false,
        {ui::SemanticAction::Activate});
    T068_CHECK(!publisher->publish(snapshot).empty());

    ui::detail::DispatcherOwner owner;
    auto target = std::make_shared<RecordingTarget>();
    target->expected_identity = {42, std::nullopt};
    target->current_info = snapshot.nodes[0].info;
    auto endpoint_lifetime = std::make_shared<int>(0);
    ui::detail::SemanticActionRouter router{
        ui::detail::SemanticSnapshotProxy::ordinary(publisher, 42),
        owner.dispatcher(),
        target,
        endpoint_lifetime};

    ui::detail::SemanticActionRequest activate;
    activate.action = ui::SemanticAction::Activate;
    T068_CHECK(router.post(activate));

    // The published native-read snapshot is deliberately left unchanged. The
    // UI-thread target has become disabled, so the second eligibility check must
    // suppress the action before widget/component mutation.
    target->current_info->enabled = false;
    T068_CHECK(owner.checkpoint() == 1);
    T068_CHECK(target->dispatch_count == 0);

    target->current_info->enabled = true;
    T068_CHECK(router.post(activate));
    target->current_info->actions.clear();
    T068_CHECK(owner.checkpoint() == 1);
    T068_CHECK(target->dispatch_count == 0);
}

void stale_snapshot_identity_and_destroyed_target_are_safe() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    auto snapshot = ordinary_snapshot(
        ui::SemanticRole::Button,
        true,
        false,
        {ui::SemanticAction::Activate});
    T068_CHECK(!publisher->publish(snapshot).empty());

    ui::detail::DispatcherOwner owner;
    auto target = std::make_shared<RecordingTarget>();
    target->expected_identity = {42, std::nullopt};
    target->current_info = snapshot.nodes[0].info;
    auto endpoint_lifetime = std::make_shared<int>(0);
    ui::detail::SemanticActionRouter router{
        ui::detail::SemanticSnapshotProxy::ordinary(publisher, 42),
        owner.dispatcher(),
        target,
        endpoint_lifetime};

    ui::detail::SemanticActionRequest activate;
    activate.action = ui::SemanticAction::Activate;
    T068_CHECK(router.post(activate));
    ui::SemanticTreeSnapshot removed;
    T068_CHECK(publisher->publish(std::move(removed)) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});
    T068_CHECK(owner.checkpoint() == 1);
    T068_CHECK(target->dispatch_count == 0);

    T068_CHECK(publisher->publish(snapshot) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});
    T068_CHECK(router.post(activate));
    target.reset();
    T068_CHECK(owner.checkpoint() == 1);
}

void payload_and_virtual_identity_are_preserved() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    auto metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    metadata->push_back({20, "Twenty", "", true, false,
                         ui::SemanticCheckedState::NotApplicable,
                         {ui::SemanticAction::Select}});
    auto token_index = std::make_shared<ui::VirtualSemanticChildren::TokenIndex>();
    token_index->emplace(20, 0);

    ui::SemanticTreeSnapshot tree;
    tree.root = 7;
    ui::SemanticNodeSnapshot list;
    list.id = 7;
    list.info.role = ui::SemanticRole::ListView;
    list.info.name = "Items";
    list.virtual_children = ui::VirtualSemanticChildren::from_indexed_metadata(
        1, metadata, token_index, std::nullopt,
        {0.0f, 0.0f, 120.0f, 40.0f}, 20.0f, 0.0f);
    tree.nodes.push_back(std::move(list));
    T068_CHECK(!publisher->publish(std::move(tree)).empty());

    const auto proxy = ui::detail::SemanticSnapshotProxy::virtual_item(publisher, 7, 20);
    const auto read = proxy.read();
    T068_CHECK(read.has_value());

    ui::detail::DispatcherOwner owner;
    auto target = std::make_shared<RecordingTarget>();
    target->expected_identity = {7, ui::VirtualSemanticItemToken{20}};
    target->current_info = read->info();
    auto endpoint_lifetime = std::make_shared<int>(0);
    ui::detail::SemanticActionRouter router{
        proxy, owner.dispatcher(), target, endpoint_lifetime};

    ui::detail::SemanticActionRequest select;
    select.action = ui::SemanticAction::Select;
    select.numeric_value = 12.5;
    select.text_value = "logical-item";
    T068_CHECK(router.post(select));
    T068_CHECK(owner.checkpoint() == 1);
    T068_CHECK(target->dispatch_count == 1);
    T068_CHECK(target->last_identity == target->expected_identity);
    T068_CHECK(target->last_request.numeric_value == std::optional<double>{12.5});
    T068_CHECK(target->last_request.text_value == std::optional<std::string>{"logical-item"});
}

} // namespace

int main() {
    try {
        post_is_noexcept_and_closed_dispatcher_rejects();
        accepted_action_is_marshalled_to_dispatcher_ui_thread();
        request_must_be_advertised_enabled_and_read_only_safe();
        live_target_eligibility_is_rechecked_after_enqueue();
        stale_snapshot_identity_and_destroyed_target_are_safe();
        payload_and_virtual_identity_are_preserved();
        std::cout << "PASS t068 semantic action router\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic action router: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
