#include <nativeui/detail/semantic_action_target_binding.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

ui::SemanticInfo enabled_button_info() {
    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Button;
    info.name = "Action";
    info.enabled = true;
    info.actions = {ui::SemanticAction::Activate};
    return info;
}

void lifetime_bound_target_fails_closed_after_owner_death() {
    auto lifetime = std::make_shared<int>(0);
    std::weak_ptr<const void> weak_lifetime{lifetime};
    int current_calls = 0;
    int dispatch_calls = 0;

    ui::detail::LifetimeBoundSemanticActionTarget target{
        weak_lifetime,
        [&](const ui::detail::SemanticIdentity&) -> std::optional<ui::SemanticInfo> {
            ++current_calls;
            return enabled_button_info();
        },
        [&](const ui::detail::SemanticIdentity&,
            const ui::detail::SemanticActionRequest&) {
            ++dispatch_calls;
            return true;
        }};

    const ui::detail::SemanticIdentity identity{17, std::nullopt};
    ui::detail::SemanticActionRequest request;
    request.action = ui::SemanticAction::Activate;

    T068_CHECK(target.current_semantics(identity).has_value());
    T068_CHECK(target.dispatch_semantic_action(identity, request));
    T068_CHECK(current_calls == 1);
    T068_CHECK(dispatch_calls == 1);

    lifetime.reset();

    T068_CHECK(!target.current_semantics(identity).has_value());
    T068_CHECK(!target.dispatch_semantic_action(identity, request));
    T068_CHECK(current_calls == 1);
    T068_CHECK(dispatch_calls == 1);
}

void reentrant_owner_death_is_visible_to_nested_semantic_work() {
    auto lifetime = std::make_shared<int>(0);
    std::weak_ptr<const void> weak_lifetime{lifetime};
    const ui::detail::SemanticIdentity identity{23, std::nullopt};
    ui::detail::SemanticActionRequest request;
    request.action = ui::SemanticAction::Activate;

    ui::detail::LifetimeBoundSemanticActionTarget* target_ptr = nullptr;
    bool nested_rejected = false;

    ui::detail::LifetimeBoundSemanticActionTarget target{
        weak_lifetime,
        [](const ui::detail::SemanticIdentity&) -> std::optional<ui::SemanticInfo> {
            return enabled_button_info();
        },
        [&](const ui::detail::SemanticIdentity&,
            const ui::detail::SemanticActionRequest&) {
            lifetime.reset();
            nested_rejected = !target_ptr->current_semantics(identity).has_value();
            return true;
        }};
    target_ptr = &target;

    T068_CHECK(target.dispatch_semantic_action(identity, request));
    T068_CHECK(nested_rejected);
    T068_CHECK(!target.current_semantics(identity).has_value());
    T068_CHECK(!target.dispatch_semantic_action(identity, request));
}

void independent_targets_do_not_share_lifetime_state() {
    auto lifetime_a = std::make_shared<int>(0);
    auto lifetime_b = std::make_shared<int>(0);
    int dispatch_a = 0;
    int dispatch_b = 0;

    auto make_target = [](const std::shared_ptr<int>& lifetime, int& dispatch_count) {
        return std::make_unique<ui::detail::LifetimeBoundSemanticActionTarget>(
            std::weak_ptr<const void>{lifetime},
            [](const ui::detail::SemanticIdentity&) -> std::optional<ui::SemanticInfo> {
                return enabled_button_info();
            },
            [&dispatch_count](const ui::detail::SemanticIdentity&,
                              const ui::detail::SemanticActionRequest&) {
                ++dispatch_count;
                return true;
            });
    };

    auto target_a = make_target(lifetime_a, dispatch_a);
    auto target_b = make_target(lifetime_b, dispatch_b);
    const ui::detail::SemanticIdentity identity{31, std::nullopt};
    ui::detail::SemanticActionRequest request;
    request.action = ui::SemanticAction::Activate;

    lifetime_a.reset();
    T068_CHECK(!target_a->dispatch_semantic_action(identity, request));
    T068_CHECK(target_b->dispatch_semantic_action(identity, request));
    T068_CHECK(dispatch_a == 0);
    T068_CHECK(dispatch_b == 1);
}

void callback_exception_leaves_target_reusable() {
    auto lifetime = std::make_shared<int>(0);
    bool throw_next = true;
    int dispatch_calls = 0;

    ui::detail::LifetimeBoundSemanticActionTarget target{
        std::weak_ptr<const void>{lifetime},
        [](const ui::detail::SemanticIdentity&) -> std::optional<ui::SemanticInfo> {
            return enabled_button_info();
        },
        [&](const ui::detail::SemanticIdentity&,
            const ui::detail::SemanticActionRequest&) {
            ++dispatch_calls;
            if (throw_next) {
                throw_next = false;
                throw std::runtime_error("injected semantic action failure");
            }
            return true;
        }};

    const ui::detail::SemanticIdentity identity{47, std::nullopt};
    ui::detail::SemanticActionRequest request;
    request.action = ui::SemanticAction::Activate;

    bool threw = false;
    try {
        (void)target.dispatch_semantic_action(identity, request);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    T068_CHECK(threw);
    T068_CHECK(dispatch_calls == 1);
    T068_CHECK(target.dispatch_semantic_action(identity, request));
    T068_CHECK(dispatch_calls == 2);
}

void suite() {
    lifetime_bound_target_fails_closed_after_owner_death();
    reentrant_owner_death_is_visible_to_nested_semantic_work();
    independent_targets_do_not_share_lifetime_state();
    callback_exception_leaves_target_reusable();
}

} // namespace

int main() {
    try {
        suite();
        std::cout << "PASS t068 semantic action target binding\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic action target binding: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
