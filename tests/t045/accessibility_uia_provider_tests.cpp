#include <nativeui/detail/semantic_uia_provider.hpp>

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/semantic_native_view_bridge.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("line " + std::to_string(__LINE__) +       \
                                     ": check failed: " #condition);            \
        }                                                                       \
    } while (false)

using ui::SemanticAction;
using ui::SemanticChange;
using ui::SemanticRole;
using ui::VirtualSemanticItemMetadata;
using ui::VirtualSemanticItemToken;
using ui::detail::SemanticActionRequest;
using ui::detail::SemanticActionTarget;
using ui::detail::SemanticIdentity;
using ui::detail::SemanticNativeGeometry;
using ui::detail::SemanticNativePublicationBatch;
using ui::detail::SemanticNativePublicationState;
using ui::detail::SemanticNativeViewBridge;
using ui::detail::UiaEventDerivation;
using ui::detail::UiaProviderEndpoint;
using ui::detail::UiaProviderHandle;
using ui::detail::UiaProviderHandlePtr;
using ui::detail::UiaProviderIdentity;
using ui::detail::UiaProviderRead;
using ui::detail::UiaProviderState;
using ui::detail::UiaValueKind;

[[nodiscard]] std::optional<UiaProviderIdentity> identity(
    ui::SemanticId node_id,
    std::optional<VirtualSemanticItemToken> token = std::nullopt) {
    return UiaProviderIdentity{node_id, token};
}

constexpr std::size_t kVirtualItemCount = 100000U;
constexpr VirtualSemanticItemToken kFirstToken = 9000U;

std::shared_ptr<const ui::SemanticTreeSnapshot> ordinary_snapshot(
    std::uint64_t generation,
    bool include_child,
    bool child_focused = false,
    bool child_selected = false) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = 1U;

    ui::SemanticNodeSnapshot root;
    root.id = 1U;
    root.info.role = SemanticRole::Group;
    root.info.name = "root";
    if (include_child) root.children.push_back(2U);
    snapshot->nodes.push_back(std::move(root));

    if (include_child) {
        ui::SemanticNodeSnapshot child;
        child.id = 2U;
        child.parent = 1U;
        child.info.role = SemanticRole::Button;
        child.info.name = "action";
        child.info.focused = child_focused;
        child.info.selected = child_selected;
        child.info.actions = {SemanticAction::Activate};
        snapshot->nodes.push_back(std::move(child));
    }
    return snapshot;
}

std::shared_ptr<const ui::SemanticTreeSnapshot> value_snapshot(
    SemanticRole role,
    std::uint64_t generation) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = 1U;

    ui::SemanticNodeSnapshot root;
    root.id = 1U;
    root.info.role = SemanticRole::Group;
    root.info.name = "root";
    root.children.push_back(2U);
    snapshot->nodes.push_back(std::move(root));

    ui::SemanticNodeSnapshot child;
    child.id = 2U;
    child.parent = 1U;
    child.info.role = role;
    child.info.name = "value";
    switch (role) {
        case SemanticRole::Slider:
            child.info.numeric_value = 0.5;
            child.info.value_range = ui::SemanticValueRange{0.0, 1.0, 0.1};
            child.info.actions = {
                SemanticAction::Increment,
                SemanticAction::Decrement,
                SemanticAction::SetValue,
                SemanticAction::Focus,
            };
            break;
        case SemanticRole::TextInput:
            child.info.text_value = std::string{"text"};
            child.info.actions = {SemanticAction::SetValue, SemanticAction::Focus};
            break;
        case SemanticRole::Checkbox:
            child.info.checked = ui::SemanticCheckedState::Checked;
            child.info.actions = {SemanticAction::Toggle, SemanticAction::Focus};
            break;
        case SemanticRole::ComboBox:
            child.info.text_value = std::string{"selected"};
            child.info.expanded = ui::SemanticExpandedState::Expanded;
            child.info.actions = {
                SemanticAction::Expand,
                SemanticAction::Collapse,
                SemanticAction::Select,
                SemanticAction::Focus,
            };
            break;
        default:
            break;
    }
    snapshot->nodes.push_back(std::move(child));
    return snapshot;
}

std::shared_ptr<const ui::SemanticTreeSnapshot> virtual_snapshot(
    std::uint64_t generation,
    std::size_t count,
    VirtualSemanticItemToken first_token,
    std::optional<VirtualSemanticItemToken> selected = std::nullopt) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = 11U;

    auto metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    auto token_index = std::make_shared<ui::VirtualSemanticChildren::TokenIndex>();
    metadata->reserve(count);
    token_index->reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const auto token = static_cast<VirtualSemanticItemToken>(
            first_token + static_cast<VirtualSemanticItemToken>(index));
        VirtualSemanticItemMetadata item;
        item.token = token;
        item.name = "row";
        item.actions = {SemanticAction::Select, SemanticAction::Focus};
        metadata->push_back(std::move(item));
        token_index->emplace(token, index);
    }

    ui::SemanticNodeSnapshot list;
    list.id = 11U;
    list.info.role = SemanticRole::ListView;
    list.bounds = {0.0f, 0.0f, 200.0f, 300.0f};
    list.virtual_children = ui::VirtualSemanticChildren::from_indexed_metadata(
        generation,
        std::shared_ptr<const ui::VirtualSemanticChildren::Metadata>{metadata},
        std::shared_ptr<const ui::VirtualSemanticChildren::TokenIndex>{token_index},
        selected,
        list.bounds,
        20.0f,
        0.0f);
    snapshot->nodes.push_back(std::move(list));
    return snapshot;
}

class RecordingTarget final : public SemanticActionTarget {
public:
    explicit RecordingTarget(std::shared_ptr<std::vector<SemanticAction>> actions)
        : actions_(std::move(actions)) {}

    [[nodiscard]] std::optional<ui::SemanticInfo> current_semantics(
        const SemanticIdentity& identity) const override {
        if (identity.virtual_token) {
            if (identity.node_id != 11U) return std::nullopt;
            ui::SemanticInfo info;
            info.role = SemanticRole::ListItem;
            info.actions = {SemanticAction::Select, SemanticAction::Focus};
            return info;
        }
        if (identity.node_id == 2U) {
            ui::SemanticInfo info;
            info.role = SemanticRole::Button;
            info.name = "action";
            info.actions = {SemanticAction::Activate, SemanticAction::Focus};
            return info;
        }
        return std::nullopt;
    }

    bool dispatch_semantic_action(
        const SemanticIdentity& identity,
        const SemanticActionRequest& request) override {
        actions_->push_back(request.action);
        return !identity.virtual_token || identity.node_id == 11U;
    }

private:
    std::shared_ptr<std::vector<SemanticAction>> actions_;
};

struct FactoryRecorder final {
    std::size_t creations{};
    std::vector<UiaProviderIdentity> identities;
    std::size_t retirements{};
};

class FakeHandle final : public UiaProviderHandle {
public:
    explicit FakeHandle(FactoryRecorder* recorder) : recorder_(recorder) {}

    void on_retired() noexcept override {
        if (recorder_) ++recorder_->retirements;
    }

private:
    FactoryRecorder* recorder_{};
};

UiaProviderHandlePtr fake_factory(void* user_data,
                                  const UiaProviderState& state,
                                  const UiaProviderRead& read) {
    auto* recorder = static_cast<FactoryRecorder*>(user_data);
    if (recorder) {
        ++recorder->creations;
        recorder->identities.push_back(
            UiaProviderIdentity{read.node_id(), read.virtual_token()});
    }
    CHECK(state.read().has_value());
    return std::make_shared<FakeHandle>(recorder);
}

struct ConcurrentFactoryRecorder final {
    std::atomic<std::size_t> creations{0U};
};

class ConcurrentFakeHandle final : public UiaProviderHandle {};

UiaProviderHandlePtr concurrent_fake_factory(
    void* user_data,
    const UiaProviderState& state,
    const UiaProviderRead&) {
    auto* recorder = static_cast<ConcurrentFactoryRecorder*>(user_data);
    if (recorder) {
        recorder->creations.fetch_add(1U, std::memory_order_relaxed);
    }
    CHECK(state.read().has_value());
    for (int attempt = 0; attempt < 64; ++attempt) {
        std::this_thread::yield();
    }
    return std::make_shared<ConcurrentFakeHandle>();
}

struct ProviderFixture final {
    ui::detail::DispatcherOwner owner;
    SemanticNativeViewBridge bridge;
    FactoryRecorder factory;
    std::shared_ptr<UiaProviderEndpoint> endpoint;
    std::shared_ptr<std::vector<SemanticAction>> dispatched{
        std::make_shared<std::vector<SemanticAction>>()};

    ProviderFixture() {
        bridge.bind_actions(
            owner.dispatcher(),
            std::make_shared<RecordingTarget>(dispatched));
        endpoint = std::make_shared<UiaProviderEndpoint>(
            bridge.native_reader_source(), bridge.native_action_endpoint());
        endpoint->set_factory(&fake_factory, &factory);
    }

    [[nodiscard]] std::optional<SemanticNativePublicationBatch> publish(
        const std::shared_ptr<const ui::SemanticTreeSnapshot>& snapshot,
        SemanticNativeGeometry geometry = {}) {
        bridge.stage(*snapshot);
        return bridge.checkpoint_native_publication(geometry);
    }
};

void publication_creates_no_providers() {
    ProviderFixture fixture;
    const auto batch = fixture.publish(ordinary_snapshot(1U, true));
    CHECK(batch.has_value());
    CHECK(fixture.endpoint->tracked_identities() == 0U);
    CHECK(fixture.factory.creations == 0U);
}

void root_and_ordinary_providers_are_stable_and_bounded() {
    ProviderFixture fixture;
    CHECK(fixture.publish(ordinary_snapshot(1U, true)).has_value());

    CHECK(fixture.endpoint->ordinary_child_count(1U) ==
          std::optional<std::size_t>{1U});

    const auto parent_read =
        fixture.endpoint->read(UiaProviderIdentity{1U, std::nullopt});
    CHECK(parent_read.has_value());
    CHECK(parent_read->parent_id() == ui::kInvalidSemanticId);
    CHECK(parent_read->child_count() == 1U);
    CHECK(parent_read->child_at(0U) == std::optional<ui::SemanticId>{2U});
    CHECK(parent_read->ordinary_child_index_of(2U) ==
          std::optional<std::size_t>{0U});
    CHECK(parent_read->ordinary_child_selected_at(0U) ==
          std::optional<bool>{false});
    CHECK(parent_read->virtual_child_count() == 0U);
    CHECK(!parent_read->virtual_selected_child_token().has_value());

    UiaProviderHandlePtr root = fixture.endpoint->root();
    CHECK(root != nullptr);
    CHECK(fixture.endpoint->root() == root);
    CHECK(fixture.endpoint->tracked_identities() == 1U);

    UiaProviderHandlePtr child = fixture.endpoint->ordinary(2U);
    CHECK(child != nullptr);
    CHECK(fixture.endpoint->ordinary(2U) == child);
    CHECK(fixture.endpoint->tracked_identities() == 2U);
    CHECK(fixture.factory.creations == 2U);
    CHECK(fixture.factory.identities.size() == 2U);
    CHECK(fixture.factory.identities[0] ==
          identity(1U));
    CHECK(fixture.factory.identities[1] ==
          identity(2U));

    CHECK(fixture.endpoint->ordinary_child_at(1U, 0U) == child);
    CHECK(fixture.endpoint->ordinary_child_at(1U, 1U) == nullptr);
    CHECK(!fixture.endpoint->ordinary(3U));
    CHECK(fixture.factory.creations == 2U);
}

void bounded_provider_cache_prunes_defunct_identities() {
    ProviderFixture fixture;
    CHECK(fixture.publish(ordinary_snapshot(1U, true)).has_value());

    const UiaProviderHandlePtr stale = fixture.endpoint->ordinary(2U);
    CHECK(stale != nullptr);
    CHECK(fixture.endpoint->tracked_identities() == 1U);

    const auto removal = fixture.publish(ordinary_snapshot(2U, false));
    CHECK(removal.has_value());
    CHECK(fixture.endpoint->apply_publication_batch(*removal) == 1U);
    CHECK(fixture.endpoint->tracked_identities() == 0U);
    CHECK(fixture.factory.retirements == 1U);

    // A still-live handle reports the stale identity as absent, exactly like a
    // native provider that outlived the semantic generation that created it.
    CHECK(!UiaProviderRead::from_state(
        UiaProviderState::ordinary(fixture.bridge.native_reader_source(), 2U)
            .value()));
    CHECK(!fixture.endpoint->ordinary(2U));
    CHECK(fixture.factory.creations == 1U);
}

void superseded_or_foreign_batches_never_prune() {
    ProviderFixture fixture;

    auto first = fixture.publish(ordinary_snapshot(1U, true));
    CHECK(first.has_value());
    const UiaProviderHandlePtr live = fixture.endpoint->ordinary(2U);
    CHECK(live != nullptr);

    auto second = fixture.publish(ordinary_snapshot(2U, false));
    CHECK(second.has_value());

    // The already superseded first batch must not prune the current cache.
    CHECK(fixture.endpoint->apply_publication_batch(*first) == 0U);
    CHECK(fixture.endpoint->tracked_identities() == 1U);

    // The exact current structured batch does.
    CHECK(fixture.endpoint->apply_publication_batch(*second) == 1U);
    CHECK(fixture.endpoint->tracked_identities() == 0U);

    // A batch with no structure category never scans the cache.
    auto third = fixture.publish(ordinary_snapshot(3U, true));
    CHECK(third.has_value());
    CHECK(fixture.endpoint->ordinary(2U) != nullptr);
    SemanticNativePublicationBatch bounds_only = *third;
    bounds_only.changes = {SemanticChange::BoundsChanged};
    CHECK(fixture.endpoint->apply_publication_batch(bounds_only) == 0U);
    CHECK(fixture.endpoint->tracked_identities() == 1U);
}

void lazy_virtual_items_do_not_materialize_the_dataset() {
    ProviderFixture fixture;
    CHECK(fixture.publish(virtual_snapshot(
              1U, kVirtualItemCount, kFirstToken))
              .has_value());

    CHECK(fixture.endpoint->virtual_child_count(11U) ==
          std::optional<std::size_t>{kVirtualItemCount});
    CHECK(fixture.endpoint->ordinary_child_count(11U) ==
          std::optional<std::size_t>{0U});
    CHECK(fixture.endpoint->tracked_identities() == 0U);
    CHECK(fixture.factory.creations == 0U);

    const auto last = static_cast<VirtualSemanticItemToken>(
        kFirstToken + static_cast<VirtualSemanticItemToken>(kVirtualItemCount - 1U));
    UiaProviderHandlePtr tail = fixture.endpoint->virtual_item(11U, last);
    CHECK(tail != nullptr);
    CHECK(fixture.endpoint->tracked_identities() == 1U);
    CHECK(fixture.endpoint->virtual_item(11U, last) == tail);
    CHECK(fixture.endpoint->tracked_identities() == 1U);
    CHECK(fixture.endpoint->virtual_child_at(11U, kVirtualItemCount - 1U) == tail);
    CHECK(fixture.endpoint->tracked_identities() == 1U);

    const auto* stored = fixture.factory.identities.empty()
        ? nullptr
        : &fixture.factory.identities.back();
    CHECK(stored != nullptr);
    CHECK(stored->node_id == 11U);
    CHECK(stored->virtual_token == std::optional<VirtualSemanticItemToken>{last});
}

void virtual_selection_and_index_resolution_stay_bounded() {
    constexpr VirtualSemanticItemToken kSelected = kFirstToken + 123U;
    ProviderFixture fixture;
    CHECK(fixture.publish(virtual_snapshot(1U, kVirtualItemCount, kFirstToken,
                                          kSelected))
              .has_value());

    CHECK(fixture.endpoint->virtual_child_index_of(11U, kSelected) ==
          std::optional<std::size_t>{kSelected - kFirstToken});
    CHECK(!fixture.endpoint->virtual_child_index_of(11U, 1U).has_value());

    UiaProviderHandlePtr selected = fixture.endpoint->selected_child(11U);
    CHECK(selected != nullptr);
    CHECK(fixture.endpoint->tracked_identities() == 1U);
    const auto* stored = fixture.factory.identities.empty()
        ? nullptr
        : &fixture.factory.identities.back();
    CHECK(stored != nullptr);
    CHECK(stored->node_id == 11U);
    CHECK(stored->virtual_token ==
          std::optional<VirtualSemanticItemToken>{kSelected});
}

void focused_and_selected_identity_resolution() {
    ProviderFixture fixture;
    CHECK(fixture.publish(ordinary_snapshot(1U, true, true, true)).has_value());

    CHECK(fixture.endpoint->focused_identity() ==
          identity(2U));
    CHECK(fixture.endpoint->selected_identity() ==
          identity(2U));
    CHECK(fixture.endpoint->focused() != nullptr);
    CHECK(fixture.endpoint->selected_child(1U) == fixture.endpoint->ordinary(2U));

    CHECK(fixture.publish(ordinary_snapshot(2U, true, false, false)).has_value());
    CHECK(!fixture.endpoint->focused_identity().has_value());
    CHECK(!fixture.endpoint->selected_identity().has_value());
    CHECK(fixture.endpoint->selected_child(1U) == nullptr);
}

void focused_identity_for_an_ordinary_child_resolves_to_a_provider() {
    ProviderFixture fixture;
    CHECK(fixture.publish(ordinary_snapshot(1U, true, true)).has_value());
    const auto focused = fixture.endpoint->focused();
    CHECK(focused != nullptr);
    CHECK(fixture.endpoint->tracked_identities() == 1U);
    CHECK(fixture.factory.identities.back() ==
          identity(2U));
}

void notification_categories_map_to_derived_events() {
    SemanticNativePublicationState state;
    auto publish = [&state](const std::shared_ptr<const ui::SemanticTreeSnapshot>& snapshot,
                            std::vector<SemanticChange> changes) {
        return state.publish(snapshot, changes, SemanticNativeGeometry{});
    };

    const auto structure = publish(
        ordinary_snapshot(1U, true), {SemanticChange::StructureChanged});
    CHECK(structure.has_value());
    const UiaEventDerivation structure_events = ui::detail::derive_uia_events(*structure);
    CHECK(structure_events.structure);
    CHECK(!structure_events.focus);
    CHECK(!structure_events.selection);
    CHECK(!structure_events.value);
    CHECK(!structure_events.bounds);
    CHECK(structure_events.structure_target ==
          identity(1U));
    CHECK(!structure_events.focus_target);

    const auto focus = publish(
        ordinary_snapshot(2U, true, /*child_focused=*/true),
        {SemanticChange::FocusChanged});
    CHECK(focus.has_value());
    const UiaEventDerivation focus_events = ui::detail::derive_uia_events(*focus);
    CHECK(focus_events.focus);
    CHECK(!focus_events.structure);
    CHECK(focus_events.focus_target ==
          identity(2U));

    const auto focus_cleared = publish(
        ordinary_snapshot(3U, true, /*child_focused=*/false),
        {SemanticChange::FocusChanged});
    CHECK(focus_cleared.has_value());
    const UiaEventDerivation cleared_events = ui::detail::derive_uia_events(*focus_cleared);
    CHECK(cleared_events.focus);
    CHECK(cleared_events.focus_target ==
          identity(1U));

    const auto selection = publish(
        ordinary_snapshot(4U, true, false, /*child_selected=*/true),
        {SemanticChange::SelectionChanged});
    CHECK(selection.has_value());
    const UiaEventDerivation selection_events = ui::detail::derive_uia_events(*selection);
    CHECK(selection_events.selection);
    CHECK(!selection_events.structure);
    CHECK(selection_events.selection_target ==
          identity(2U));

    const auto bounds = publish(
        ordinary_snapshot(5U, true), {SemanticChange::BoundsChanged});
    CHECK(bounds.has_value());
    const UiaEventDerivation bounds_events = ui::detail::derive_uia_events(*bounds);
    CHECK(bounds_events.bounds);
    CHECK(!bounds_events.structure);
    CHECK(bounds_events.bounds_target ==
          identity(1U));
}

void virtual_selection_notification_targets_the_logical_item() {
    SemanticNativePublicationState state;
    constexpr VirtualSemanticItemToken kSelected = 9005U;
    const auto published = state.publish(
        virtual_snapshot(1U, 64U, kFirstToken, kSelected),
        {SemanticChange::SelectionChanged},
        SemanticNativeGeometry{});
    CHECK(published.has_value());

    const UiaEventDerivation events = ui::detail::derive_uia_events(*published);
    CHECK(events.selection);
    CHECK(events.selection_target == identity(11U, kSelected));
}

void value_notification_classifies_the_target_domain() {
    SemanticNativePublicationState state;
    auto publish_value = [&state](SemanticRole role, std::uint64_t generation) {
        return state.publish(
            value_snapshot(role, generation),
            {SemanticChange::ValueChanged},
            SemanticNativeGeometry{});
    };

    const auto slider = publish_value(SemanticRole::Slider, 1U);
    CHECK(slider.has_value());
    const UiaEventDerivation slider_events = ui::detail::derive_uia_events(*slider);
    CHECK(slider_events.value);
    CHECK(slider_events.value_target ==
          identity(2U));
    CHECK(slider_events.value_kind == UiaValueKind::Numeric);

    const auto input = publish_value(SemanticRole::TextInput, 2U);
    CHECK(input.has_value());
    const UiaEventDerivation input_events = ui::detail::derive_uia_events(*input);
    CHECK(input_events.value_kind == UiaValueKind::Text);

    const auto checkbox = publish_value(SemanticRole::Checkbox, 3U);
    CHECK(checkbox.has_value());
    const UiaEventDerivation checkbox_events = ui::detail::derive_uia_events(*checkbox);
    CHECK(checkbox_events.value_kind == UiaValueKind::Toggle);

    const auto combo = publish_value(SemanticRole::ComboBox, 4U);
    CHECK(combo.has_value());
    const UiaEventDerivation combo_events = ui::detail::derive_uia_events(*combo);
    CHECK(combo_events.value_kind == UiaValueKind::Expanded);

    // A generic container has no faithful UIA property to raise; the adapter
    // must not fabricate a value notification for an unknown domain.
    SemanticNativePublicationState plain_state;
    const auto plain = plain_state.publish(
        ordinary_snapshot(1U, true),
        {SemanticChange::ValueChanged},
        SemanticNativeGeometry{});
    CHECK(plain.has_value());
    const UiaEventDerivation plain_events = ui::detail::derive_uia_events(*plain);
    CHECK(plain_events.value);
    CHECK(plain_events.value_kind == UiaValueKind::None);
}

void derivation_fails_closed_without_a_publication() {
    SemanticNativePublicationBatch empty;
    const UiaEventDerivation events = ui::detail::derive_uia_events(empty);
    CHECK(!events.structure);
    CHECK(!events.focus);
    CHECK(!events.selection);
    CHECK(!events.value);
    CHECK(!events.bounds);
}

void hit_testing_primitives_stay_bounded_for_virtual_lists() {
    ProviderFixture fixture;
    CHECK(fixture.publish(virtual_snapshot(1U, kVirtualItemCount, kFirstToken))
              .has_value());

    const auto list_read =
        fixture.endpoint->read(UiaProviderIdentity{11U, std::nullopt});
    CHECK(list_read.has_value());
    CHECK(list_read->parent_id() == ui::kInvalidSemanticId);
    CHECK(list_read->child_count() == 0U);
    CHECK(list_read->virtual_child_count() == kVirtualItemCount);
    CHECK(list_read->virtual_child_token_at(0U) ==
          std::optional<VirtualSemanticItemToken>{kFirstToken});
    CHECK(list_read->virtual_child_token_at(kVirtualItemCount) ==
          std::nullopt);
    CHECK(list_read->virtual_child_index_of(
              static_cast<VirtualSemanticItemToken>(kFirstToken + 7U)) ==
          std::optional<std::size_t>{7U});
    // List bounds y=0 h=300, fixed 20px rows, scroll 0: y=205 resolves to row
    // 10 without visiting any metadata or materializing a native provider.
    CHECK(list_read->virtual_child_index_at_logical_y(205.0f) ==
          std::optional<std::size_t>{10U});
    CHECK(!list_read->virtual_child_index_at_logical_y(305.0f).has_value());
    CHECK(list_read->virtual_child_index_at_logical_y(0.0f) ==
          std::optional<std::size_t>{0U});

    const auto item_read =
        fixture.endpoint->read(UiaProviderIdentity{11U, kFirstToken});
    CHECK(item_read.has_value());
    CHECK(item_read->parent_id() == 11U);
    CHECK(item_read->child_count() == 0U);
    CHECK(item_read->virtual_child_count() == 0U);
}

void concurrent_queries_share_one_cached_provider() {
    SemanticNativePublicationState state;
    CHECK(state.publish(ordinary_snapshot(1U, true),
                        {SemanticChange::StructureChanged},
                        SemanticNativeGeometry{})
              .has_value());

    ConcurrentFactoryRecorder factory;
    UiaProviderEndpoint endpoint{state.reader_source()};
    endpoint.set_factory(&concurrent_fake_factory, &factory);

    constexpr std::size_t kReaderCount = 16U;
    std::array<UiaProviderHandlePtr, kReaderCount> handles;
    std::array<std::thread, kReaderCount> readers;
    std::atomic<std::size_t> ready{0U};
    std::atomic<bool> start{false};

    for (std::size_t index = 0U; index < kReaderCount; ++index) {
        readers[index] = std::thread([&, index] {
            ready.fetch_add(1U, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            handles[index] = endpoint.ordinary(2U);
        });
    }
    while (ready.load(std::memory_order_acquire) != kReaderCount) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);
    for (auto& reader : readers) {
        reader.join();
    }

    CHECK(handles.front() != nullptr);
    for (const auto& handle : handles) {
        CHECK(handle == handles.front());
    }
    CHECK(factory.creations.load(std::memory_order_relaxed) == 1U);
    CHECK(endpoint.tracked_identities() == 1U);
}

void two_views_keep_isolated_caches() {
    SemanticNativePublicationState first_state;
    SemanticNativePublicationState second_state;
    CHECK(first_state.publish(ordinary_snapshot(1U, true),
                              {SemanticChange::StructureChanged},
                              SemanticNativeGeometry{})
              .has_value());
    CHECK(second_state.publish(ordinary_snapshot(1U, true),
                               {SemanticChange::StructureChanged},
                               SemanticNativeGeometry{})
              .has_value());

    FactoryRecorder first_factory;
    FactoryRecorder second_factory;
    UiaProviderEndpoint first_view{first_state.reader_source()};
    UiaProviderEndpoint second_view{second_state.reader_source()};
    first_view.set_factory(&fake_factory, &first_factory);
    second_view.set_factory(&fake_factory, &second_factory);

    const UiaProviderHandlePtr first_root = first_view.root();
    const UiaProviderHandlePtr second_root = second_view.root();
    const UiaProviderHandlePtr first_child = first_view.ordinary(2U);
    const UiaProviderHandlePtr second_child = second_view.ordinary(2U);
    CHECK(first_root != nullptr);
    CHECK(second_root != nullptr);
    CHECK(first_child != nullptr);
    CHECK(second_child != nullptr);
    CHECK(first_root != second_root);
    CHECK(first_child != second_child);
    CHECK(first_view.tracked_identities() == 2U);
    CHECK(second_view.tracked_identities() == 2U);

    const auto removal = first_state.publish(
        ordinary_snapshot(2U, false),
        {SemanticChange::StructureChanged},
        SemanticNativeGeometry{});
    CHECK(removal.has_value());
    CHECK(first_view.apply_publication_batch(*removal) == 1U);
    CHECK(first_view.tracked_identities() == 1U);
    CHECK(first_view.root() == first_root);
    CHECK(first_view.ordinary(2U) == nullptr);

    // The independent second view is untouched: same semantic identity in
    // another view never shares a provider, cache entry or notification state.
    CHECK(second_view.tracked_identities() == 2U);
    CHECK(second_view.root() == second_root);
    CHECK(second_view.ordinary(2U) == second_child);
    CHECK(second_factory.creations == 2U);
}

void bridge_shutdown_stales_existing_providers() {
    ProviderFixture fixture;
    CHECK(fixture.publish(ordinary_snapshot(1U, true)).has_value());

    const UiaProviderState state =
        *UiaProviderState::ordinary(fixture.bridge.native_reader_source(), 2U);
    CHECK(state.read().has_value());

    fixture.bridge.shutdown();
    CHECK(!state.read().has_value());
    CHECK(!UiaProviderRead::from_state(state).has_value());
    CHECK(!fixture.endpoint->ordinary(2U));
    CHECK(fixture.endpoint->root() == nullptr);
}

void provider_actions_route_through_dispatcher() {
    ProviderFixture fixture;
    CHECK(fixture.publish(ordinary_snapshot(1U, true)).has_value());

    const UiaProviderState child =
        *UiaProviderState::ordinary(fixture.bridge.native_reader_source(), 2U,
                                    fixture.bridge.native_action_endpoint());
    CHECK(child.post_action(SemanticAction::Activate));
    CHECK(fixture.dispatched->empty());
    CHECK(fixture.owner.checkpoint() == 1U);
    CHECK(fixture.dispatched->size() == 1U);
    CHECK(fixture.dispatched->front() == SemanticAction::Activate);

    // A stale identity fails closed before anything can reach the UI thread.
    const UiaProviderState stale =
        *UiaProviderState::ordinary(fixture.bridge.native_reader_source(), 42U,
                                    fixture.bridge.native_action_endpoint());
    CHECK(!stale.post_action(SemanticAction::Activate));
    CHECK(fixture.owner.checkpoint() == 0U);

    // Without a lifetime-safe action endpoint a read-only view stays readable
    // but cannot mutate.
    const UiaProviderState read_only =
        *UiaProviderState::ordinary(fixture.bridge.native_reader_source(), 2U);
    CHECK(read_only.read().has_value());
    CHECK(!read_only.post_action(SemanticAction::Activate));
}

void retired_handles_are_released_through_the_retirement_hook() {
    ProviderFixture fixture;
    CHECK(fixture.publish(ordinary_snapshot(1U, true)).has_value());

    {
        const UiaProviderHandlePtr child = fixture.endpoint->ordinary(2U);
        CHECK(child != nullptr);
        CHECK(fixture.factory.retirements == 0U);
    }

    fixture.endpoint->clear();
    CHECK(fixture.endpoint->tracked_identities() == 0U);
    CHECK(fixture.factory.retirements == 1U);
}

} // namespace

int main() {
    try {
        publication_creates_no_providers();
        root_and_ordinary_providers_are_stable_and_bounded();
        bounded_provider_cache_prunes_defunct_identities();
        superseded_or_foreign_batches_never_prune();
        lazy_virtual_items_do_not_materialize_the_dataset();
        virtual_selection_and_index_resolution_stay_bounded();
        focused_and_selected_identity_resolution();
        focused_identity_for_an_ordinary_child_resolves_to_a_provider();
        notification_categories_map_to_derived_events();
        virtual_selection_notification_targets_the_logical_item();
        value_notification_classifies_the_target_domain();
        derivation_fails_closed_without_a_publication();
        hit_testing_primitives_stay_bounded_for_virtual_lists();
        concurrent_queries_share_one_cached_provider();
        two_views_keep_isolated_caches();
        bridge_shutdown_stales_existing_providers();
        provider_actions_route_through_dispatcher();
        retired_handles_are_released_through_the_retirement_hook();
        std::cout << "PASS semantic UIA provider\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL semantic UIA provider: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
