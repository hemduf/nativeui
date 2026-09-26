#include "test_support.hpp"

#include <nativeui/detail/virtual_list_retained.hpp>
#include <nativeui/detail/virtual_list_semantic_action.hpp>

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
}

} // namespace

int main() { return test::run("t068_virtual_list_action", &suite); }
