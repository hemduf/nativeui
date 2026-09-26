#pragma once

#include <nativeui/semantics.hpp>

#include <atomic>
#include <memory>
#include <new>
#include <unordered_map>
#include <vector>

namespace ui::detail {

namespace semantic_snapshot_detail {

[[nodiscard]] inline bool rect_equal(const Rect& lhs, const Rect& rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.w == rhs.w && lhs.h == rhs.h;
}

[[nodiscard]] inline bool value_fields_equal(const SemanticInfo& lhs,
                                             const SemanticInfo& rhs) {
    return lhs.role == rhs.role && lhs.name == rhs.name &&
           lhs.description == rhs.description && lhs.text_value == rhs.text_value &&
           lhs.numeric_value == rhs.numeric_value && lhs.value_range == rhs.value_range &&
           lhs.enabled == rhs.enabled && lhs.read_only == rhs.read_only &&
           lhs.checked == rhs.checked && lhs.expanded == rhs.expanded &&
           lhs.focusable == rhs.focusable && lhs.actions == rhs.actions;
}

[[nodiscard]] inline bool virtual_structure_equal(const VirtualSemanticChildren& lhs,
                                                   const VirtualSemanticChildren& rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    const auto& lhs_metadata = lhs.metadata_snapshot();
    const auto& rhs_metadata = rhs.metadata_snapshot();
    if (lhs_metadata.get() == rhs_metadata.get()) {
        return true;
    }
    if (!lhs_metadata || !rhs_metadata || lhs_metadata->size() != rhs_metadata->size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs_metadata->size(); ++index) {
        if ((*lhs_metadata)[index].token != (*rhs_metadata)[index].token) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool virtual_common_values_equal(const VirtualSemanticChildren& lhs,
                                                       const VirtualSemanticChildren& rhs) {
    const auto& lhs_metadata = lhs.metadata_snapshot();
    const auto& rhs_metadata = rhs.metadata_snapshot();
    if (lhs_metadata.get() == rhs_metadata.get()) {
        return true;
    }
    if (!lhs_metadata || !rhs_metadata) {
        return true;
    }

    std::unordered_map<VirtualSemanticItemToken, const VirtualSemanticItemMetadata*> lhs_by_token;
    lhs_by_token.reserve(lhs_metadata->size());
    for (const auto& item : *lhs_metadata) {
        lhs_by_token.emplace(item.token, &item);
    }

    for (const auto& item : *rhs_metadata) {
        const auto before = lhs_by_token.find(item.token);
        if (before != lhs_by_token.end() && *before->second != item) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool virtual_selection_equal(const VirtualSemanticChildren& lhs,
                                                   const VirtualSemanticChildren& rhs) noexcept {
    return lhs.selected_token() == rhs.selected_token();
}

[[nodiscard]] inline bool virtual_bounds_equal(const VirtualSemanticChildren& lhs,
                                                const VirtualSemanticChildren& rhs) noexcept {
    return rect_equal(lhs.list_bounds(), rhs.list_bounds()) &&
           lhs.row_height() == rhs.row_height() && lhs.scroll_y() == rhs.scroll_y();
}

[[nodiscard]] inline bool node_has_selection(const SemanticNodeSnapshot& node) noexcept {
    if (node.info.selected) {
        return true;
    }
    return node.virtual_children && node.virtual_children->selected_token().has_value();
}

// A virtual dataset replacement can intentionally carry identical exposed
// semantic values while still replacing the immutable T067 metadata object.
// Native readers must observe the new backing generation, but the semantic
// generation must not advance when the exposed accessibility data is unchanged.
[[nodiscard]] inline bool virtual_storage_refresh_required(
    const SemanticTreeSnapshot& before,
    const SemanticTreeSnapshot& after) noexcept {
    for (const auto& after_node : after.nodes) {
        const SemanticNodeSnapshot* before_node = nullptr;
        for (const auto& candidate : before.nodes) {
            if (candidate.id == after_node.id) {
                before_node = &candidate;
                break;
            }
        }

        if (before_node == nullptr ||
            before_node->virtual_children.has_value() != after_node.virtual_children.has_value()) {
            continue;
        }
        if (!before_node->virtual_children || !after_node.virtual_children) {
            continue;
        }

        const auto& before_virtual = *before_node->virtual_children;
        const auto& after_virtual = *after_node.virtual_children;
        if (before_virtual.dataset_generation() != after_virtual.dataset_generation() ||
            before_virtual.metadata_snapshot().get() != after_virtual.metadata_snapshot().get() ||
            before_virtual.token_index_snapshot().get() !=
                after_virtual.token_index_snapshot().get()) {
            return true;
        }
    }
    return false;
}

} // namespace semantic_snapshot_detail

[[nodiscard]] inline std::vector<SemanticChange> diff_semantic_snapshots(
    const SemanticTreeSnapshot& before,
    const SemanticTreeSnapshot& after) {
    bool structure_changed = before.root != after.root || before.nodes.size() != after.nodes.size();
    bool focus_changed = false;
    bool selection_changed = false;
    bool value_changed = false;
    bool bounds_changed = false;

    // Initial root publication is represented by StructureChanged only. State
    // categories describe transitions inside an already-live semantic root, so
    // native adapters do not emit duplicate focus/selection events while first
    // exposing a view. The same rule naturally avoids teardown notifications
    // when a complete root disappears rather than publishing a successor root.
    const bool compare_structural_state =
        before.root != kInvalidSemanticId && after.root != kInvalidSemanticId;

    std::unordered_map<SemanticId, const SemanticNodeSnapshot*> before_by_id;
    before_by_id.reserve(before.nodes.size());
    for (const auto& node : before.nodes) {
        before_by_id.emplace(node.id, &node);
    }

    std::unordered_map<SemanticId, const SemanticNodeSnapshot*> after_by_id;
    after_by_id.reserve(after.nodes.size());
    for (const auto& node : after.nodes) {
        after_by_id.emplace(node.id, &node);
    }

    if (before_by_id.size() != before.nodes.size() || after_by_id.size() != after.nodes.size()) {
        structure_changed = true;
    }

    for (const auto& after_node : after.nodes) {
        const auto before_it = before_by_id.find(after_node.id);
        if (before_it == before_by_id.end()) {
            structure_changed = true;
            // Structure publication alone is not enough when a new subtree in
            // an already-live root also changes focused/selected semantic state.
            // Native adapters need those categories from this exact generation
            // instead of inferring state through a later current/live lookup.
            if (compare_structural_state) {
                focus_changed = focus_changed || after_node.info.focused;
                selection_changed = selection_changed ||
                                    semantic_snapshot_detail::node_has_selection(after_node);
            }
            continue;
        }

        const auto& before_node = *before_it->second;
        if (before_node.parent != after_node.parent || before_node.children != after_node.children) {
            structure_changed = true;
        }

        if (before_node.virtual_children.has_value() != after_node.virtual_children.has_value()) {
            structure_changed = true;
        } else if (before_node.virtual_children && after_node.virtual_children) {
            structure_changed = structure_changed ||
                !semantic_snapshot_detail::virtual_structure_equal(
                    *before_node.virtual_children, *after_node.virtual_children);
            selection_changed = selection_changed ||
                !semantic_snapshot_detail::virtual_selection_equal(
                    *before_node.virtual_children, *after_node.virtual_children);
            value_changed = value_changed ||
                !semantic_snapshot_detail::virtual_common_values_equal(
                    *before_node.virtual_children, *after_node.virtual_children);
            bounds_changed = bounds_changed ||
                !semantic_snapshot_detail::virtual_bounds_equal(
                    *before_node.virtual_children, *after_node.virtual_children);
        }

        focus_changed = focus_changed || before_node.info.focused != after_node.info.focused;
        selection_changed =
            selection_changed || before_node.info.selected != after_node.info.selected;
        value_changed = value_changed ||
                        !semantic_snapshot_detail::value_fields_equal(before_node.info,
                                                                     after_node.info);
        bounds_changed = bounds_changed ||
                         !semantic_snapshot_detail::rect_equal(before_node.bounds,
                                                               after_node.bounds);
    }

    // Always scan removals, even when another structural difference has already
    // set structure_changed. A removed focused/selected subtree in a live root
    // still changes corresponding semantic state and must contribute to the same
    // coalesced notification batch.
    for (const auto& before_node : before.nodes) {
        if (!after_by_id.contains(before_node.id)) {
            structure_changed = true;
            if (compare_structural_state) {
                focus_changed = focus_changed || before_node.info.focused;
                selection_changed = selection_changed ||
                                    semantic_snapshot_detail::node_has_selection(before_node);
            }
        }
    }

    std::vector<SemanticChange> changes;
    changes.reserve(5);
    if (structure_changed) {
        changes.push_back(SemanticChange::StructureChanged);
    }
    if (focus_changed) {
        changes.push_back(SemanticChange::FocusChanged);
    }
    if (selection_changed) {
        changes.push_back(SemanticChange::SelectionChanged);
    }
    if (value_changed) {
        changes.push_back(SemanticChange::ValueChanged);
    }
    if (bounds_changed) {
        changes.push_back(SemanticChange::BoundsChanged);
    }
    return changes;
}

class SemanticSnapshotPublisher {
public:
    enum class FailurePointForTest {
        None,
        DiffPreparation,
        SnapshotAllocation,
    };

    SemanticSnapshotPublisher()
        : current_(std::make_shared<const SemanticTreeSnapshot>()) {}

    [[nodiscard]] std::shared_ptr<const SemanticTreeSnapshot> current() const noexcept {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        return current_.load(std::memory_order_acquire);
#else
        return std::atomic_load_explicit(&current_, std::memory_order_acquire);
#endif
    }

    /// Permanently retire this publisher without allocating. Concurrent native
    /// readers either retain the immutable generation loaded before this store
    /// or observe a null current generation afterwards. Publication is UI-thread
    /// owned; once retired it cannot be resurrected even if another internal
    /// object still holds a strong publisher reference.
    void shutdown() noexcept {
        retired_ = true;
        store(nullptr);
    }

    // Deterministic, per-publisher failure seam used by the T068 exception
    // regressions. Publication is UI-thread owned, so this test control remains
    // instance-local and never introduces mutable process/global test state.
    void fail_next_publish_at_for_test(FailurePointForTest point) noexcept {
        failure_point_for_test_ = point;
    }

    [[nodiscard]] std::vector<SemanticChange> publish(SemanticTreeSnapshot candidate) {
        if (retired_) return {};

        const auto previous = current();
        fail_if_requested_for_test(FailurePointForTest::DiffPreparation);
        auto changes = diff_semantic_snapshots(*previous, candidate);
        if (changes.empty()) {
            if (!semantic_snapshot_detail::virtual_storage_refresh_required(*previous, candidate)) {
                return changes;
            }

            // Replace only the immutable backing storage. The exposed semantic
            // data did not change, so keep the semantic generation stable while
            // allowing new readers to retain the current T067 dataset object.
            candidate.generation = previous->generation;
            fail_if_requested_for_test(FailurePointForTest::SnapshotAllocation);
            store(std::make_shared<const SemanticTreeSnapshot>(std::move(candidate)));
            return changes;
        }

        candidate.generation = previous->generation + 1;
        fail_if_requested_for_test(FailurePointForTest::SnapshotAllocation);
        store(std::make_shared<const SemanticTreeSnapshot>(std::move(candidate)));
        return changes;
    }

private:
    void fail_if_requested_for_test(FailurePointForTest point) {
        if (failure_point_for_test_ != point) {
            return;
        }
        failure_point_for_test_ = FailurePointForTest::None;
        throw std::bad_alloc{};
    }

    void store(std::shared_ptr<const SemanticTreeSnapshot> published) noexcept {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        current_.store(std::move(published), std::memory_order_release);
#else
        std::atomic_store_explicit(&current_, std::move(published), std::memory_order_release);
#endif
    }

    FailurePointForTest failure_point_for_test_{FailurePointForTest::None};
    bool retired_{};

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
    std::atomic<std::shared_ptr<const SemanticTreeSnapshot>> current_;
#else
    // Some supported libc++ versions still lack the C++20 atomic<shared_ptr<T>>
    // specialization. Keep the same immutable snapshot publication contract via
    // the standard shared_ptr atomic access functions on those toolchains.
    mutable std::shared_ptr<const SemanticTreeSnapshot> current_;
#endif
};

} // namespace ui::detail
