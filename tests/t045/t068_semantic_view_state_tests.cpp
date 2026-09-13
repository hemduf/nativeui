#include <nativeui/detail/semantic_proxy.hpp>
#include <nativeui/detail/semantic_view_state.hpp>
#include <nativeui/semantics.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
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

ui::SemanticTreeSnapshot snapshot(ui::SemanticId id, std::string name) {
    ui::SemanticTreeSnapshot tree;
    tree.root = id;

    ui::SemanticNodeSnapshot node;
    node.id = id;
    node.info.role = ui::SemanticRole::Button;
    node.info.name = std::move(name);
    node.info.enabled = true;
    node.info.focusable = true;
    node.info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};
    tree.nodes.push_back(std::move(node));
    return tree;
}

void independent_view_state_destroy_a_keeps_b_alive() {
    auto first = std::make_unique<ui::detail::SemanticViewState>();
    auto second = std::make_unique<ui::detail::SemanticViewState>();

    T068_CHECK(first->publish(snapshot(11, "First")) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});
    T068_CHECK(second->publish(snapshot(22, "Second")) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});

    const auto first_proxy = ui::detail::SemanticSnapshotProxy::ordinary(
        first->publisher(), 11);
    const auto second_proxy = ui::detail::SemanticSnapshotProxy::ordinary(
        second->publisher(), 22);

    T068_CHECK(first_proxy.read().has_value());
    T068_CHECK(first_proxy.read()->info().name == "First");
    T068_CHECK(second_proxy.read().has_value());
    T068_CHECK(second_proxy.read()->info().name == "Second");

    first.reset();
    T068_CHECK(!first_proxy.read().has_value());
    T068_CHECK(second_proxy.read().has_value());
    T068_CHECK(second_proxy.read()->info().name == "Second");

    T068_CHECK(second->publish(snapshot(22, "Second updated")) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::ValueChanged});
    T068_CHECK(second_proxy.read().has_value());
    T068_CHECK(second_proxy.read()->generation() == 2);
    T068_CHECK(second_proxy.read()->info().name == "Second updated");
}

void no_change_publish_preserves_generation_and_snapshot_identity() {
    ui::detail::SemanticViewState view;
    T068_CHECK(!view.publish(snapshot(7, "Stable")).empty());

    const auto first = view.current();
    T068_CHECK(first);
    T068_CHECK(first->generation == 1);

    T068_CHECK(view.publish(snapshot(7, "Stable")).empty());
    const auto second = view.current();
    T068_CHECK(second.get() == first.get());
    T068_CHECK(second->generation == 1);
}

} // namespace

int main() {
    try {
        independent_view_state_destroy_a_keeps_b_alive();
        no_change_publish_preserves_generation_and_snapshot_identity();
        std::cout << "PASS t068 semantic view state\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic view state: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
