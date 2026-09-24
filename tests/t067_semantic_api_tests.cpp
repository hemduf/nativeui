#include "test_support.hpp"

#include <nativeui/detail/semantic_live_action.hpp>
#include <nativeui/detail/semantic_tree.hpp>
#include <nativeui/nativeui.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

class SemanticProbe final : public ui::Component {
public:
    SemanticProbe(ui::SemanticInfo info, ui::ComponentAvailability availability = {})
        : info_(std::move(info)), availability_(availability) {}

    [[nodiscard]] ui::SemanticInfo semantics() const override { return info_; }
    [[nodiscard]] ui::ComponentAvailability local_availability() const noexcept override {
        return availability_;
    }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {};
    }
    void paint(ui::PaintContext&) const override {}

private:
    ui::SemanticInfo info_;
    ui::ComponentAvailability availability_;
};

class VirtualSemanticProbe final : public ui::Component,
                                   public ui::detail::VirtualSemanticChildrenSource {
public:
    VirtualSemanticProbe(ui::VirtualSemanticChildren::MetadataSnapshot metadata,
                         ui::VirtualSemanticChildren::TokenIndexSnapshot token_index)
        : metadata_(std::move(metadata)), token_index_(std::move(token_index)) {}

    [[nodiscard]] ui::SemanticInfo semantics() const override {
        ui::SemanticInfo result;
        result.role = ui::SemanticRole::ListView;
        result.name = "Items";
        result.focusable = true;
        result.actions = {ui::SemanticAction::Focus};
        return result;
    }

    [[nodiscard]] ui::VirtualSemanticChildren virtual_semantic_children(
        ui::Rect semantic_bounds) const override {
        return ui::VirtualSemanticChildren::from_indexed_metadata(
            9,
            metadata_,
            token_index_,
            ui::VirtualSemanticItemToken{20},
            semantic_bounds,
            20.0f,
            5.0f);
    }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {};
    }
    void paint(ui::PaintContext&) const override {}

private:
    ui::VirtualSemanticChildren::MetadataSnapshot metadata_;
    ui::VirtualSemanticChildren::TokenIndexSnapshot token_index_;
};

class LiveActionComponent final : public ui::Component,
                                  public ui::detail::SemanticActionHandler {
public:
    ui::SemanticInfo info;
    int dispatch_count{};

    [[nodiscard]] ui::SemanticInfo semantics() const override { return info; }

    [[nodiscard]] bool perform_semantic_action(
        const ui::detail::SemanticActionRequest&) override {
        ++dispatch_count;
        return true;
    }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {};
    }

    void paint(ui::PaintContext&) const override {}
};

struct LiveTreeFixture {
    ui::Node root;
    LiveActionComponent* target{};

    LiveTreeFixture() {
        root.id = 1;

        auto component = std::make_unique<LiveActionComponent>();
        target = component.get();
        target->info.role = ui::SemanticRole::Slider;
        target->info.name = "Value";
        target->info.focusable = true;
        target->info.actions = {
            ui::SemanticAction::Increment,
            ui::SemanticAction::Decrement,
            ui::SemanticAction::SetValue,
            ui::SemanticAction::Focus,
        };

        auto child = std::make_unique<ui::Node>();
        child->id = 42;
        child->parent = &root;
        child->component = std::move(component);
        root.children.push_back(std::move(child));
    }
};

ui::SemanticInfo semantic_info(ui::SemanticRole role, std::string name = {}) {
    ui::SemanticInfo result;
    result.role = role;
    result.name = std::move(name);
    return result;
}

std::unique_ptr<ui::Node> semantic_node(
    ui::NodeId id,
    ui::SemanticInfo semantic,
    ui::ComponentAvailability availability = {}) {
    auto result = std::make_unique<ui::Node>();
    result->id = id;
    result->component = std::make_unique<SemanticProbe>(std::move(semantic), availability);
    return result;
}

void append_semantic_child(ui::Node& parent, std::unique_ptr<ui::Node> child) {
    child->parent = &parent;
    parent.children.push_back(std::move(child));
}

const ui::SemanticNodeSnapshot& require_semantic_node(
    const ui::SemanticTreeSnapshot& snapshot,
    ui::SemanticId id) {
    for (const auto& item : snapshot.nodes) {
        if (item.id == id) return item;
    }
    throw std::runtime_error("semantic node not found");
}

void retained_tree_semantics_flatten_wrappers_and_honor_availability() {
    auto root = semantic_node(1, semantic_info(ui::SemanticRole::Group, "Root"));
    root->bounds = {0.0f, 0.0f, 200.0f, 100.0f};

    auto wrapper_info = semantic_info(ui::SemanticRole::None);
    wrapper_info.description = "Inherited help";
    auto wrapper = semantic_node(2, std::move(wrapper_info));
    wrapper->bounds = {0.0f, 0.0f, 80.0f, 20.0f};

    auto button_info = semantic_info(ui::SemanticRole::Button, "Apply");
    button_info.focusable = true;
    button_info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};
    auto button = semantic_node(3, std::move(button_info));
    button->bounds = {0.0f, 0.0f, 80.0f, 20.0f};
    append_semantic_child(*wrapper, std::move(button));
    append_semantic_child(*root, std::move(wrapper));

    ui::ComponentAvailability hidden;
    hidden.visibility = ui::VisibilityMode::Hidden;
    auto hidden_text = semantic_node(4, semantic_info(ui::SemanticRole::Text, "Hidden"), hidden);
    hidden_text->bounds = {0.0f, 20.0f, 80.0f, 20.0f};
    append_semantic_child(*root, std::move(hidden_text));

    auto text = semantic_node(5, semantic_info(ui::SemanticRole::Text, "Visible"));
    text->bounds = {0.0f, 40.0f, 80.0f, 20.0f};
    append_semantic_child(*root, std::move(text));

    ui::ComponentAvailability disabled;
    disabled.enabled = false;
    auto disabled_info = semantic_info(ui::SemanticRole::Button, "Disabled");
    disabled_info.focusable = true;
    disabled_info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};
    auto disabled_button = semantic_node(6, std::move(disabled_info), disabled);
    disabled_button->bounds = {0.0f, 60.0f, 80.0f, 20.0f};
    append_semantic_child(*root, std::move(disabled_button));

    ui::Node* root_ptr = root.get();
    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(*root_ptr, 3);
    NUI_CHECK(snapshot.root == 1);
    NUI_CHECK(snapshot.nodes.size() == 4);

    const auto& semantic_root = require_semantic_node(snapshot, 1);
    NUI_CHECK(semantic_root.parent == ui::kInvalidSemanticId);
    NUI_CHECK(semantic_root.children == std::vector<ui::SemanticId>({3, 5, 6}));

    const auto& button_node = require_semantic_node(snapshot, 3);
    NUI_CHECK(button_node.parent == 1);
    NUI_CHECK(button_node.info.description == "Inherited help");
    NUI_CHECK(button_node.info.enabled);
    NUI_CHECK(button_node.info.focusable);
    NUI_CHECK(button_node.info.focused);
    NUI_CHECK(button_node.info.actions.size() == 2);

    const auto& text_node = require_semantic_node(snapshot, 5);
    NUI_CHECK(text_node.parent == 1);
    NUI_CHECK(text_node.info.name == "Visible");

    const auto& disabled_node = require_semantic_node(snapshot, 6);
    NUI_CHECK(!disabled_node.info.enabled);
    NUI_CHECK(!disabled_node.info.focusable);
    NUI_CHECK(!disabled_node.info.focused);
    NUI_CHECK(disabled_node.info.actions.empty());

    for (const auto& item : snapshot.nodes) {
        NUI_CHECK(item.id != 2);
        NUI_CHECK(item.id != 4);
    }
}

void retained_tree_semantics_attach_virtual_collection_without_metadata_copy() {
    auto metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    metadata->push_back({
        10,
        "Ten",
        "",
        true,
        false,
        ui::SemanticCheckedState::NotApplicable,
        {ui::SemanticAction::Select, ui::SemanticAction::Focus},
    });
    metadata->push_back({
        20,
        "Twenty",
        "",
        true,
        false,
        ui::SemanticCheckedState::NotApplicable,
        {ui::SemanticAction::Select, ui::SemanticAction::Focus},
    });
    auto token_index = std::make_shared<ui::VirtualSemanticChildren::TokenIndex>();
    token_index->emplace(10, 0);
    token_index->emplace(20, 1);

    auto root = std::make_unique<ui::Node>();
    root->id = 7;
    root->bounds = {10.0f, 20.0f, 120.0f, 40.0f};
    root->component = std::make_unique<VirtualSemanticProbe>(metadata, token_index);

    ui::Node* root_ptr = root.get();
    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(*root_ptr, 7);
    NUI_CHECK(snapshot.root == 7);
    NUI_CHECK(snapshot.nodes.size() == 1);

    const auto& list = require_semantic_node(snapshot, 7);
    NUI_CHECK(list.info.role == ui::SemanticRole::ListView);
    NUI_CHECK(list.virtual_children.has_value());
    NUI_CHECK(list.virtual_children->metadata_snapshot().get() == metadata.get());
    NUI_CHECK(list.virtual_children->token_index_snapshot().get() == token_index.get());
    NUI_CHECK(list.virtual_children->size() == 2);

    const auto item = list.virtual_children->item_at(1);
    NUI_CHECK(item.has_value());
    NUI_CHECK(item->token == 20);
    NUI_CHECK(item->info.name == "Twenty");
    NUI_CHECK(item->info.selected);
    NUI_CHECK_NEAR(item->logical_bounds.x, 10.0f, 0.0001f);
    NUI_CHECK_NEAR(item->logical_bounds.y, 35.0f, 0.0001f);
    NUI_CHECK_NEAR(item->logical_bounds.w, 120.0f, 0.0001f);
    NUI_CHECK_NEAR(item->logical_bounds.h, 20.0f, 0.0001f);

    const ui::detail::SemanticIdentity current_item_identity{
        7, ui::VirtualSemanticItemToken{20}};
    const auto current_item = ui::detail::current_live_semantics(
        *root_ptr, current_item_identity, 7);
    NUI_CHECK(current_item.has_value());
    NUI_CHECK(current_item->role == ui::SemanticRole::ListItem);
    NUI_CHECK(current_item->name == "Twenty");
    NUI_CHECK(current_item->selected);
    NUI_CHECK(current_item->focusable);
    NUI_CHECK(!current_item->focused);
    NUI_CHECK(current_item->supports(ui::SemanticAction::Select));
    NUI_CHECK(current_item->supports(ui::SemanticAction::Focus));

    const ui::detail::SemanticIdentity stale_item_identity{
        7, ui::VirtualSemanticItemToken{999}};
    NUI_CHECK(!ui::detail::current_live_semantics(
        *root_ptr, stale_item_identity, 7));

    const ui::detail::SemanticIdentity container_identity{7, std::nullopt};
    const auto current_container = ui::detail::current_live_semantics(
        *root_ptr, container_identity, 7);
    NUI_CHECK(current_container.has_value());
    NUI_CHECK(current_container->role == ui::SemanticRole::ListView);
    NUI_CHECK(current_container->focused);
}

void live_semantic_actions_resolve_and_recheck_current_state() {
    LiveTreeFixture fixture;
    const ui::detail::SemanticIdentity identity{42, std::nullopt};

    const auto info = ui::detail::current_live_semantics(fixture.root, identity, 42);
    NUI_CHECK(info.has_value());
    NUI_CHECK(info->role == ui::SemanticRole::Slider);
    NUI_CHECK(info->focused);
    NUI_CHECK(info->supports(ui::SemanticAction::Increment));

    ui::detail::SemanticActionRequest increment;
    increment.action = ui::SemanticAction::Increment;
    NUI_CHECK(ui::detail::dispatch_live_semantic_action(
        fixture.root, identity, increment, 42));
    NUI_CHECK(fixture.target->dispatch_count == 1);

    fixture.target->info.read_only = true;
    auto current = ui::detail::current_live_semantics(fixture.root, identity);
    NUI_CHECK(current.has_value());
    NUI_CHECK(current->read_only);
    NUI_CHECK(!current->supports(ui::SemanticAction::Increment));
    NUI_CHECK(!ui::detail::dispatch_live_semantic_action(
        fixture.root, identity, increment));
    NUI_CHECK(fixture.target->dispatch_count == 1);

    fixture.target->info.read_only = false;
    fixture.target->info.enabled = false;
    current = ui::detail::current_live_semantics(fixture.root, identity);
    NUI_CHECK(current.has_value());
    NUI_CHECK(!current->enabled);
    NUI_CHECK(current->actions.empty());
    NUI_CHECK(!ui::detail::dispatch_live_semantic_action(
        fixture.root, identity, increment));
    NUI_CHECK(fixture.target->dispatch_count == 1);

    fixture.target->info.enabled = true;
    fixture.target->info.actions = {ui::SemanticAction::Focus};
    NUI_CHECK(!ui::detail::dispatch_live_semantic_action(
        fixture.root, identity, increment));
    NUI_CHECK(fixture.target->dispatch_count == 1);

    const ui::detail::SemanticIdentity stale{999, std::nullopt};
    NUI_CHECK(!ui::detail::current_live_semantics(fixture.root, stale));
    NUI_CHECK(!ui::detail::dispatch_live_semantic_action(
        fixture.root, stale, increment));

    const ui::detail::SemanticIdentity virtual_item{
        42, ui::VirtualSemanticItemToken{7}};
    NUI_CHECK(!ui::detail::current_live_semantics(fixture.root, virtual_item));
    NUI_CHECK(!ui::detail::dispatch_live_semantic_action(
        fixture.root, virtual_item, increment));

    fixture.target->info.role = ui::SemanticRole::None;
    NUI_CHECK(!ui::detail::current_live_semantics(fixture.root, identity));
    NUI_CHECK(!ui::detail::dispatch_live_semantic_action(
        fixture.root, identity, increment));
    NUI_CHECK(fixture.target->dispatch_count == 1);
}

void suite() {
    using VirtualState = ui::VirtualListState<int>;

    ui::State<std::optional<int>> selected{std::nullopt};
    std::size_t row_factory_calls = 0;
    VirtualState state{
        selected,
        20.0f,
        [&row_factory_calls](const VirtualState::Item&) {
            ++row_factory_calls;
            return ui::Spacer{320.0f, 20.0f};
        }};

    std::vector<VirtualState::Item> items;
    items.reserve(100000);
    for (int index = 0; index < 100000; ++index) {
        items.emplace_back(index, "row " + std::to_string(index));
    }
    NUI_CHECK(state.replace(items));
    NUI_CHECK(row_factory_calls == 0);

    const auto generation = state.dataset_generation();
    const auto old_metadata = state.metadata_snapshot();
    NUI_CHECK(old_metadata);
    NUI_CHECK(old_metadata->size() == 100000);

    const ui::Rect list_bounds{10.0f, 20.0f, 320.0f, 400.0f};
    const auto projection = state.semantic_children(list_bounds);
    NUI_CHECK(projection.dataset_generation() == generation);
    NUI_CHECK(projection.metadata_snapshot().get() == old_metadata.get());

    const auto offscreen = projection.item_at(99999);
    NUI_CHECK(offscreen.has_value());
    NUI_CHECK(offscreen->info.name == "row 99999");
    NUI_CHECK_NEAR(offscreen->logical_bounds.x, 10.0f, 0.0001f);
    NUI_CHECK_NEAR(offscreen->logical_bounds.y, 20.0f + 99999.0f * 20.0f, 0.01f);
    NUI_CHECK_NEAR(offscreen->logical_bounds.w, 320.0f, 0.0001f);
    NUI_CHECK_NEAR(offscreen->logical_bounds.h, 20.0f, 0.0001f);
    NUI_CHECK(row_factory_calls == 0);

    // Selection changes only the small scalar semantic projection. They must
    // never rebuild/copy the O(N) immutable metadata snapshot.
    selected.set(99999);
    for (int iteration = 0; iteration < 1000; ++iteration) {
        const auto view = state.semantic_children(list_bounds);
        NUI_CHECK(view.dataset_generation() == generation);
        NUI_CHECK(view.metadata_snapshot().get() == old_metadata.get());
    }
    const auto selected_item = state.semantic_children(list_bounds).item_at(99999);
    NUI_CHECK(selected_item.has_value());
    NUI_CHECK(selected_item->info.selected);
    NUI_CHECK(row_factory_calls == 0);

    // A real dataset metadata change publishes exactly one new immutable
    // generation. A native-side reader may retain the old shared snapshot
    // concurrently while the UI thread publishes the replacement generation.
    std::atomic<bool> reader_started{false};
    std::atomic<bool> release_reader{false};
    std::atomic<bool> reader_ok{true};
    std::thread reader{
        [snapshot = old_metadata, &reader_started, &release_reader, &reader_ok] {
            if (!snapshot || snapshot->empty() || snapshot->back().name != "row 99999") {
                reader_ok.store(false, std::memory_order_relaxed);
            }
            reader_started.store(true, std::memory_order_release);
            while (!release_reader.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            if (!snapshot || snapshot->empty() || snapshot->back().name != "row 99999") {
                reader_ok.store(false, std::memory_order_relaxed);
            }
        }};

    while (!reader_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    items.back().name = "updated row";
    NUI_CHECK(state.replace(items));
    NUI_CHECK(state.dataset_generation() == generation + 1);
    const auto new_metadata = state.metadata_snapshot();
    NUI_CHECK(new_metadata);
    NUI_CHECK(new_metadata.get() != old_metadata.get());
    NUI_CHECK(new_metadata->size() == old_metadata->size());
    NUI_CHECK(old_metadata->back().name == "row 99999");
    NUI_CHECK(new_metadata->back().name == "updated row");
    NUI_CHECK(row_factory_calls == 0);

    release_reader.store(true, std::memory_order_release);
    reader.join();
    NUI_CHECK(reader_ok.load(std::memory_order_relaxed));

    retained_tree_semantics_flatten_wrappers_and_honor_availability();
    retained_tree_semantics_attach_virtual_collection_without_metadata_copy();
    live_semantic_actions_resolve_and_recheck_current_state();
}

} // namespace

int main() { return test::run("t067_semantic_api", &suite); }
