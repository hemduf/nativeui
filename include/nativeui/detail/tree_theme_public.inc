    /// Borrow the current tree-owned Theme.
    [[nodiscard]] const Theme& theme() const noexcept { return theme_; }

    /// Replace the Theme and publish only the paint/layout invalidation required
    /// by the classified theme change.
    void set_theme(Theme value) {
        const auto invalidation = classify_theme_change(theme_, value);
        if (invalidation == ThemeInvalidation::None) {
            if (root_) bind_theme(*root_);
            return;
        }

        mark_paint_culling_dirty();
        theme_ = std::move(value);
        if (root_) bind_theme(*root_);
        if (!mounted_ || !root_) return;

        if (invalidation == ThemeInvalidation::Layout) {
            invalidate_layout_from(*root_);
        } else {
            publish_all_visual_bounds_then_invalidate();
        }
    }
