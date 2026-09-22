    struct T094CullEntry final {
        NodeId id{kInvalidNodeId};
        Rect own_visual_bounds{};
        Rect subtree_visual_bounds{};
        bool own_valid{};
        bool subtree_valid{};
    };

    struct T094BoundsResult final {
        Rect bounds{};
        bool valid{};
    };

    void t094_mark_cache_dirty() noexcept { t094_cache_dirty_ = true; }

    // Long-lived retained invalidators enter through the wrappers compiled in
    // tree_retained_invalidation.inc. Mark uncertainty before any publication,
    // callback or structural work can run so a re-entrant paint traversal can
    // only lose culling, never under-cull with stale bounds.
    void t094_invalidate_node_paint(Node& node) {
        t094_mark_cache_dirty();
        invalidate_node_paint(node);
    }

    void t094_invalidate_layout_from(Node& node) {
        t094_mark_cache_dirty();
        invalidate_layout_from(node);
    }

    void t094_queue_dynamic_mutation(NodeId owner) {
        t094_mark_cache_dirty();
        queue_dynamic_mutation(owner);
    }

    void t094_apply_scoped_theme_change(NodeId id, ThemeInvalidation invalidation) {
        t094_mark_cache_dirty();
        apply_scoped_theme_change(id, invalidation);
    }

    void t094_sync_availability_inactive() {
        t094_mark_cache_dirty();
        sync_availability_inactive();
    }

    void t094_sync_availability_structure(PlatformServices& platform) {
        t094_mark_cache_dirty();
        sync_availability_structure(platform);
    }

    [[nodiscard]] static bool t094_same_rect(Rect a, Rect b) noexcept {
        return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
    }

    [[nodiscard]] static bool t094_same_size(Size a, Size b) noexcept {
        return a.w == b.w && a.h == b.h;
    }

    [[nodiscard]] Rect t094_canonical_repaint_region(Rect region) const noexcept {
        if (!std::isfinite(region.x) || !std::isfinite(region.y) ||
            !std::isfinite(region.w) || !std::isfinite(region.h) ||
            !(region.w > 0.0f) || !(region.h > 0.0f)) {
            return {};
        }

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
            !std::isfinite(right) || !std::isfinite(bottom) ||
            !(left < right) || !(top < bottom)) {
            return {};
        }

        return {static_cast<float>(left),
                static_cast<float>(top),
                static_cast<float>(right - left),
                static_cast<float>(bottom - top)};
    }

    [[nodiscard]] static std::size_t t094_node_count(const Node& node) noexcept {
        std::size_t count = 1;
        for (const auto& child : node.children) count += t094_node_count(*child);
        return count;
    }

    [[nodiscard]] T094BoundsResult t094_collect_cache(const Node& node) {
        T094BoundsResult subtree{node.published_visual_bounds, node.visual_bounds_published};

        for (const auto& child : node.children) {
            const auto child_result = t094_collect_cache(*child);
            if (!child_result.valid) {
                subtree.valid = false;
            } else if (subtree.valid) {
                subtree.bounds = unite(subtree.bounds, child_result.bounds);
            }
        }

        t094_cull_cache_.push_back(T094CullEntry{
            node.id,
            node.published_visual_bounds,
            subtree.bounds,
            node.visual_bounds_published,
            subtree.valid});
        return subtree;
    }

    void t094_rebuild_cache() noexcept {
        if (!root_) {
            t094_cull_cache_.clear();
            t094_cache_dirty_ = false;
            t094_cached_viewport_ = viewport_;
            return;
        }

        try {
            t094_cull_cache_.clear();
            t094_cull_cache_.reserve(t094_node_count(*root_));
            (void)t094_collect_cache(*root_);
            std::sort(
                t094_cull_cache_.begin(),
                t094_cull_cache_.end(),
                [](const T094CullEntry& a, const T094CullEntry& b) {
                    return a.id < b.id;
                });
            t094_cached_viewport_ = viewport_;
            t094_cache_dirty_ = false;
        } catch (...) {
            // T094 is an optimization cache. Allocation/storage failure is a
            // correctness-neutral fallback to ordinary full retained traversal.
            t094_cull_cache_.clear();
            t094_cache_dirty_ = true;
        }
    }

    void t094_prepare_cache() noexcept {
        if (!t094_same_size(t094_cached_viewport_, viewport_) || !paint_dirty_.empty()) {
            t094_cache_dirty_ = true;
        }
        if (t094_cache_dirty_) t094_rebuild_cache();
    }

    [[nodiscard]] const T094CullEntry* t094_cache_entry(NodeId id) const noexcept {
        if (t094_cache_dirty_) return nullptr;
        const auto it = std::lower_bound(
            t094_cull_cache_.begin(),
            t094_cull_cache_.end(),
            id,
            [](const T094CullEntry& entry, NodeId value) {
                return entry.id < value;
            });
        return it != t094_cull_cache_.end() && it->id == id ? &*it : nullptr;
    }

    void t094_paint_node(const Node& node,
                         Painter& painter,
                         PlatformServices& platform,
                         Rect inherited_clip,
                         Rect repaint_region) {
        if (inherited_clip.empty() ||
            node.component->effective_visibility() != VisibilityMode::Visible ||
            intersect(inherited_clip, repaint_region).empty()) {
            return;
        }

        const auto* entry = t094_cache_entry(node.id);
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
                t094_paint_node(*child, painter, platform, child_clip, repaint_region);
            }
            return;
        }

        for (const auto& child : node.children) {
            t094_paint_node(*child, painter, platform, inherited_clip, repaint_region);
        }
    }

    void t094_paint_region(SkCanvas& canvas,
                           PlatformServices& platform,
                           Rect requested_region) {
        if (lifecycle_transition_active()) return;

        flush_dynamic_mutations();
        if (active_) {
            sync_availability_structure(platform);
            sync_focus_structure(platform);
        }
        ensure_layout();

        const Rect region = t094_canonical_repaint_region(requested_region);
        if (region.empty() || !root_) return;

        t094_prepare_cache();
        Painter painter{canvas};
        const Rect viewport = viewport_rect();

        // A full-root request remains the exact pre-T094 reference traversal,
        // including callback order for zero-area/unknown visual metadata.
        if (t094_same_rect(region, viewport)) {
            paint_node(*root_, painter, platform, viewport);
            return;
        }

        t094_paint_node(*root_, painter, platform, viewport, region);
    }

    std::vector<T094CullEntry> t094_cull_cache_;
    Size t094_cached_viewport_{-1.0f, -1.0f};
    bool t094_cache_dirty_{true};
