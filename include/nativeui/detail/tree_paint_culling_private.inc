    struct PaintCullEntry final {
        NodeId id{kInvalidNodeId};
        Rect own_visual_bounds{};
        Rect subtree_visual_bounds{};
        bool own_valid{};
        bool subtree_valid{};
    };

    struct PaintCullBoundsResult final {
        Rect bounds{};
        bool valid{};
    };

    struct PaintCullRegion final {
        Rect bounds{};
        bool valid{};
    };

    void mark_paint_culling_dirty() noexcept { paint_cull_cache_dirty_ = true; }

    // Long-lived retained invalidators enter through the wrappers compiled in
    // tree_retained_invalidation.inc. Mark uncertainty before any publication,
    // callback or structural work can run so a re-entrant paint traversal can
    // only lose culling, never under-cull with stale bounds.






    [[nodiscard]] static bool paint_cull_same_rect(Rect a, Rect b) noexcept {
        return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
    }

    [[nodiscard]] static bool paint_cull_same_size(Size a, Size b) noexcept {
        return a.w == b.w && a.h == b.h;
    }

    [[nodiscard]] PaintCullRegion paint_cull_canonical_repaint_region(
        Rect region) const noexcept {
        if (!std::isfinite(region.x) || !std::isfinite(region.y) ||
            !std::isfinite(region.w) || !std::isfinite(region.h) ||
            region.w < 0.0f || region.h < 0.0f) {
            return {{}, false};
        }
        if (region.w == 0.0f || region.h == 0.0f) return {{}, true};

        const Rect viewport = viewport_rect();
        const double left = (std::max)(static_cast<double>(viewport.x),
                                       static_cast<double>(region.x));
        const double top = (std::max)(static_cast<double>(viewport.y),
                                      static_cast<double>(region.y));
        const double right = (std::min)(
            static_cast<double>(viewport.x) + static_cast<double>(viewport.w),
            static_cast<double>(region.x) + static_cast<double>(region.w));
        const double bottom = (std::min)(
            static_cast<double>(viewport.y) + static_cast<double>(viewport.h),
            static_cast<double>(region.y) + static_cast<double>(region.h));
        if (!std::isfinite(left) || !std::isfinite(top) ||
            !std::isfinite(right) || !std::isfinite(bottom)) {
            return {{}, false};
        }
        if (!(left < right) || !(top < bottom)) return {{}, true};

        return {{
                    static_cast<float>(left),
                    static_cast<float>(top),
                    static_cast<float>(right - left),
                    static_cast<float>(bottom - top)},
                true};
    }

    [[nodiscard]] static std::size_t paint_cull_node_count(const Node& node) noexcept {
        std::size_t count = 1;
        for (const auto& child : node.children) count += paint_cull_node_count(*child);
        return count;
    }

    [[nodiscard]] PaintCullBoundsResult paint_cull_collect_cache(const Node& node) {
        PaintCullBoundsResult subtree{node.published_visual_bounds, node.visual_bounds_published};

        for (const auto& child : node.children) {
            const auto child_result = paint_cull_collect_cache(*child);
            if (!child_result.valid) {
                subtree.valid = false;
            } else if (subtree.valid) {
                subtree.bounds = unite(subtree.bounds, child_result.bounds);
            }
        }

        paint_cull_cache_.push_back(PaintCullEntry{
            node.id,
            node.published_visual_bounds,
            subtree.bounds,
            node.visual_bounds_published,
            subtree.valid});
        return subtree;
    }

    void paint_cull_rebuild_cache() noexcept {
        if (!root_) {
            paint_cull_cache_.clear();
            paint_cull_cache_dirty_ = false;
            paint_cull_cached_viewport_ = viewport_;
            return;
        }

        try {
            paint_cull_cache_.clear();
            paint_cull_cache_.reserve(paint_cull_node_count(*root_));
            (void)paint_cull_collect_cache(*root_);
            std::sort(
                paint_cull_cache_.begin(),
                paint_cull_cache_.end(),
                [](const PaintCullEntry& a, const PaintCullEntry& b) {
                    return a.id < b.id;
                });
            paint_cull_cached_viewport_ = viewport_;
            paint_cull_cache_dirty_ = false;
        } catch (...) {
            // Paint culling is an optimization cache. Allocation/storage failure is a
            // correctness-neutral fallback to ordinary full retained traversal.
            paint_cull_cache_.clear();
            paint_cull_cache_dirty_ = true;
        }
    }

    void paint_cull_prepare_cache() noexcept {
        if (!paint_cull_same_size(paint_cull_cached_viewport_, viewport_)) {
            paint_cull_cache_dirty_ = true;
        }
        if (paint_cull_cache_dirty_) paint_cull_rebuild_cache();
    }

    [[nodiscard]] const PaintCullEntry* paint_cull_cache_entry(NodeId id) const noexcept {
        if (paint_cull_cache_dirty_) return nullptr;
        const auto it = std::lower_bound(
            paint_cull_cache_.begin(),
            paint_cull_cache_.end(),
            id,
            [](const PaintCullEntry& entry, NodeId value) {
                return entry.id < value;
            });
        return it != paint_cull_cache_.end() && it->id == id ? &*it : nullptr;
    }

    void paint_cull_paint_node(const Node& node,
                         Painter& painter,
                         PlatformServices& platform,
                         Rect inherited_clip,
                         Rect repaint_region) {
        if (inherited_clip.empty() ||
            node.component->effective_visibility() != VisibilityMode::Visible ||
            intersect(inherited_clip, repaint_region).empty()) {
            return;
        }

        const auto* entry = paint_cull_cache_entry(node.id);
        if (entry && entry->subtree_valid &&
            intersect(entry->subtree_visual_bounds, repaint_region).empty()) {
            return;
        }

        const bool paint_own = !entry || !entry->own_valid ||
            !intersect(entry->own_visual_bounds, repaint_region).empty();
        if (paint_own) {
            const bool focused =
                focus_active_ && !focusables_.empty() && focusables_[focused_index_] == &node;
            PaintContext context{painter, node.bounds, inherited_clip, focused, platform};
            auto paint_scope = painter.scoped_state();
            node.component->paint(context);
        }

        if (node.component->clips_children()) {
            const Rect child_clip = intersect(inherited_clip, node.bounds);
            if (child_clip.empty() || intersect(child_clip, repaint_region).empty()) return;
            auto clip_scope = painter.scoped_clip(node.bounds);
            for (const auto& child : node.children) {
                paint_cull_paint_node(*child, painter, platform, child_clip, repaint_region);
            }
            return;
        }

        for (const auto& child : node.children) {
            paint_cull_paint_node(*child, painter, platform, inherited_clip, repaint_region);
        }
    }

    void paint_cull_paint_region(Painter& painter,
                                 PlatformServices& platform,
                                 Rect requested_region) {
        if (lifecycle_transition_active()) return;

        flush_dynamic_mutations();
        if (active_) {
            sync_availability_structure(platform);
            sync_focus_structure(platform);
        }
        ensure_layout();

        const auto canonical = paint_cull_canonical_repaint_region(requested_region);
        if (!root_) return;

        const Rect viewport = viewport_rect();

        // Invalid/unrepresentable traversal geometry is uncertainty, not an empty
        // repaint. Fail conservatively to the reference full retained traversal.
        if (!canonical.valid) {
            LifecycleTransitionScope paint_scope{
                *this, LifecycleTransition::Paint, true};
            paint_node(*root_, painter, platform, viewport);
            return;
        }

        const Rect region = canonical.bounds;
        if (region.empty()) return;

        // Full-root traversal does not need the culling cache at all. Besides
        // preserving the exact reference path, this keeps cache allocation/failure
        // out of a request that cannot benefit from selective traversal.
        if (paint_cull_same_rect(region, viewport)) {
            LifecycleTransitionScope paint_scope{
                *this, LifecycleTransition::Paint, true};
            paint_node(*root_, painter, platform, viewport);
            return;
        }

        paint_cull_prepare_cache();
        LifecycleTransitionScope paint_scope{
            *this, LifecycleTransition::Paint, true};
        paint_cull_paint_node(*root_, painter, platform, viewport, region);
    }

    std::vector<PaintCullEntry> paint_cull_cache_;
    Size paint_cull_cached_viewport_{-1.0f, -1.0f};
    bool paint_cull_cache_dirty_{true};
