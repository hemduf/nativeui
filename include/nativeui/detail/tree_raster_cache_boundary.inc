    // Retained content identity is separate from renderer cache-entry presence.
    // This state contains no backend resource and is confined to one Tree.
    struct RasterCacheBoundarySnapshot final {
        NodeId node_id{kInvalidNodeId};
        std::uint64_t lifetime_identity{};
        std::uint64_t content_generation{};
        std::uint64_t invalidation_serial{};
        Size local_size{};
        bool content_stale{true};
    };

    struct RasterCacheBoundaryRecord final {
        NodeId node_id{kInvalidNodeId};
        std::uint64_t lifetime_identity{};
        std::uint64_t content_generation{1U};
        std::uint64_t invalidation_serial{};
        Size local_size{};
        bool content_stale{true};
        bool generation_exhausted{};
        bool invalidation_serial_exhausted{};
    };

    [[nodiscard]] RasterCacheBoundaryRecord* raster_cache_boundary_record(
        NodeId id) noexcept {
        const auto found = std::find_if(
            raster_cache_boundaries_.begin(),
            raster_cache_boundaries_.end(),
            [id](const RasterCacheBoundaryRecord& record) {
                return record.node_id == id;
            });
        return found == raster_cache_boundaries_.end() ? nullptr : &*found;
    }

    [[nodiscard]] const RasterCacheBoundaryRecord* raster_cache_boundary_record(
        NodeId id) const noexcept {
        const auto found = std::find_if(
            raster_cache_boundaries_.begin(),
            raster_cache_boundaries_.end(),
            [id](const RasterCacheBoundaryRecord& record) {
                return record.node_id == id;
            });
        return found == raster_cache_boundaries_.end() ? nullptr : &*found;
    }

    [[nodiscard]] bool raster_cache_node_is_live(NodeId id) const noexcept {
        return id != kInvalidNodeId &&
            std::binary_search(retained_invalidation_live_nodes_.begin(),
                               retained_invalidation_live_nodes_.end(), id);
    }

    [[nodiscard]] std::uint64_t allocate_raster_cache_lifetime_identity() noexcept {
        if (raster_cache_lifetime_identity_exhausted_) return 0U;

        const auto identity = next_raster_cache_lifetime_identity_;
        if (identity == std::numeric_limits<std::uint64_t>::max()) {
            raster_cache_lifetime_identity_exhausted_ = true;
        } else {
            ++next_raster_cache_lifetime_identity_;
        }
        return identity;
    }

    [[nodiscard]] static Size raster_cache_local_size(const Node& node) noexcept {
        const float width =
            std::isfinite(node.bounds.w) ? std::max(0.0f, node.bounds.w) : 0.0f;
        const float height =
            std::isfinite(node.bounds.h) ? std::max(0.0f, node.bounds.h) : 0.0f;
        return {width, height};
    }

    [[nodiscard]] static bool raster_cache_same_size(Size a, Size b) noexcept {
        return a.w == b.w && a.h == b.h;
    }

    [[nodiscard]] bool register_raster_cache_boundary(Node& node) {
        if (!raster_cache_node_is_live(node.id)) return false;
        if (raster_cache_boundary_record(node.id)) return true;

        RasterCacheBoundaryRecord record;
        record.node_id = node.id;
        record.lifetime_identity = allocate_raster_cache_lifetime_identity();
        record.local_size = raster_cache_local_size(node);
        raster_cache_boundaries_.push_back(record);
        return true;
    }

    void unregister_raster_cache_boundary(NodeId id) noexcept {
        std::erase_if(
            raster_cache_boundaries_,
            [id](const RasterCacheBoundaryRecord& record) {
                return record.node_id == id;
            });
    }

    static void advance_raster_cache_invalidation_serial(
        RasterCacheBoundaryRecord& record) noexcept {
        if (record.invalidation_serial_exhausted) return;
        if (record.invalidation_serial == std::numeric_limits<std::uint64_t>::max()) {
            record.invalidation_serial_exhausted = true;
            return;
        }
        ++record.invalidation_serial;
    }

    static void mark_raster_cache_record_stale(
        RasterCacheBoundaryRecord& record) noexcept {
        advance_raster_cache_invalidation_serial(record);
        if (record.content_stale) return;

        record.content_stale = true;
        if (record.generation_exhausted) return;
        if (record.content_generation == std::numeric_limits<std::uint64_t>::max()) {
            record.generation_exhausted = true;
            return;
        }
        ++record.content_generation;
    }

    void mark_raster_cache_content_stale_from(Node& node) noexcept {
        for (auto* current = &node; current; current = current->parent) {
            if (auto* record = raster_cache_boundary_record(current->id)) {
                mark_raster_cache_record_stale(*record);
            }
        }
    }

    void mark_raster_cache_subtree_stale(
        Node& node, NodeId skip = kInvalidNodeId) noexcept {
        if (node.id != skip) {
            if (auto* record = raster_cache_boundary_record(node.id)) {
                mark_raster_cache_record_stale(*record);
            }
        }
        for (auto& child : node.children) {
            mark_raster_cache_subtree_stale(*child, skip);
        }
    }

    void mark_raster_cache_subtree_and_ancestors_stale(Node& node) noexcept {
        mark_raster_cache_content_stale_from(node);
        mark_raster_cache_subtree_stale(node, node.id);
    }

    void mark_all_raster_cache_content_stale() noexcept {
        for (auto& record : raster_cache_boundaries_) {
            mark_raster_cache_record_stale(record);
        }
    }

    void note_raster_cache_layout_size(Node& node, Rect candidate) noexcept {
        auto* record = raster_cache_boundary_record(node.id);
        if (!record) return;

        const Size next{
            std::isfinite(candidate.w) ? std::max(0.0f, candidate.w) : 0.0f,
            std::isfinite(candidate.h) ? std::max(0.0f, candidate.h) : 0.0f};
        if (raster_cache_same_size(record->local_size, next)) return;

        record->local_size = next;
        mark_raster_cache_record_stale(*record);
    }

    [[nodiscard]] std::optional<RasterCacheBoundarySnapshot>
    raster_cache_boundary_snapshot(NodeId id) const noexcept {
        const auto* record = raster_cache_boundary_record(id);
        if (!record || record->lifetime_identity == 0U ||
            record->generation_exhausted ||
            record->invalidation_serial_exhausted ||
            !raster_cache_node_is_live(id)) {
            return std::nullopt;
        }

        return RasterCacheBoundarySnapshot{
            record->node_id,
            record->lifetime_identity,
            record->content_generation,
            record->invalidation_serial,
            record->local_size,
            record->content_stale};
    }

    [[nodiscard]] bool try_commit_raster_cache_boundary(
        const RasterCacheBoundarySnapshot& snapshot) noexcept {
        auto* record = raster_cache_boundary_record(snapshot.node_id);
        if (!record ||
            record->content_stale != snapshot.content_stale ||
            record->lifetime_identity == 0U ||
            record->generation_exhausted ||
            record->invalidation_serial_exhausted ||
            !raster_cache_node_is_live(snapshot.node_id) ||
            record->lifetime_identity != snapshot.lifetime_identity ||
            record->content_generation != snapshot.content_generation ||
            record->invalidation_serial != snapshot.invalidation_serial ||
            !raster_cache_same_size(record->local_size, snapshot.local_size)) {
            return false;
        }

        record->content_stale = false;
        return true;
    }

    std::vector<RasterCacheBoundaryRecord> raster_cache_boundaries_;
    std::uint64_t next_raster_cache_lifetime_identity_{1U};
    bool raster_cache_lifetime_identity_exhausted_{};

