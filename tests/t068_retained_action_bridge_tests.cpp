#include "test_support.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/overlay_service.hpp>
#include <nativeui/detail/semantic_action.hpp>
#include <nativeui/detail/semantic_tree_action_access.hpp>
#include <nativeui/widgets.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ui {

/// Test-only accessor for the per-tree borrowed overlay seam. Production code
/// obtains this pointer from the owning UI before mount; the T068 semantic
/// action suites inject the same seam to exercise popup presentation from a
/// T065 checkpoint without a native view.
struct TreeTestAccess {
    static void set_overlay_service(Tree& tree, detail::OverlayService* service) {
        tree.set_overlay_service(service);
    }
};

} // namespace ui

namespace {

[[nodiscard]] ui::detail::SemanticActionRequest action_request(
    ui::SemanticAction action) {
    ui::detail::SemanticActionRequest request;
    request.action = action;
    return request;
}

/// Overlay seam double that behaves exactly like the production presenter
/// (`OverlayState::show`/`close`) and records how many times the semantic path
/// presented or dismissed a popup.
class RecordingOverlayService final : public ui::detail::OverlayService {
public:
    RecordingOverlayService()
        : state_(std::make_shared<ui::detail::OverlayState>()) {
        state_->structural_invalidator = [this] { ++structural_invalidations; };
    }

    [[nodiscard]] ui::OverlayHandle present(ui::OverlaySpec overlay) override {
        ++present_calls;
        last_mode = overlay.mode;
        last_anchor = overlay.anchor;
        last_dismiss_on_escape = overlay.dismiss_on_escape;
        return state_->show(std::move(overlay));
    }

    bool dismiss(ui::OverlayHandle handle) override {
        ++dismiss_calls;
        return state_->close(std::move(handle));
    }

    [[nodiscard]] std::size_t open_entries() const noexcept {
        return state_->entries.size();
    }

    std::shared_ptr<ui::detail::OverlayState> state_;
    int present_calls{};
    int dismiss_calls{};
    int structural_invalidations{};
    ui::OverlayMode last_mode{ui::OverlayMode::NonModal};
    std::optional<ui::NodeId> last_anchor;
    bool last_dismiss_on_escape{};
};

[[nodiscard]] ui::Spec menu_item_spec(
    const std::shared_ptr<ui::detail::MenuPopupSession>& session,
    std::size_t index) {
    return ui::Spec{
        [session, index] {
            return std::make_unique<ui::detail::MenuPopupItemComponent>(session, index);
        },
        {}};
}

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

void combo_box_semantic_expand_collapse_execute_once() {
    ui::State<int> selection{99};
    const std::vector<ui::ComboBoxOption<int>> options{
        {1, "One", true}, {2, "Two", true}};
    auto root = ui::compile(ui::make_spec(ui::ComboBox<int>{selection, options}));
    ui::Node* combo = root.get();
    const auto combo_id = static_cast<ui::SemanticId>(combo->id);

    RecordingOverlayService service;
    ui::Tree tree{std::move(root)};
    ui::TreeTestAccess::set_overlay_service(tree, &service);
    tree.mount();

    const ui::detail::SemanticIdentity identity{combo_id, std::nullopt};
    NUI_CHECK(combo->component->semantics().expanded ==
              ui::SemanticExpandedState::Collapsed);

    NUI_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, action_request(ui::SemanticAction::Expand)));
    NUI_CHECK(service.present_calls == 1);
    NUI_CHECK(service.open_entries() == 1);
    NUI_CHECK(service.last_mode == ui::OverlayMode::Modal);
    NUI_CHECK(service.last_anchor.has_value());
    NUI_CHECK(*service.last_anchor == static_cast<ui::NodeId>(combo_id));
    NUI_CHECK(service.last_dismiss_on_escape);
    NUI_CHECK(combo->component->semantics().expanded ==
              ui::SemanticExpandedState::Expanded);

    // Repeated Expand never presents a duplicate popup.
    NUI_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, action_request(ui::SemanticAction::Expand)));
    NUI_CHECK(service.present_calls == 1);
    NUI_CHECK(service.open_entries() == 1);

    NUI_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, action_request(ui::SemanticAction::Collapse)));
    NUI_CHECK(service.dismiss_calls == 1);
    NUI_CHECK(service.open_entries() == 0);
    NUI_CHECK(combo->component->semantics().expanded ==
              ui::SemanticExpandedState::Collapsed);

    // Repeated Collapse is an idempotent no-op, never a second dismiss.
    NUI_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, action_request(ui::SemanticAction::Collapse)));
    NUI_CHECK(service.dismiss_calls == 1);
}

void combo_box_semantic_select_commits_highlighted_option_once() {
    ui::State<int> selection{99};
    const std::vector<ui::ComboBoxOption<int>> options{
        {1, "One", true}, {2, "Two", true}};
    auto root = ui::compile(ui::make_spec(ui::ComboBox<int>{selection, options}));
    ui::Node* combo = root.get();
    const auto combo_id = static_cast<ui::SemanticId>(combo->id);

    int observation_count = 0;
    auto subscription = selection.observe(
        [&observation_count](const int&) { ++observation_count; });

    RecordingOverlayService service;
    ui::Tree tree{std::move(root)};
    ui::TreeTestAccess::set_overlay_service(tree, &service);
    tree.mount();

    const ui::detail::SemanticIdentity identity{combo_id, std::nullopt};

    // Select with no open popup has no selection target and fails closed.
    NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, action_request(ui::SemanticAction::Select)));
    NUI_CHECK(selection.get() == 99);
    NUI_CHECK(observation_count == 0);
    NUI_CHECK(service.present_calls == 0);
    NUI_CHECK(service.dismiss_calls == 0);

    NUI_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, action_request(ui::SemanticAction::Expand)));
    NUI_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, action_request(ui::SemanticAction::Select)));
    NUI_CHECK(selection.get() == 1);
    NUI_CHECK(observation_count == 1);
    NUI_CHECK(service.dismiss_calls == 1);
    NUI_CHECK(service.open_entries() == 0);
    NUI_CHECK(combo->component->semantics().expanded ==
              ui::SemanticExpandedState::Collapsed);

    // The committed popup cannot commit again.
    selection.set(42);
    NUI_CHECK(observation_count == 2);
    NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, action_request(ui::SemanticAction::Select)));
    NUI_CHECK(selection.get() == 42);
    NUI_CHECK(observation_count == 2);
    NUI_CHECK(service.dismiss_calls == 1);

    // A disabled option is never a commit target, even when it is the bound
    // value; the first enabled option is highlighted and committed instead.
    ui::State<int> disabled_selection{10};
    const std::vector<ui::ComboBoxOption<int>> mixed_options{
        {10, "Ten", false}, {20, "Twenty", true}};
    auto mixed_root = ui::compile(
        ui::make_spec(ui::ComboBox<int>{disabled_selection, mixed_options}));
    const auto mixed_id = static_cast<ui::SemanticId>(mixed_root->id);
    RecordingOverlayService mixed_service;
    ui::Tree mixed_tree{std::move(mixed_root)};
    ui::TreeTestAccess::set_overlay_service(mixed_tree, &mixed_service);
    mixed_tree.mount();
    const ui::detail::SemanticIdentity mixed_identity{mixed_id, std::nullopt};
    NUI_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        mixed_tree, mixed_identity, action_request(ui::SemanticAction::Expand)));
    NUI_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        mixed_tree, mixed_identity, action_request(ui::SemanticAction::Select)));
    NUI_CHECK(disabled_selection.get() == 20);
}

void combo_box_semantic_actions_revalidate_availability() {
    const std::vector<ui::ComboBoxOption<int>> options{{1, "One", true}};

    {
        ui::State<bool> enabled{false};
        ui::State<int> selection{99};
        auto root = ui::compile(ui::make_spec(ui::Enabled{
            enabled, ui::ComboBox<int>{selection, options}}));
        ui::Node* combo = root->children.front().get();
        const auto combo_id = static_cast<ui::SemanticId>(combo->id);

        RecordingOverlayService service;
        ui::Tree tree{std::move(root)};
        ui::TreeTestAccess::set_overlay_service(tree, &service);
        tree.mount();

        const ui::detail::SemanticIdentity identity{combo_id, std::nullopt};
        NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
            tree, identity, action_request(ui::SemanticAction::Expand)));
        NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
            tree, identity, action_request(ui::SemanticAction::Select)));
        NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
            tree, identity, action_request(ui::SemanticAction::Collapse)));
        NUI_CHECK(service.present_calls == 0);
        NUI_CHECK(service.dismiss_calls == 0);
        NUI_CHECK(selection.get() == 99);
    }

    {
        ui::State<bool> read_only{true};
        ui::State<int> selection{99};
        auto root = ui::compile(ui::make_spec(ui::ReadOnly{
            read_only, ui::ComboBox<int>{selection, options}}));
        ui::Node* combo = root->children.front().get();
        const auto combo_id = static_cast<ui::SemanticId>(combo->id);

        RecordingOverlayService service;
        ui::Tree tree{std::move(root)};
        ui::TreeTestAccess::set_overlay_service(tree, &service);
        tree.mount();

        const ui::detail::SemanticIdentity identity{combo_id, std::nullopt};
        NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
            tree, identity, action_request(ui::SemanticAction::Expand)));
        NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
            tree, identity, action_request(ui::SemanticAction::Select)));
        NUI_CHECK(service.present_calls == 0);
        NUI_CHECK(service.dismiss_calls == 0);
        NUI_CHECK(selection.get() == 99);
    }
}

void combo_box_stale_identity_fails_closed() {
    const std::vector<ui::ComboBoxOption<int>> options{{1, "One", true}};
    ui::State<bool> visible{true};
    ui::State<int> selection{99};
    auto root = ui::compile(ui::make_spec(
        ui::If{visible, ui::ComboBox<int>{selection, options}}));
    ui::Node* combo = root->children.front().get();
    const auto combo_id = static_cast<ui::SemanticId>(combo->id);

    RecordingOverlayService service;
    ui::Tree tree{std::move(root)};
    ui::TreeTestAccess::set_overlay_service(tree, &service);
    tree.mount();

    // The retained identity is live before removal.
    NUI_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, {combo_id, std::nullopt}, action_request(ui::SemanticAction::Expand)));
    NUI_CHECK(service.present_calls == 1);

    // Removing the retained subtree retires the identity; a retained
    // native-proxy request must fail closed without presenting or dismissing.
    visible.set(false);
    tree.layout({200.0f, 120.0f});
    NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, {combo_id, std::nullopt}, action_request(ui::SemanticAction::Expand)));
    NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, {combo_id, std::nullopt}, action_request(ui::SemanticAction::Select)));
    NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, {combo_id, std::nullopt}, action_request(ui::SemanticAction::Collapse)));
    NUI_CHECK(service.present_calls == 1);
    NUI_CHECK(service.dismiss_calls == 0);
    NUI_CHECK(selection.get() == 99);
}

void menu_item_semantic_activate_executes_once() {
    RecordingOverlayService service;
    auto session = std::make_shared<ui::detail::MenuPopupSession>();
    session->anchor = std::make_shared<ui::detail::MenuAnchorRuntime>();
    session->anchor->mounted = true;
    int callback_calls = 0;
    bool callback_saw_closed = false;
    session->items.push_back(ui::PopupMenuItem::action("Open", [&] {
        ++callback_calls;
        callback_saw_closed = service.open_entries() == 0;
    }));
    session->items.push_back(ui::PopupMenuItem::separator());
    session->items.push_back(ui::PopupMenuItem::action("Disabled", [] {}, false));
    session->highlighted = 0;
    session->handle = service.state_->show(ui::OverlaySpec{});
    NUI_CHECK(session->handle.valid());
    NUI_CHECK(service.open_entries() == 1);

    auto root = ui::compile(ui::Spec{
        [session] { return std::make_unique<ui::detail::MenuPopupComponent>(session); },
        {menu_item_spec(session, 0), menu_item_spec(session, 1), menu_item_spec(session, 2)}});
    const auto open_id = static_cast<ui::SemanticId>(root->children[0]->id);
    const auto disabled_id = static_cast<ui::SemanticId>(root->children[2]->id);

    ui::Tree tree{std::move(root)};
    ui::TreeTestAccess::set_overlay_service(tree, &service);
    tree.mount();

    NUI_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, {open_id, std::nullopt}, action_request(ui::SemanticAction::Activate)));
    NUI_CHECK(callback_calls == 1);
    NUI_CHECK(callback_saw_closed);
    NUI_CHECK(service.dismiss_calls == 1);
    NUI_CHECK(service.open_entries() == 0);

    // Exactly-once: the same menu commit cannot activate twice.
    NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, {open_id, std::nullopt}, action_request(ui::SemanticAction::Activate)));
    NUI_CHECK(callback_calls == 1);
    NUI_CHECK(service.dismiss_calls == 1);

    // Disabled and separator items fail closed.
    NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, {disabled_id, std::nullopt},
        action_request(ui::SemanticAction::Activate)));
    NUI_CHECK(callback_calls == 1);
}

void menu_item_semantic_failed_close_is_not_executed() {
    RecordingOverlayService service;
    auto session = std::make_shared<ui::detail::MenuPopupSession>();
    session->anchor = std::make_shared<ui::detail::MenuAnchorRuntime>();
    session->anchor->mounted = true;
    int callback_calls = 0;
    session->items.push_back(
        ui::PopupMenuItem::action("Open", [&] { ++callback_calls; }));
    session->highlighted = 0;

    auto root = ui::compile(ui::Spec{
        [session] { return std::make_unique<ui::detail::MenuPopupComponent>(session); },
        {menu_item_spec(session, 0)}});
    const auto item_id = static_cast<ui::SemanticId>(root->children[0]->id);

    ui::Tree tree{std::move(root)};
    ui::TreeTestAccess::set_overlay_service(tree, &service);
    tree.mount();

    // No committed overlay handle: the close cannot be executed, so the
    // callback must not run and the request stays retryable.
    NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, {item_id, std::nullopt}, action_request(ui::SemanticAction::Activate)));
    NUI_CHECK(callback_calls == 0);
    NUI_CHECK(!session->completion_queued);
    NUI_CHECK(service.dismiss_calls == 0);

    // Once the menu owns a committed handle the same node activates once.
    session->handle = service.state_->show(ui::OverlaySpec{});
    NUI_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, {item_id, std::nullopt}, action_request(ui::SemanticAction::Activate)));
    NUI_CHECK(callback_calls == 1);
    NUI_CHECK(service.dismiss_calls == 1);
    NUI_CHECK(service.open_entries() == 0);
}

void combo_box_and_menu_item_actions_route_through_dispatcher() {
    ui::State<int> selection{99};
    const std::vector<ui::ComboBoxOption<int>> options{
        {1, "One", true}, {2, "Two", true}};
    auto combo_root = ui::compile(ui::make_spec(ui::ComboBox<int>{selection, options}));
    const auto combo_id = static_cast<ui::SemanticId>(combo_root->id);

    RecordingOverlayService combo_service;
    ui::Tree combo_tree{std::move(combo_root)};
    ui::TreeTestAccess::set_overlay_service(combo_tree, &combo_service);
    combo_tree.mount();

    ui::detail::DispatcherOwner combo_dispatcher;
    auto combo_lifetime = std::make_shared<int>(0);
    ui::detail::SemanticRetainedViewDomain combo_domain{
        combo_dispatcher.dispatcher(),
        std::weak_ptr<const void>{combo_lifetime},
        combo_tree};
    NUI_CHECK(combo_domain.checkpoint_snapshot().has_value());

    auto combo_router = combo_domain.bridge().make_ordinary_router(combo_id);
    NUI_CHECK(combo_router.post(action_request(ui::SemanticAction::Expand)));
    NUI_CHECK(combo_service.present_calls == 0);
    NUI_CHECK(combo_dispatcher.checkpoint() == 1);
    NUI_CHECK(combo_service.present_calls == 1);

    // A second accepted request cannot present a duplicate popup.
    NUI_CHECK(combo_router.post(action_request(ui::SemanticAction::Expand)));
    NUI_CHECK(combo_dispatcher.checkpoint() == 1);
    NUI_CHECK(combo_service.present_calls == 1);

    NUI_CHECK(combo_router.post(action_request(ui::SemanticAction::Select)));
    NUI_CHECK(combo_dispatcher.checkpoint() == 1);
    NUI_CHECK(selection.get() == 1);
    NUI_CHECK(combo_service.dismiss_calls == 1);

    RecordingOverlayService menu_service;
    auto session = std::make_shared<ui::detail::MenuPopupSession>();
    session->anchor = std::make_shared<ui::detail::MenuAnchorRuntime>();
    session->anchor->mounted = true;
    int menu_callback_calls = 0;
    session->items.push_back(
        ui::PopupMenuItem::action("Open", [&] { ++menu_callback_calls; }));
    session->highlighted = 0;
    session->handle = menu_service.state_->show(ui::OverlaySpec{});

    auto menu_root = ui::compile(ui::Spec{
        [session] { return std::make_unique<ui::detail::MenuPopupComponent>(session); },
        {menu_item_spec(session, 0)}});
    const auto menu_item_id = static_cast<ui::SemanticId>(menu_root->children[0]->id);

    ui::Tree menu_tree{std::move(menu_root)};
    ui::TreeTestAccess::set_overlay_service(menu_tree, &menu_service);
    menu_tree.mount();

    ui::detail::DispatcherOwner menu_dispatcher;
    auto menu_lifetime = std::make_shared<int>(0);
    ui::detail::SemanticRetainedViewDomain menu_domain{
        menu_dispatcher.dispatcher(),
        std::weak_ptr<const void>{menu_lifetime},
        menu_tree};
    NUI_CHECK(menu_domain.checkpoint_snapshot().has_value());

    auto menu_router = menu_domain.bridge().make_ordinary_router(menu_item_id);
    NUI_CHECK(menu_router.post(action_request(ui::SemanticAction::Activate)));
    NUI_CHECK(menu_callback_calls == 0);
    NUI_CHECK(menu_dispatcher.checkpoint() == 1);
    NUI_CHECK(menu_callback_calls == 1);
    NUI_CHECK(menu_service.dismiss_calls == 1);
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
    combo_box_semantic_expand_collapse_execute_once();
    combo_box_semantic_select_commits_highlighted_option_once();
    combo_box_semantic_actions_revalidate_availability();
    combo_box_stale_identity_fails_closed();
    menu_item_semantic_activate_executes_once();
    menu_item_semantic_failed_close_is_not_executed();
    combo_box_and_menu_item_actions_route_through_dispatcher();
}

} // namespace

int main() { return test::run("t068_retained_action_bridge", &suite); }
