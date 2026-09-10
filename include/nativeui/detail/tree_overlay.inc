    [[nodiscard]] std::optional<Rect> overlay_anchor_bounds(NodeId id) const noexcept {
        if (!root_ || id == kInvalidNodeId) return std::nullopt;
        const auto* node = find_node(*root_, id);
        if (!node) return std::nullopt;
        return node->bounds;
    }
