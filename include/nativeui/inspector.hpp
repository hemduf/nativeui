#pragma once

#include <nativeui/component_base.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace ui {

class UI;

namespace debug {

/// Immutable, fully owned diagnostic value for one retained node.
///
/// Geometry is expressed in root-logical pixels. The snapshot never exposes
/// Node*, Component*, callbacks, references, or other retained-runtime
/// ownership, so copying an InspectorNode cannot keep a live node alive.
struct InspectorNode {
    /// Retained NodeId captured for this node. Runtime snapshots never publish
    /// kInvalidNodeId; the value is diagnostic identity only, not a live handle.
    NodeId id{kInvalidNodeId};

    /// Parent NodeId in the same snapshot, or kInvalidNodeId for the root.
    NodeId parent_id{kInvalidNodeId};

    /// Owned UTF-8 diagnostic label. V1 currently reports a generic component
    /// label; consumers must not use this string as a stable type identifier.
    std::string debug_name{"Component"};

    /// Last published retained bounds in root-logical pixels. When layout is
    /// dirty these are the previous committed bounds, not speculative geometry.
    Rect bounds{};

    /// Effective inherited clip in root-logical pixels before this node applies
    /// its own child clip. The root starts with the current viewport rectangle.
    Rect clip_bounds{};

    /// True when this retained node still has pending layout work. Taking an
    /// inspector snapshot observes this bit and does not consume layout dirtiness.
    bool layout_dirty{};

    /// True when at least one current Tree dirty rectangle intersects bounds.
    /// This is a derived damage-intersection flag and does not imply that this
    /// node originally caused the invalidation.
    bool paint_dirty{};

    /// Current value of the component's focusable predicate.
    bool focusable{};

    /// True only for the retained node that owns active keyboard focus.
    bool focused{};

    /// True when this node owns an active pointer capture at snapshot time.
    bool pointer_capture_owner{};

    /// Effective inherited visibility/enabled/read-only state seen by the node.
    ComponentAvailability availability{};

    /// Zero-based sibling position in the retained child vector. The root uses 0.
    std::size_t child_order{};

    /// Retained hierarchy depth, with the root at depth 0.
    std::size_t depth{};
};

/// Owned point-in-time diagnostic copy of the retained tree and paint damage.
///
/// nodes is emitted in retained pre-order (parent before descendants), and
/// dirty_regions copies the Tree's current root-logical paint-damage rectangles.
/// The snapshot remains valid after the live Tree mutates or destroys nodes.
/// Mutating either vector can invalidate pointers previously returned by find().
struct InspectorSnapshot {
    /// Fully owned retained-node values in deterministic pre-order traversal.
    std::vector<InspectorNode> nodes;

    /// Fully owned root-logical dirty rectangles pending at snapshot time.
    /// Reading a snapshot does not clear or otherwise consume Tree damage.
    std::vector<Rect> dirty_regions;

    /// Find the first node whose captured NodeId equals id.
    ///
    /// The lookup is linear in nodes.size(). The returned pointer borrows from
    /// this snapshot and remains valid only until the nodes vector is mutated
    /// or the InspectorSnapshot is destroyed. Missing/stale IDs return nullptr.
    /// No live retained object is consulted and the lookup never throws.
    [[nodiscard]] const InspectorNode* find(NodeId id) const noexcept {
        for (const auto& node : nodes) {
            if (node.id == id) return &node;
        }
        return nullptr;
    }
};

#if defined(NATIVEUI_ENABLE_INSPECTOR)
/// Query whether the passive inspector overlay is enabled for ui.
///
/// This is an O(1), non-owning read. const UI& does not add synchronization:
/// call it on the same UI/main thread that owns the UI. It allocates nothing,
/// invokes no application callback and is not an audio-real-time API.
[[nodiscard]] bool inspector_enabled(const UI& ui) noexcept;

/// Enable or disable the passive per-UI inspector overlay.
///
/// Repeating the current value is a no-op. A real state change invalidates paint
/// for the UI but does not request layout. Invalidation follows normal Tree/UI
/// callback and reentrancy rules. The inspector remains diagnostic-only: it does
/// not enter hit testing, focus, retained z-order, or application overlay slots.
/// Call on the owning UI/main thread; do not call from an audio callback.
void set_inspector_enabled(UI& ui, bool enabled);

/// Return the NodeId selected for inspector highlighting, or kInvalidNodeId when
/// no diagnostic selection is active.
///
/// The ID is a copied value, not a retained-node borrow and not an ownership
/// token. The query is O(1), non-allocating and UI/main-thread confined.
[[nodiscard]] NodeId inspector_selected_node(const UI& ui) noexcept;

/// Select a NodeId for passive inspector highlighting.
///
/// Stale, missing and kInvalidNodeId values are accepted and remain safe
/// diagnostic values; selection never keeps a retained node alive. Repeating the
/// current ID is a no-op. A changed ID invalidates paint but not layout and obeys
/// the normal UI invalidation/reentrancy contract. UI/main-thread only; not RT.
void set_inspector_selected_node(UI& ui, NodeId id);

/// Copy the current retained hierarchy and paint-damage diagnostics.
///
/// Before traversal, queued dynamic mutations are flushed at the normal retained
/// checkpoint. If a lifecycle transition is already active, the function returns
/// an empty snapshot instead of observing a partially transitioned tree. The
/// query does not clear layout/paint dirtiness. It copies strings/vectors and may
/// allocate or propagate allocation/callback failures from that checkpoint.
///
/// The returned value owns all diagnostic data and may outlive later Tree
/// mutation or node destruction. Call on the owning UI/main thread; this is not
/// an audio-real-time operation.
[[nodiscard]] InspectorSnapshot inspector_snapshot(UI& ui);
#endif

} // namespace debug
} // namespace ui
