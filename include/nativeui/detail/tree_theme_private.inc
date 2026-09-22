    // Theme is ordinary per-tree value state. It intentionally precedes the
    // retained root in member declaration order so borrowed component theme
    // views cannot outlive it during Tree destruction.
    Theme theme_{default_theme()};

    [[nodiscard]] const Theme& inherited_theme_for(const Node& node) const noexcept {
        for (auto* parent = node.parent; parent; parent = parent->parent) {
            if (const auto* binding =
                    dynamic_cast<const detail::ThemeBinding*>(parent->component.get())) {
                return binding->descendant_theme();
            }
        }
        return theme_;
    }

    void bind_theme(Node& node, const Theme& inherited) noexcept {
        const Theme* descendants = &inherited;
        if (auto* binding = dynamic_cast<detail::ThemeBinding*>(node.component.get())) {
            binding->bind_theme(inherited);
            descendants = &binding->descendant_theme();
        }
        for (auto& child : node.children) bind_theme(*child, *descendants);
    }

    void bind_theme(Node& node) noexcept {
        bind_theme(node, node.parent ? inherited_theme_for(node) : theme_);
    }

    [[nodiscard]] static Node* find_theme_node(Node& node, NodeId id) noexcept {
        if (node.id == id) return &node;
        for (auto& child : node.children) {
            if (auto* result = find_theme_node(*child, id)) return result;
        }
        return nullptr;
    }

    void apply_scoped_theme_change(NodeId id, ThemeInvalidation invalidation) {
        if (!root_ || invalidation == ThemeInvalidation::None) return;
        mark_paint_culling_dirty();
        auto* node = find_theme_node(*root_, id);
        if (!node) return;

        // Re-resolve the complete lexical subtree from retained ancestry so a
        // changed outer scope refreshes nested scopes before any measurement or
        // paint callback observes the new value.
        bind_theme(*node);
        if (!mounted_) return;

        if (invalidation == ThemeInvalidation::Layout) {
            invalidate_layout_from(*node);
        } else {
            publish_subtree_visual_bounds_then_invalidate(*node);
        }
    }
