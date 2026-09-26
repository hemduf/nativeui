#include "test_support.hpp"

#include <nativeui/detail/semantic_tree_action_access.hpp>
#include <nativeui/detail/virtual_list_retained.hpp>
#include <nativeui/detail/virtual_list_semantic_action.hpp>
#include <nativeui/widgets.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

class SemanticActionRow final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 20.0f};
    }

    void paint(ui::PaintContext&) const override {}
};

void suite() {
    using Runtime = ui::detail::VirtualListRetainedRuntime<int>;

    const auto request = [](ui::SemanticAction action) {
        return ui::detail::SemanticActionRequest{action, std::nullopt, std::nullopt};
    };

    std::size_t row_factory_calls = 0;
    auto runtime = std::make_shared<Runtime>(
        20.0f,
        [&row_factory_calls](const Runtime::Item&) {
            ++row_factory_calls;
            return ui::Spec{
                [] { return std::make_unique<SemanticActionRow>(); },
                {}};
        });

    ui::State<std::optional<int>> selection{std::nullopt};
    runtime->bind_selection(selection);

    std::size_t activation_calls = 0;
    std::optional<int> activated_key;
    runtime->set_activation_callback([&](const int& key) {
        ++activation_calls;
        activated_key = key;
    });

    auto selectable = [](int key, std::string name) {
        return Runtime::Item{
            key,
            std::move(name),
            true,
            {},
            false,
            ui::SemanticCheckedState::NotApplicable,
            {ui::SemanticAction::Select}};
    };

    NUI_CHECK(runtime->replace({
        selectable(10, "ten"),
        selectable(20, "twenty"),
        selectable(30, "thirty"),
    }));

    const auto initial = runtime->semantic_children({0.0f, 0.0f, 100.0f, 60.0f});
    const auto twenty = initial.item_at(1);
    NUI_CHECK(twenty.has_value());
    const auto twenty_token = twenty->token;
    NUI_CHECK(twenty_token != ui::kInvalidVirtualSemanticItemToken);
    NUI_CHECK(row_factory_calls == 0);

    // Stable semantic identity follows the logical key through reorder and is
    // resolved against the current token index immediately before selection.
    NUI_CHECK(runtime->replace({
        selectable(20, "twenty"),
        selectable(30, "thirty"),
        selectable(10, "ten"),
    }));
    NUI_CHECK(ui::detail::dispatch_virtual_list_semantic_action(
        *runtime,
        twenty_token,
        request(ui::SemanticAction::Select)));
    NUI_CHECK(selection.get().has_value());
    NUI_CHECK(*selection.get() == 20);
    NUI_CHECK(row_factory_calls == 0);

    // A removed logical item must not redirect through its old index to the
    // replacement row. Semantic lookup remains data-only and fail-closed.
    NUI_CHECK(runtime->replace({
        selectable(40, "forty"),
        selectable(30, "thirty"),
        selectable(10, "ten"),
    }));
    NUI_CHECK(!ui::detail::dispatch_virtual_list_semantic_action(
        *runtime,
        twenty_token,
        request(ui::SemanticAction::Select)));
    NUI_CHECK(*selection.get() == 20);
    NUI_CHECK(row_factory_calls == 0);

    const auto current = runtime->semantic_children({0.0f, 0.0f, 100.0f, 60.0f});
    const auto forty = current.item_at(0);
    NUI_CHECK(forty.has_value());

    // Focus still needs the owning tree's focus manager and unsupported or
    // unadvertised actions remain fail-closed rather than being synthesized
    // through a materialized visual row.
    NUI_CHECK(!ui::detail::dispatch_virtual_list_semantic_action(
        *runtime,
        forty->token,
        request(ui::SemanticAction::Focus)));
    NUI_CHECK(!ui::detail::dispatch_virtual_list_semantic_action(
        *runtime,
        forty->token,
        request(ui::SemanticAction::Activate)));
    NUI_CHECK(activation_calls == 0);
    NUI_CHECK(!ui::detail::dispatch_virtual_list_semantic_action(
        *runtime,
        ui::kInvalidVirtualSemanticItemToken,
        request(ui::SemanticAction::Select)));
    NUI_CHECK(row_factory_calls == 0);

    // Current item policy is rechecked at action time. Disabled and read-only
    // Select actions cannot mutate the bound selection.
    NUI_CHECK(runtime->replace({
        Runtime::Item{
            50,
            "disabled",
            false,
            {},
            false,
            ui::SemanticCheckedState::NotApplicable,
            {ui::SemanticAction::Select}},
        Runtime::Item{
            60,
            "read only",
            true,
            {},
            true,
            ui::SemanticCheckedState::NotApplicable,
            {ui::SemanticAction::Select}},
    }));
    const auto unavailable = runtime->semantic_children({0.0f, 0.0f, 100.0f, 40.0f});
    const auto disabled = unavailable.item_at(0);
    const auto read_only = unavailable.item_at(1);
    NUI_CHECK(disabled.has_value());
    NUI_CHECK(read_only.has_value());
    NUI_CHECK(!ui::detail::dispatch_virtual_list_semantic_action(
        *runtime,
        disabled->token,
        request(ui::SemanticAction::Select)));
    NUI_CHECK(!ui::detail::dispatch_virtual_list_semantic_action(
        *runtime,
        read_only->token,
        request(ui::SemanticAction::Select)));
    NUI_CHECK(*selection.get() == 20);
    NUI_CHECK(row_factory_calls == 0);

    // Activation uses the same stable logical token and the retained runtime's
    // normal activate() path. Metadata/token lookup itself stays data-only and
    // does not materialize a visual row; one accepted request invokes the
    // activation callback exactly once.
    NUI_CHECK(runtime->replace({
        Runtime::Item{
            70,
            "activate",
            true,
            {},
            false,
            ui::SemanticCheckedState::NotApplicable,
            {ui::SemanticAction::Select, ui::SemanticAction::Activate}},
        selectable(80, "select only"),
    }));
    const auto actionable = runtime->semantic_children({0.0f, 0.0f, 100.0f, 40.0f});
    const auto activate_item = actionable.item_at(0);
    NUI_CHECK(activate_item.has_value());
    const auto activate_index = ui::detail::virtual_list_semantic_index_for_token(
        actionable,
        activate_item->token);
    NUI_CHECK(activate_index.has_value());
    NUI_CHECK(*activate_index == 0);
    NUI_CHECK(row_factory_calls == 0);

    NUI_CHECK(ui::detail::dispatch_virtual_list_semantic_action(
        *runtime,
        activate_item->token,
        request(ui::SemanticAction::Activate)));
    NUI_CHECK(activation_calls == 1);
    NUI_CHECK(activated_key.has_value());
    NUI_CHECK(*activated_key == 70);
    NUI_CHECK(selection.get().has_value());
    NUI_CHECK(*selection.get() == 70);

    // Logical Focus: the token is re-resolved against the current indexed
    // metadata, the owning tree focus manager must accept the composite
    // ListView owner first, and only then does normal scroll/materialization
    // run. Focus never mutates selection and a denied or stale request fails
    // closed before touching the visual window.
    {
        auto focus_runtime = std::make_shared<Runtime>(
            20.0f,
            [&row_factory_calls](const Runtime::Item&) {
                ++row_factory_calls;
                return ui::Spec{
                    [] { return std::make_unique<SemanticActionRow>(); },
                    {}};
            });
        ui::State<std::optional<int>> focus_selection{std::nullopt};
        focus_runtime->bind_selection(focus_selection);

        const auto focusable_item = [](int key, std::string name) {
            Runtime::Item item{key, std::move(name), true};
            item.actions = {ui::SemanticAction::Select, ui::SemanticAction::Focus};
            return item;
        };

        NUI_CHECK(focus_runtime->replace({
            focusable_item(1, "one"),
            focusable_item(2, "two"),
            focusable_item(3, "three"),
        }));

        const auto generation = focus_runtime->dataset_generation();
        const auto metadata = focus_runtime->metadata_snapshot();
        const auto initial_calls = row_factory_calls;
        const auto window = focus_runtime->semantic_children({0.0f, 0.0f, 100.0f, 40.0f});
        const auto third = window.item_at(2);
        NUI_CHECK(third.has_value());

        std::size_t focus_requests = 0;
        bool owner_accepts = false;
        const auto request_owner_focus = [&]() {
            ++focus_requests;
            return owner_accepts;
        };

        NUI_CHECK(!ui::detail::dispatch_virtual_list_semantic_action(
            *focus_runtime,
            third->token,
            request(ui::SemanticAction::Focus),
            request_owner_focus));
        NUI_CHECK(focus_requests == 1);
        NUI_CHECK(!focus_runtime->focused_index().has_value());
        NUI_CHECK(focus_runtime->scroll().offset().y == 0.0f);
        NUI_CHECK(row_factory_calls == initial_calls);
        NUI_CHECK(!focus_selection.get().has_value());

        owner_accepts = true;
        NUI_CHECK(ui::detail::dispatch_virtual_list_semantic_action(
            *focus_runtime,
            third->token,
            request(ui::SemanticAction::Focus),
            request_owner_focus));
        NUI_CHECK(focus_requests == 2);
        NUI_CHECK(focus_runtime->focused_index() ==
                  std::optional<std::size_t>{2});
        NUI_CHECK(focus_runtime->scroll().offset().y > 0.0f);
        NUI_CHECK(row_factory_calls > initial_calls);
        NUI_CHECK(row_factory_calls - initial_calls <= 8);
        NUI_CHECK(focus_runtime->metadata_snapshot().get() == metadata.get());
        NUI_CHECK(focus_runtime->dataset_generation() == generation);
        NUI_CHECK(!focus_selection.get().has_value());

        // Removing the logical key makes its token stale: Focus fails closed
        // without reaching the owner or materializing a replacement row.
        Runtime::Item select_only_item{5, "select only", true};
        select_only_item.actions = {ui::SemanticAction::Select};
        NUI_CHECK(focus_runtime->replace({
            select_only_item,
            focusable_item(4, "four"),
        }));
        const auto stale_requests = focus_requests;
        const auto stale_calls = row_factory_calls;
        NUI_CHECK(!ui::detail::dispatch_virtual_list_semantic_action(
            *focus_runtime,
            third->token,
            request(ui::SemanticAction::Focus),
            request_owner_focus));
        NUI_CHECK(focus_requests == stale_requests);
        NUI_CHECK(row_factory_calls == stale_calls);
        // An unadvertised Focus stays fail-closed even with an accepting owner.
        const auto select_only = focus_runtime->semantic_children(
            {0.0f, 0.0f, 100.0f, 40.0f}).item_at(0);
        NUI_CHECK(select_only.has_value());
        NUI_CHECK(!ui::detail::dispatch_virtual_list_semantic_action(
            *focus_runtime,
            select_only->token,
            request(ui::SemanticAction::Focus),
            request_owner_focus));
        NUI_CHECK(focus_requests == stale_requests);

        // Current item policy is rechecked at action time: a disabled item
        // rejects Focus before the owner callback, while read-only items keep
        // the non-mutating Focus path available.
        Runtime::Item disabled_item{6, "disabled", false};
        disabled_item.actions = {ui::SemanticAction::Select, ui::SemanticAction::Focus};
        Runtime::Item read_only_item{7, "read only", true};
        read_only_item.read_only = true;
        read_only_item.actions = {ui::SemanticAction::Select, ui::SemanticAction::Focus};
        NUI_CHECK(focus_runtime->replace({disabled_item, read_only_item}));
        const auto policy = focus_runtime->semantic_children(
            {0.0f, 0.0f, 100.0f, 40.0f});
        const auto disabled_entry = policy.item_at(0);
        const auto read_only_entry = policy.item_at(1);
        NUI_CHECK(disabled_entry.has_value());
        NUI_CHECK(read_only_entry.has_value());

        const auto policy_requests = focus_requests;
        NUI_CHECK(!ui::detail::dispatch_virtual_list_semantic_action(
            *focus_runtime,
            disabled_entry->token,
            request(ui::SemanticAction::Focus),
            request_owner_focus));
        NUI_CHECK(focus_requests == policy_requests);

        NUI_CHECK(ui::detail::dispatch_virtual_list_semantic_action(
            *focus_runtime,
            read_only_entry->token,
            request(ui::SemanticAction::Focus),
            request_owner_focus));
        NUI_CHECK(focus_requests == policy_requests + 1);
        NUI_CHECK(!focus_selection.get().has_value());
    }
}

[[nodiscard]] const ui::SemanticNodeSnapshot* find_semantic_node(
    const ui::SemanticTreeSnapshot& snapshot,
    ui::SemanticId id) {
    for (const auto& node : snapshot.nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

/// Production-path virtual Focus: one retained ListView under a real Tree with
/// the generic Focus fallback disabled by an unrelated focusable sibling. The
/// semantic action must focus the composite ListView, reveal/materialize the
/// offscreen logical row through normal T067 state and retain the exact shared
/// metadata generation.
void retained_virtual_focus_contract() {
    using VirtualState = ui::VirtualListState<int>;

    test::MockPlatform platform;
    ui::State<std::optional<int>> selection{std::nullopt};
    std::size_t row_factory_calls = 0;
    VirtualState state{
        selection,
        20.0f,
        [&row_factory_calls](const VirtualState::Item&) {
            ++row_factory_calls;
            return ui::Spacer{280.0f, 20.0f};
        }};

    std::vector<VirtualState::Item> items;
    items.reserve(5000);
    for (int index = 0; index < 5000; ++index) {
        VirtualState::Item item{index, "Row " + std::to_string(index)};
        item.actions = {ui::SemanticAction::Select, ui::SemanticAction::Focus};
        items.push_back(std::move(item));
    }
    NUI_CHECK(state.replace(std::move(items)));

    auto root = ui::compile(ui::make_spec(ui::Column{
        ui::Button{"Outside", [] {}},
        ui::Flex{ui::ListView<int>{state}}.grow(1.0f).shrink(1.0f)}));
    NUI_CHECK(root->children.size() == 2);
    NUI_CHECK(root->children[1]->children.size() == 1);
    const auto list_id = static_cast<ui::SemanticId>(root->children[1]->children.front()->id);

    ui::Tree tree{std::move(root)};
    tree.mount();
    tree.layout({280.0f, 120.0f});
    tree.activate_focus(platform);

    const auto initial = ui::detail::SemanticTreeActionAccess::build_snapshot(tree);
    NUI_CHECK(initial.has_value());
    const auto* list_initial = find_semantic_node(*initial, list_id);
    NUI_CHECK(list_initial != nullptr);
    NUI_CHECK(list_initial->info.role == ui::SemanticRole::ListView);
    NUI_CHECK(list_initial->info.focusable);
    NUI_CHECK(!list_initial->info.focused);
    NUI_CHECK(list_initial->virtual_children.has_value());
    NUI_CHECK(list_initial->virtual_children->size() == 5000);

    const auto metadata = state.metadata_snapshot();
    const auto generation = state.dataset_generation();
    NUI_CHECK(list_initial->virtual_children->metadata_snapshot().get() ==
              metadata.get());

    const ui::Rect list_bounds{0.0f, 0.0f, 280.0f, 120.0f};
    const auto target = state.semantic_children(list_bounds).item_at(4999);
    NUI_CHECK(target.has_value());

    const auto calls_before = row_factory_calls;
    ui::detail::SemanticActionRequest focus;
    focus.action = ui::SemanticAction::Focus;
    NUI_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{list_id, target->token}, focus));

    // Normal T067 materialization pulled the offscreen row (and only a
    // viewport/overscan-bounded window) into the visual tree.
    NUI_CHECK(row_factory_calls > calls_before);
    NUI_CHECK(row_factory_calls - calls_before <= 40);
    NUI_CHECK(state.offset().y > 0.0f);
    NUI_CHECK(!selection.get().has_value());

    const auto after = state.semantic_children(list_bounds);
    NUI_CHECK(after.dataset_generation() == generation);
    NUI_CHECK(after.metadata_snapshot().get() == metadata.get());
    NUI_CHECK(after.size() == 5000);

    const auto focused = ui::detail::SemanticTreeActionAccess::build_snapshot(tree);
    NUI_CHECK(focused.has_value());
    const auto* list_focused = find_semantic_node(*focused, list_id);
    NUI_CHECK(list_focused != nullptr);
    NUI_CHECK(list_focused->info.focused);
    NUI_CHECK(list_focused->virtual_children.has_value());
    NUI_CHECK(list_focused->virtual_children->metadata_snapshot().get() ==
              metadata.get());
    const auto offscreen = list_focused->virtual_children->item_at(4999);
    NUI_CHECK(offscreen.has_value());
    NUI_CHECK(offscreen->info.name == "Row 4999");
    NUI_CHECK(offscreen->info.actions == std::vector<ui::SemanticAction>({
        ui::SemanticAction::Select, ui::SemanticAction::Focus}));

    // A removed logical key makes its token stale: the live action is rejected
    // and no replacement row is materialized.
    std::vector<VirtualState::Item> replaced;
    replaced.reserve(4999);
    for (int index = 0; index < 4999; ++index) {
        VirtualState::Item item{index, "Row " + std::to_string(index)};
        item.actions = {ui::SemanticAction::Select, ui::SemanticAction::Focus};
        replaced.push_back(std::move(item));
    }
    NUI_CHECK(state.replace(std::move(replaced)));
    NUI_CHECK(state.dataset_generation() == generation + 1);
    const auto stale_calls = row_factory_calls;
    NUI_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{list_id, target->token}, focus));
    NUI_CHECK(row_factory_calls == stale_calls);
}

} // namespace

int main() {
    return test::run("t068_virtual_list_action", [] {
        suite();
        retained_virtual_focus_contract();
    });
}
