#include <nativeui/detail/semantic_action.hpp>
#include <nativeui/detail/semantic_rules.hpp>
#include <nativeui/detail/semantic_virtual_source.hpp>
#include <nativeui/semantics.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define ACCESSIBILITY_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

class VirtualActionProbe final : public ui::detail::VirtualSemanticActionHandler {
public:
    [[nodiscard]] bool perform_virtual_semantic_action(
        ui::VirtualSemanticItemToken token,
        const ui::detail::SemanticActionRequest& request) override {
        ++calls;
        last_token = token;
        last_action = request.action;
        return true;
    }

    int calls{};
    ui::VirtualSemanticItemToken last_token{ui::kInvalidVirtualSemanticItemToken};
    ui::SemanticAction last_action{ui::SemanticAction::Focus};
};

void read_only_mutation_policy_is_single_and_exhaustive() {
    using ui::SemanticAction;

    ACCESSIBILITY_CHECK(!ui::detail::semantic_action_mutates_value(SemanticAction::Activate));
    ACCESSIBILITY_CHECK(ui::detail::semantic_action_mutates_value(SemanticAction::Toggle));
    ACCESSIBILITY_CHECK(!ui::detail::semantic_action_mutates_value(SemanticAction::Focus));
    ACCESSIBILITY_CHECK(ui::detail::semantic_action_mutates_value(SemanticAction::Increment));
    ACCESSIBILITY_CHECK(ui::detail::semantic_action_mutates_value(SemanticAction::Decrement));
    ACCESSIBILITY_CHECK(ui::detail::semantic_action_mutates_value(SemanticAction::SetValue));
    ACCESSIBILITY_CHECK(ui::detail::semantic_action_mutates_value(SemanticAction::Select));
    ACCESSIBILITY_CHECK(!ui::detail::semantic_action_mutates_value(SemanticAction::Expand));
    ACCESSIBILITY_CHECK(!ui::detail::semantic_action_mutates_value(SemanticAction::Collapse));

    ui::SemanticInfo info;
    info.enabled = true;
    info.read_only = true;
    info.focusable = true;
    info.actions = {
        SemanticAction::Activate,
        SemanticAction::Toggle,
        SemanticAction::Focus,
        SemanticAction::Increment,
        SemanticAction::Decrement,
        SemanticAction::SetValue,
        SemanticAction::Select,
        SemanticAction::Expand,
        SemanticAction::Collapse,
    };

    const auto normalized = ui::detail::normalize_semantic_info(
        info, true, true, false);
    const std::vector<SemanticAction> expected{
        SemanticAction::Activate,
        SemanticAction::Focus,
        SemanticAction::Expand,
        SemanticAction::Collapse,
    };
    ACCESSIBILITY_CHECK(normalized.actions == expected);

    for (const auto action : info.actions) {
        ACCESSIBILITY_CHECK(ui::detail::semantic_action_allowed(info, action) ==
                   !ui::detail::semantic_action_mutates_value(action));
    }
}

void disabled_state_rejects_every_advertised_action() {
    ui::SemanticInfo info;
    info.enabled = false;
    info.actions = {
        ui::SemanticAction::Activate,
        ui::SemanticAction::Focus,
        ui::SemanticAction::SetValue,
    };

    for (const auto action : info.actions) {
        ACCESSIBILITY_CHECK(!ui::detail::semantic_action_allowed(info, action));
    }
}

void virtual_action_handler_uses_stable_logical_token() {
    VirtualActionProbe probe;
    ui::detail::VirtualSemanticActionHandler* handler = &probe;
    ui::detail::SemanticActionRequest request;
    request.action = ui::SemanticAction::Select;

    constexpr ui::VirtualSemanticItemToken token = 73;
    ACCESSIBILITY_CHECK(handler->perform_virtual_semantic_action(token, request));
    ACCESSIBILITY_CHECK(probe.calls == 1);
    ACCESSIBILITY_CHECK(probe.last_token == token);
    ACCESSIBILITY_CHECK(probe.last_action == ui::SemanticAction::Select);
}

void virtual_live_resolution_requires_indexed_token_map() {
    using Children = ui::VirtualSemanticChildren;
    constexpr ui::VirtualSemanticItemToken token = 91;

    auto metadata = std::make_shared<Children::Metadata>();
    metadata->push_back(ui::VirtualSemanticItemMetadata{
        token,
        "row",
        "",
        true,
        false,
        ui::SemanticCheckedState::NotApplicable,
        {ui::SemanticAction::Select},
    });

    const auto unindexed = Children::from_metadata(
        1,
        metadata,
        std::nullopt,
        ui::Rect{0.0f, 0.0f, 100.0f, 20.0f},
        20.0f,
        0.0f);
    ACCESSIBILITY_CHECK(unindexed.item_for_token(token).has_value());
    ACCESSIBILITY_CHECK(!ui::detail::resolve_indexed_virtual_semantic_item(unindexed, token));

    auto token_index = std::make_shared<Children::TokenIndex>();
    token_index->emplace(token, 0);
    const auto indexed = Children::from_indexed_metadata(
        1,
        metadata,
        token_index,
        std::nullopt,
        ui::Rect{0.0f, 0.0f, 100.0f, 20.0f},
        20.0f,
        0.0f);

    const auto resolved = ui::detail::resolve_indexed_virtual_semantic_item(indexed, token);
    ACCESSIBILITY_CHECK(resolved.has_value());
    ACCESSIBILITY_CHECK(resolved->token == token);
    ACCESSIBILITY_CHECK(resolved->info.name == "row");
    ACCESSIBILITY_CHECK(resolved->info.supports(ui::SemanticAction::Select));

    auto malformed_index = std::make_shared<Children::TokenIndex>();
    malformed_index->emplace(token, 1);
    const auto malformed = Children::from_indexed_metadata(
        1,
        metadata,
        malformed_index,
        std::nullopt,
        ui::Rect{0.0f, 0.0f, 100.0f, 20.0f},
        20.0f,
        0.0f);
    ACCESSIBILITY_CHECK(!ui::detail::resolve_indexed_virtual_semantic_item(malformed, token));
    ACCESSIBILITY_CHECK(!ui::detail::resolve_indexed_virtual_semantic_item(
        indexed, ui::kInvalidVirtualSemanticItemToken));
}

} // namespace

int main() {
    try {
        read_only_mutation_policy_is_single_and_exhaustive();
        disabled_state_rejects_every_advertised_action();
        virtual_action_handler_uses_stable_logical_token();
        virtual_live_resolution_requires_indexed_token_map();
        std::cout << "PASS accessibility semantic action policy\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL accessibility semantic action policy: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
