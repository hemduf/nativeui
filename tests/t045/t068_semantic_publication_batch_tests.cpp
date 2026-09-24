#include <nativeui/detail/semantic_view_state.hpp>
#include <nativeui/semantics.hpp>

#include <memory>
#include <new>
#include <optional>
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

ui::SemanticTreeSnapshot snapshot(double value) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 17;

    ui::SemanticNodeSnapshot node;
    node.id = 17;
    node.info.role = ui::SemanticRole::Slider;
    node.info.name = "Gain";
    node.info.numeric_value = value;
    node.info.value_range = ui::SemanticValueRange{0.0, 10.0, 1.0};
    node.info.enabled = true;
    node.info.actions = {ui::SemanticAction::Increment};
    tree.nodes.push_back(std::move(node));
    return tree;
}

void publication_batch_retains_exact_immutable_generation() {
    ui::detail::SemanticViewState view;

    view.stage(snapshot(0.0));
    auto first = view.checkpoint_publication();
    T068_CHECK(first.has_value());
    T068_CHECK(first->snapshot);
    T068_CHECK(first->generation() == 1);
    T068_CHECK(first->changes ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});
    T068_CHECK(first->snapshot.get() == view.current().get());
    T068_CHECK(first->snapshot->nodes[0].info.numeric_value ==
               std::optional<double>{0.0});

    const auto retained_first = first->snapshot;

    view.stage(snapshot(1.0));
    auto second = view.checkpoint_publication();
    T068_CHECK(second.has_value());
    T068_CHECK(second->snapshot);
    T068_CHECK(second->generation() == 2);
    T068_CHECK(second->changes ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::ValueChanged});
    T068_CHECK(second->snapshot.get() == view.current().get());
    T068_CHECK(second->snapshot->nodes[0].info.numeric_value ==
               std::optional<double>{1.0});

    // A native callback that retained the previous batch keeps reading exactly
    // that immutable generation after the UI thread publishes a replacement.
    T068_CHECK(retained_first->generation == 1);
    T068_CHECK(retained_first->nodes[0].info.numeric_value ==
               std::optional<double>{0.0});

    // Consuming a staged candidate is distinct from having no checkpoint work.
    // An identical candidate therefore returns the current exact generation with
    // an empty notification vector, while a subsequent unstaged call is nullopt.
    view.stage(snapshot(1.0));
    auto unchanged = view.checkpoint_publication();
    T068_CHECK(unchanged.has_value());
    T068_CHECK(unchanged->changes.empty());
    T068_CHECK(unchanged->generation() == 2);
    T068_CHECK(unchanged->snapshot.get() == second->snapshot.get());
    T068_CHECK(!view.checkpoint_publication().has_value());
}

void failed_publication_keeps_pending_candidate_for_exact_retry_batch() {
    ui::detail::SemanticViewState view;
    auto baseline = view.publish_publication(snapshot(0.0));
    T068_CHECK(baseline.has_value());
    T068_CHECK(baseline->generation() == 1);

    view.stage(snapshot(2.0));
    const auto before_failure = view.current();
    auto publisher = view.publisher();
    T068_CHECK(publisher);
    publisher->fail_next_publish_at_for_test(
        ui::detail::SemanticSnapshotPublisher::FailurePointForTest::SnapshotAllocation);

    bool threw = false;
    try {
        (void)view.checkpoint_publication();
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    T068_CHECK(threw);
    T068_CHECK(view.has_pending_publication());
    T068_CHECK(view.current().get() == before_failure.get());
    T068_CHECK(view.current()->generation == 1);

    auto retry = view.checkpoint_publication();
    T068_CHECK(retry.has_value());
    T068_CHECK(retry->generation() == 2);
    T068_CHECK(retry->changes ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::ValueChanged});
    T068_CHECK(retry->snapshot.get() == view.current().get());
    T068_CHECK(retry->snapshot->nodes[0].info.numeric_value ==
               std::optional<double>{2.0});
    T068_CHECK(!view.has_pending_publication());
}

} // namespace

int main() {
    try {
        publication_batch_retains_exact_immutable_generation();
        failed_publication_keeps_pending_candidate_for_exact_retry_batch();
        return 0;
    } catch (const std::exception&) {
        return 1;
    }
}
