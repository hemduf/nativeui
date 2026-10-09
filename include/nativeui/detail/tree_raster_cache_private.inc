    struct RetainedRasterCacheBoundary final {
        detail::RasterCacheEpoch epoch;
        std::shared_ptr<const detail::CachedLayerSource> source;
        std::unique_ptr<RetainedRasterCacheBoundary> inner;
        std::vector<std::uint64_t> revisions;
        std::vector<std::unique_ptr<detail::CachedLayerDependency::Subscription>> subscriptions;

        ~RetainedRasterCacheBoundary() noexcept { disconnect(); }

        void disconnect() noexcept {
            subscriptions.clear();
            if (inner) inner->disconnect();
        }

        [[nodiscard]] bool dependencies_current() const noexcept {
            if (!source) return true;
            for (std::size_t i = 0; i < source->dependencies.size(); ++i) {
                if (source->dependencies[i].revision() != revisions[i]) return false;
            }
            return true;
        }

        void acknowledge_dependencies() noexcept {
            if (!source) return;
            for (std::size_t i = 0; i < source->dependencies.size(); ++i)
                revisions[i] = source->dependencies[i].revision();
        }
    };

    // Sparse retained metadata only. Rendered images, device signatures and
    // memory accounting belong to a renderer, never to these records.
    std::unordered_map<NodeId, std::unique_ptr<RetainedRasterCacheBoundary>> raster_cache_epochs_;

    // Per-Tree deterministic setup seam. The disabled production path performs
    // no fault allocation or user callback and never changes the source values.
    std::optional<std::size_t> cached_layer_setup_failure_after_;
    std::size_t cached_layer_setup_checkpoints_{};

    void cached_layer_setup_checkpoint() {
        ++cached_layer_setup_checkpoints_;
        if (!cached_layer_setup_failure_after_) return;
        if (*cached_layer_setup_failure_after_ == 0) {
            cached_layer_setup_failure_after_.reset();
            throw std::bad_alloc{};
        }
        --*cached_layer_setup_failure_after_;
    }

    static void invalidate_raster_cache_layers(RetainedRasterCacheBoundary* layer) noexcept {
        for (; layer; layer = layer->inner.get()) layer->epoch.invalidate();
    }

    void invalidate_raster_cache_ancestry(const Node* node) noexcept {
        if (raster_cache_epochs_.empty()) return;
        for (auto* current = node; current; current = current->parent) {
            const auto found = raster_cache_epochs_.find(current->id);
            if (found != raster_cache_epochs_.end())
                invalidate_raster_cache_layers(found->second.get());
        }
    }

    void publish_raster_cache_invalidation(Node& node) {
        mark_paint_culling_dirty();
        if (layout_transaction_active_ || !node.visual_bounds_published) {
            invalidate_paint(viewport_rect());
        } else {
            publish_subtree_visual_bounds_then_invalidate(node);
        }
    }

    [[nodiscard]] std::function<void()> raster_cache_invalidator(
        NodeId id, const detail::RasterCacheEpoch::Token& identity) {
        const auto weak_tree = retained_invalidation_weak();
        return [weak_tree, id, identity] {
            const auto lifetime = weak_tree.lock();
            if (!lifetime || !lifetime->tree) return;
            auto* tree = lifetime->tree;
            const auto entry = tree->raster_cache_epochs_.find(id);
            if (entry == tree->raster_cache_epochs_.end()) return;
            auto* layer = entry->second.get();
            for (; layer && !layer->epoch.same_lifetime(identity); layer = layer->inner.get()) {}
            if (!layer) return;
            auto* node = tree->retained_invalidation_node(id);
            if (!node) return;

            // A dependency changes this layer and its containers, not a clean
            // inner layer. Actual descendant invalidation still stales all.
            layer->acknowledge_dependencies();
            for (auto* outer = entry->second.get();; outer = outer->inner.get()) {
                outer->epoch.invalidate();
                if (outer == layer) break;
            }
            tree->invalidate_raster_cache_ancestry(node->parent);
            // All retained accesses precede the potentially throwing/reentrant
            // redraw notification, which may remove the owning subtree.
            tree->publish_raster_cache_invalidation(*node);
        };
    }

    [[nodiscard]] std::function<void()> raster_cache_invalidator(NodeId id) {
        const auto found = raster_cache_epochs_.find(id);
        return found == raster_cache_epochs_.end() ? std::function<void()>{}
            : raster_cache_invalidator(id, found->second->epoch.lifetime_token());
    }

    [[nodiscard]] std::unique_ptr<RetainedRasterCacheBoundary>
    prepare_raster_cache_boundary(const Node& node) {
        if (node.cached_layer) cached_layer_setup_checkpoint();
        auto prepared = std::make_unique<RetainedRasterCacheBoundary>();
        auto* layer = prepared.get();
        for (auto source = node.cached_layer; source; source = source->inner) {
            layer->source = source;
            layer->revisions.reserve(source->dependencies.size());
            layer->subscriptions.reserve(source->dependencies.size());
            cached_layer_setup_checkpoint();
            for (const auto& dependency : source->dependencies) {
                layer->revisions.push_back(dependency.revision());
                auto subscription = dependency.observe(
                    raster_cache_invalidator(node.id, layer->epoch.lifetime_token()));
                layer->subscriptions.push_back(std::move(subscription));
                cached_layer_setup_checkpoint();
            }
            if (source->inner) {
                layer->inner = std::make_unique<RetainedRasterCacheBoundary>();
                layer = layer->inner.get();
            }
        }
        if (node.cached_layer) cached_layer_setup_checkpoint();
        return prepared;
    }

    [[nodiscard]] bool register_raster_cache_boundary(NodeId id) {
        if (layout_transaction_active_ || scene_frame_transaction_active_ ||
            (lifecycle_transition_ != LifecycleTransition::None &&
             lifecycle_transition_ != LifecycleTransition::Mount)) {
            return false;
        }
        const auto* node = retained_invalidation_node(id);
        if (!node) return false;
        if (raster_cache_epochs_.contains(id)) return true;
        auto prepared = prepare_raster_cache_boundary(*node);
        // Publication is the last fallible step. Failed subscriptions, chain
        // allocation or map insertion release every already-created listener.
        raster_cache_epochs_.emplace(id, std::move(prepared));
        invalidate_raster_cache_ancestry(node->parent);
        return true;
    }

    void disconnect_raster_cache_dependencies() noexcept {
        for (auto& entry : raster_cache_epochs_) entry.second->disconnect();
    }

    void refresh_raster_cache_dependencies() {
        for (auto& entry : raster_cache_epochs_) {
            bool changed = false;
            for (auto* layer = entry.second.get(); layer; layer = layer->inner.get()) {
                if (layer->dependencies_current()) continue;
                layer->acknowledge_dependencies();
                for (auto* outer = entry.second.get();; outer = outer->inner.get()) {
                    outer->epoch.invalidate();
                    if (outer == layer) break;
                }
                changed = true;
            }
            if (!changed) continue;
            if (auto* node = retained_invalidation_node(entry.first)) {
                invalidate_raster_cache_ancestry(node->parent);
                publish_raster_cache_invalidation(*node);
            }
        }
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
        return found == raster_cache_epochs_.end() ||
               !found->second->dependencies_current()
            ? detail::RasterCacheEpoch::Token{} : found->second->epoch.capture();
    }

    [[nodiscard]] bool reusable_raster_cache_content(
        NodeId id, const detail::RasterCacheEpoch::Token& token) const noexcept {
        if (!raster_cache_snapshot_ready()) return false;
        const auto found = raster_cache_epochs_.find(id);
        return found != raster_cache_epochs_.end() &&
            found->second->dependencies_current() && found->second->epoch.reusable(token);
    }

    [[nodiscard]] bool committable_raster_cache_content(
        NodeId id, const detail::RasterCacheEpoch::Token& token) const noexcept {
        if (!raster_cache_snapshot_ready()) return false;
        const auto found = raster_cache_epochs_.find(id);
        return found != raster_cache_epochs_.end() &&
            found->second->dependencies_current() && found->second->epoch.committable(token);
    }

    [[nodiscard]] bool commit_raster_cache_content(
        NodeId id, const detail::RasterCacheEpoch::Token& token) noexcept {
        if (!raster_cache_snapshot_ready()) return false;
        const auto found = raster_cache_epochs_.find(id);
        return found != raster_cache_epochs_.end() &&
            found->second->dependencies_current() && found->second->epoch.commit(token);
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
