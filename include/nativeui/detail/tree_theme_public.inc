    /// Borrow the current tree-owned Theme.
    ///
    /// The reference ends with Tree lifetime and should not be retained across
    /// `set_theme()`; copy when an independent lifetime is needed.
    [[nodiscard]] const Theme& theme() const noexcept { return theme_; }

    /// Consume a Theme value and publish only the required invalidation.
    ///
    /// Mounted theme-aware components are rebound before paint/layout publication.
    /// Theme-owned strings/vectors may allocate. Synchronous invalidation callback
    /// reentrancy/error semantics are the same as other Tree invalidation APIs.
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
