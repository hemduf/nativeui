    // Sparse retained metadata only. Rendered images, device signatures and
    // memory accounting belong to a renderer, never to these records.
    std::unordered_map<NodeId, detail::RasterCacheEpoch> raster_cache_epochs_;

    void invalidate_raster_cache_ancestry(const Node* node) noexcept {
        if (raster_cache_epochs_.empty()) return;
        for (auto* current = node; current; current = current->parent) {
            const auto found = raster_cache_epochs_.find(current->id);
            if (found != raster_cache_epochs_.end()) found->second.invalidate();
        }
    }

    [[nodiscard]] bool register_raster_cache_boundary(NodeId id) {
        if (layout_transaction_active_ || scene_frame_transaction_active_ ||
            (lifecycle_transition_ != LifecycleTransition::None &&
             lifecycle_transition_ != LifecycleTransition::Mount)) {
            return false;
        }
        const auto* node = retained_invalidation_node(id);
        if (!node) return false;
        const auto inserted = raster_cache_epochs_.try_emplace(id).second;
        if (inserted) invalidate_raster_cache_ancestry(node->parent);
        return true;
    }

    [[nodiscard]] bool raster_cache_snapshot_ready() const noexcept {
        return mounted_ && !layout_dirty_ && !layout_transaction_active_ &&
            dirty_dynamic_nodes_.empty() && !dynamic_reconcile_retry_all_ &&
            !availability_structure_dirty_ && !syncing_availability_ &&
            (!active_ || !focus_structure_dirty_) &&
            (lifecycle_transition_ == LifecycleTransition::None ||
             lifecycle_transition_ == LifecycleTransition::Paint);
    }

    [[nodiscard]] detail::RasterCacheEpoch::Token capture_raster_cache_content(
        NodeId id) noexcept {
        if (!raster_cache_snapshot_ready()) return {};
        const auto found = raster_cache_epochs_.find(id);
        return found == raster_cache_epochs_.end()
            ? detail::RasterCacheEpoch::Token{} : found->second.capture();
    }

    [[nodiscard]] bool reusable_raster_cache_content(
        NodeId id, const detail::RasterCacheEpoch::Token& token) const noexcept {
        if (!raster_cache_snapshot_ready()) return false;
        const auto found = raster_cache_epochs_.find(id);
        return found != raster_cache_epochs_.end() && found->second.reusable(token);
    }

    [[nodiscard]] bool commit_raster_cache_content(
        NodeId id, const detail::RasterCacheEpoch::Token& token) noexcept {
        if (!raster_cache_snapshot_ready()) return false;
        const auto found = raster_cache_epochs_.find(id);
        return found != raster_cache_epochs_.end() && found->second.commit(token);
    }

    [[nodiscard]] std::function<void()> raster_cache_invalidator(NodeId id) {
        const auto found = raster_cache_epochs_.find(id);
        if (found == raster_cache_epochs_.end()) return {};
        const auto weak_tree = retained_invalidation_weak();
        // Identity-only capture must not start a render transaction or disturb
        // generation coalescing just to create a long-lived callback.
        const auto identity = found->second.lifetime_token();
        return [weak_tree, id, identity] {
            const auto lifetime = weak_tree.lock();
            if (!lifetime || !lifetime->tree) return;
            auto* tree = lifetime->tree;
            const auto entry = tree->raster_cache_epochs_.find(id);
            if (entry == tree->raster_cache_epochs_.end() ||
                !entry->second.same_lifetime(identity)) return;
            auto* node = tree->retained_invalidation_node(id);
            if (!node) return;

            // Revoke the content before visual publication or a throwing redraw
            // callback. A boundary dependency affects its entire visual subtree.
            tree->invalidate_raster_cache_ancestry(node);
            tree->mark_paint_culling_dirty();
            if (tree->layout_transaction_active_ || !node->visual_bounds_published) {
                tree->invalidate_paint(tree->viewport_rect());
            } else {
                tree->publish_subtree_visual_bounds_then_invalidate(*node);
            }
        };
    }

    struct RasterCachePlacementDelta final {
        double x{};
        double y{};
    };

    [[nodiscard]] RasterCachePlacementDelta note_raster_cache_geometry(
        const Node& node, Rect next, RasterCachePlacementDelta parent_delta) noexcept {
        if (raster_cache_epochs_.empty()) return {};
        const Rect previous = node.bounds;
        const RasterCachePlacementDelta delta{
            static_cast<double>(next.x) - previous.x,
            static_cast<double>(next.y) - previous.y};
        if (previous.w != next.w || previous.h != next.h ||
            !std::isfinite(delta.x) || !std::isfinite(delta.y)) {
            invalidate_raster_cache_ancestry(&node);
        } else if (delta.x != parent_delta.x || delta.y != parent_delta.y) {
            // Moving this node changes containing rasters, but not its own local
            // pixels. A common subtree translation preserves inner generations.
            // Renderer-side device/transform signatures still govern actual reuse.
            invalidate_raster_cache_ancestry(node.parent);
        }
        return delta;
    }
